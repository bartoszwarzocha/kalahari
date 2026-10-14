/// @file document_coordinator.cpp
/// @brief Document lifecycle and file operations coordination implementation
///
/// OpenSpec #00038 - Phase 7: Extract Document Operations from MainWindow

#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/utils/tab_focus.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/user_project_dialog.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/standalone_file.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/style_resolver.h"
#include "kalahari/editor/statistics_collector.h"
#include <QMainWindow>
#include <QTabBar>
#include <QTabWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>

Q_DECLARE_METATYPE(kalahari::core::StandaloneFile)

namespace kalahari {
namespace gui {

namespace {

/// How long the status bar says where a book created without opening is: the only sign of it
constexpr int CREATED_WITHOUT_OPENING_MS = 8000;

/// How long the status bar keeps a sentence that says why a command did what it did: that Save
/// As does not save a chapter of the book on its own, that a file of the book opens as its
/// element
constexpr int EXPLANATION_MS = 5000;

/// What an editor tab shows, which decides how it is saved
enum class EditorKind {
    ProjectChapter,  ///< Chapter of the open project (saved with the project)
    StandaloneFile,  ///< Chapter or text file outside the project, or a new one without a file
                     ///< yet (saved on its own)
    Opening          ///< A tab before it knows what it shows: nothing to save
};

EditorKind editorKind(const EditorPanel* editor) {
    if (!editor->property("elementId").toString().isEmpty()
        && core::ProjectManager::getInstance().isProjectOpen()) {
        return EditorKind::ProjectChapter;
    }
    if (editor->property("isStandaloneFile").toBool()) {
        return EditorKind::StandaloneFile;
    }
    return EditorKind::Opening;
}

/// File of a standalone file tab
core::StandaloneFile standaloneFileOf(const EditorPanel* editor) {
    return editor->property("standaloneFile").value<core::StandaloneFile>();
}

/// What is wrong, one problem per line, for the details of a message
QString detailsOf(const QStringList& problems) {
    return problems.join(QLatin1Char('\n'));
}

/// The key of the file of a standalone file tab in the Navigator: its path, or the key of a new
/// file that has no file yet
QString listKeyOf(const EditorPanel* editor) {
    const QString path = editor->property("standaloneFilePath").toString();
    return path.isEmpty() ? editor->property("newFileKey").toString() : path;
}

/// Icon of a standalone file's tab
QString standaloneIconId(const QString& path) {
    return core::StandaloneFile::typeOf(path) == core::StandaloneFile::Type::Chapter
        ? QStringLiteral("template.chapter")
        : QStringLiteral("common.file");
}

}  // anonymous namespace

DocumentCoordinator::DocumentCoordinator(QMainWindow* mainWindow,
                                           QTabWidget* centralTabs,
                                           NavigatorPanel* navigatorPanel,
                                           PropertiesPanel* propertiesPanel,
                                           DashboardPanel* dashboardPanel,
                                           NavigatorCoordinator* navigatorCoordinator,
                                           StandaloneInfoBar* standaloneInfoBar,
                                           QStatusBar* statusBar,
                                           WindowTitleUpdater updateTitle,
                                           HasUnsavedChangesGetter hasUnsavedChanges,
                                           QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_centralTabs(centralTabs)
    , m_navigatorPanel(navigatorPanel)
    , m_propertiesPanel(propertiesPanel)
    , m_dashboardPanel(dashboardPanel)
    , m_navigatorCoordinator(navigatorCoordinator)
    , m_standaloneInfoBar(standaloneInfoBar)
    , m_statusBar(statusBar)
    , m_updateWindowTitle(std::move(updateTitle))
    , m_hasUnsavedChanges(std::move(hasUnsavedChanges))
{
    // Files opened outside the project: the info bar shows the file of the current tab,
    // and the navigator's "Other Files" open, add and drop them
    connect(m_centralTabs, &QTabWidget::currentChanged,
            this, &DocumentCoordinator::updateStandaloneInfoBar);
    connect(m_standaloneInfoBar, &StandaloneInfoBar::dismissed, this, [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->setProperty("infoBarDismissed", true);  // closed for this tab
        }
    });
    connect(m_navigatorPanel, &NavigatorPanel::standaloneFileSelected,
            this, &DocumentCoordinator::openStandaloneFile);
    connect(m_navigatorPanel, &NavigatorPanel::requestAddToProject,
            this, &DocumentCoordinator::addToProject);
    connect(m_navigatorPanel, &NavigatorPanel::requestRemoveStandaloneFile,
            this, &DocumentCoordinator::removeStandaloneFile);

    auto& logger = core::Logger::getInstance();
    logger.debug("DocumentCoordinator created");
}

EditorPanel* DocumentCoordinator::getCurrentEditor() const {
    if (!m_centralTabs) {
        return nullptr;
    }
    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);
}

// =============================================================================
// Document Operations
// =============================================================================

void DocumentCoordinator::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New File");

    //: The tab of a new file outside a book, before it is saved; %1: its number. In Polish:
    //: Bez tytułu %1
    const QString untitled = tr("Untitled %1").arg(m_newFiles + 1);

    // With a book open, the new file is an element of the book, of the kind and at the place
    // the writer chooses in the New File window, or an empty text file, with the book or
    // outside it
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        if (m_navigatorCoordinator) {
            using Made = NavigatorCoordinator::NewFile::Made;
            const NavigatorCoordinator::NewFile file =
                m_navigatorCoordinator->onRequestNewFile(untitled);
            if (file.made == Made::BookFile || file.made == Made::OutsideFile) {
                ++m_newFiles;
            }
            if (file.made == Made::OutsideFile) {
                openNewFile(file.title);
            }
        }
        return;
    }

    // Without a book, an empty file outside one
    ++m_newFiles;
    openNewFile(untitled);
}

void DocumentCoordinator::openNewFile(const QString& title) {
    auto& logger = core::Logger::getInstance();
    auto* newEditor = new EditorPanel(m_mainWindow);
    const QString iconId = QStringLiteral("template.chapter");
    const int tabIndex = m_centralTabs->addTab(
        newEditor, core::ArtProvider::getInstance().getIcon(iconId), title);
    newEditor->setProperty("tabIconId", iconId);
    newEditor->setProperty("standaloneFile", QVariant::fromValue(core::StandaloneFile()));
    newEditor->setProperty("standaloneFilePath", QString());
    newEditor->setProperty("isStandaloneFile", true);
    newEditor->setProperty("dirty", false);
    newEditor->setContent(QString());

    // The Navigator lists it at once with the files outside the book, by the name of its tab,
    // until its first save gives it a file; it leaves the list with its tab
    const QString key = QStringLiteral("untitled:%1").arg(++m_newFileKeys);
    newEditor->setProperty("newFileKey", key);
    newEditor->setProperty("newFileName", title);
    m_navigatorPanel->addNewFile(key, title);
    NavigatorPanel* navigator = m_navigatorPanel;
    connect(newEditor, &QObject::destroyed, navigator,
            [navigator, key]() { navigator->removeStandaloneFile(key); });

    m_centralTabs->setCurrentIndex(tabIndex);
    trackStandaloneChanges(newEditor);
    updateStandaloneInfoBar();

    logger.info("New file outside a book: {}", title.toStdString());
    m_statusBar->showMessage(tr("New file created"), 2000);
}

void DocumentCoordinator::onNewProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Project (OpenSpec #00033)");
    auto& pm = core::ProjectManager::getInstance();

    // When a book is open, the window that creates the new book asks whether the new book
    // takes its place or is created without opening; Cancel returns to that window
    const dialogs::NewBookQuestion question = [this](QWidget* window, const QString& title) {
        // Cancel returns to the window that asks: the New Book window, or the window of the
        // kinds of a user project
        const bool fromKinds = qobject_cast<dialogs::UserProjectDialog*>(window) != nullptr;
        const OpenBookChoice choice = askAboutOpenBook(
            window,
            //: %1: the title of the new book. In Polish: Nowa książka „%1” może zastąpić
            //: otwartą albo powstać bez otwierania.
            tr("The new book \"%1\" can take the place of the open one or be created without "
               "opening.")
                .arg(title),
            fromKinds
                //: In Polish: Książkę utworzoną bez otwierania znajdziesz na Pulpicie
                //: programu i w menu Plik > Ostatnie książki. „Anuluj” wraca do okna „Nowa
                //: książka – projekt użytkownika” z zaznaczonymi rodzajami.
                ? tr("A book created without opening is on the Dashboard and in File > Recent "
                     "Books. Cancel returns to the New Book – User Project window with the "
                     "kinds you checked.")
                //: In Polish: Książkę utworzoną bez otwierania znajdziesz na Pulpicie
                //: programu i w menu Plik > Ostatnie książki. „Anuluj” wraca do okna „Nowa
                //: książka” z wpisanymi danymi.
                : tr("A book created without opening is on the Dashboard and in File > Recent "
                     "Books. Cancel returns to the New Book window with your data."),
            //: In Polish: Zamknij i otwórz nową
            tr("Close and Open New"),
            //: In Polish: Utwórz bez otwierania
            tr("Create Without Opening"),
            tr("Do you want to save changes to '%1' before creating the new book?"));
        switch (choice) {
            case OpenBookChoice::Close:
                return dialogs::NewBookOpening::Open;
            case OpenBookChoice::KeepOpen:
                return dialogs::NewBookOpening::DontOpen;
            case OpenBookChoice::Cancel:
                break;
        }
        return dialogs::NewBookOpening::Cancel;
    };

    // The New Book window and, for a user project, the window of its kinds, whose Back
    // returns to the New Book window; both keep what the writer chose in them
    dialogs::NewItemDialog dialog(dialogs::NewItemMode::Project, m_mainWindow);
    dialog.setOpeningQuestion(question);
    std::unique_ptr<dialogs::UserProjectDialog> kindsDialog;
    dialogs::NewItemResult result;
    core::NewProject request;
    bool open = true;
    while (true) {
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        result = dialog.result();
        request = core::NewProject{};
        request.parentDir = result.location;
        request.title = result.title;
        request.author = result.author;
        request.language = result.language;
        request.createSubfolder = result.createSubfolder;
        request.typeId =
            dialogs::TemplateRegistry::getInstance().getTemplate(result.templateId).typeId;
        request.sections = result.sections;
        if (!dialogs::NewItemDialog::isUserProject(result.templateId)) {
            open = result.open;
            break;
        }
        if (!kindsDialog) {
            kindsDialog =
                std::make_unique<dialogs::UserProjectDialog>(pm.bookTypes(), m_mainWindow);
            kindsDialog->setOpeningQuestion(question);
        }
        kindsDialog->setBook(result.title, result.language, result.sections);
        const int answer = kindsDialog->exec();
        if (answer == QDialog::Accepted) {
            request.palette = kindsDialog->choice();
            open = kindsDialog->opens();
            break;
        }
        if (answer != dialogs::UserProjectDialog::BACK) {
            return;
        }
    }

    QStringList problems;
    if (!open) {
        // The open book stays open; the new one is on the Dashboard and in Recent Books
        const QString manifest = pm.createProjectWithoutOpening(request, &problems);
        if (manifest.isEmpty()) {
            logger.error("Failed to create project: {}", result.title.toStdString());
            dialogs::MessageDialog::error(m_mainWindow, tr("New Book"),
                                          tr("Could not create the book '%1'.").arg(result.title),
                                          detailsOf(problems));
            return;
        }
        // The next new book starts in the folder this one was created in
        const QString projectFolder = QFileInfo(manifest).absolutePath();
        ProgramFolders::remember(ProgramFolders::Operation::NewBook,
                                 QFileInfo(projectFolder).absolutePath());
        core::RecentBooksManager::getInstance().addRecentFile(manifest);
        logger.info("Project created without opening: {} in {}", result.title.toStdString(),
                    projectFolder.toStdString());
        //: %1: the title of the new book. In Polish: Utworzono książkę „%1” bez otwierania.
        //: Znajdziesz ją na Pulpicie programu i w menu Plik > Ostatnie książki.
        m_statusBar->showMessage(tr("Book \"%1\" created without opening. It is on the "
                                    "Dashboard and in File > Recent Books.")
                                     .arg(result.title),
                                 CREATED_WITHOUT_OPENING_MS);
        return;
    }

    // ProjectManager closes the open book, creates the new one and opens it, and
    // onProjectOpened() shows it
    if (!pm.createProject(request, &problems)) {
        logger.error("Failed to create project: {}", result.title.toStdString());
        dialogs::MessageDialog::error(m_mainWindow, tr("New Book"),
                                      tr("Could not create the book '%1'.").arg(result.title),
                                      detailsOf(problems));
        return;
    }

    // The next new book starts in the folder this one was created in
    ProgramFolders::remember(ProgramFolders::Operation::NewBook,
                             QFileInfo(pm.getProjectPath()).absolutePath());

    m_updateWindowTitle();
    logger.info("Project created: {} in {}", result.title.toStdString(),
                result.location.toStdString());
    m_statusBar->showMessage(tr("Book created: %1").arg(result.title), 3000);
    emit documentOpened();

    // The writer can start writing at once
    openFirstText();
}

bool DocumentCoordinator::agreeToCloseBook(const QString& saveQuestion,
                                           const QString& closeQuestion) {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        return true;
    }

    QString bookName = pm.book() ? pm.book()->title : QString();
    if (bookName.isEmpty()) {
        bookName = QFileInfo(pm.getProjectPath()).fileName();
    }

    // The check keeps an annotation being written first. Files open outside the book stay
    // open with their changes, so only the changes of the book are asked about.
    if (m_hasUnsavedChanges() && bookHasUnsavedChanges()) {
        // Dirty: offer to save before switching
        const auto reply = dialogs::MessageDialog::ask(m_mainWindow, tr("Unsaved Changes"),
                                                       saveQuestion.arg(bookName), tr("&Save"),
                                                       tr("Do&n't Save"));
        if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return false;
        }
        if (reply == dialogs::MessageDialog::Answer::Accept) {
            onSaveBook();
            return !bookHasUnsavedChanges();  // a failed save stops the command
        }
        return true;  // Don't Save: the changes go with the book
    }

    // Clean: plain confirmation to avoid an accidental project switch. ProjectManager closes
    // the book, and its projectAboutToClose() prepares the services for that.
    return dialogs::MessageDialog::confirm(m_mainWindow, tr("Close Book?"),
                                           closeQuestion.arg(bookName), tr("&Close Book"));
}

DocumentCoordinator::OpenBookChoice DocumentCoordinator::askAboutOpenBook(
    QWidget* parent, const QString& description, const QString& note, const QString& closeText,
    const QString& keepText, const QString& saveQuestion) {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        return OpenBookChoice::Close;
    }

    QString bookName = pm.book() ? pm.book()->title : QString();
    if (bookName.isEmpty()) {
        bookName = QFileInfo(pm.getProjectPath()).fileName();
    }

    //: The heading of the question; %1: the title of the open book. In Polish: Zamknąć „%1”?
    const QString heading = tr("Close \"%1\"?").arg(bookName);
    dialogs::MessageDialog question(dialogs::MessageDialog::Kind::Question, heading, note, parent);
    question.setHeading(heading, description);
    question.setWindowTitle(tr("Close Book?"));
    question.setQuestionButtons(closeText);
    question.setAlternativeText(keepText);
    question.exec();
    switch (question.answer()) {
        case dialogs::MessageDialog::Answer::Accept:
            break;
        case dialogs::MessageDialog::Answer::Alternative:
            return OpenBookChoice::KeepOpen;
        case dialogs::MessageDialog::Answer::Cancel:
            return OpenBookChoice::Cancel;
    }

    // The open book closes: its changes are saved first, or not, as the writer says. The
    // standalone files stay open without the book, with their changes.
    if (bookHasUnsavedChanges()) {
        const auto reply = dialogs::MessageDialog::ask(parent, tr("Unsaved Changes"),
                                                       saveQuestion.arg(bookName), tr("&Save"),
                                                       tr("Do&n't Save"));
        if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return OpenBookChoice::Cancel;
        }
        if (reply == dialogs::MessageDialog::Answer::Accept && !saveBook()) {
            dialogs::MessageDialog::warning(
                parent, tr("Save Warning"),
                tr("Some changes in the book could not be saved. Check the log for details."));
            return OpenBookChoice::Cancel;  // a failed save stops the command
        }
    }
    return OpenBookChoice::Close;
}

void DocumentCoordinator::showOpenError(const QString& path, const QStringList& problems,
                                        core::OpenFailure failure) {
    const QFileInfo file(path);
    const QString nativePath = QDir::toNativeSeparators(file.absoluteFilePath());

    // A book of a newer version is not damaged: the writer needs a newer program
    if (failure == core::OpenFailure::Newer) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Book of a Newer Version"),
            tr("This book was saved in a newer version of Kalahari. Update the program to "
               "open it."),
            nativePath);
        return;
    }

    // Under Show Details: the file, then what is wrong, in the words of the log
    //: The first line of the details of a book that cannot be opened. In Polish: Plik: %1
    const QString details = detailsOf(QStringList{tr("File: %1").arg(nativePath)} + problems);

    // Another copy of the program has the book open (another build of it, which runs on its
    // own): the book can be closed there and tried again
    if (failure == core::OpenFailure::OpenElsewhere) {
        QStringList ignored;
        const std::optional<core::BookProject> project = core::BookProject::load(path, ignored);
        QString title = project && !project->books.isEmpty() ? project->books.first().title
                                                             : QString();
        if (title.isEmpty()) {
            title = file.completeBaseName();
        }
        //: The window of a book that another copy of the program has open. In Polish:
        //: Książka jest już otwarta
        dialogs::MessageDialog message(
            dialogs::MessageDialog::Kind::Information, tr("Book Already Open"),
            //: %1: the title of the book. In Polish: Książka „%1” jest już otwarta w innym
            //: oknie programu Kalahari. Zamknij ją tam i kliknij „Spróbuj ponownie”.
            tr("The book \"%1\" is already open in another window of Kalahari. Close it there "
               "and click Try Again.")
                .arg(title),
            m_mainWindow);
        message.setDetails(details);
        //: In Polish: &Spróbuj ponownie
        message.setQuestionButtons(tr("&Try Again"), tr("Close"));
        message.exec();
        if (message.answer() == dialogs::MessageDialog::Answer::Accept) {
            // The same way as from the recent books: the open book is asked about again
            QTimer::singleShot(0, this, [this, path]() { onOpenRecentFile(path); });
        }
        return;
    }

    QString title;
    QString text;
    switch (failure) {
        case core::OpenFailure::Missing:
            //: In Polish: Nie znaleziono książki
            title = tr("Book Not Found");
            //: %1: the name of the book file, %2: its folder. In Polish: W folderze „%2” nie
            //: ma pliku książki „%1”. Plik mógł zostać przeniesiony, usunięty albo mieć
            //: zmienioną nazwę.
            text = tr("There is no book file \"%1\" in the folder \"%2\". The file may have "
                      "been moved, deleted or renamed.")
                       .arg(file.fileName(), QDir::toNativeSeparators(file.absolutePath()));
            break;
        case core::OpenFailure::NotBook:
            //: In Polish: To nie jest książka
            title = tr("Not a Book");
            //: %1: the name of the file. In Polish: „%1” nie jest książką programu Kalahari.
            //: Plik książki ma rozszerzenie .klh.
            text = tr("\"%1\" is not a Kalahari book. A book file has the extension .klh.")
                       .arg(file.fileName());
            break;
        case core::OpenFailure::Older:
            //: In Polish: Książka starszej wersji
            title = tr("Book of an Older Version");
            //: In Polish: Tę książkę zapisała starsza wersja programu Kalahari, której
            //: formatu ta wersja jeszcze nie odczytuje.
            text = tr("This book was saved by an older version of Kalahari, whose format this "
                      "version cannot read yet.");
            break;
        case core::OpenFailure::Database:
            //: In Polish: Nie można otworzyć książki
            title = tr("Book Cannot Be Opened");
            //: In Polish: Nie można otworzyć bazy danych książki: plik project.db w jej
            //: folderze może być uszkodzony albo używany przez inny program.
            text = tr("The database of the book cannot be opened: the file project.db in its "
                      "folder may be damaged or used by another program.");
            break;
        case core::OpenFailure::CloseFailed:
            title = tr("Book Cannot Be Opened");
            //: In Polish: Nie można zamknąć otwartej książki, więc nie otwarto drugiej.
            //: Szczegóły są w dzienniku programu.
            text = tr("The open book cannot be closed, so the other one was not opened. The "
                      "program's log has the details.");
            break;
        default:
            //: In Polish: Nie można odczytać książki
            title = tr("Book Cannot Be Read");
            //: %1: the name of the book file. In Polish: Plik książki „%1” jest uszkodzony
            //: albo niepełny, więc program nie może go odczytać. Co jest w nim nie tak, pokazuje
            //: przycisk „Pokaż szczegóły”.
            text = tr("The book file \"%1\" is damaged or incomplete, so the program cannot "
                      "read it. Show Details says what is wrong in it.")
                       .arg(file.fileName());
            break;
    }
    dialogs::MessageDialog::error(m_mainWindow, title, text, details);
}

bool DocumentCoordinator::reportOpenFailure(const QString& path) {
    QStringList problems;
    const core::OpenFailure failure = core::ProjectManager::openFailure(path, &problems);
    if (failure == core::OpenFailure::None) {
        return false;
    }
    showOpenError(path, problems, failure);
    return true;
}

void DocumentCoordinator::openFirstText() {
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectBook* book = pm.book();
    if (!book || !m_navigatorCoordinator) {
        return;
    }

    // The first text of the body, also inside a part
    const std::function<const core::ProjectElement*(const QList<core::ProjectElement>&)>
        firstText = [&pm, &firstText](const QList<core::ProjectElement>& elements)
        -> const core::ProjectElement* {
        for (const core::ProjectElement& element : elements) {
            if (pm.formOf(element) == core::ElementForm::Text) {
                return &element;
            }
            if (const core::ProjectElement* inner = firstText(element.elements)) {
                return inner;
            }
        }
        return nullptr;
    };
    if (const core::ProjectElement* text = firstText(book->mainElements)) {
        m_navigatorCoordinator->onElementSelected(text->id, text->title);
    }
}

void DocumentCoordinator::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // Use ProjectManager for opening projects
    QString filename = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open Book"),
        ProgramFolders::windowFolder(ProgramFolders::Operation::OpenBook),
        tr("Kalahari Books (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Open cancelled by user");
        return;
    }
    // The next window starts in the folder that holds the folder of this book, with the
    // folders of the other books
    ProgramFolders::remember(ProgramFolders::Operation::OpenBook,
                             QFileInfo(QFileInfo(filename).absolutePath()).absolutePath());

    // Use ProjectManager to open the project
    auto& pm = core::ProjectManager::getInstance();

    // If a project is already open, ask user for confirmation before closing it
    if (pm.isProjectOpen()) {
        // If the chosen file IS the already-open project, just ignore — no close/reopen
        // prompt. Compare against the MANIFEST (.klh) path, not getProjectPath() (the
        // project directory), which would never match the chosen .klh file.
        if (QFileInfo(pm.getManifestPath()) == QFileInfo(filename)) {
            logger.debug("Project is already open, ignoring: {}", filename.toStdString());
            return;
        }
    }
    // A book that cannot be opened is reported before the open book is closed for it
    if (reportOpenFailure(filename)) {
        return;
    }
    if (!agreeToCloseBook(
            tr("Do you want to save changes to '%1' before opening the selected book?"),
            tr("Do you want to close '%1' and open the selected book?"))) {
        logger.debug("User cancelled opening new project");
        return;
    }

    QStringList problems;
    if (!pm.openProject(filename, &problems)) {
        showOpenError(filename, problems, pm.lastOpenFailure());
        return;
    }
    // ProjectManager emits projectOpened, which triggers onProjectOpened()

    // Add to recent files list
    core::RecentBooksManager::getInstance().addRecentFile(filename);
}

void DocumentCoordinator::onOpenRecentFile(const QString& filePath) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening recent file: {}", filePath.toStdString());

    // Check if file still exists
    if (!QFileInfo::exists(filePath)) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("File Not Found"),
            tr("The file '%1' no longer exists.").arg(QDir::toNativeSeparators(filePath)));

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    // Another file opens outside a book, in a tab of its own, with nothing else changed
    if (!filePath.endsWith(".klh", Qt::CaseInsensitive)) {
        openStandaloneFile(filePath);
        return;
    }

    // A book: the files open outside a book stay open with their changes, and switching from
    // an open project asks below, once, after checking that it is a different project
    auto& pm = core::ProjectManager::getInstance();

    // If a project is already open, check if it's the SAME project
    if (pm.isProjectOpen()) {
        // If trying to open the same project that's already open, just ignore.
        // Compare against the MANIFEST (.klh) path: getProjectPath() is the project
        // DIRECTORY, so comparing it to the clicked .klh file never matched and the
        // "close & reopen?" prompt fired even for the already-open project.
        if (QFileInfo(pm.getManifestPath()) == QFileInfo(filePath)) {
            logger.debug("Project is already open, ignoring: {}", filePath.toStdString());
            return;
        }

        // Different project: a book that cannot be opened is reported before the open
        // book is closed for it, and unsaved changes are handled before closing
        if (reportOpenFailure(filePath)) {
            return;
        }
        if (!agreeToCloseBook(
                tr("Do you want to save changes to '%1' before opening the selected book?"),
                tr("Do you want to close '%1' and open the selected book?"))) {
            logger.debug("User cancelled opening new project");
            return;
        }
    }

    QStringList problems;
    if (pm.openProject(filePath, &problems)) {
        // Successfully opened as project
        logger.info("Opened .klh file as project: {}", filePath.toStdString());
        // Add to recent files so it's remembered as last opened
        core::RecentBooksManager::getInstance().addRecentFile(filePath);
        return;
    }

    // The book cannot be opened: the message says why. It stays in the recent books,
    // as it may be readable later.
    logger.error("Failed to open .klh file as project: {}", filePath.toStdString());
    showOpenError(filePath, problems, pm.lastOpenFailure());
}

void DocumentCoordinator::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // A file opened outside the project is saved on its own, also with a project open
    EditorPanel* editor = getCurrentEditor();
    if (editor && editorKind(editor) == EditorKind::StandaloneFile) {
        saveEditor(editor);
        return;
    }

    // Check if we're in project mode - delegate to Save All for project saves
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        // In project mode, Ctrl+S saves the manifest (including metadata like notes)
        // and any dirty chapter content
        onSaveBook();
        return;
    }

    logger.debug("No editor tab active - cannot save");
    m_statusBar->showMessage(tr("No document to save"), 2000);
}

void DocumentCoordinator::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");

    // Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor || editorKind(editor) == EditorKind::Opening) {
        logger.debug("No editor tab active - cannot save");
        m_statusBar->showMessage(tr("No document to save"), 2000);
        return;
    }

    if (editorKind(editor) == EditorKind::StandaloneFile) {
        saveStandaloneFileAs(editor);
        return;
    }

    // A chapter of the book has its file in the book: Save writes it with the book
    const QString elementId = editor->property("elementId").toString();
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    const QHash<QString, core::KindWords> nouns{
        {QStringLiteral("kind"), element ? wordsOf(*element) : mainWords()}};
    //: %1: the title of the element. In Polish: {Kind} „%1” {kind:s=zapisuje|p=zapisują} się
    //: razem z książką: użyj polecenia „Zapisz”.
    m_statusBar->showMessage(
        core::fillWords(tr("{Kind} \"%1\" {kind:s=is|p=are} saved with the book: use Save."),
                        nouns)
            .arg(element ? element->title : QString()),
        EXPLANATION_MS);
    logger.info("Save As: {} is saved with the book", elementId.toStdString());
}

// =============================================================================
// Per-editor save state
// =============================================================================

bool DocumentCoordinator::isEditorDirty(const EditorPanel* editor) const {
    if (!editor) {
        return false;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
    case EditorKind::StandaloneFile:
        // Its text differs from the saved text: typed and deleted again, it does not
        return editor->isModified();
    case EditorKind::Opening:
        return false;
    }
    return false;
}

bool DocumentCoordinator::saveEditor(EditorPanel* editor) {
    if (!editor) {
        return true;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
        // Chapters are saved with the project; onSaveBook() clears a chapter's flag only
        // once its content is written
        onSaveBook();
        return !isEditorDirty(editor);
    case EditorKind::StandaloneFile: {
        // A new file has no file yet: the writer chooses where it goes
        const QString path = standaloneFileOf(editor).path();
        return path.isEmpty() ? saveStandaloneFileAs(editor) : writeStandaloneFile(editor, path);
    }
    case EditorKind::Opening:
        return true;  // nothing to save
    }
    return false;
}

void DocumentCoordinator::discardEditorChanges(EditorPanel* editor) {
    if (!editor) {
        return;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->discardChapterChanges(editor->property("elementId").toString());
        }
        break;
    case EditorKind::StandaloneFile:
        // The file stays in the Navigator as it is on the disk
        editor->setProperty("dirty", false);
        m_navigatorPanel->setFileModified(listKeyOf(editor), false);
        break;
    case EditorKind::Opening:
        break;
    }
}

bool DocumentCoordinator::saveAllChanges() {
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        // Every dirty chapter and the project structure, in one save
        onSaveBook();
    }

    // Then every other tab that still has unsaved changes
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor || !isEditorDirty(editor)) {
            continue;
        }
        if (editorKind(editor) == EditorKind::ProjectChapter) {
            return false;  // onSaveBook() above could not write it, and said so
        }
        if (!saveEditor(editor)) {
            return false;
        }
    }
    return !m_hasUnsavedChanges();
}

QStringList DocumentCoordinator::unsavedDocumentNames() const {
    QStringList names;

    // Chapters and structure are saved together, as the book
    const core::ProjectBook* book = core::ProjectManager::getInstance().book();
    if (book && bookHasUnsavedChanges()) {
        names << book->title;
    }

    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editorKind(editor) != EditorKind::ProjectChapter && isEditorDirty(editor)) {
            QString name = m_centralTabs->tabText(i);
            if (name.startsWith(QLatin1Char('*'))) {
                name.remove(0, 1);
            }
            names << name;
        }
    }

    names.removeDuplicates();
    return names;
}

void DocumentCoordinator::onSaveAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save All");

    // The editor of tab @p index when it is a file outside the book with unsaved changes
    const auto unsavedFile = [this](int index) -> EditorPanel* {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(index));
        const bool unsaved =
            editor && editorKind(editor) != EditorKind::ProjectChapter && isEditorDirty(editor);
        return unsaved ? editor : nullptr;
    };

    // What has unsaved changes now: the book, and the open files outside it
    const bool bookChanged = bookHasUnsavedChanges();
    bool filesChanged = false;
    for (int i = 0; i < m_centralTabs->count() && !filesChanged; ++i) {
        filesChanged = unsavedFile(i) != nullptr;
    }
    if (!bookChanged && !filesChanged) {
        m_statusBar->showMessage(tr("Nothing to save: all changes are saved"), 3000);
        return;
    }

    // Each save reports what it could not write, and that does not stop the others. Saving
    // closes no tab.
    bool saved = true;
    if (bookChanged) {
        onSaveBook();
        saved = !bookHasUnsavedChanges();
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        if (EditorPanel* editor = unsavedFile(i); editor && !saveEditor(editor)) {
            saved = false;
        }
    }
    if (saved) {
        m_statusBar->showMessage(tr("All changes saved"), 3000);
    }
}

void DocumentCoordinator::onSaveBook() {
    auto& logger = core::Logger::getInstance();
    logger.info("Saving the book");

    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project open - nothing to save");
        m_statusBar->showMessage(tr("No project open"), 2000);
        return;
    }

    if (!saveBook()) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Save Warning"),
            tr("Some changes in the book could not be saved. Check the log for details."));
    }
}

bool DocumentCoordinator::bookHasUnsavedChanges() const {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        return false;
    }
    // The texts of its open elements that differ from their saved texts, and its data
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        const auto* editor = qobject_cast<const EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editorKind(editor) == EditorKind::ProjectChapter && isEditorDirty(editor)) {
            return true;
        }
    }
    return pm.isDirty();
}

bool DocumentCoordinator::saveBook() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // First, give ProjectManager the texts of the chapters whose tabs have changes
    QList<EditorPanel*> changed;
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editorKind(editor) == EditorKind::ProjectChapter && isEditorDirty(editor)) {
            const QString elemId = editor->property("elementId").toString();
            pm.setChapterContent(elemId, editor->getContent());
            changed.append(editor);
            logger.debug("Updated content cache for: {}", elemId.toStdString());
        }
    }

    // Save all dirty elements via ProjectManager
    bool success = pm.saveAllDirty();

    // Also save the .klh file to persist the changes of the book's data (title, author...)
    bool manifestSaved = pm.saveManifest();

    if (success && manifestSaved) {
        // The tabs no longer show changes: their texts are the saved texts now
        for (EditorPanel* editor : std::as_const(changed)) {
            m_navigatorCoordinator->setChapterDirty(editor->property("elementId").toString(),
                                                    false);
            editor->setUnmodified();
            const int index = m_centralTabs->indexOf(editor);
            const QString tabText = m_centralTabs->tabText(index);
            if (tabText.startsWith(QLatin1Char('*'))) {
                m_centralTabs->setTabText(index, tabText.mid(1));
            }
        }

        // Clear all modified indicators in NavigatorPanel (OpenSpec #00042 Phase 7.5)
        m_navigatorPanel->clearAllModifiedIndicators();

        pm.setDirty(false);
        logger.info("All chapters and manifest saved successfully");
        m_statusBar->showMessage(tr("Book saved"), 2000);
        return true;
    }
    logger.error("Failed to save some chapters");
    m_statusBar->showMessage(tr("Some changes could not be saved"), 3000);
    return false;
}

void DocumentCoordinator::onCloseDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Close Book");

    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        //: In Polish: Żadna książka nie jest otwarta
        m_statusBar->showMessage(tr("No book is open"), 2000);
        return;
    }

    // Only the book closes: the files open outside it stay open with their changes, so only
    // the changes of the book are asked about. The check keeps an annotation being written
    // first.
    if (m_hasUnsavedChanges() && bookHasUnsavedChanges()) {
        QString bookName = pm.book() ? pm.book()->title : QString();
        if (bookName.isEmpty()) {
            bookName = QFileInfo(pm.getProjectPath()).fileName();
        }
        const auto reply = dialogs::MessageDialog::ask(
            m_mainWindow, tr("Unsaved Changes"),
            //: %1: the title of the book. In Polish: Czy zapisać zmiany w „%1” przed
            //: zamknięciem książki?
            tr("Do you want to save changes to '%1' before closing the book?").arg(bookName),
            tr("&Save"), tr("Do&n't Save"));
        if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return;
        }
        if (reply == dialogs::MessageDialog::Answer::Accept) {
            onSaveBook();
            if (bookHasUnsavedChanges()) {
                return;  // what could not be saved keeps the book open, and was reported
            }
        }
        // Don't Save: the changes go with the book
    }

    // Prepare services for close BEFORE the database is destroyed. The book is not saved
    // again: what was to be saved is saved. onProjectClosed() closes the book's tabs.
    prepareForProjectClose();
    pm.closeProject(false);
    m_updateWindowTitle();
    logger.info("Book closed");
}

// =============================================================================
// Standalone File Operations
// =============================================================================

void DocumentCoordinator::onOpenStandaloneFile() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Standalone File");

    // Show file dialog with supported file types: a .kchapter file holds a text of any kind
    // (a chapter, a story, a poem), so it is a Kalahari document
    QString filename = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open File"),
        ProgramFolders::windowFolder(ProgramFolders::Operation::OpenFile),
        tr("Kalahari Documents and Text Files (*.kchapter *.txt);;"
           "Kalahari Documents (*.kchapter);;Text Files (*.txt);;All Files (*)")
    );

    if (filename.isEmpty()) {
        logger.info("Open standalone file cancelled by user");
        return;
    }
    ProgramFolders::remember(ProgramFolders::Operation::OpenFile,
                             QFileInfo(filename).absolutePath());

    openStandaloneFile(filename);
}

void DocumentCoordinator::openStandaloneFile(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening standalone file: {}", path.toStdString());

    // A file open in a tab is shown there, not opened twice
    if (EditorPanel* openEditor = findStandaloneEditor(path)) {
        m_centralTabs->setCurrentWidget(openEditor);
        return;
    }

    // The file of an element of the open book opens as that element, in its tab
    auto& pm = core::ProjectManager::getInstance();
    if (const core::ProjectElement* element = pm.elementWithFile(path);
        element && m_navigatorCoordinator && pm.formOf(*element) == core::ElementForm::Text) {
        const QString elementId = element->id;
        const QString title = element->title;
        const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};
        m_navigatorCoordinator->onElementSelected(elementId, title);
        m_navigatorPanel->revealElement(elementId);
        //: %1: the title of the element. In Polish: Ten plik należy do otwartej książki:
        //: to {kind} „%1”.
        m_statusBar->showMessage(
            core::fillWords(tr("This file belongs to the open book: it is the {kind} \"%1\"."),
                            nouns)
                .arg(title),
            EXPLANATION_MS);
        logger.info("The file is the element {} of the open book", elementId.toStdString());
        return;
    }

    // Check if file exists
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("File Not Found"),
            tr("The file '%1' does not exist.").arg(QDir::toNativeSeparators(path)));
        logger.error("Standalone file not found: {}", path.toStdString());
        return;
    }

    // Read the file in its own format
    const core::StandaloneFile::Type type = core::StandaloneFile::typeOf(path);
    if (type == core::StandaloneFile::Type::Unsupported) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("Unsupported File"),
            tr("Kalahari cannot open '%1'.\n\nIt opens Kalahari documents (*.kchapter) and text "
               "files (*.txt).")
                .arg(fileInfo.fileName()));
        logger.warn("Unsupported standalone file: {}", path.toStdString());
        return;
    }
    QString content;
    QString error;
    const auto file = core::StandaloneFile::open(path, content, &error);
    if (!file) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Open Error"),
            tr("Failed to open file: %1\n\n%2").arg(
                QDir::toNativeSeparators(path),
                type == core::StandaloneFile::Type::Chapter
                    ? tr("It is not a Kalahari document, or it cannot be read.")
                    : error));
        logger.error("Failed to open standalone file: {}", path.toStdString());
        return;
    }

    // Create new editor tab with icon based on file type
    EditorPanel* newEditor = new EditorPanel(m_mainWindow);
    QString tabTitle = fileInfo.fileName();
    const QString iconId = standaloneIconId(path);
    QIcon tabIcon = core::ArtProvider::getInstance().getIcon(iconId);
    int tabIndex = m_centralTabs->addTab(newEditor, tabIcon, tabTitle);
    newEditor->setProperty("tabIconId", iconId);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store file for this tab
    newEditor->setProperty("standaloneFile", QVariant::fromValue(*file));
    newEditor->setProperty("standaloneFilePath", path);
    newEditor->setProperty("isStandaloneFile", true);
    // Whether the tab shows changes, seeded clean; trackStandaloneChanges() keeps it as the
    // editor says (the questions ask the editor itself)
    newEditor->setProperty("dirty", false);

    // Set content
    const bool complete = newEditor->setContent(content);

    // Add to standalone files list
    if (!m_standaloneFilePaths.contains(path)) {
        m_standaloneFilePaths.append(path);
    }

    // Add to Navigator "Other Files" section
    m_navigatorPanel->addStandaloneFile(path);

    // Show info bar for standalone files with context-aware message
    updateStandaloneInfoBar();

    // Connected AFTER setContent() above so merely opening a file does not mark it
    // dirty (same rationale as the project-chapter path in NavigatorCoordinator).
    trackStandaloneChanges(newEditor);

    logger.info("Standalone file opened: {}", path.toStdString());
    m_statusBar->showMessage(tr("Opened: %1").arg(tabTitle), 2000);

    if (!complete) {
        EditorPanel::warnDamagedChapter(m_mainWindow, tabTitle);
    }
}

EditorPanel* DocumentCoordinator::findStandaloneEditor(const QString& path) const {
    if (path.isEmpty()) {
        return nullptr;
    }
    const QFileInfo fileInfo(path);
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor || !editor->property("isStandaloneFile").toBool()) {
            continue;
        }
        // A new file that has no file yet by its key in the Navigator
        const QString filePath = editor->property("standaloneFilePath").toString();
        if (filePath.isEmpty() ? editor->property("newFileKey").toString() == path
                               : QFileInfo(filePath) == fileInfo) {
            return editor;
        }
    }
    return nullptr;
}

void DocumentCoordinator::trackStandaloneChanges(EditorPanel* editor) {
    // The tab and the file in the Navigator show a "*" while the text differs from the saved
    // text: a letter typed and deleted again, or an edit undone, leaves none
    connect(editor, &EditorPanel::modifiedChanged, this, [this, editor](bool modified) {
        editor->setProperty("dirty", modified);
        m_navigatorPanel->setFileModified(listKeyOf(editor), modified);
        const int index = m_centralTabs->indexOf(editor);
        const QString tabText = index >= 0 ? m_centralTabs->tabText(index) : QString();
        if (index >= 0 && modified && !tabText.startsWith(QLatin1Char('*'))) {
            m_centralTabs->setTabText(index, QLatin1Char('*') + tabText);
        } else if (!modified && tabText.startsWith(QLatin1Char('*'))) {
            m_centralTabs->setTabText(index, tabText.mid(1));
        }
    });
}

void DocumentCoordinator::setInfoBarAllowed(bool allowed) {
    m_infoBarAllowed = allowed;
    updateStandaloneInfoBar();
}

void DocumentCoordinator::updateStandaloneInfoBar() {
    EditorPanel* editor = getCurrentEditor();
    if (!m_infoBarAllowed || !editor || editorKind(editor) != EditorKind::StandaloneFile
        || editor->property("infoBarDismissed").toBool()) {
        m_standaloneInfoBar->hide();
        return;
    }

    const QString path = editor->property("standaloneFilePath").toString();
    m_standaloneInfoBar->setFilePath(path);
    const bool bookOpen = core::ProjectManager::getInstance().isProjectOpen();
    if (path.isEmpty()) {
        // A new file: Save asks where it goes and in which format
        //: The info bar of a new file that has no file yet. In Polish: Nowy plik – jeszcze
        //: niezapisany
        m_standaloneInfoBar->setMessage(tr("New file – not saved yet"));
    } else if (bookOpen) {
        m_standaloneInfoBar->setMessage(tr("This file is not part of the open book."));
    } else {
        m_standaloneInfoBar->setMessage(
            tr("This file is not part of a book. Some features are not available."));
    }
    // Without a book, Add to Book could only say that one must be opened first
    m_standaloneInfoBar->setBookOpen(bookOpen);
    m_standaloneInfoBar->show();
}

void DocumentCoordinator::relistStandaloneFiles() {
    for (const QString& path : std::as_const(m_standaloneFilePaths)) {
        m_navigatorPanel->addStandaloneFile(path);
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        const auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editorKind(editor) == EditorKind::StandaloneFile &&
            editor->property("standaloneFilePath").toString().isEmpty()) {
            m_navigatorPanel->addNewFile(editor->property("newFileKey").toString(),
                                         editor->property("newFileName").toString());
        }
    }
}

bool DocumentCoordinator::writeStandaloneFile(EditorPanel* editor, const QString& path) {
    auto& logger = core::Logger::getInstance();

    const QString kml = editor->getContent();
    const core::StandaloneFile::TextLoss lost =
        core::StandaloneFile::typeOf(path) == core::StandaloneFile::Type::PlainText
            ? core::StandaloneFile::lostAsText(kml)
            : core::StandaloneFile::TextLoss{};
    const bool loses = lost.formatting || lost.annotations;
    if (loses && !agreeToSaveAsText(lost, path)) {
        logger.info("Not saved as plain text, which would lose formatting or annotations: {}",
                    path.toStdString());
        return false;
    }

    core::StandaloneFile file = standaloneFileOf(editor);
    const QString oldPath = file.path();
    QString error;
    if (!file.saveAs(path, kml, editor->getText(), &error)) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Save Error"),
            tr("Failed to save file: %1").arg(QDir::toNativeSeparators(path)), error);
        logger.error("Failed to save standalone file: {}", path.toStdString());
        return false;
    }
    editor->setProperty("standaloneFile", QVariant::fromValue(file));
    if (loses) {
        showTextOfFile(editor, path);  // the text the file keeps, as the question said
    }
    editor->setUnmodified();  // its text is the saved text now

    const int index = m_centralTabs->indexOf(editor);
    if (path != oldPath) {
        // The tab, the "Other Files" list and the info bar follow the file to its new name; a
        // new file leaves its key in the list for its file
        if (oldPath.isEmpty()) {
            m_navigatorPanel->removeStandaloneFile(editor->property("newFileKey").toString());
            editor->setProperty("newFileKey", QVariant());
        }
        editor->setProperty("standaloneFilePath", path);
        const QString iconId = standaloneIconId(path);
        editor->setProperty("tabIconId", iconId);
        if (index >= 0) {
            m_centralTabs->setTabIcon(index, core::ArtProvider::getInstance().getIcon(iconId));
        }
        m_standaloneFilePaths.removeAll(oldPath);
        m_standaloneFilePaths.append(path);
        m_navigatorPanel->removeStandaloneFile(oldPath);
        m_navigatorPanel->addStandaloneFile(path);
        updateStandaloneInfoBar();
    }
    if (index >= 0) {
        m_centralTabs->setTabText(index, QFileInfo(path).fileName());  // without the "*"
    }

    logger.info("Standalone file saved: {}", path.toStdString());
    m_statusBar->showMessage(tr("Saved: %1").arg(QFileInfo(path).fileName()), 2000);
    return true;
}

bool DocumentCoordinator::agreeToSaveAsText(const core::StandaloneFile::TextLoss& lost,
                                            const QString& path) {
    QString text;
    if (lost.formatting && lost.annotations) {
        //: %1: the name of the file. In Polish: Plik tekstowy (.txt) przechowuje tylko tekst,
        //: więc formatowanie (np. pogrubienie) i uwagi nie zostaną zapisane w pliku „%1”.
        text = tr("A plain text file (.txt) keeps only the text, so the formatting (such as "
                  "bold) and the annotations will not be saved in \"%1\".");
    } else if (lost.formatting) {
        //: %1: the name of the file. In Polish: Plik tekstowy (.txt) przechowuje tylko tekst,
        //: więc formatowanie (np. pogrubienie) nie zostanie zapisane w pliku „%1”.
        text = tr("A plain text file (.txt) keeps only the text, so the formatting (such as "
                  "bold) will not be saved in \"%1\".");
    } else {
        //: %1: the name of the file. In Polish: Plik tekstowy (.txt) przechowuje tylko tekst,
        //: więc uwagi nie zostaną zapisane w pliku „%1”.
        text = tr("A plain text file (.txt) keeps only the text, so the annotations will not be "
                  "saved in \"%1\".");
    }
    //: %1: the type of the files of Save As that keeps them. In Polish: Aby zachować wszystko,
    //: wybierz „Anuluj”, a potem Plik > Zapisz jako... i typ „%1”.
    text = text.arg(QFileInfo(path).fileName()) + QStringLiteral("\n\n") +
           tr("To keep everything, choose Cancel, then File > Save As... and the type \"%1\".")
               .arg(tr("Kalahari Documents (*.kchapter)"));
    //: In Polish: Zapisać jako plik tekstowy?
    return dialogs::MessageDialog::confirm(m_mainWindow, tr("Save as Plain Text?"), text,
                                           tr("Save as &Text"),
                                           dialogs::MessageDialog::Kind::Warning,
                                           dialogs::MessageDialog::DefaultButton::Cancel);
}

void DocumentCoordinator::showTextOfFile(EditorPanel* editor, const QString& path) {
    QString kml;
    QString error;
    if (!core::StandaloneFile::open(path, kml, &error)) {
        core::Logger::getInstance().warn("Saved plain text file not read again, its tab keeps "
                                         "the formatting: {} ({})",
                                         path.toStdString(), error.toStdString());
        return;
    }
    // The same text: the cursor and the view stay at it
    editor->getBookEditor()->replaceMarkupWithKml(kml);
}

bool DocumentCoordinator::saveStandaloneFileAs(EditorPanel* editor) {
    const QString oldPath = standaloneFileOf(editor).path();
    const QString chapters = tr("Kalahari Documents (*.kchapter)");
    const QString textFiles = tr("Text Files (*.txt)");
    QString selectedFilter =
        core::StandaloneFile::typeOf(oldPath) == core::StandaloneFile::Type::PlainText
            ? textFiles : chapters;

    // A new file starts in the folder of Open File, as a Kalahari document named after its tab
    QString start = oldPath;
    if (oldPath.isEmpty()) {
        QString name = m_centralTabs->tabText(m_centralTabs->indexOf(editor));
        if (name.startsWith(QLatin1Char('*'))) {
            name.remove(0, 1);
        }
        start = QDir(ProgramFolders::windowFolder(ProgramFolders::Operation::OpenFile))
                    .filePath(name + QStringLiteral(".kchapter"));
    }
    QString path = QFileDialog::getSaveFileName(
        m_mainWindow,
        tr("Save File As"),
        start,
        chapters + QStringLiteral(";;") + textFiles,
        &selectedFilter
    );
    if (path.isEmpty()) {
        return false;
    }
    if (oldPath.isEmpty()) {
        ProgramFolders::remember(ProgramFolders::Operation::OpenFile,
                                 QFileInfo(path).absolutePath());
    }

    // A name without one of the two extensions gets the one of the chosen type
    if (core::StandaloneFile::typeOf(path) == core::StandaloneFile::Type::Unsupported) {
        path += selectedFilter == textFiles ? QStringLiteral(".txt") : QStringLiteral(".kchapter");
    }

    // One file is edited in one tab: a file of the open book in the tab of its element
    if (const core::ProjectElement* element =
            core::ProjectManager::getInstance().elementWithFile(path)) {
        const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Save Error"),
            element->hasKind()
                //: %1: the name of the file, %2: the title of the element. In Polish: „%1” to
                //: plik {kind:genitive} „%2” z otwartej książki. Wybierz inną nazwę.
                ? core::fillWords(tr("'%1' is the file of the {kind} \"%2\" in the open book. "
                                     "Choose another name."),
                                  nouns)
                      .arg(QFileInfo(path).fileName(), element->title)
                //: A file kept with the book, outside its text; %1: the name of the file, %2:
                //: its title. In Polish: „%1” to plik „%2” z otwartej książki. Wybierz inną
                //: nazwę.
                : tr("'%1' is the file \"%2\" of the open book. Choose another name.")
                      .arg(QFileInfo(path).fileName(), element->title));
        return false;
    }
    EditorPanel* other = findStandaloneEditor(path);
    if (other && other != editor) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Save Error"),
            tr("'%1' is open in another tab.").arg(QFileInfo(path).fileName()));
        return false;
    }
    return writeStandaloneFile(editor, path);
}

void DocumentCoordinator::onAddToProject() {
    // The info bar shows the file of the current tab. A new file that has no file yet goes
    // into the book at once, asked nothing: the command says where it goes, and Save and
    // Save As keep a file of its own, in the folder the writer chooses.
    const QString path = m_standaloneInfoBar->filePath();
    EditorPanel* editor = getCurrentEditor();
    if (path.isEmpty() && editor && editorKind(editor) == EditorKind::StandaloneFile
        && core::ProjectManager::getInstance().isProjectOpen()) {
        addNewFileToProject(editor);
        return;
    }
    addToProject(path);
}

void DocumentCoordinator::addNewFileToProject(EditorPanel* editor) {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Add to Book, a new file");
    auto& pm = core::ProjectManager::getInstance();

    // The main text kind of the book, at the place of its kind and with the title of a new
    // element of it, as the Add to Book window proposes them
    const dialogs::AddToBookDialog::Book book = dialogs::AddToBookDialog::openBook(QString());
    if (book.choices.isEmpty()) {
        dialogs::MessageDialog::warning(m_mainWindow, tr("Add to Book"),
                                        tr("No element of this book can have a text."));
        return;
    }
    const dialogs::NewElementChoice& choice = book.choices.at(book.current);
    const QString title = pm.defaultTitle(choice.kind);
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(choice.kind)}};
    const QString elementId =
        pm.addElement(choice.kind, title, choice.place.place, choice.place.groupId,
                      choice.place.index, {}, editor->getContent());
    if (elementId.isEmpty()) {
        //: In Polish: Nie udało się dodać {kind:genitive}
        dialogs::MessageDialog::warning(
            m_mainWindow, core::fillWords(tr("Add {Kind} Failed"), nouns),
            tr("The file could not be added to the book. The program's log has the details."));
        logger.error("Add to Book: Failed to add a new file");
        return;
    }

    // The element takes the place of the file's tab, with its text
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->refreshNavigator();
    }
    m_navigatorPanel->revealElement(elementId);
    utils::removeTabKeepingFocus(*m_centralTabs, m_centralTabs->indexOf(editor));
    editor->deleteLater();
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->onElementSelected(elementId, title);
    }
    updateStandaloneInfoBar();

    //: In Polish: Dodano {kind:accusative} do książki: %1
    m_statusBar->showMessage(
        core::fillWords(tr("{Kind} added to the book: %1"), nouns).arg(title), 3000);
    logger.info("Add to Book: Added a new file as {}", elementId.toStdString());
}

void DocumentCoordinator::addToProject(const QString& filePath) {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Add to Book");

    auto& pm = core::ProjectManager::getInstance();

    // A book must be open
    if (!pm.isProjectOpen()) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("No Book Open"),
            tr("Open or create a book first.\n\n"
               "Use File > New Book... or File > Open Book... to start."));
        logger.info("Add to Book: No book open");
        return;
    }

    if (filePath.isEmpty()) {
        logger.warn("Add to Book: No file path");
        return;
    }

    // A new file that has no file yet, by its key in the Navigator, goes in at once
    if (EditorPanel* newFile = findStandaloneEditor(filePath);
        newFile && standaloneFileOf(newFile).path().isEmpty()) {
        addNewFileToProject(newFile);
        return;
    }

    // The book gets the file with the changes made in its tab
    EditorPanel* fileEditor = findStandaloneEditor(filePath);
    if (fileEditor && isEditorDirty(fileEditor) && !saveEditor(fileEditor)) {
        return;
    }

    // A chapter keeps its title, its status and its notes; a text file gets its name as the
    // title
    dialogs::AddToBookDialog::File file{filePath, QString(), QString(), false};
    if (core::StandaloneFile::typeOf(filePath) == core::StandaloneFile::Type::Chapter) {
        if (const std::optional<core::ChapterDocument> chapter =
                core::ChapterDocument::load(filePath)) {
            file.title = chapter->title();
            file.status = chapter->status();
        }
    }
    const dialogs::AddToBookDialog::Book book = dialogs::AddToBookDialog::openBook(filePath);
    if (book.choices.isEmpty()) {
        dialogs::MessageDialog::warning(m_mainWindow, tr("Add to Book"),
                                        tr("No element of this book can have a text."));
        return;
    }
    dialogs::AddToBookDialog dialog(file, book, pm.bookTypes(), m_mainWindow);
    if (dialog.exec() != QDialog::Accepted) {
        logger.info("Add to Book: User cancelled");
        return;
    }

    // ProjectManager copies or moves the file to the book's folder and saves the book at once
    const core::ElementPlace at = dialog.place();
    const QString title = dialog.title();
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(dialog.kind())}};
    const QString elementId = pm.addFile(filePath, dialog.copy(), dialog.kind(), title, at.place,
                                         at.groupId, at.index);
    if (elementId.isEmpty()) {
        //: In Polish: Nie udało się dodać {kind:genitive}
        dialogs::MessageDialog::warning(
            m_mainWindow, core::fillWords(tr("Add {Kind} Failed"), nouns),
            tr("The file could not be added to the book. The program's log has the details."));
        logger.error("Add to Book: Failed to add file");
        return;
    }

    // The navigator shows the new element, the file leaves "Files Outside the Book", and the
    // element takes the place of its tab
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->refreshNavigator();
    }
    m_navigatorPanel->revealElement(elementId);
    m_navigatorPanel->removeStandaloneFile(filePath);
    m_standaloneFilePaths.removeAll(filePath);
    if (fileEditor) {
        utils::removeTabKeepingFocus(*m_centralTabs, m_centralTabs->indexOf(fileEditor));
        fileEditor->deleteLater();
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->onElementSelected(elementId, title);
        }
    }
    updateStandaloneInfoBar();

    //: In Polish: Dodano {kind:accusative} do książki: %1
    m_statusBar->showMessage(
        core::fillWords(tr("{Kind} added to the book: %1"), nouns).arg(title), 3000);
    logger.info("Add to Book: Added {} as {}", filePath.toStdString(), elementId.toStdString());
}

void DocumentCoordinator::removeStandaloneFile(const QString& path) {
    // Only the list entry goes: a tab with the file stays open
    m_navigatorPanel->removeStandaloneFile(path);
    m_standaloneFilePaths.removeAll(path);
}

// =============================================================================
// Archive Operations
// =============================================================================

void DocumentCoordinator::onExportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        dialogs::MessageDialog::information(m_mainWindow, tr("No Book Open"),
                                            tr("Please open a book first before exporting."));
        return;
    }

    // One window: the archive, the files that are not ready, the progress and the result
    dialogs::ExportArchiveDialog::Book book = dialogs::ExportArchiveDialog::openBook();
    book.unsavedChanges = bookHasUnsavedChanges();
    dialogs::ExportArchiveDialog dialog(
        book, ProgramFolders::startFolder(ProgramFolders::Operation::ExportArchive),
        [this, &pm, &logger](const QString& path, bool withBackups,
                             const dialogs::ArchiveDialog::Progress& progress,
                             core::ArchiveProblem* problem) {
            ProgramFolders::remember(ProgramFolders::Operation::ExportArchive,
                                     QFileInfo(path).absolutePath());
            // The archive has the book as the writer sees it
            if (bookHasUnsavedChanges() && !saveBook()) {
                problem->kind = core::ArchiveProblem::Kind::ChangesNotSaved;
                return false;
            }
            if (!pm.exportArchive(path, progress, problem, withBackups)) {
                logger.error("Failed to export project archive");
                return false;
            }
            logger.info("Project exported to: {}", path.toStdString());
            return true;
        },
        m_mainWindow);
    dialog.exec();
}

void DocumentCoordinator::onImportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // One window: the archive and its book, the folder, the progress and the result
    dialogs::ImportArchiveDialog dialog(
        pm.bookTypes(), ProgramFolders::startFolder(ProgramFolders::Operation::ImportDestination),
        [&pm, &logger](const QString& archive, const QString& folder, bool open,
                       const dialogs::ArchiveDialog::Progress& progress,
                       core::ArchiveProblem* problem) {
            ProgramFolders::remember(ProgramFolders::Operation::ImportDestination, folder);
            QString manifest;
            if (open) {
                // ProjectManager closes the open book, and onProjectOpened() shows this one
                if (pm.importArchive(archive, folder, progress, problem)) {
                    manifest = pm.getManifestPath();
                }
            } else {
                manifest = core::ProjectManager::unpackArchive(archive, folder, progress, problem);
            }
            if (manifest.isEmpty()) {
                logger.error("Failed to import project archive");
                return false;
            }
            // Opened or not, the book is on the Dashboard and in Recent Books
            core::RecentBooksManager::getInstance().addRecentFile(manifest);
            logger.info("Project imported from: {}", archive.toStdString());
            return true;
        },
        m_mainWindow);
    dialog.setOpeningQuestion([this, &dialog](const QString& title) {
        const OpenBookChoice choice = askAboutOpenBook(
            &dialog,
            //: %1: the title of the book in the archive. In Polish: Książka „%1” z archiwum może
            //: zastąpić otwartą albo zostać zaimportowana bez otwierania.
            tr("The book \"%1\" from the archive can take the place of the open one or be "
               "imported without opening.")
                .arg(title),
            //: In Polish: Książkę zaimportowaną bez otwierania znajdziesz na Pulpicie programu
            //: i w menu Plik > Ostatnie książki. „Anuluj” wraca do okna „Importuj archiwum
            //: projektu” z wpisanymi danymi.
            tr("A book imported without opening is on the Dashboard and in File > Recent Books. "
               "Cancel returns to the Import Project Archive window with your choices."),
            //: In Polish: Zamknij i otwórz zaimportowaną
            tr("Close and Open Imported"),
            //: In Polish: Importuj bez otwierania
            tr("Import Without Opening"),
            tr("Do you want to save changes to '%1' before importing the archive?"));
        switch (choice) {
            case OpenBookChoice::Close:
                return dialogs::ImportArchiveDialog::Opening::Open;
            case OpenBookChoice::KeepOpen:
                return dialogs::ImportArchiveDialog::Opening::DontOpen;
            case OpenBookChoice::Cancel:
                break;
        }
        return dialogs::ImportArchiveDialog::Opening::Cancel;
    });
    dialog.exec();
}

// =============================================================================
// Project Lifecycle
// =============================================================================

void DocumentCoordinator::onProjectOpened(const QString& projectPath) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();

    if (!project) {
        logger.error("onProjectOpened: No project");
        return;
    }

    // =========================================================================
    // OpenSpec #00042 Task 7.6: Connect StyleResolver to database
    // =========================================================================
    core::ProjectDatabase* database = pm.getDatabase();
    if (database && database->isOpen()) {
        // Create or reset StyleResolver
        if (!m_styleResolver) {
            m_styleResolver = std::make_unique<editor::StyleResolver>(this);
            logger.debug("StyleResolver created for project");
        }

        // Connect to database and load styles
        m_styleResolver->setDatabase(database);
        m_styleResolver->reloadFromDatabase();

        // Connect PropertiesPanel to StyleResolver for style operations
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(m_styleResolver.get());
        }

        logger.info("StyleResolver connected to project database, loaded styles");

        // =========================================================================
        // OpenSpec #00042 Task 7.7: Connect StatisticsCollector to database
        // =========================================================================
        if (!m_statisticsCollector) {
            m_statisticsCollector = std::make_unique<editor::StatisticsCollector>(this);
            logger.debug("StatisticsCollector created for project");
        }

        // Connect to database
        m_statisticsCollector->setDatabase(database);

        // Start writing session
        m_statisticsCollector->startSession();
        logger.info("StatisticsCollector connected to project database, session started");

        // Connect NavigatorCoordinator to statistics collector for new editor panels
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(m_statisticsCollector.get());
            logger.debug("NavigatorCoordinator connected to StatisticsCollector");
        }
    } else {
        logger.warn("Project database not available for StyleResolver/StatisticsCollector");
    }

    // Update Navigator panel with the book's structure, and the files no element has; the
    // expansion state of the last session decides whether their item is open
    m_navigatorPanel->loadProject(*project, pm.bookTypes());
    m_navigatorPanel->setUnassignedFiles(pm.unassignedFiles(), pm.getProjectPath());

    // Restore Navigator expansion state
    QFileInfo pathInfo(projectPath);
    QString projectId = pathInfo.absoluteFilePath()
        .replace("/", "_")
        .replace("\\", "_")
        .replace(":", "_")
        .replace(" ", "_");
    m_navigatorPanel->restoreExpansionState(projectId);
    logger.debug("Restored expansion state for project: {}", projectId.toStdString());

    // Update window title with book title
    emit windowTitleChanged((pm.book() ? pm.book()->title : QString()) + " - Kalahari");

    // Log and status bar
    logger.info("Project opened: {}", projectPath.toStdString());
    m_statusBar->showMessage(tr("Book opened: %1").arg(projectPath), 3000);
    adoptBookFiles();
    updateStandaloneInfoBar();  // its message names the open project
    emit documentOpened();
}

void DocumentCoordinator::adoptBookFiles() {
    auto& pm = core::ProjectManager::getInstance();
    if (!m_navigatorCoordinator) {
        return;
    }

    QWidget* current = m_centralTabs->currentWidget();
    QList<EditorPanel*> files;
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* file = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (file && editorKind(file) == EditorKind::StandaloneFile) {
            files << file;
        }
    }

    QStringList adopted;
    for (EditorPanel* file : files) {
        const QString path = standaloneFileOf(file).path();
        const core::ProjectElement* element = pm.elementWithFile(path);
        if (!element || pm.formOf(*element) != core::ElementForm::Text) {
            continue;
        }

        // The tab of the element opens, with the changes of the file's tab, and takes its place
        const QString elementId = element->id;
        const QString title = element->title;
        const bool changed = isEditorDirty(file);
        const QString text = file->getContent();
        m_navigatorCoordinator->onElementSelected(elementId, title);
        auto* chapter = qobject_cast<EditorPanel*>(m_centralTabs->currentWidget());
        if (!chapter || chapter == file || chapter->property("elementId").toString() != elementId) {
            continue;  // it could not be opened: the file stays in its tab
        }
        if (changed) {
            // The changes are the chapter's now, not saved yet: one edit, which undo takes back
            chapter->getBookEditor()->replaceWithKml(text);
        }
        m_centralTabs->tabBar()->moveTab(m_centralTabs->indexOf(chapter),
                                         m_centralTabs->indexOf(file));
        if (current == file) {
            current = chapter;
        }
        utils::removeTabKeepingFocus(*m_centralTabs, m_centralTabs->indexOf(file));
        file->deleteLater();
        m_standaloneFilePaths.removeAll(path);
        m_navigatorPanel->removeStandaloneFile(path);
        adopted << title;
        core::Logger::getInstance().info("The file {} is open as the element {} of the book",
                                         path.toStdString(), elementId.toStdString());
    }
    if (current) {
        m_centralTabs->setCurrentWidget(current);
    }
    if (!adopted.isEmpty()) {
        //: %1: the titles of the elements. In Polish: Pliki tej książki otwarte wcześniej
        //: osobno są teraz otwarte jako jej elementy: %1
        m_statusBar->showMessage(
            tr("Files of this book that were open on their own are open as its elements now: "
               "%1")
                .arg(adopted.join(QStringLiteral(", "))),
            EXPLANATION_MS);
    }
}

void DocumentCoordinator::onProjectClosed() {
    auto& logger = core::Logger::getInstance();

    // =========================================================================
    // OpenSpec #00042 Task 7.7: End statistics session and disconnect
    // NOTE: This may already be done by prepareForProjectClose() if opening
    // a new project. These calls are idempotent and safe to call twice.
    // =========================================================================
    if (m_statisticsCollector) {
        // Disconnect NavigatorCoordinator from statistics collector first
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(nullptr);
        }

        // End session (flushes stats to database) - no-op if already ended
        m_statisticsCollector->endSession();

        // Disconnect from database
        m_statisticsCollector->setDatabase(nullptr);
    }

    // =========================================================================
    // OpenSpec #00042 Task 7.6: Disconnect StyleResolver from database
    // NOTE: May already be done by prepareForProjectClose() - idempotent
    // =========================================================================
    if (m_styleResolver) {
        // Disconnect PropertiesPanel from StyleResolver first
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(nullptr);
        }

        m_styleResolver->setDatabase(nullptr);
        m_styleResolver->invalidateCache();
    }

    // The book's chapters close with it; their changes were saved or discarded before
    for (int i = m_centralTabs->count() - 1; i >= 0; --i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (qobject_cast<EditorPanel*>(widget) && !widget->property("elementId").toString().isEmpty()) {
            utils::removeTabKeepingFocus(*m_centralTabs, i);
            widget->deleteLater();
        }
    }

    // Clear Navigator panel
    m_navigatorPanel->clearAllModifiedIndicators();  // Clear modified indicators first (OpenSpec #00042 Phase 7.5)
    m_navigatorPanel->clearDocument();
    relistStandaloneFiles();  // they stay open without the book

    // Reset window title
    emit windowTitleChanged("Kalahari");

    // Clear chapter editing state via NavigatorCoordinator
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->clearDirtyChapters();
        m_navigatorCoordinator->clearCurrentElement();
    }

    // Log and status bar
    logger.info("Project closed");
    m_statusBar->showMessage(tr("Book closed"), 2000);
    updateStandaloneInfoBar();
    emit documentClosed();
}

void DocumentCoordinator::prepareForProjectClose() {
    auto& logger = core::Logger::getInstance();

    // =========================================================================
    // Save Navigator expansion state while the project path is still known.
    // closeProject() clears the path before it emits projectClosed, so this
    // cannot wait for onProjectClosed().
    // =========================================================================
    const QString projectPath = core::ProjectManager::getInstance().getProjectPath();
    if (!projectPath.isEmpty() && m_navigatorPanel) {
        QFileInfo pathInfo(projectPath);
        QString projectId = pathInfo.absoluteFilePath()
            .replace("/", "_")
            .replace("\\", "_")
            .replace(":", "_")
            .replace(" ", "_");
        m_navigatorPanel->saveExpansionState(projectId);
        // Persist to disk immediately
        core::SettingsManager::getInstance().save();
        logger.debug("Saved expansion state for project: {}", projectId.toStdString());
    }

    // =========================================================================
    // End statistics session BEFORE database is closed
    // This prevents crash from dangling pointer when ProjectManager destroys
    // the database and StatisticsCollector tries to flush
    // =========================================================================
    if (m_statisticsCollector) {
        // Disconnect NavigatorCoordinator from statistics collector first
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(nullptr);
            logger.debug("prepareForProjectClose: NavigatorCoordinator disconnected from StatisticsCollector");
        }

        // End session (flushes stats to database while it's still open)
        m_statisticsCollector->endSession();

        // Disconnect from database
        m_statisticsCollector->setDatabase(nullptr);
        logger.debug("prepareForProjectClose: StatisticsCollector session ended, disconnected from database");
    }

    // =========================================================================
    // Disconnect StyleResolver from database
    // =========================================================================
    if (m_styleResolver) {
        // Disconnect PropertiesPanel from StyleResolver first
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(nullptr);
        }

        m_styleResolver->setDatabase(nullptr);
        m_styleResolver->invalidateCache();
        logger.debug("prepareForProjectClose: StyleResolver disconnected from project database");
    }
}

} // namespace gui
} // namespace kalahari
