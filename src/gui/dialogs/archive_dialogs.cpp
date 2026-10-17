/// @file archive_dialogs.cpp
/// @brief Export Project Archive and Import Project Archive: each in one window, from the
/// choice of the archive to the result of the work

#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/package_card.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMargins>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int WIDTH_IN_CHARACTERS = 80;  ///< The window has room for a path of this length
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

/// The row under the last row of @p grid; a grid without widgets has none
int nextRow(const QGridLayout* grid) {
    return grid->count() == 0 ? 0 : grid->rowCount();
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
    // The layout takes the whole bar, so the content that scrolls to it shows it whole: on
    // macOS the style puts the bar a few pixels past the room the layout gives it, past the
    // end of the content
    m_progressBar->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    //: The bar of the progress of an export or an import. In Polish: Postęp
    m_progressBar->setAccessibleName(tr("Progress"));
    m_progressBar->hide();
    contentLayout()->addWidget(m_progressBar);

    // The result: its icon, the sentence and Show in Folder, or why the work failed
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
    m_reasonText = newValue(m_result);
    m_reasonText->hide();
    resultTexts->addWidget(m_reasonText);
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

QString ArchiveDialog::reasonText() const
{
    return m_result->isVisibleTo(this) && m_failed ? m_reasonText->text() : QString();
}

QString ArchiveDialog::detailsText() const
{
    return m_result->isVisibleTo(this) && m_failed ? m_details.join(QLatin1Char('\n'))
                                                   : QString();
}

QString ArchiveDialog::clipboardText() const
{
    // What the window says went wrong, each followed by how, as the log has it
    QStringList parts{windowTitle()};
    if (!m_fieldProblem.isEmpty()) {
        parts << m_fieldProblem << m_fieldDetails.join(QLatin1Char('\n'));
    }
    if (m_result->isVisibleTo(this)) {
        QStringList sentences{resultText(), reasonText()};
        sentences.removeAll(QString());
        parts << sentences.join(QLatin1Char('\n')) << detailsText();
    }
    parts.removeAll(QString());
    return parts.join(QStringLiteral("\n\n"));
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

QGridLayout* ArchiveDialog::addGroup(const QString& title)
{
    auto* group = new QGroupBox(title, m_fields);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    m_fieldsLayout->addWidget(group);
    return grid;
}

QLabel* ArchiveDialog::addRow(QGridLayout* group, const QString& label, QWidget* field)
{
    auto* rowLabel = new QLabel(label, group->parentWidget());
    rowLabel->setTextFormat(Qt::PlainText);
    rowLabel->setBuddy(field);
    // At the side the system puts the labels of forms; level with a field of one line, and
    // with the first line of a field of more lines
    const Qt::Alignment side =
        Qt::Alignment(style()->styleHint(QStyle::SH_FormLayoutLabelAlignment, nullptr, this)) &
        Qt::AlignHorizontal_Mask;
    const int row = nextRow(group);
    group->addWidget(rowLabel, row, 0,
                     side | (field->hasHeightForWidth() ? Qt::AlignTop : Qt::AlignVCenter));
    group->addWidget(field, row, 1);

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

ArchiveDialog::FieldNote ArchiveDialog::addNote(QGridLayout* group)
{
    FieldNote note;
    note.widget = newNote(group->parentWidget(), &note.icon, &note.text);
    note.widget->hide();
    group->addWidget(note.widget, nextRow(group), 1);
    return note;
}

void ArchiveDialog::setRowVisible(QWidget* field, bool visible)
{
    field->setVisible(visible);
    for (QLabel* label : std::as_const(m_rowLabels)) {
        if (label->buddy() == field) {
            label->setVisible(visible);
        }
    }
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
    note.widget->setVisible(!text.isEmpty());
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
    m_failed = false;
    m_details.clear();
    m_fields->setEnabled(false);
    acceptButton()->setEnabled(false);
    cancelButton()->setEnabled(false);
    updateCopyButton();
    m_hint.widget->hide();
    m_result->hide();
    m_progressBar->setValue(0);
    m_progressBar->show();
    bringIntoView(m_progressBar);
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
    m_reasonText->hide();
    m_showLink->show();
    m_result->show();

    // The fields stay as the work used them; the window has only Close
    //: The button that closes the window after an export or an import. In Polish: Zamknij
    setAcceptText(tr("Close"));
    cancelButton()->hide();
    acceptButton()->setEnabled(true);
    acceptButton()->setFocus();
    bringIntoView(m_result);
}

void ArchiveDialog::failWork(const QString& text, const QString& reason,
                             const QStringList& details)
{
    m_state = State::Ready;
    m_failed = true;
    m_details = details;
    m_progressBar->hide();
    setNoteIcon(m_resultIcon, QStringLiteral("common.error"));
    m_resultText->setText(text);
    // A cause the window does not name is in the details
    m_reasonText->setText(reason.isEmpty() && !details.isEmpty() ? copyHint() : reason);
    m_reasonText->setVisible(!m_reasonText->text().isEmpty());
    m_showLink->hide();
    m_result->show();
    updateCopyButton();

    // Another try: the fields can be changed again
    m_fields->setEnabled(true);
    acceptButton()->setEnabled(true);
    cancelButton()->setEnabled(true);
    bringIntoView(m_result);
}

void ArchiveDialog::dropFailure()
{
    if (m_state != State::Ready || !m_failed) {
        return;
    }
    m_failed = false;
    m_details.clear();
    m_result->hide();
    m_hint.widget->setVisible(!m_hint.text->text().isEmpty());
    updateCopyButton();
}

void ArchiveDialog::setFieldProblem(const QString& text, const QStringList& details)
{
    m_fieldProblem = text;
    m_fieldDetails = text.isEmpty() ? QStringList() : details;
    updateCopyButton();
}

QString ArchiveDialog::copyHint()
{
    //: Under what went wrong when the program does not know its cause. In Polish: Opis
    //: techniczny błędu skopiujesz przyciskiem „Kopiuj”.
    return tr("Copy puts the technical description of the error on the clipboard.");
}

void ArchiveDialog::updateCopyButton()
{
    m_copyButton->setVisible(m_state == State::Ready && (m_failed || !m_fieldProblem.isEmpty()));
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
    const auto* area = findChild<FittingScrollArea*>(QString(), Qt::FindDirectChildrenOnly);
    if (!isVisible() || !area) {
        return;
    }
    // The content's own height at the width it has, in place of the height the area of the
    // content asks for, as in KalahariDialog::sizeHint(). The width is the one beside the
    // scroll bar while the area shows it: the area keeps the bar as long as the content does
    // not fit at that width, where wrapped sentences take more lines, and the end of the
    // result would stay out of view. In the higher window the bar goes, and the fields take
    // the room the wider sentences leave.
    const int height = layout()->hasHeightForWidth() ? layout()->totalHeightForWidth(width())
                                                     : layout()->totalSizeHint().height();
    const int wanted = height - area->sizeHint().height() +
                       area->contentHeightForWidth(area->viewport()->width());
    // Within the screen, where the content scrolls; a window that grew past the bottom of the
    // screen moves up. The layout has just taken in the content: asking it again would bring
    // another LayoutRequest, and a window that cannot grow would never stop asking.
    if (wanted > this->height()) {
        resizeOnScreen(QSize(width(), wanted));
    }
}

void ArchiveDialog::bringIntoView(QWidget* widget)
{
    auto* area = findChild<FittingScrollArea*>(QString(), Qt::FindDirectChildrenOnly);
    if (!isVisible() || !area) {
        return;
    }
    // The window grows for what it shows now (fitHeight()) and the content takes its place,
    // before the area scrolls to it
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    // It stays in view in a window the system or the writer makes lower later; a result
    // higher than the area shows its sentence, with the reason under it
    area->keepInView(widget, widget == m_result ? m_resultText : nullptr);
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
    book.backups = static_cast<int>(pm.databaseBackups().size());
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
    QGridLayout* archive = addGroup(tr("Archive"));
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
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportArchiveDialog::dropFailure);
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportArchiveDialog::updateFields);

    // The backups of the database go only when the writer wants them
    //: A check box of the window; %1: how many backups the book has. In Polish: Dołącz kopie
    //: zapasowe bazy (%1)
    m_backupsBox = new QCheckBox(tr("Include the database backups (%1)").arg(m_book.backups),
                                 archive->parentWidget());
    m_backupsBox->setEnabled(m_book.backups > 0);
    connect(m_backupsBox, &QCheckBox::toggled, this, &ExportArchiveDialog::dropFailure);
    archive->addWidget(m_backupsBox, nextRow(archive), 1);
    m_backupsNote = addNote(archive);
    if (m_book.backups > 0) {
        //: Under the check box of the backups. In Polish: Archiwum zabierze kopie bazy
        //: zrobione przy zamykaniu książki, więc po imporcie na innym komputerze książka będzie
        //: miała swoje kopie zapasowe.
        showNote(m_backupsNote,
                 tr("The archive takes the copies of the database made when the book was "
                    "closed, so after an import on another computer the book has its "
                    "backups."));
    } else {
        //: Under the check box of the backups, which cannot be checked. In Polish: Książka nie
        //: ma jeszcze kopii bazy: program robi je przy zamykaniu książki.
        showNote(m_backupsNote, tr("The book has no backups of its database yet: Kalahari "
                                   "makes them when the book is closed."));
    }

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
    dropFailure();
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
    QString reason;
    core::ArchiveProblem problem;
    bool written = false;
    if (!QDir().mkpath(m_folder)) {
        //: %1: the folder of the archive. In Polish: Kalahari nie może utworzyć folderu %1.
        reason =
            tr("Kalahari cannot create the folder %1.").arg(QDir::toNativeSeparators(m_folder));
    } else {
        written = m_exporter(path, m_backupsBox->isChecked(), progress(), &problem);
        reason = reasonOf(problem);
    }
    if (written) {
        //: %1: the path of the archive. In Polish: Zapisano archiwum %1.
        finishWork(tr("Saved the archive %1.").arg(QDir::toNativeSeparators(path)), path);
    } else {
        //: In Polish: Nie udało się zapisać archiwum.
        failWork(tr("The archive could not be saved."), reason, problem.lines);
        updateFields();
    }
}

QString ExportArchiveDialog::reasonOf(const core::ArchiveProblem& problem) const
{
    using Kind = core::ArchiveProblem::Kind;
    switch (problem.kind) {
        case Kind::None:
            return QString();
        case Kind::ChangesNotSaved:
            //: In Polish: Nie udało się zapisać niektórych zmian w książce. Szczegóły są
            //: w dzienniku.
            return tr("Some changes in the book could not be saved. Check the log for details.");
        case Kind::UnreadableFile:
            //: In Polish: Kalahari nie może odczytać jednego z plików książki.
            return tr("Kalahari cannot read one of the files of the book.");
        case Kind::CannotWrite:
            //: %1: the folder of the archive. In Polish: Kalahari nie może zapisywać plików
            //: w folderze %1. Wybierz inny folder.
            return tr("Kalahari cannot save files in the folder %1. Choose another folder.")
                .arg(QDir::toNativeSeparators(m_folder));
        case Kind::DiskFull:
            //: In Polish: Na dysku zabrakło miejsca. Zwolnij miejsce albo wybierz inny folder.
            return tr("There is no room left on the disk. Free some space or choose another "
                      "folder.");
        case Kind::Other:
        case Kind::NotFound:
        case Kind::CannotRead:
        case Kind::NotArchive:
        case Kind::Damaged:
        case Kind::Unsupported:
        case Kind::NoBook:
        case Kind::DamagedBook:
        case Kind::NewerBook:
        case Kind::FolderExists:
        case Kind::NotOpened:
            break;  // Not named, or not of an export
    }
    return QString();
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
    QGridLayout* archive = addGroup(tr("Archive"));
    QPushButton* browseArchiveButton = nullptr;
    //: In Polish: Plik:
    addRow(archive, tr("File:"), newPathField(&m_archiveField, &browseArchiveButton));
    //: In the empty field of the archive. In Polish: Wybierz archiwum przyciskiem Przeglądaj...
    m_archiveField->setPlaceholderText(tr("Choose the archive with Browse..."));
    browseArchiveButton->setToolTip(tr("Choose the archive in the system window"));
    m_archiveNote = addNote(archive);
    const auto addBookRow = [this, archive](const QString& label, QLabel** value) {
        *value = newValue(this);
        QLabel* rowLabel = addRow(archive, label, *value);
        m_bookRows.emplaceBack(rowLabel, *value);
        setRowVisible(*value, false);
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
    //: The backups of the database in the archive. In Polish: Kopie zapasowe:
    addBookRow(tr("Backups:"), &m_backupsValue);
    connect(browseArchiveButton, &QPushButton::clicked, this,
            &ImportArchiveDialog::browseArchive);

    // The folder in which the folder of the book is made
    //: A group of the window: where the imported book goes. In Polish: Miejsce
    QGridLayout* place = addGroup(tr("Place"));
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
    m_archiveProblem = core::ArchiveProblem();
    m_project.reset();
    m_backups = 0;
    if (!path.isEmpty()) {
        m_project = core::ProjectManager::readArchive(path, &m_archiveProblem, &m_backups);
    }

    // The type and the kinds that the program does not have are named after the archive's
    // cards of their packages, as the book will show them after the import
    m_cards.clear();
    m_cardsFolder.reset();
    if (m_project && !m_project->missingPackages(*m_registry).isEmpty()) {
        m_cardsFolder = std::make_unique<QTemporaryDir>();
        if (m_cardsFolder->isValid() &&
            core::ProjectManager::unpackArchiveCards(path, m_cardsFolder->path())) {
            m_cards = core::PackageCards::readAll(m_cardsFolder->path(), *m_project);
        }
    }
    dropFailure();
    showBook();
    updateFields();
}

void ImportArchiveDialog::setFolder(const QString& folder)
{
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    dropFailure();
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
        if (value->isVisibleTo(this)) {
            lines << label->text() + QLatin1Char(' ') + value->text();
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString ImportArchiveDialog::contentsOf(const core::BookTypeRegistry& registry,
                                        const QList<const core::ProjectElement*>& elements,
                                        const QHash<QString, core::PackageCard>& cards)
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

    const auto counted = [&registry, &cards](const KindCount& kind, bool alone) {
        const auto card = cards.constFind(kind.first->kind.packageId);
        const QHash<QString, core::KindWords> nouns{
            {QStringLiteral("kind"),
             wordsOf(registry, *kind.first, card != cards.cend() ? &*card : nullptr)}};
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
    core::ArchiveProblem problem;
    if (m_importer(m_archive, m_folder, open, progress(), &problem)) {
        const QString shown = QDir::toNativeSeparators(folder);
        QString text;
        if (open) {
            //: %1: the title of the book; %2: its folder. In Polish: Zaimportowano „%1” do
            //: folderu %2. Książka jest już otwarta.
            text = tr("Imported \"%1\" to the folder %2. The book is open.");
        } else {
            //: %1: the title of the book; %2: its folder. In Polish: Zaimportowano „%1” do
            //: folderu %2. Książkę znajdziesz na Pulpicie programu i w menu Plik > Ostatnie
            //: książki.
            text = tr("Imported \"%1\" to the folder %2. The book is on the Dashboard and in "
                      "File > Recent Books.");
        }
        finishWork(text.arg(bookTitle(), shown), folder);
    } else {
        //: In Polish: Nie udało się zaimportować archiwum.
        failWork(tr("The archive could not be imported."), reasonOf(problem), problem.lines);
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
    // The archive: why its book cannot be read, with Copy for how
    if (!m_archive.isEmpty() && !m_project) {
        QString text = reasonOf(m_archiveProblem);
        if (text.isEmpty()) {
            // A cause the window does not name is in the details
            //: Under an archive whose book cannot be read. In Polish: Kalahari nie może
            //: odczytać książki z tego archiwum.
            text = tr("Kalahari cannot read the book of this archive.");
            if (!m_archiveProblem.lines.isEmpty()) {
                text += QLatin1Char(' ') + copyHint();
            }
        }
        showNote(m_archiveNote, text, QStringLiteral("common.warning"));
        setFieldProblem(text, m_archiveProblem.lines);
    } else {
        showNote(m_archiveNote, QString());
        setFieldProblem(QString(), {});
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
    setRowVisible(m_titleValue, m_project.has_value());
    setRowVisible(m_typeValue, m_project.has_value());
    setRowVisible(m_contentsValue, m_project.has_value());
    if (!m_project) {
        setRowVisible(m_authorValue, false);
        setRowVisible(m_workshopValue, false);
        setRowVisible(m_backupsValue, false);
        return;
    }

    m_titleValue->setText(bookTitle());
    const QString author = book ? book->author.trimmed() : QString();
    m_authorValue->setText(author);
    setRowVisible(m_authorValue, !author.isEmpty());
    // A type the program does not have is greyed out: the book opens read-only
    const bool typeMissing = m_project->type && !m_registry->package(m_project->type->id);
    const auto typeCard =
        m_project->type ? m_cards.constFind(m_project->type->id) : m_cards.cend();
    m_typeValue->setText(typeNameOf(*m_registry, *m_project,
                                    typeCard != m_cards.cend() ? &*typeCard : nullptr));
    m_typeValue->setEnabled(!typeMissing);

    // The elements of the book: the main part first, as it holds the most of them
    QList<const core::ProjectElement*> elements;
    if (book) {
        appendAll(book->mainElements, elements);
        appendAll(book->frontElements, elements);
        appendAll(book->backElements, elements);
    }
    m_contentsValue->setText(contentsOf(*m_registry, elements, m_cards));

    // The Workshop, when it has elements, by the name of its node
    QList<const core::ProjectElement*> workshop;
    appendAll(m_project->workshop.elements, workshop);
    m_workshopLabel->setText(m_project->workshop.name.isEmpty()
                                 ? tr("Workshop:")
                                 : m_project->workshop.name + QLatin1Char(':'));
    m_workshopValue->setText(contentsOf(*m_registry, workshop, m_cards));
    setRowVisible(m_workshopValue, !workshop.isEmpty());

    // The backups of the database, which go where Kalahari keeps the backups of books
    if (m_backups == 1) {
        //: The one backup of the database in the archive. In Polish: 1 kopia bazy
        m_backupsValue->setText(tr("1 database backup"));
    } else {
        //: How many backups of the database the archive has. In Polish: %n kopia bazy |
        //: %n kopie bazy | %n kopii bazy
        m_backupsValue->setText(tr("%n database backups", nullptr, m_backups));
    }
    m_backupsValue->setToolTip(
        //: In Polish: Po imporcie kopie trafią tam, gdzie program trzyma kopie zapasowe
        //: (Ustawienia > Pliki > Foldery).
        tr("After the import the backups are where Kalahari keeps backups (Settings > Files > "
           "Folders)."));
    setRowVisible(m_backupsValue, m_backups > 0);
}

QString ImportArchiveDialog::reasonOf(const core::ArchiveProblem& problem) const
{
    using Kind = core::ArchiveProblem::Kind;
    switch (problem.kind) {
        case Kind::None:
            return QString();
        case Kind::NotFound:
            //: Under an archive, or why it could not be imported. In Polish: Ten plik już nie
            //: istnieje.
            return tr("This file no longer exists.");
        case Kind::CannotRead:
            //: In Polish: Kalahari nie może odczytać tego pliku. Sprawdź, czy masz do niego
            //: dostęp.
            return tr("Kalahari cannot read this file. Check that you have access to it.");
        case Kind::NotArchive:
            //: In Polish: Ten plik nie jest archiwum ZIP albo jest uszkodzony.
            return tr("This file is not a ZIP archive, or it is damaged.");
        case Kind::Damaged:
            //: In Polish: To archiwum jest uszkodzone.
            return tr("This archive is damaged.");
        case Kind::Unsupported:
            //: In Polish: To archiwum jest zaszyfrowane albo spakowane w sposób, którego
            //: Kalahari nie obsługuje.
            return tr("This archive is encrypted or packed in a way that Kalahari does not "
                      "support.");
        case Kind::NoBook:
            //: In Polish: W tym archiwum nie ma książki programu Kalahari.
            return tr("This archive has no Kalahari book.");
        case Kind::DamagedBook:
            //: In Polish: Plik książki w tym archiwum jest uszkodzony.
            return tr("The book file in this archive is damaged.");
        case Kind::NewerBook:
            // Not a damaged archive: the writer needs a newer program
            //: Under an archive with a book of a newer version of the program. In Polish: Tę
            //: książkę zapisano w nowszej wersji programu Kalahari. Zaktualizuj program, aby
            //: ją zaimportować.
            return tr("This book was saved in a newer version of Kalahari. Update the program "
                      "to import it.");
        case Kind::FolderExists:
            //: %1: the name of the folder of the book. In Polish: W tym miejscu jest już
            //: folder „%1”. Wybierz dla książki inne miejsce.
            return tr("The folder \"%1\" is already here. Choose another place for the book.")
                .arg(core::ProjectManager::archiveProjectName(m_archive));
        case Kind::CannotWrite:
            //: %1: the folder in which the folder of the book is made. In Polish: Kalahari nie
            //: może zapisywać plików w folderze %1. Wybierz dla książki inne miejsce.
            return tr("Kalahari cannot save files in the folder %1. Choose another place for "
                      "the book.")
                .arg(QDir::toNativeSeparators(m_folder));
        case Kind::DiskFull:
            //: In Polish: Na dysku zabrakło miejsca. Zwolnij miejsce albo wybierz dla książki
            //: inne miejsce.
            return tr("There is no room left on the disk. Free some space or choose another "
                      "place for the book.");
        case Kind::NotOpened:
            //: In Polish: Kalahari nie może otworzyć książki z tego archiwum.
            return tr("Kalahari cannot open the book of this archive.");
        case Kind::Other:
        case Kind::ChangesNotSaved:
        case Kind::UnreadableFile:
            break;  // Not named, or not of an import
    }
    return QString();
}

QString ImportArchiveDialog::bookTitle() const
{
    if (m_project && !m_project->books.isEmpty() &&
        !m_project->books.first().title.trimmed().isEmpty()) {
        return m_project->books.first().title;
    }
    return core::ProjectManager::archiveProjectName(m_archive);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
