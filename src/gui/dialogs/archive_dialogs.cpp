/// @file archive_dialogs.cpp
/// @brief Export Project Archive and Import Project Archive: each in one window, from the
/// choice of the archive to the result of the work

#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMargins>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTextDocument>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int WIDTH_IN_CHARACTERS = 80;  ///< The window has room for a path of this length
constexpr int DETAILS_LINES = 4;         ///< Height of the problems, in lines
constexpr int RESULT_SPACING = 8;        ///< Between the icon of the result and its sentence

/// Characters that file systems do not allow in a name
const QString& forbiddenCharacters() {
    static const QString characters = QStringLiteral("\\/:*?\"<>|");
    return characters;
}

/// @p elements and the elements inside them, at any depth, each group before its elements
void appendAll(const QList<core::ProjectElement>& elements,
               QList<const core::ProjectElement*>& all) {
    for (const core::ProjectElement& element : elements) {
        all.append(&element);
        appendAll(element.elements, all);
    }
}

/// A value of the rows of the book of an archive: plain text that can be selected
QLabel* newValue(QWidget* parent) {
    auto* value = new QLabel(parent);
    value->setTextFormat(Qt::PlainText);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return value;
}

} // anonymous namespace

// =============================================================================
// ArchiveDialog
// =============================================================================

ArchiveDialog::ArchiveDialog(QWidget* parent)
    : KalahariDialog(parent)
{
    setMinimumWidth(
        std::max(minimumWidth(), fontMetrics().averageCharWidth() * WIDTH_IN_CHARACTERS));

    // The fields, which get the room of a resized window
    m_fields = new QWidget(this);
    m_fieldsLayout = new QVBoxLayout(m_fields);
    m_fieldsLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(m_fields, 1);

    // The hint, until the work starts
    m_hint.widget = newNote(this, &m_hint.icon, &m_hint.text);
    m_hint.widget->hide();
    contentLayout()->addWidget(m_hint.widget);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    //: The bar of the progress of an export or an import. In Polish: Postęp
    m_progressBar->setAccessibleName(tr("Progress"));
    m_progressBar->hide();
    contentLayout()->addWidget(m_progressBar);

    // The result: its icon, the sentence and Show in Folder, or the problems of the work
    m_result = new QWidget(this);
    auto* resultLayout = new QHBoxLayout(m_result);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(RESULT_SPACING);
    m_resultIcon = new QLabel(m_result);
    m_resultIcon->setAlignment(Qt::AlignTop);
    resultLayout->addWidget(m_resultIcon);
    auto* resultTexts = new QVBoxLayout();
    m_resultText = newValue(m_result);
    resultTexts->addWidget(m_resultText);
    m_showLink = new QLabel(m_result);
    m_showLink->setTextFormat(Qt::RichText);
    //: A link under the result of an export or an import. In Polish: Pokaż w folderze
    m_showLink->setText(
        QStringLiteral("<a href=\"show\">%1</a>").arg(tr("Show in Folder").toHtmlEscaped()));
    m_showLink->setTextInteractionFlags(Qt::LinksAccessibleByMouse |
                                        Qt::LinksAccessibleByKeyboard);
    m_showLink->setToolTip(tr("Open the folder in the file manager of the system"));
    connect(m_showLink, &QLabel::linkActivated, this, &ArchiveDialog::showResult);
    resultTexts->addWidget(m_showLink, 0, Qt::AlignLeft);
    m_details = new QPlainTextEdit(m_result);
    m_details->setReadOnly(true);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setFixedHeight(
        m_details->fontMetrics().lineSpacing() * DETAILS_LINES +
        2 * (m_details->frameWidth() + qRound(m_details->document()->documentMargin())));
    //: The problems under the sentence of what went wrong. In Polish: Szczegóły
    m_details->setAccessibleName(tr("Details"));
    resultTexts->addWidget(m_details);
    resultLayout->addLayout(resultTexts, 1);
    m_result->hide();
    contentLayout()->addWidget(m_result);

    m_copyButton = addSideButton(QString());
    setUpCopyButton(m_copyButton, [this]() { return clipboardText(); });
    m_copyButton->hide();
}

QString ArchiveDialog::resultText() const
{
    return m_result->isVisibleTo(this) ? m_resultText->text() : QString();
}

QString ArchiveDialog::detailsText() const
{
    return m_result->isVisibleTo(this) && m_details->isVisibleTo(this)
               ? m_details->toPlainText()
               : QString();
}

QString ArchiveDialog::clipboardText() const
{
    QString text = windowTitle() + QStringLiteral("\n\n") + resultText();
    if (!detailsText().isEmpty()) {
        text += QStringLiteral("\n\n") + detailsText();
    }
    return text;
}

void ArchiveDialog::showResult()
{
    if (!showInFolder(m_resultPath)) {
        MessageDialog::warning(this, tr("Show in Folder"),
                               tr("The file manager of the system could not show:\n%1")
                                   .arg(QDir::toNativeSeparators(m_resultPath)));
    }
}

void ArchiveDialog::accept()
{
    switch (m_state) {
        case State::Ready:
            if (acceptButton()->isEnabled()) {
                run();
            }
            return;
        case State::Working:
            return;
        case State::Done:
            KalahariDialog::accept();
            return;
    }
}

void ArchiveDialog::reject()
{
    // The work cannot be stopped halfway: Esc and the close button of the window wait for it
    if (m_state != State::Working) {
        KalahariDialog::reject();
    }
}

QFormLayout* ArchiveDialog::addGroup(const QString& title)
{
    auto* group = new QGroupBox(title, m_fields);
    auto* form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_fieldsLayout->addWidget(group);
    return form;
}

QLabel* ArchiveDialog::addRow(QFormLayout* form, const QString& label, QWidget* field)
{
    auto* rowLabel = new QLabel(label, form->parentWidget());
    rowLabel->setTextFormat(Qt::PlainText);
    rowLabel->setBuddy(field);
    form->addRow(rowLabel, field);

    // The fields of all the groups start in one line
    m_rowLabels.append(rowLabel);
    int width = 0;
    for (const QLabel* each : std::as_const(m_rowLabels)) {
        width = std::max(width, each->sizeHint().width());
    }
    for (QLabel* each : std::as_const(m_rowLabels)) {
        each->setMinimumWidth(width);
    }
    return rowLabel;
}

ArchiveDialog::FieldNote ArchiveDialog::addNote(QFormLayout* form)
{
    FieldNote note;
    note.form = form;
    note.widget = newNote(form->parentWidget(), &note.icon, &note.text);
    // An empty label: a row without one gets the height its wrapped sentence has in a narrow
    // column, whatever the width of the window
    form->addRow(new QWidget(form->parentWidget()), note.widget);
    form->setRowVisible(note.widget, false);
    return note;
}

void ArchiveDialog::showNote(const FieldNote& note, const QString& text, const QString& iconId)
{
    note.text->setText(text);
    // A remark is grey and has no icon; a warning reads like the rest of the window
    note.text->setForegroundRole(iconId.isEmpty() ? QPalette::PlaceholderText
                                                  : QPalette::WindowText);
    if (iconId.isEmpty()) {
        note.icon->clear();
    } else {
        setNoteIcon(note.icon, iconId);
    }
    note.icon->setVisible(!iconId.isEmpty());
    if (note.form) {
        note.form->setRowVisible(note.widget, !text.isEmpty());
    } else {
        note.widget->setVisible(!text.isEmpty());
    }
}

QString ArchiveDialog::noteText(const FieldNote& note) const
{
    return note.widget->isVisibleTo(this) ? note.text->text() : QString();
}

QWidget* ArchiveDialog::newPathField(QLineEdit** field, QPushButton** browse)
{
    auto* row = new QWidget(m_fields);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    *field = new QLineEdit(row);
    (*field)->setReadOnly(true);
    layout->addWidget(*field, 1);
    //: Opens the system window that chooses a file or a folder. In Polish: Przeglądaj...
    *browse = new QPushButton(tr("Browse..."), row);
    (*browse)->setAutoDefault(false);
    layout->addWidget(*browse);
    return row;
}

void ArchiveDialog::showPath(QLineEdit* field, const QString& path)
{
    const QString shown = QDir::toNativeSeparators(path);
    field->setText(shown);
    field->setToolTip(shown);
    // The end of a long path names the folder or the file
    field->setCursorPosition(static_cast<int>(shown.size()));
}

void ArchiveDialog::setHint(const QString& text)
{
    showNote(m_hint, text, QStringLiteral("help.about"));
    m_hint.widget->setVisible(!text.isEmpty() && m_state == State::Ready && m_result->isHidden());
}

void ArchiveDialog::startWork()
{
    m_state = State::Working;
    m_fields->setEnabled(false);
    acceptButton()->setEnabled(false);
    cancelButton()->setEnabled(false);
    m_copyButton->hide();
    m_hint.widget->hide();
    m_result->hide();
    m_progressBar->setValue(0);
    m_progressBar->show();
    // The window shows that the work started
    QCoreApplication::processEvents();
}

ArchiveDialog::Progress ArchiveDialog::progress()
{
    return [this](int percent) {
        m_progressBar->setValue(percent);
        // The window shows the progress while the work lasts; reject() waits for the work
        QCoreApplication::processEvents();
    };
}

void ArchiveDialog::finishWork(const QString& text, const QString& path)
{
    m_state = State::Done;
    m_resultPath = path;
    m_progressBar->setValue(100);
    setNoteIcon(m_resultIcon, QStringLiteral("common.checkCircle"));
    m_resultText->setText(text);
    m_showLink->show();
    m_details->hide();
    m_result->show();

    // The fields stay as the work used them; the window has only Close
    //: The button that closes the window after an export or an import. In Polish: Zamknij
    setAcceptText(tr("Close"));
    cancelButton()->hide();
    acceptButton()->setEnabled(true);
    acceptButton()->setFocus();
}

void ArchiveDialog::failWork(const QString& text, const QStringList& problems)
{
    m_state = State::Ready;
    m_progressBar->hide();
    setNoteIcon(m_resultIcon, QStringLiteral("common.error"));
    m_resultText->setText(text);
    m_showLink->hide();
    m_details->setPlainText(problems.join(QLatin1Char('\n')));
    m_details->setVisible(!problems.isEmpty());
    m_result->show();
    m_copyButton->show();

    // Another try: the fields can be changed again
    m_fields->setEnabled(true);
    acceptButton()->setEnabled(true);
    cancelButton()->setEnabled(true);
}

bool ArchiveDialog::event(QEvent* event)
{
    const bool handled = KalahariDialog::event(event);
    // The layout makes the window as high as the fields need with each wrapped sentence in
    // one line: a sentence of more lines, as the result with a long path, would take the room
    // of the list
    if (event->type() == QEvent::LayoutRequest) {
        fitHeight();
    }
    return handled;
}

void ArchiveDialog::fitHeight()
{
    if (!isVisible() || !layout()->hasHeightForWidth()) {
        return;
    }
    int wanted = layout()->totalHeightForWidth(width());
    QRect available;
    if (const QScreen* screen = this->screen()) {
        // The screen without the title bar and the border of the window
        const QRect frame = frameGeometry();
        const QRect inner = geometry();
        available = screen->availableGeometry() -
                    QMargins(inner.left() - frame.left(), inner.top() - frame.top(),
                             frame.right() - inner.right(), frame.bottom() - inner.bottom());
        wanted = std::min(wanted, available.height());
    }
    // On a small screen the list gives up rows, but the fields keep their room
    wanted = std::max(wanted, layout()->totalMinimumHeightForWidth(width()));
    if (wanted <= height()) {
        return;
    }
    resize(width(), wanted);

    // A window that grew past the bottom of the screen moves up
    if (available.isValid()) {
        const int below = geometry().bottom() - available.bottom();
        const int above = geometry().top() - available.top();
        if (below > 0 && above > 0) {
            move(pos().x(), pos().y() - std::min(below, above));
        }
    }
}

// =============================================================================
// ExportArchiveDialog
// =============================================================================

ExportArchiveDialog::Book ExportArchiveDialog::openBook()
{
    Book book;
    const auto& pm = core::ProjectManager::getInstance();
    if (const core::ProjectBook* projectBook = pm.book()) {
        book.title = projectBook->title;
    }
    for (const auto& [id, status] : pm.getIncompleteElements()) {
        if (const core::ProjectElement* element = pm.findElement(id)) {
            book.unready.append(
                {element->title, status, NavigatorPanel::iconIdOf(pm.bookTypes(), *element)});
        }
    }
    return book;
}

ExportArchiveDialog::ExportArchiveDialog(const Book& book, const QString& folder,
                                         Exporter exporter, QWidget* parent)
    : ArchiveDialog(parent)
    , m_book(book)
    , m_exporter(std::move(exporter))
{
    //: The title of the window. In Polish: Eksportuj archiwum projektu
    setHeading(tr("Export Project Archive"),
               //: %1: the title of the book. In Polish: Archiwum zawiera całą książkę „%1”:
               //: plik projektu, pliki elementów i bazę.
               tr("The archive holds the whole book \"%1\": the project file, the files of the "
                  "elements and the database.")
                   .arg(m_book.title));
    setHeadingIcon(QStringLiteral("file.export.archive"));

    // The archive: its folder and its name
    //: A group of the window: the folder and the name of the archive. In Polish: Archiwum
    QFormLayout* archive = addGroup(tr("Archive"));
    QPushButton* browseButton = nullptr;
    //: In Polish: Folder:
    addRow(archive, tr("Folder:"), newPathField(&m_folderField, &browseButton));
    browseButton->setToolTip(tr("Choose the folder in the system window"));
    m_folderNote = addNote(archive);
    m_nameField = new QLineEdit(core::ProjectManager::archiveFileName(m_book.title), this);
    //: In Polish: Nazwa:
    addRow(archive, tr("Name:"), m_nameField);
    m_nameNote = addNote(archive);
    connect(browseButton, &QPushButton::clicked, this, &ExportArchiveDialog::browse);
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportArchiveDialog::updateFields);

    // The text elements that are not final go as they are
    //: A group of the window; %1: how many files it lists. In Polish: Niegotowe pliki (%1)
    m_unreadyGroup = new QGroupBox(tr("Files Not Ready (%1)").arg(m_book.unready.size()), this);
    auto* unreadyLayout = new QVBoxLayout(m_unreadyGroup);
    m_unreadyList = newRowList(m_unreadyGroup, tr("Files not ready"));
    auto& artProvider = core::ArtProvider::getInstance();
    for (const UnreadyFile& file : std::as_const(m_book.unready)) {
        auto* item = new QTreeWidgetItem(m_unreadyList);
        item->setIcon(0, artProvider.getIcon(file.iconId, core::IconContext::TreeView));
        item->setText(0, file.title);
        item->setText(1, AddToBookDialog::statusName(file.status));
        item->setForeground(1, m_unreadyList->palette().brush(QPalette::PlaceholderText));
    }
    m_unreadyList->setRows(static_cast<int>(m_book.unready.size()));
    unreadyLayout->addWidget(m_unreadyList, 1);
    auto* unreadyNote = newValue(m_unreadyGroup);
    //: Under the list of the files that are not ready. In Polish: Te pliki trafią do archiwum
    //: w obecnym stanie. Stan zmienisz w Nawigatorze.
    unreadyNote->setText(tr("These files go to the archive as they are. You change their "
                            "status in the Navigator."));
    unreadyNote->setForegroundRole(QPalette::PlaceholderText);
    unreadyLayout->addWidget(unreadyNote);
    m_unreadyGroup->setVisible(!m_book.unready.isEmpty());
    fieldsLayout()->addWidget(m_unreadyGroup, 1);
    fieldsLayout()->addStretch();

    QString hint;
    if (m_book.unsavedChanges) {
        //: In Polish: Niezapisane zmiany program zapisze przed eksportem.
        hint = tr("Kalahari saves the unsaved changes before the export.") + QLatin1Char(' ');
    }
    //: In Polish: Postęp i wynik pokażą się tutaj.
    setHint(hint + tr("The progress and the result will show here."));

    //: The button that writes the archive. In Polish: Eksportuj
    setAcceptText(tr("Export"));
    setFolder(folder);

    // The name is typed over, without its extension
    m_nameField->setFocus();
    const QString name = m_nameField->text();
    const auto extension = QLatin1String(core::ProjectManager::ARCHIVE_EXTENSION);
    m_nameField->setSelection(0, static_cast<int>(name.endsWith(extension)
                                                      ? name.size() - extension.size()
                                                      : name.size()));
}

void ExportArchiveDialog::setFolder(const QString& folder)
{
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    updateFields();
}

QString ExportArchiveDialog::archivePath() const
{
    if (!ProgramFolders::problemOf(m_folder).isEmpty() || !nameProblem().isEmpty()) {
        return QString();
    }
    const QString path = QDir(m_folder).filePath(fileName());
    return QFileInfo(path).isDir() ? QString() : path;
}

QTreeWidget* ExportArchiveDialog::unreadyList() const
{
    return m_unreadyList;
}

void ExportArchiveDialog::run()
{
    const QString path = archivePath();
    if (path.isEmpty()) {
        return;
    }
    startWork();
    QStringList problems;
    bool written = false;
    if (!QDir().mkpath(m_folder)) {
        //: %1: the folder of the archive. In Polish: Kalahari nie może utworzyć folderu %1.
        problems.append(
            tr("Kalahari cannot create the folder %1.").arg(QDir::toNativeSeparators(m_folder)));
    } else {
        written = m_exporter(path, progress(), &problems);
    }
    if (written) {
        //: %1: the path of the archive. In Polish: Zapisano archiwum %1.
        finishWork(tr("Saved the archive %1.").arg(QDir::toNativeSeparators(path)), path);
    } else {
        //: In Polish: Nie udało się zapisać archiwum.
        failWork(tr("The archive could not be saved."), problems);
        updateFields();
    }
}

void ExportArchiveDialog::browse()
{
    // A folder that does not exist yet opens the window in the nearest one that does
    QString start = ProgramFolders::existingFolderAt(m_folder);
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    //: The title of the system window that chooses the folder of the archive. In Polish:
    //: Folder archiwum
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Folder of the Archive"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

void ExportArchiveDialog::updateFields()
{
    // The folder: why the program cannot use it, or that it is made for the archive
    const QString folderProblem = ProgramFolders::problemOf(m_folder);
    if (!folderProblem.isEmpty()) {
        showNote(m_folderNote, folderProblem, QStringLiteral("common.warning"));
    } else if (!QFileInfo(m_folder).isDir()) {
        //: Under a folder that does not exist yet. In Polish: Kalahari utworzy ten folder
        //: podczas eksportu.
        showNote(m_folderNote, tr("Kalahari creates this folder during the export."));
    } else {
        showNote(m_folderNote, QString());
    }

    // The name: why it cannot be the name of the archive, the name the archive gets, or the
    // archive it replaces
    bool nameUsable = true;
    const QString problem = nameProblem();
    if (!problem.isEmpty()) {
        nameUsable = false;
        showNote(m_nameNote, problem, QStringLiteral("common.warning"));
    } else if (!folderProblem.isEmpty()) {
        showNote(m_nameNote, QString());
    } else {
        const QFileInfo target(QDir(m_folder).filePath(fileName()));
        if (target.isDir()) {
            nameUsable = false;
            //: %1: the name of the archive. In Polish: W tym folderze jest folder „%1”. Wybierz
            //: inną nazwę.
            showNote(m_nameNote,
                     tr("The folder has a folder named \"%1\". Choose another name.")
                         .arg(fileName()),
                     QStringLiteral("common.warning"));
        } else if (target.exists()) {
            //: %1: the name of the archive. In Polish: W tym folderze jest już archiwum „%1”.
            //: Eksport je zastąpi.
            showNote(m_nameNote,
                     tr("The folder already has the archive \"%1\". The export replaces it.")
                         .arg(fileName()),
                     QStringLiteral("common.warning"));
        } else if (fileName() != m_nameField->text().trimmed()) {
            //: %1: the name with ".klh.zip". In Polish: Archiwum dostanie nazwę „%1”.
            showNote(m_nameNote, tr("The archive gets the name \"%1\".").arg(fileName()));
        } else {
            showNote(m_nameNote, QString());
        }
    }

    acceptButton()->setEnabled(folderProblem.isEmpty() && nameUsable);
}

QString ExportArchiveDialog::fileName() const
{
    const QString name = m_nameField->text().trimmed();
    if (name.isEmpty()) {
        return QString();
    }
    const auto extension = QLatin1String(core::ProjectManager::ARCHIVE_EXTENSION);
    return name.endsWith(extension, Qt::CaseInsensitive) ? name : name + extension;
}

QString ExportArchiveDialog::nameProblem() const
{
    const QString name = m_nameField->text().trimmed();
    if (name.isEmpty()) {
        //: Under an empty name of the archive. In Polish: Wpisz nazwę archiwum.
        return tr("Type the name of the archive.");
    }
    const bool forbidden = std::any_of(name.cbegin(), name.cend(), [](QChar character) {
        return forbiddenCharacters().contains(character) || character.unicode() < 32;
    });
    if (forbidden) {
        QStringList characters;
        for (const QChar character : forbiddenCharacters()) {
            characters << QString(character);
        }
        //: %1: the characters, with spaces between them. In Polish: Nazwa nie może zawierać
        //: znaków %1.
        return tr("A name cannot have the characters %1.")
            .arg(characters.join(QLatin1Char(' ')));
    }
    return QString();
}

// =============================================================================
// ImportArchiveDialog
// =============================================================================

ImportArchiveDialog::ImportArchiveDialog(const core::BookTypeRegistry& registry,
                                         const QString& folder, Importer importer,
                                         QWidget* parent)
    : ArchiveDialog(parent)
    , m_registry(&registry)
    , m_importer(std::move(importer))
{
    //: The title of the window. In Polish: Importuj archiwum projektu
    setHeading(tr("Import Project Archive"),
               //: In Polish: Książka z archiwum trafi do nowego folderu i otworzy się
               //: w programie.
               tr("The book from the archive goes to a new folder and opens in the program."));
    setHeadingIcon(QStringLiteral("file.import.archive"));

    // The archive and its book
    m_archiveForm = addGroup(tr("Archive"));
    QPushButton* browseArchiveButton = nullptr;
    //: In Polish: Plik:
    addRow(m_archiveForm, tr("File:"), newPathField(&m_archiveField, &browseArchiveButton));
    //: In the empty field of the archive. In Polish: Wybierz archiwum przyciskiem Przeglądaj...
    m_archiveField->setPlaceholderText(tr("Choose the archive with Browse..."));
    browseArchiveButton->setToolTip(tr("Choose the archive in the system window"));
    m_archiveNote = addNote(m_archiveForm);
    const auto addBookRow = [this](const QString& label, QLabel** value) {
        *value = newValue(this);
        QLabel* rowLabel = addRow(m_archiveForm, label, *value);
        m_bookRows.emplaceBack(rowLabel, *value);
        m_archiveForm->setRowVisible(*value, false);
        return rowLabel;
    };
    //: A row of the book of the archive. In Polish: Tytuł:
    addBookRow(tr("Title:"), &m_titleValue);
    //: In Polish: Autor:
    addBookRow(tr("Author:"), &m_authorValue);
    //: In Polish: Typ:
    addBookRow(tr("Type:"), &m_typeValue);
    //: The elements of the book: "8 elements: 5 chapters, a prologue...". In Polish:
    //: Zawartość:
    addBookRow(tr("Contents:"), &m_contentsValue);
    //: The elements of the Workshop of the book. In Polish: Warsztat:
    m_workshopLabel = addBookRow(tr("Workshop:"), &m_workshopValue);
    connect(browseArchiveButton, &QPushButton::clicked, this,
            &ImportArchiveDialog::browseArchive);

    // The folder in which the folder of the book is made
    //: A group of the window: where the imported book goes. In Polish: Miejsce
    QFormLayout* place = addGroup(tr("Place"));
    QPushButton* browseFolderButton = nullptr;
    addRow(place, tr("Folder:"), newPathField(&m_folderField, &browseFolderButton));
    browseFolderButton->setToolTip(tr("Choose the folder in the system window"));
    m_folderNote = addNote(place);
    connect(browseFolderButton, &QPushButton::clicked, this, &ImportArchiveDialog::browseFolder);
    fieldsLayout()->addStretch();

    //: In Polish: Postęp importu pokaże się tutaj. Gdy coś się nie uda, opis błędu zostanie
    //: w tym oknie i skopiujesz go przyciskiem „Kopiuj”.
    setHint(tr("The progress of the import will show here. When something goes wrong, the "
               "description of the error stays in this window, and Copy puts it on the "
               "clipboard."));

    //: The button that imports the archive. In Polish: Importuj
    setAcceptText(tr("Import"));
    setFolder(folder);
    browseArchiveButton->setFocus();
}

void ImportArchiveDialog::setOpeningQuestion(OpeningQuestion question)
{
    m_openingQuestion = std::move(question);
}

void ImportArchiveDialog::setArchive(const QString& path)
{
    m_archive = path;
    showPath(m_archiveField, path);
    m_archiveProblems.clear();
    m_project.reset();
    if (!path.isEmpty()) {
        m_project = core::ProjectManager::readArchive(path, &m_archiveProblems);
    }
    showBook();
    updateFields();
}

void ImportArchiveDialog::setFolder(const QString& folder)
{
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    updateFields();
}

QString ImportArchiveDialog::bookFolder() const
{
    return QDir(m_folder).filePath(core::ProjectManager::archiveProjectName(m_archive));
}

QString ImportArchiveDialog::bookText() const
{
    QStringList lines;
    for (const auto& [label, value] : m_bookRows) {
        if (m_archiveForm->isRowVisible(value)) {
            lines << label->text() + QLatin1Char(' ') + value->text();
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString ImportArchiveDialog::contentsOf(const core::BookTypeRegistry& registry,
                                        const QList<const core::ProjectElement*>& elements)
{
    if (elements.isEmpty()) {
        //: The contents of a book without elements. In Polish: brak elementów
        return tr("no elements");
    }

    // The kinds with their numbers of elements, the kinds of more elements first
    struct KindCount {
        const core::ProjectElement* first;
        int count;
    };
    QList<KindCount> kinds;
    for (const core::ProjectElement* element : elements) {
        const auto found = std::find_if(kinds.begin(), kinds.end(),
                                        [&registry, element](const KindCount& kind) {
                                            return sameKind(registry, *kind.first, *element);
                                        });
        if (found != kinds.end()) {
            ++found->count;
        } else {
            kinds.append({element, 1});
        }
    }
    std::stable_sort(kinds.begin(), kinds.end(), [](const KindCount& a, const KindCount& b) {
        return a.count > b.count;
    });

    const auto counted = [&registry](const KindCount& kind, bool alone) {
        const QHash<QString, core::KindWords> nouns{
            {QStringLiteral("kind"), wordsOf(registry, *kind.first)}};
        if (kind.count > 1) {
            //: How many elements of a kind: "5 chapters". In Polish: %n {kind} |
            //: %n {kind:plural} | %n {kind:genitivePlural}
            return core::fillWords(tr("%n {kind:plural}", nullptr, kind.count), nouns);
        }
        if (alone) {
            //: The one element of a book: "1 chapter". In Polish: 1 {kind}
            return core::fillWords(tr("1 {kind}"), nouns);
        }
        //: One element among the elements of other kinds: "a prologue". In Polish: {kind}
        return core::fillWords(tr("{kind:indefinite}"), nouns);
    };
    if (kinds.size() == 1) {
        return counted(kinds.first(), true);
    }
    QStringList parts;
    for (const KindCount& kind : std::as_const(kinds)) {
        parts << counted(kind, false);
    }
    //: %1: the elements of each kind, "5 chapters, a prologue and an epilogue". In Polish:
    //: %n element: %1 | %n elementy: %1 | %n elementów: %1
    return tr("%n elements: %1", nullptr, static_cast<int>(elements.size()))
        .arg(QLocale(programLanguage()).createSeparatedList(parts));
}

void ImportArchiveDialog::run()
{
    if (!m_project) {
        return;
    }

    // The question about the open book comes before the work; Cancel returns to the window
    bool open = true;
    if (m_openingQuestion) {
        switch (m_openingQuestion(bookTitle())) {
            case Opening::Open:
                break;
            case Opening::DontOpen:
                open = false;
                break;
            case Opening::Cancel:
                return;
        }
    }

    startWork();
    const QString folder = bookFolder();
    QStringList problems;
    if (m_importer(m_archive, m_folder, open, progress(), &problems)) {
        const QString shown = QDir::toNativeSeparators(folder);
        QString text;
        if (open) {
            //: %1: the title of the book; %2: its folder. In Polish: Zaimportowano „%1” do
            //: folderu %2. Książka jest już otwarta.
            text = tr("Imported \"%1\" to the folder %2. The book is open.");
        } else {
            //: %1: the title of the book; %2: its folder. In Polish: Zaimportowano „%1” do
            //: folderu %2. Książkę znajdziesz na Pulpicie i w Plik > Ostatnie książki.
            text = tr("Imported \"%1\" to the folder %2. The book is on the Dashboard and in "
                      "File > Recent Books.");
        }
        finishWork(text.arg(bookTitle(), shown), folder);
    } else {
        //: In Polish: Nie udało się zaimportować archiwum.
        failWork(tr("The archive could not be imported."), problems);
        updateFields();
    }
}

void ImportArchiveDialog::browseArchive()
{
    const QString start =
        m_archive.isEmpty()
            ? ProgramFolders::windowFolder(ProgramFolders::Operation::ImportArchive)
            : QFileInfo(m_archive).absolutePath();
    //: The title of the system window that chooses the archive. In Polish: Wybierz archiwum
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose the Archive"), start,
                                                        tr("Kalahari Archive (*.klh.zip)"));
    if (chosen.isEmpty()) {
        return;
    }
    ProgramFolders::remember(ProgramFolders::Operation::ImportArchive,
                             QFileInfo(chosen).absolutePath());
    setArchive(chosen);
}

void ImportArchiveDialog::browseFolder()
{
    // A folder that does not exist yet opens the window in the nearest one that does
    QString start = ProgramFolders::existingFolderAt(m_folder);
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    //: The title of the system window that chooses where the imported book goes. In Polish:
    //: Miejsce książki
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Place of the Book"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

void ImportArchiveDialog::updateFields()
{
    // The archive: why its book cannot be read
    if (!m_archive.isEmpty() && !m_project) {
        //: Under an archive whose book cannot be read; the problems follow it. In Polish:
        //: Kalahari nie może odczytać książki z tego archiwum.
        QString text = tr("Kalahari cannot read the book of this archive.");
        if (!m_archiveProblems.isEmpty()) {
            text += QLatin1Char('\n') + m_archiveProblems.join(QLatin1Char('\n'));
        }
        showNote(m_archiveNote, text, QStringLiteral("common.warning"));
    } else {
        showNote(m_archiveNote, QString());
    }

    // The folder: where the book of the archive goes, or why it cannot go there
    bool folderUsable = true;
    const QString problem = ProgramFolders::problemOf(m_folder);
    if (!problem.isEmpty()) {
        folderUsable = false;
        showNote(m_folderNote, problem, QStringLiteral("common.warning"));
    } else if (m_project) {
        const QString name = core::ProjectManager::archiveProjectName(m_archive);
        if (QFileInfo::exists(bookFolder())) {
            folderUsable = false;
            //: %1: the name of the folder of the book. In Polish: W tym miejscu jest już
            //: folder „%1”. Wybierz dla książki inne miejsce.
            showNote(m_folderNote,
                     tr("The folder \"%1\" is already here. Choose another place for the book.")
                         .arg(name),
                     QStringLiteral("common.warning"));
        } else {
            //: %1: the name of the folder of the book. In Polish: Książka powstanie w folderze
            //: „%1”.
            showNote(m_folderNote, tr("The book goes to the folder \"%1\".").arg(name));
        }
    } else {
        showNote(m_folderNote, QString());
    }

    acceptButton()->setEnabled(m_project.has_value() && folderUsable);
}

void ImportArchiveDialog::showBook()
{
    const core::ProjectBook* book =
        m_project && !m_project->books.isEmpty() ? &m_project->books.first() : nullptr;
    m_archiveForm->setRowVisible(m_titleValue, m_project.has_value());
    m_archiveForm->setRowVisible(m_typeValue, m_project.has_value());
    m_archiveForm->setRowVisible(m_contentsValue, m_project.has_value());
    if (!m_project) {
        m_archiveForm->setRowVisible(m_authorValue, false);
        m_archiveForm->setRowVisible(m_workshopValue, false);
        return;
    }

    m_titleValue->setText(bookTitle());
    const QString author = book ? book->author.trimmed() : QString();
    m_authorValue->setText(author);
    m_archiveForm->setRowVisible(m_authorValue, !author.isEmpty());
    m_typeValue->setText(typeName());

    // The elements of the book: the main part first, as it holds the most of them
    QList<const core::ProjectElement*> elements;
    if (book) {
        appendAll(book->mainElements, elements);
        appendAll(book->frontElements, elements);
        appendAll(book->backElements, elements);
    }
    m_contentsValue->setText(contentsOf(*m_registry, elements));

    // The Workshop, when it has elements, by the name of its node
    QList<const core::ProjectElement*> workshop;
    appendAll(m_project->workshop.elements, workshop);
    m_workshopLabel->setText(m_project->workshop.name.isEmpty()
                                 ? tr("Workshop:")
                                 : m_project->workshop.name + QLatin1Char(':'));
    m_workshopValue->setText(contentsOf(*m_registry, workshop));
    m_archiveForm->setRowVisible(m_workshopValue, !workshop.isEmpty());
}

QString ImportArchiveDialog::bookTitle() const
{
    if (m_project && !m_project->books.isEmpty() &&
        !m_project->books.first().title.trimmed().isEmpty()) {
        return m_project->books.first().title;
    }
    return core::ProjectManager::archiveProjectName(m_archive);
}

QString ImportArchiveDialog::typeName() const
{
    if (!m_project || !m_project->type) {
        //: The type of a book whose author chose its kinds. In Polish: Projekt użytkownika
        return tr("User Project");
    }
    if (const core::BookTypePackage* package = m_registry->package(m_project->type->id)) {
        return package->name.text(programLanguage());
    }
    //: The type of a book whose package is not installed; %1: the id of the type. In Polish:
    //: %1 (typ niezainstalowany)
    return tr("%1 (not installed)").arg(m_project->type->id);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
