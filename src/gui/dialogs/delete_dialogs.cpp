/// @file delete_dialogs.cpp
/// @brief The windows that delete an element from the book and that delete files: what goes,
/// and where its files go

#include "kalahari/gui/dialogs/delete_dialogs.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/utils/file_location.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <iterator>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int LIST_ROWS = 8;            ///< Rows the list shows at most; the others scroll
constexpr int MINIMUM_ROWS = 3;         ///< Rows the list keeps on a small screen
constexpr int NOTE_ICON_SIZE = 16;      ///< Size of the icon of the note about the files
constexpr int COPIED_NOTICE_MS = 2000;  ///< How long Copy says "Copied"

#if defined(Q_OS_WIN)
constexpr bool RECYCLE_BIN = true;      ///< The system's Trash is the Recycle Bin of Windows
#else
constexpr bool RECYCLE_BIN = false;
#endif

/// The list of a window, as high as its rows, up to LIST_ROWS
class RowList : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    /// Set the number of rows the list has; its height follows it
    void setRows(int rows) {
        m_rows = rows;
        updateGeometry();
    }

    QSize sizeHint() const override {
        return {QTreeWidget::sizeHint().width(), heightFor(qBound(1, m_rows, LIST_ROWS))};
    }

    QSize minimumSizeHint() const override {
        return {QTreeWidget::minimumSizeHint().width(),
                heightFor(qBound(1, m_rows, MINIMUM_ROWS))};
    }

private:
    int heightFor(int rows) const {
        const int measured = topLevelItemCount() > 0 ? sizeHintForRow(0) : 0;
        const int rowHeight =
            qMax(measured, qMax(fontMetrics().height() + 6, iconSize().height() + 4));
        return rowHeight * rows + 2 * frameWidth();
    }

    int m_rows = 0;
};

/// A list of what a window deletes: a title with its icon, and the name of its file
RowList* newList(QWidget* parent, const QString& accessibleName) {
    auto* list = new RowList(parent);
    list->setColumnCount(2);
    list->setHeaderHidden(true);
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setAccessibleName(accessibleName);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    return list;
}

/// The column of the files of @p item: the name of @p path, grey, with the whole path in its
/// tooltip
void showFile(QTreeWidgetItem* item, const QString& path, const QTreeWidget* list) {
    item->setText(1, QFileInfo(path).fileName());
    item->setToolTip(1, QDir::toNativeSeparators(path));
    item->setForeground(1, list->palette().brush(QPalette::PlaceholderText));
}

/// A note of a window: an icon, and a sentence beside it
QWidget* newNote(QWidget* parent, QLabel** icon, QLabel** text) {
    auto* note = new QWidget(parent);
    auto* layout = new QHBoxLayout(note);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    *icon = new QLabel(note);
    (*icon)->setAlignment(Qt::AlignTop);
    layout->addWidget(*icon);
    *text = new QLabel(note);
    (*text)->setTextFormat(Qt::PlainText);
    (*text)->setWordWrap(true);
    (*text)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*text, 1);
    return note;
}

void setNoteIcon(QLabel* icon, const QString& iconId) {
    icon->setPixmap(core::ArtProvider::getInstance().getIcon(iconId).pixmap(NOTE_ICON_SIZE,
                                                                            NOTE_ICON_SIZE));
}

/// Make @p button the Copy button of a window: it puts @p text() on the clipboard and says so
/// for a while
void setUpCopyButton(QPushButton* button, const std::function<QString()>& text) {
    const QString name = QCoreApplication::translate("DeleteDialogs", "&Copy");
    const QString copied = QCoreApplication::translate("DeleteDialogs", "Copied");
    button->setToolTip(
        QCoreApplication::translate("DeleteDialogs", "Copy what the window says to the clipboard"));
    // As wide as its longest text, so "Copied" does not move the other buttons
    button->setText(copied);
    const int copiedWidth = button->sizeHint().width();
    button->setText(name);
    button->setMinimumWidth(std::max(copiedWidth, button->sizeHint().width()));
    QObject::connect(button, &QPushButton::clicked, button, [button, text, name, copied]() {
        QApplication::clipboard()->setText(text());
        button->setText(copied);
        QTimer::singleShot(COPIED_NOTICE_MS, button, [button, name]() { button->setText(name); });
    });
}

/// Make Cancel the button that Enter presses, with the focus: deleting needs the click of
/// Delete or its mnemonic
void makeCancelDefault(QPushButton* accept, QPushButton* cancel) {
    accept->setDefault(false);
    cancel->setDefault(true);
    cancel->setFocus();
}

/// The rows of @p list as text, one a line, indented by their depth: "Title – file"
QString listText(const QTreeWidget* list) {
    QStringList lines;
    const std::function<void(const QTreeWidgetItem*, int)> add = [&](const QTreeWidgetItem* item,
                                                                    int depth) {
        QString line = QString(depth * 2, QLatin1Char(' ')) + item->text(0);
        if (!item->text(1).isEmpty()) {
            line += QStringLiteral(" – ") + item->text(1);
        }
        lines << line;
        for (int i = 0; i < item->childCount(); ++i) {
            add(item->child(i), depth + 1);
        }
    };
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        add(list->topLevelItem(i), 0);
    }
    return lines.join(QLatin1Char('\n'));
}

/// @p element and the elements inside it, at any depth, in reading order
void appendAll(const core::ProjectElement& element, QList<const core::ProjectElement*>& all) {
    all.append(&element);
    for (const core::ProjectElement& inner : element.elements) {
        appendAll(inner, all);
    }
}

/// Whether @p a and @p b are elements of the same kind
bool sameKind(const core::BookTypeRegistry& registry, const core::ProjectElement& a,
              const core::ProjectElement& b) {
    const core::KindRef kindA = core::BookProject::kindOf(registry, a);
    const core::KindRef kindB = core::BookProject::kindOf(registry, b);
    if (kindA || kindB) {
        return kindA && kindB && kindA.kind == kindB.kind;
    }
    return a.kind.kindId == b.kind.kindId;
}

/// The words of the kind of @p elements when they all have the same one, else the word
/// "element"; none without elements
core::KindWords wordsOfAll(const core::BookTypeRegistry& registry,
                           const QList<const core::ProjectElement*>& elements) {
    if (elements.isEmpty()) {
        return {};
    }
    const core::ProjectElement& first = *elements.first();
    const bool same = std::all_of(elements.cbegin(), elements.cend(),
                                  [&registry, &first](const core::ProjectElement* element) {
                                      return sameKind(registry, first, *element);
                                  });
    return same ? wordsOf(registry, first) : elementWords();
}

/// The file of @p element among the files of @p content; empty when it has none there
QString fileOf(const DeleteFromBookDialog::Content& content,
               const core::ProjectElement& element) {
    if (element.file.isEmpty()) {
        return QString();
    }
    const QString path = QDir::cleanPath(QDir(content.projectFolder).filePath(element.file));
    return content.files.contains(path) ? path : QString();
}

} // anonymous namespace

core::KindWords trashWords() {
    core::KindWords words;
    //: The system's Trash in sentences: "The file goes to the {bin}". A translation can write
    //: the name in its sentences itself, in the form they need (in Polish: do Kosza)
    words.forms.insert(QStringLiteral("singular"),
                       RECYCLE_BIN ? QCoreApplication::translate("DeleteDialogs", "Recycle Bin")
                                   : QCoreApplication::translate("DeleteDialogs", "Trash"));
    return words;
}

// =============================================================================
// DeleteFromBookDialog
// =============================================================================

DeleteFromBookDialog::DeleteFromBookDialog(const Content& content,
                                           const core::BookTypeRegistry& registry,
                                           QWidget* parent)
    : KalahariDialog(parent)
    , m_content(content)
    , m_registry(&registry)
{
    auto& artProvider = core::ArtProvider::getInstance();
    const core::ProjectElement& element = m_content.element;

    // The elements inside it, and the elements whose files can go, named by their kind
    QList<const core::ProjectElement*> all;
    appendAll(element, all);
    const QList<const core::ProjectElement*> inside = all.mid(1);
    QList<const core::ProjectElement*> owners;
    std::copy_if(all.cbegin(), all.cend(), std::back_inserter(owners),
                 [this](const core::ProjectElement* each) {
                     return !fileOf(m_content, *each).isEmpty();
                 });
    m_childWords = wordsOfAll(registry, inside);
    m_ownerWords = wordsOfAll(registry, owners);

    // What goes from the book with it
    const auto count = static_cast<int>(inside.size());
    if (count == 1) {
        //: %1: the title of the element inside. In Polish: Razem z
        //: {kind:m=nim|f=nią|n=nim|p=nimi} z książki {child:s=zniknie|p=znikną} {child} „%1”.
        m_description = named(tr("Together with {kind:s=it|p=them}, the {child} \"%1\" will be "
                                 "deleted from the book."))
                            .arg(inside.first()->title);
    } else if (count > 1) {
        //: In Polish, with the form of {child} for the number: Razem z
        //: {kind:m=nim|f=nią|n=nim|p=nimi} z książki znikną %n {child:plural} (2-4), zniknie
        //: %n {child:genitivePlural} (5 and more)
        m_description = named(tr("Together with {kind:s=it|p=them}, %n {child:plural} will be "
                                 "deleted from the book.",
                                 nullptr, count));
    } else if (core::BookProject::formOf(registry, element) == core::ElementForm::Group) {
        //: In Polish: {Kind} {kind:s=jest|p=są} pust{kind:m=y|f=a|n=e|p=e}.
        m_description = named(tr("The {kind} {kind:s=is|p=are} empty."));
    }
    //: %1: the title of the element. In Polish: Usunąć {kind:accusative} „%1” z książki?
    m_title = named(tr("Delete the {kind} \"%1\" from the book?")).arg(element.title);
    setHeading(m_title, m_description);
    setWindowTitle(tr("Delete from Book"));
    setHeadingIcon(QStringLiteral("help.help"));

    // The list of what goes: the element and the elements inside it, with their files
    auto* list = newList(this, tr("Elements to delete"));
    m_list = list;
    int rows = 0;
    const std::function<void(QTreeWidgetItem*, const core::ProjectElement&)> addRow =
        [&](QTreeWidgetItem* parentItem, const core::ProjectElement& shown) {
            auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(list);
            item->setText(0, shown.title);
            item->setToolTip(0, shown.title);
            item->setIcon(0, artProvider.getIcon(NavigatorPanel::iconIdOf(registry, shown),
                                                 core::IconContext::TreeView));
            if (const QString path = fileOf(m_content, shown); !path.isEmpty()) {
                showFile(item, path, list);
            }
            item->setFlags(Qt::ItemIsEnabled);
            ++rows;
            for (const core::ProjectElement& inner : shown.elements) {
                addRow(item, inner);
            }
        };
    addRow(nullptr, element);
    list->expandAll();
    list->setRows(rows);
    contentLayout()->addWidget(list, 1);

    // The files: they stay unless the writer deletes them too
    const auto files = static_cast<int>(m_content.files.size());
    m_deleteFilesBox = new QCheckBox(this);
    if (files == 1) {
        m_deleteFilesBox->setText(
            m_content.trash
                //: In Polish: Usuń &też plik {owner:genitive} – trafi do Kosza
                ? named(tr("&Also delete the file of the {owner} – it goes to the {bin}"))
                //: A drive without a Trash. In Polish: Usuń &też plik {owner:genitive} –
                //: zniknie na zawsze
                : named(tr("&Also delete the file of the {owner} – it will be gone for good")));
    } else {
        m_deleteFilesBox->setText(
            m_content.trash
                //: %1: the number of files. In Polish: Usuń &też pliki {owner:genitivePlural}
                //: (%1) – trafią do Kosza
                ? named(tr("&Also delete the files of the {owner:plural} (%1) – they go to the "
                           "{bin}"))
                      .arg(files)
                //: In Polish: Usuń &też pliki {owner:genitivePlural} (%1) – znikną na zawsze
                : named(tr("&Also delete the files of the {owner:plural} (%1) – they will be "
                           "gone for good"))
                      .arg(files));
    }
    m_deleteFilesBox->setVisible(files > 0);
    contentLayout()->addWidget(m_deleteFilesBox);
    connect(m_deleteFilesBox, &QCheckBox::toggled, this, &DeleteFromBookDialog::updateFilesNote);

    // What the window says about the files: two pages, for the files that stay and for the
    // files that go, in a place as high as the higher one, so that checking the box moves
    // nothing in the window
    m_filesPages = new QStackedWidget(this);
    m_filesPages->setVisible(files > 0);
    contentLayout()->addWidget(m_filesPages);
    const auto newPage = [this](QVBoxLayout** layout) {
        auto* page = new QWidget(m_filesPages);
        *layout = new QVBoxLayout(page);
        (*layout)->setContentsMargins(0, 0, 0, 0);
        (*layout)->setSpacing(contentLayout()->spacing());
        m_filesPages->addWidget(page);
        return page;
    };

    // The files stay, in the folder that the system's file manager can show
    QVBoxLayout* stayLayout = nullptr;
    QWidget* stayPage = newPage(&stayLayout);
    QLabel* stayIcon = nullptr;
    stayLayout->addWidget(newNote(stayPage, &stayIcon, &m_stayText));
    setNoteIcon(stayIcon, QStringLiteral("help.about"));
    m_stayText->setText(filesText(true));
    auto* folderRow = new QWidget(stayPage);
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(8);
    auto* folderLabel = new QLabel(tr("Folder:"), folderRow);
    folderLayout->addWidget(folderLabel);
    m_folderField = new QLineEdit(folderRow);
    m_folderField->setReadOnly(true);
    folderLabel->setBuddy(m_folderField);
    folderLayout->addWidget(m_folderField, 1);
    m_showInFolderButton = new QPushButton(tr("Show in &Folder"), folderRow);
    m_showInFolderButton->setToolTip(tr("Open the folder in the file manager of the system"));
    m_showInFolderButton->setAutoDefault(false);
    folderLayout->addWidget(m_showInFolderButton);
    stayLayout->addWidget(folderRow);
    stayLayout->addStretch(1);
    if (files > 0) {
        // The end of a long path shows the folder of the book
        const QString folder = QDir::toNativeSeparators(
            QFileInfo(m_content.files.first()).absolutePath());
        m_folderField->setText(folder);
        m_folderField->setToolTip(folder);
        m_folderField->setCursorPosition(static_cast<int>(folder.size()));
    }
    connect(m_showInFolderButton, &QPushButton::clicked, this, [this]() {
        if (!showInFolder(m_content.files.value(0))) {
            MessageDialog::warning(this, tr("Show in Folder"),
                                   tr("The file manager of the system could not show the "
                                      "folder:\n%1")
                                       .arg(m_folderField->text()));
        }
    });

    // The files go
    QVBoxLayout* goLayout = nullptr;
    QWidget* goPage = newPage(&goLayout);
    QLabel* goIcon = nullptr;
    goLayout->addWidget(newNote(goPage, &goIcon, &m_goText));
    setNoteIcon(goIcon, QStringLiteral("common.warning"));
    m_goText->setText(filesText(false));
    goLayout->addStretch(1);

    setAcceptText(tr("&Delete"));
    m_copyButton = addSideButton(QString());
    setUpCopyButton(m_copyButton, [this]() { return clipboardText(); });
    makeCancelDefault(acceptButton(), cancelButton());
    updateFilesNote();
}

bool DeleteFromBookDialog::deleteFiles() const
{
    return !m_content.files.isEmpty() && m_deleteFilesBox->isChecked();
}

QString DeleteFromBookDialog::clipboardText() const
{
    QStringList parts{windowTitle(), m_title, m_description, listText(m_list)};
    if (!m_content.files.isEmpty()) {
        parts << filesNote()->text();
        if (!deleteFiles()) {
            parts << tr("Folder:") + QLatin1Char(' ') + m_folderField->text();
        }
    }
    parts.removeAll(QString());
    return parts.join(QStringLiteral("\n\n"));
}

void DeleteFromBookDialog::updateFilesNote()
{
    const bool deleting = deleteFiles();
    setHeadingIcon(deleting ? QStringLiteral("common.warning") : QStringLiteral("help.help"));
    m_filesPages->setCurrentIndex(deleting ? 1 : 0);
}

QString DeleteFromBookDialog::filesText(bool stay) const
{
    const bool one = m_content.files.size() == 1;
    QString text;
    if (!stay && m_content.trash) {
        text = one
                   //: In Polish: Plik {owner:genitive} trafi do systemowego Kosza. Przywrócisz go
                   //: z Kosza, ale już nie z programu.
                   ? tr("The file of the {owner} goes to the system's {bin}. You can restore it "
                        "from the {bin}, but not from the program.")
                   //: In Polish: Pliki {owner:genitivePlural} trafią do systemowego Kosza.
                   //: Przywrócisz je z Kosza, ale już nie z programu.
                   : tr("The files of the {owner:plural} go to the system's {bin}. You can "
                        "restore them from the {bin}, but not from the program.");
    } else if (!stay) {
        text = one
                   //: In Polish: Plik {owner:genitive} zniknie na zawsze: z tego folderu nie da
                   //: się przenieść pliku do Kosza.
                   ? tr("The file of the {owner} will be gone for good: files in this folder "
                        "cannot go to the {bin}.")
                   //: In Polish: Pliki {owner:genitivePlural} znikną na zawsze: z tego folderu
                   //: nie da się przenieść plików do Kosza.
                   : tr("The files of the {owner:plural} will be gone for good: files in this "
                        "folder cannot go to the {bin}.");
    } else if (m_content.unassigned) {
        text = one
                   //: In Polish: Plik zostanie w folderze książki. Znajdziesz go w Nawigatorze,
                   //: w węźle „Pliki bez przydziału”, skąd możesz go dodać z powrotem do
                   //: książki albo usunąć.
                   ? tr("The file stays in the book's folder. You will find it in the Navigator, "
                        "under \"Unassigned Files\", where you can add it back to the book or "
                        "delete it.")
                   //: In Polish: Pliki zostaną w folderze książki. Znajdziesz je w Nawigatorze,
                   //: w węźle „Pliki bez przydziału”, skąd możesz je dodać z powrotem do
                   //: książki albo usunąć.
                   : tr("The files stay in the book's folder. You will find them in the "
                        "Navigator, under \"Unassigned Files\", where you can add them back to "
                        "the book or delete them.");
    } else {
        // Files outside the folders of the book and the Workshop, which the Navigator does not
        // list
        text = one ? tr("The file stays in its folder.") : tr("The files stay in their folders.");
    }
    return named(text);
}

QString DeleteFromBookDialog::named(const QString& text) const
{
    return core::fillWords(text, {{QStringLiteral("kind"), wordsOf(*m_registry, m_content.element)},
                                  {QStringLiteral("child"), m_childWords},
                                  {QStringLiteral("owner"), m_ownerWords},
                                  {QStringLiteral("bin"), trashWords()}});
}

// =============================================================================
// DeleteFilesDialog
// =============================================================================

DeleteFilesDialog::DeleteFilesDialog(const QList<core::UnassignedFile>& files,
                                     const QString& projectFolder, bool trash, QWidget* parent)
    : KalahariDialog(parent)
{
    auto& artProvider = core::ArtProvider::getInstance();
    const bool one = files.size() == 1;
    const QDir project(projectFolder);

    //: %1: the name of the file. In Polish: Usunąć plik „%1”?
    m_title = one ? tr("Delete the file \"%1\"?").arg(QFileInfo(files.first().file).fileName())
                  //: %1: the number of files. In Polish: Usunąć wszystkie pliki bez
                  //: przydziału (%1)?
                  : tr("Delete all unassigned files (%1)?").arg(files.size());
    setHeading(m_title);
    setWindowTitle(one ? tr("Delete File") : tr("Delete Files"));
    setHeadingIcon(QStringLiteral("common.warning"));

    // The files: the title of the text in each, and its name
    auto* list = newList(this, tr("Files to delete"));
    m_list = list;
    for (const core::UnassignedFile& file : files) {
        auto* item = new QTreeWidgetItem(list);
        const QString name = QFileInfo(file.file).fileName();
        item->setText(0, file.title.isEmpty() ? name : file.title);
        item->setToolTip(0, item->text(0));
        item->setIcon(0, artProvider.getIcon(QStringLiteral("template.chapter"),
                                             core::IconContext::TreeView));
        if (!file.title.isEmpty()) {
            showFile(item, QDir::cleanPath(project.filePath(file.file)), list);
        } else {
            item->setToolTip(0, QDir::toNativeSeparators(
                                    QDir::cleanPath(project.filePath(file.file))));
        }
        item->setFlags(Qt::ItemIsEnabled);
    }
    list->setRows(static_cast<int>(files.size()));
    contentLayout()->addWidget(list, 1);

    // Where they go
    QLabel* noteIcon = nullptr;
    contentLayout()->addWidget(newNote(this, &noteIcon, &m_noteText));
    setNoteIcon(noteIcon, QStringLiteral("common.warning"));
    QString text;
    if (trash) {
        text = one
                   //: In Polish: Plik trafi do systemowego Kosza. Przywrócisz go z Kosza, ale
                   //: już nie z programu.
                   ? tr("The file goes to the system's {bin}. You can restore it from the {bin}, "
                        "but not from the program.")
                   //: In Polish: Pliki trafią do systemowego Kosza. Przywrócisz je z Kosza, ale
                   //: już nie z programu.
                   : tr("The files go to the system's {bin}. You can restore them from the "
                        "{bin}, but not from the program.");
    } else {
        text = one
                   //: In Polish: Plik zniknie na zawsze: z tego folderu nie da się przenieść
                   //: pliku do Kosza.
                   ? tr("The file will be gone for good: files in this folder cannot go to the "
                        "{bin}.")
                   //: In Polish: Pliki znikną na zawsze: z tego folderu nie da się przenieść
                   //: plików do Kosza.
                   : tr("The files will be gone for good: files in this folder cannot go to the "
                        "{bin}.");
    }
    m_noteText->setText(core::fillWords(text, {{QStringLiteral("bin"), trashWords()}}));

    setAcceptText(tr("&Delete"));
    m_copyButton = addSideButton(QString());
    setUpCopyButton(m_copyButton, [this]() { return clipboardText(); });
    makeCancelDefault(acceptButton(), cancelButton());
}

QString DeleteFilesDialog::clipboardText() const
{
    QStringList parts{windowTitle(), m_title, listText(m_list), m_noteText->text()};
    parts.removeAll(QString());
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
