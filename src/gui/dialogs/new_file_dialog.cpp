/// @file new_file_dialog.cpp
/// @brief New File: a new element of the open book, of the kind, with the title and at the
/// place the writer chooses, or an empty text file, outside the book or with it; and Add
/// Element, the same window for the place of the Navigator or of the editor it was opened on

#include "kalahari/gui/dialogs/new_file_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int MINIMUM_WIDTH = 700;         ///< The description and three tiles in a row
constexpr QSize DEFAULT_SIZE{880, 740};    ///< The tiles of the main part and the place, under
                                           ///< the heading
constexpr int DESCRIPTION_WIDTH = 250;     ///< Of the description at the left, as in New Book
constexpr int DESCRIPTION_ICON_SIZE = 64;  ///< Of the icon above the description
constexpr int NAME_POINT_SIZE = 12;        ///< Of the name of the kind above its description
constexpr int KIND_ROLE = Qt::UserRole;          ///< Index of the kind of a tile in Book::kinds
constexpr int HEADING_ROLE = Qt::UserRole + 1;   ///< The item names a part of the book
constexpr int EMPTY_FILE_ROLE = Qt::UserRole + 2;  ///< The tile of an empty text file
constexpr qreal TILE_WIDTH_EMS = 7.5;      ///< Room for a name of two short words in a line
constexpr int TILE_TEXT_LINES = 2;         ///< Lines of a name the tiles have room for at least
constexpr int MAXIMUM_TEXT_LINES = 3;      ///< Lines of the longest name the tiles make room for
constexpr int TILE_PADDING = 8;            ///< Around the icon and the name of a tile
constexpr int TILE_SPACING = 6;            ///< Between the tiles and their rows
constexpr int HEADING_TOP = 6;             ///< Above the name of a part of the book
constexpr int HEADING_BOTTOM = 3;          ///< Between the name of a part and its line
constexpr int NOTICE_ICON_SIZE = 16;       ///< Of the icon of the note about a new group

/// The parts of the book, in reading order
constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                     core::BookPlace::Back};

/// The icon of an empty text file, as the Navigator shows a file of the book outside its text
const QString EMPTY_FILE_ICON = QStringLiteral("common.file");

/// The window that chooses the file of a new material in the tests; empty: the system's one
std::function<QString(const QString&)>& fileChooser()
{
    static std::function<QString(const QString&)> chooser;
    return chooser;
}

/// The kinds of the groups of the book (a part, a cycle), for the words that say where the
/// elements of a kind can be: "in parts"
QList<core::KindRef> groupKindsOf(const core::BookProject& project,
                                  const core::BookTypeRegistry& registry)
{
    QList<core::KindRef> groups;
    for (const core::BookPlace place : BOOK_PLACES) {
        for (const core::KindRef& kind : project.kindsIn(registry, place)) {
            const auto same = [&kind](const core::KindRef& group) {
                return group.kind == kind.kind;
            };
            if (kind.kind->form == core::ElementForm::Group &&
                std::none_of(groups.cbegin(), groups.cend(), same)) {
                groups.append(kind);
            }
        }
    }
    return groups;
}

/// Whether @p kinds has the kind of @p kind
bool hasKind(const QList<core::KindRef>& kinds, const core::KindRef& kind)
{
    return std::any_of(kinds.cbegin(), kinds.cend(),
                       [&kind](const core::KindRef& other) { return other.kind == kind.kind; });
}

/// Element @p id of @p elements, at any depth
const core::ProjectElement* findIn(const QList<core::ProjectElement>& elements, const QString& id)
{
    for (const core::ProjectElement& element : elements) {
        if (element.id == id) {
            return &element;
        }
        if (const core::ProjectElement* inside = findIn(element.elements, id)) {
            return inside;
        }
    }
    return nullptr;
}

/// @p elements without the elements @p ids, at any depth
QList<core::ProjectElement> without(QList<core::ProjectElement> elements, const QStringList& ids)
{
    elements.removeIf(
        [&ids](const core::ProjectElement& element) { return ids.contains(element.id); });
    for (core::ProjectElement& element : elements) {
        element.elements = without(element.elements, ids);
    }
    return elements;
}

QStringList titlesOf(const QList<core::ProjectElement>& elements)
{
    QStringList titles;
    for (const core::ProjectElement& element : elements) {
        titles << element.title;
    }
    return titles;
}

QStringList idsOf(const QList<core::ProjectElement>& elements)
{
    QStringList ids;
    for (const core::ProjectElement& element : elements) {
        ids << element.id;
    }
    return ids;
}

/// Index in @p kinds of the first kind of which an element can be made, preferring the kind of
/// @p kind and then the main part of the book; -1: none
int indexOfKind(const QList<NewFileKind>& kinds, const core::KindRef& kind)
{
    int found = -1;
    for (qsizetype index = 0; index < kinds.size(); ++index) {
        const NewFileKind& entry = kinds.at(index);
        if (!entry.unavailable.isEmpty() || !kind || entry.kind.kind != kind.kind) {
            continue;
        }
        if (found < 0 || entry.section == core::BookPlace::Main) {
            found = static_cast<int>(index);
        }
        if (entry.section == core::BookPlace::Main) {
            break;
        }
    }
    return found;
}

/// The tiles of the kinds, with the names of the parts of the book between them: each name has
/// a row of its own, and only the tiles that are not greyed can be chosen
class KindList : public QListWidget {
public:
    explicit KindList(QWidget* parent)
        : QListWidget(parent)
    {
        setViewMode(QListView::IconMode);
        setMovement(QListView::Static);
        setResizeMode(QListView::Adjust);
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setWordWrap(true);  // Translated names may need two lines
        setSpacing(TILE_SPACING);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setSelectionRectVisible(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    /// The width of the name of a part of the book: the whole row of the tiles
    int headingWidth() const
    {
        // The rows of an icon view leave room for the vertical scroll bar, also when it is
        // hidden
        int width = maximumViewportSize().width();
        if (style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr,
                                 verticalScrollBar()) == 0) {
            width -= style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, verticalScrollBar());
        }
        return std::max(1, width - 2 * spacing() - 2 * frameWidth());
    }

    /// After the tiles are laid out (when the list is shown, and at a new width), the list
    /// starts at the top; when the chosen tile is not in view there, it starts at the name of
    /// the part of the book of that tile, so that no name is cut in half above the tiles.
    ///
    /// Qt lays the list out also while it draws it, and a scroll then breaks the drawing on
    /// macOS (Qt built with its checks stops the program): the list scrolls just after.
    void doItemsLayout() override
    {
        QListWidget::doItemsLayout();
        if (!m_startPending) {
            m_startPending = true;
            QTimer::singleShot(0, this, [this]() { showStart(); });
        }
    }

protected:
    /// Home and End go to the first and the last tile that can be chosen; the arrows pass
    /// over the names of the parts and the greyed tiles
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override
    {
        const QModelIndex current = currentIndex();
        if (action == MoveHome || action == MoveEnd) {
            const int count = model()->rowCount(rootIndex());
            for (int i = 0; i < count; ++i) {
                const int row = action == MoveHome ? i : count - 1 - i;
                const QModelIndex index = model()->index(row, 0, rootIndex());
                if (index.flags().testFlag(Qt::ItemIsEnabled)) {
                    return index;
                }
            }
            return current;
        }
        const QModelIndex next = QListWidget::moveCursor(action, modifiers);
        return next.isValid() && next.flags().testFlag(Qt::ItemIsEnabled) ? next : current;
    }

    /// A chosen tile stays chosen: Ctrl and a click do not unselect it
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                         const QEvent* event) const override
    {
        if (!index.isValid() || !index.flags().testFlag(Qt::ItemIsSelectable) ||
            (event && event->type() == QEvent::MouseButtonRelease)) {
            return QItemSelectionModel::NoUpdate;
        }
        return QItemSelectionModel::ClearAndSelect;
    }

private:
    /// The list at its start, as doItemsLayout() says
    void showStart()
    {
        m_startPending = false;
        verticalScrollBar()->setValue(verticalScrollBar()->minimum());
        const QModelIndex current = currentIndex();
        if (!current.isValid() || viewport()->rect().contains(visualRect(current))) {
            return;
        }
        for (int row = current.row(); row >= 0; --row) {
            const QModelIndex index = model()->index(row, 0, rootIndex());
            if (index.data(HEADING_ROLE).toBool()) {
                scrollTo(index, PositionAtTop);
                break;
            }
        }
        scrollTo(current, EnsureVisible);
    }

    bool m_startPending = false;  ///< showStart() is to come
};

/// Draws the names of the parts of the book as the palette of kinds does, in bold with a line
/// under them, and the tiles as an icon view does
class KindDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!index.data(HEADING_ROLE).toBool()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        painter->save();
        const QFont font = headingFont(option.font);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::Active, QPalette::Text));
        const QRect text = option.rect.adjusted(0, HEADING_TOP, 0, -(HEADING_BOTTOM + 1));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(),
                                                        Qt::ElideRight, text.width()));
        painter->setPen(option.palette.color(QPalette::Active, QPalette::Midlight));
        painter->drawLine(option.rect.left(), option.rect.bottom(), option.rect.right(),
                          option.rect.bottom());
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!index.data(HEADING_ROLE).toBool()) {
            return QStyledItemDelegate::sizeHint(option, index);
        }
        const auto* list = dynamic_cast<const KindList*>(option.widget);
        return {list ? list->headingWidth() : option.rect.width(), headingHeight(option.font)};
    }

    /// The font of the names of the parts of the book, for the list's font @p font
    static QFont headingFont(QFont font)
    {
        font.setWeight(QFont::DemiBold);
        return font;
    }

    /// The height of the row of the name of a part of the book, with its line
    static int headingHeight(const QFont& font)
    {
        return HEADING_TOP + QFontMetrics(headingFont(font)).height() + HEADING_BOTTOM + 1;
    }
};

} // anonymous namespace

NewFileKind NewFileDialog::entryOf(const core::KindRef& kind, core::BookPlace section,
                                   const QList<core::KindRef>& offered,
                                   const QList<core::KindRef>& groups)
{
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    const core::BookTypeRegistry& registry = pm.bookTypes();
    const QString language = programLanguage();
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(kind)}};
    const bool workshop = section == core::BookPlace::Workshop;

    NewFileKind entry;
    entry.kind = kind;
    entry.section = section;

    // Where the elements of the kind are added: in the parts of the book and in groups, or in
    // the group of the kind in the Workshop
    if (workshop) {
        //: Where the elements of a kind of the Workshop are: in the group of the kind in the
        //: Navigator; %1: its name, e.g. Work Notes. In Polish: w Warsztacie, w grupie „%1”
        entry.where =
            tr("in the Workshop, in the group \"%1\"").arg(kind.kind->plural.text(language));
    } else {
        QStringList places;
        for (const core::BookPlace place : BOOK_PLACES) {
            if (kind.offeredIn(place)) {
                places << SectionWords::forPart(projectBook, place).inPart;
            }
        }
        for (const core::KindRef& group : groups) {
            if (kind.kind->allowsInside(group.kind->id)) {
                //: Where the elements of a kind can be: in the groups of a kind, e.g. in parts.
                //: In Polish: w {group:locativePlural}
                places << core::fillWords(tr("in {group:plural}"),
                                          {{QStringLiteral("group"), wordsOf(group)}});
            }
        }
        if (places.size() < 2) {
            entry.where = places.value(0);
        } else {
            const QString last = places.takeLast();
            //: The places of a kind in a sentence: %1 is the places but the last one, joined
            //: with commas; %2 is the last one. In Polish: %1 i %2
            entry.where = tr("%1 and %2").arg(places.join(QStringLiteral(", ")), last);
        }
    }

    // How many elements of the kind the book can have
    if (kind.kind->limit <= 0) {
        //: How many elements of a kind the book can have. In Polish: dowolnie wiele
        entry.count = tr("any number");
    } else if (kind.kind->limit == 1) {
        //: How many elements of a kind the book can have, e.g. one title page. In Polish:
        //: {kind:m=jeden|f=jedna|n=jedno|p=jedne} w książce
        entry.count = core::fillWords(tr("one {kind} in the book"), nouns);
    } else {
        //: How many elements of a kind the book can have; %1: the number. In Polish:
        //: najwyżej %1 w książce
        entry.count = tr("at most %1 in the book").arg(kind.kind->limit);
    }

    // The type of the book; in a user project, the type each kind comes from
    entry.type = project && project->type ? typeNameOf(registry, *project)
                                          : kind.package->name.text(language);

    // The program makes the elements of a window kind of the Workshop that it has a window
    // for (a material); in the book none yet (a table of contents)
    const bool hasEditor =
        workshop ? core::ProjectManager::canAdd(kind) : kind.kind->form != core::ElementForm::Window;
    if (!hasEditor) {
        //: The tooltip of a greyed kind that opens in a window of its own, e.g. a table of
        //: contents. In Polish: Program nie ma jeszcze edytora {kind:genitive}.
        entry.unavailable =
            core::fillWords(tr("The program does not have an editor for the {kind} yet."), nouns);
        return entry;
    }
    if (!hasKind(offered, kind)) {
        entry.unavailable =
            kind.kind->limit == 1
                //: The tooltip of a greyed kind of which a book can have one element, e.g. a
                //: title page. In Polish: Książka ma już {kind:accusative}.
                ? core::fillWords(tr("The book already has {kind:indefinite}."), nouns)
                //: The tooltip of a greyed kind; %1: how many elements of the kind a book can
                //: have. In Polish: Książka ma już tyle elementów tego rodzaju, ile może mieć:
                //: %1.
                : tr("The book already has as many elements of this kind as it can have: %1.")
                      .arg(kind.kind->limit);
        return entry;
    }

    entry.title = pm.defaultTitle(kind);
    if (kind.kind->form == core::ElementForm::Group) {
        //: The file of a new group, e.g. a part: it has none. In Polish: brak – {kind} nie ma
        //: własnego tekstu
        entry.file = core::fillWords(tr("none: {kind:indefinite} has no text of its own"), nouns);
    } else if (kind.kind->isMaterial()) {
        //: The file of a new material: a copy of the file the writer chooses; %1: the folder
        //: of the Workshop. In Polish: kopia wybranego pliku w folderze „%1”, z tą samą nazwą
        entry.file = tr("a copy of the chosen file in the folder \"%1\", with the same name")
                         .arg(QString::fromLatin1(core::BookProject::WORKSHOP_FOLDER));
    } else {
        const QFileInfo file(pm.newChapterFile(kind, section));
        //: The file of a new element: %1 is its name, %2 the folder of the book it is in. In
        //: Polish: %1 w folderze „%2”
        entry.file = tr("%1 in the folder \"%2\"").arg(file.fileName(), file.path());
    }

    // The elements of the Workshop are in alphabetical order: no list of places
    if (workshop) {
        const QString group = kind.kind->plural.text(language);
        entry.placeNote =
            kind.kind->isMaterial()
                //: Where a new material goes, in place of the list of places; %1: the folder
                //: of the Workshop, %2: the group of materials in the Navigator. In Polish:
                //: Kopia pliku trafi do folderu „%1” książki; plik na dysku zostaje bez zmian.
                //: {Kind} trafi do grupy „%2” na miejsce wynikające z
                //: {kind:m=jego|f=jej|n=jego|p=ich} tytułu.
                ? core::fillWords(tr("A copy of the file goes to the book's \"%1\" folder; the "
                                     "file on the disk stays as it is. The {kind} goes to the "
                                     "group \"%2\" at the place of its title."),
                                  nouns)
                      .arg(QString::fromLatin1(core::BookProject::WORKSHOP_FOLDER), group)
                //: Where a new element of the Workshop goes, in place of the list of places;
                //: %1: the group of its kind in the Navigator. In Polish: W Warsztacie
                //: elementy są ułożone alfabetycznie, więc {kind} trafi do grupy „%1” na
                //: miejsce wynikające z {kind:m=jego|f=jej|n=jego|p=ich} tytułu.
                : core::fillWords(tr("In the Workshop the elements are in alphabetical order, "
                                     "so the {kind} goes to the group \"%1\" at the place of "
                                     "its title."),
                                  nouns)
                      .arg(group);
    }
    return entry;
}

NewFileDialog::Book NewFileDialog::openBook(const QString& untitled)
{
    Book book;
    book.untitled = untitled;
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    if (!project || !projectBook) {
        return book;
    }
    const core::BookTypeRegistry& registry = pm.bookTypes();
    const QList<core::KindRef> groups = groupKindsOf(*project, registry);
    book.mainKind = pm.mainTextKind();

    for (const core::BookPlace place : BOOK_PLACES) {
        // A book without sections has no rows of its parts, and the groups of its tiles are
        // named after their place in the book
        book.sections.append({place,
                              projectBook->partsLayer ? projectBook->sectionName(place)
                                                      : QString(),
                              SectionWords::forPart(projectBook, place),
                              project->elementsIn(place)});
        book.groupNames.append(projectBook->partsLayer ? projectBook->sectionName(place)
                                                       : SectionWords::groupName(place));

        // The kinds of which the book can have more elements
        const QList<core::KindRef> offered = pm.kindsFor(place);
        for (const core::KindRef& kind : project->kindsIn(registry, place)) {
            // A group is no file: the Navigator adds it
            if (kind.kind->form == core::ElementForm::Group || !kind.offeredIn(place)) {
                continue;
            }
            NewFileKind entry = entryOf(kind, place, offered, groups);
            if (entry.unavailable.isEmpty()) {
                entry.place = pm.newPlaceOf(kind, place);
            }
            book.kinds.append(entry);
        }
    }

    // An empty text file with the book goes to the files of the book outside its text
    if (const QString path = pm.newBookFile(); !path.isEmpty()) {
        const QFileInfo bookFile(path);
        book.bookFile = tr("%1 in the folder \"%2\"").arg(bookFile.fileName(), bookFile.path());
    }

    // The main text kind in the main part; without it, the first kind of which an element can
    // be made
    book.current = indexOfKind(book.kinds, book.mainKind);
    if (book.current < 0) {
        const auto first = std::find_if(
            book.kinds.cbegin(), book.kinds.cend(),
            [](const NewFileKind& entry) { return entry.unavailable.isEmpty(); });
        book.current =
            first != book.kinds.cend() ? static_cast<int>(first - book.kinds.cbegin()) : -1;
    }
    return book;
}

NewFileDialog::Book NewFileDialog::forAdding(const AddTarget& target)
{
    Book book;
    book.adding = true;
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    if (!project || !projectBook) {
        return book;
    }
    const core::BookTypeRegistry& registry = pm.bookTypes();
    const QList<core::KindRef> groups = groupKindsOf(*project, registry);
    book.mainKind = pm.mainTextKind();

    // The kinds chosen at the start, the first that can be made: the one of the target, then
    // the kind of the element the window was opened on, then the main text kind of the book
    QList<core::KindRef> wanted{target.kind};

    // The Workshop: its kinds only, or those of a group or an element of it; its elements are
    // in alphabetical order, so no list of places
    if (target.scope == AddTarget::Scope::Workshop) {
        book.sections.append({core::BookPlace::Workshop, QString(), SectionWords{},
                              project->elementsIn(core::BookPlace::Workshop)});
        book.groupNames.append(NavigatorPanel::workshopName(*project));
        const QList<core::KindRef> offered = pm.kindsFor(core::BookPlace::Workshop);
        for (const core::KindRef& kind : project->kindsIn(registry, core::BookPlace::Workshop)) {
            if (!target.kinds.isEmpty() && !hasKind(target.kinds, kind)) {
                continue;
            }
            NewFileKind entry = entryOf(kind, core::BookPlace::Workshop, offered, groups);
            if (entry.unavailable.isEmpty()) {
                entry.place = pm.newPlaceOf(kind, core::BookPlace::Workshop);
            }
            book.kinds.append(entry);
        }
    } else {
        // The parts of the book the window offers the kinds of, and the group whose kinds it
        // offers inside it; an element: the kinds of its list, right after it
        QList<core::BookPlace> places(BOOK_PLACES.cbegin(), BOOK_PLACES.cend());
        QString groupId;
        QString afterId;
        if (target.scope == AddTarget::Scope::Section) {
            places = {target.place};
        } else if (target.scope == AddTarget::Scope::Group ||
                   target.scope == AddTarget::Scope::Element) {
            const std::optional<core::ElementPlace> at = project->placeOf(target.id);
            if (!at) {
                return book;
            }
            places = {at->place};
            if (target.scope == AddTarget::Scope::Group) {
                groupId = target.id;
            } else {
                groupId = at->groupId;
                afterId = target.id;
                if (const core::ProjectElement* element = pm.findElement(target.id)) {
                    wanted.append(pm.kindOf(*element));
                }
            }
        }
        const core::ProjectElement* group = groupId.isEmpty() ? nullptr : pm.findElement(groupId);
        if (!groupId.isEmpty() && !group) {
            return book;
        }

        for (const core::BookPlace place : BOOK_PLACES) {
            book.sections.append({place,
                                  projectBook->partsLayer ? projectBook->sectionName(place)
                                                          : QString(),
                                  SectionWords::forPart(projectBook, place),
                                  project->elementsIn(place)});
            // The tiles of a group are named after it: "In the part "Part I""
            book.groupNames.append(
                group ? core::fillWords(
                            //: The name of the tiles of the kinds of a group, e.g. a part, in
                            //: the Add Element window; %1: its title. In Polish:
                            //: W {group:locative} „%1”
                            tr("In the {group} \"%1\""),
                            {{QStringLiteral("group"), wordsOf(registry, *group)}})
                            .arg(group->title)
                : projectBook->partsLayer ? projectBook->sectionName(place)
                                          : SectionWords::groupName(place));
            if (!places.contains(place)) {
                continue;
            }

            const QList<core::KindRef> offered = pm.kindsFor(place, groupId);
            const QList<core::KindRef> kinds =
                group ? project->kindsInside(registry, group->kind.kindId)
                      : project->kindsIn(registry, place);
            for (const core::KindRef& kind : kinds) {
                if (!group && !kind.offeredIn(place)) {
                    continue;
                }
                NewFileKind entry = entryOf(kind, place, offered, groups);
                if (entry.unavailable.isEmpty()) {
                    const std::optional<core::ElementPlace> after =
                        afterId.isEmpty() ? std::nullopt : pm.newPlaceAfter(afterId, kind);
                    entry.place = after ? *after : pm.newPlaceOf(kind, place, groupId);
                }
                book.kinds.append(entry);
            }
        }
    }

    wanted.append(book.mainKind);
    for (const core::KindRef& kind : std::as_const(wanted)) {
        book.current = indexOfKind(book.kinds, kind);
        if (book.current >= 0) {
            return book;
        }
    }
    const auto first =
        std::find_if(book.kinds.cbegin(), book.kinds.cend(),
                     [](const NewFileKind& entry) { return entry.unavailable.isEmpty(); });
    book.current = first != book.kinds.cend() ? static_cast<int>(first - book.kinds.cbegin()) : -1;
    return book;
}

void NewFileDialog::setFileChooserForTests(std::function<QString(const QString& folder)> chooser)
{
    fileChooser() = std::move(chooser);
}

NewFileDialog::NewFileDialog(const Book& book, const core::BookTypeRegistry& registry,
                             QWidget* parent)
    : KalahariDialog(parent)
    , m_book(book)
    , m_registry(&registry)
{
    if (m_book.adding) {
        // The heading says what the writer chooses for the chosen tile (updateHeading())
        setHeadingIcon(QStringLiteral("book.addElement"));
    } else {
        //: The window of a new element of the open book. In Polish: Nowy plik
        setHeading(tr("New File"),
                   //: The heading of the New File window. In Polish: Wybierz rodzaj pliku, jego
                   //: tytuł i miejsce, w którym ma się znaleźć.
                   tr("Choose the kind of the file, its title and where it goes."));
        setHeadingIcon(QStringLiteral("file.new"));
    }
    // A low heading leaves the room to the tiles
    setCompactHeading(true);

    // As wide as the description and three tiles in a row; the height of the content can be
    // lower: on a small screen (1366x768 at 150%) the content scrolls above the buttons
    setMinimumWidth(MINIMUM_WIDTH);

    // The description of the chosen kind at the left, along the whole height, and at the
    // right the choices in their order: the kind, the title, the place. So the title is close
    // under the tiles and stays in view also when the content scrolls on a small screen.
    auto* content = new QWidget(this);
    auto* columns = new QHBoxLayout(content);
    columns->setSpacing(11);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->addWidget(createDescriptionPanel(content));
    auto* choices = new QVBoxLayout();
    choices->setSpacing(11);
    // The tiles and the place share the room the window has above their minimum, 3 to 2; the
    // file of a new material is chosen before its title, which follows it
    choices->addWidget(createKindsGroup(content), 3);
    choices->addWidget(createMaterialRow(content));
    choices->addLayout(createTitleRow(content));
    choices->addWidget(createPlaceGroup(content), 2);
    columns->addLayout(choices, 1);
    contentLayout()->addWidget(content, 1);

    setAcceptText(m_book.adding
                      //: The button that adds the new element. In Polish: Dodaj
                      ? tr("Add")
                      //: The button that makes the new element. In Polish: Utwórz
                      : tr("Create"));

    fillKinds();
    connect(m_kindList, &QListWidget::currentItemChanged, this, &NewFileDialog::onKindChanged);
    connect(m_kindList, &QListWidget::itemDoubleClicked, this, [this]() {
        if (acceptButton()->isEnabled()) {
            accept();
        }
    });
    connect(m_titleEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        // An emptied title follows the kind again
        m_titleChanged = !text.trimmed().isEmpty();
    });
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() {
        m_picker->setTitle(title());
        updateCreateButton();
    });
    connect(m_picker, &ElementPlacePicker::placeChanged, this,
            &NewFileDialog::updateCreateButton);
    // An empty text file goes outside the book or with it
    connect(m_insideButton, &QRadioButton::toggled, this, [this]() {
        if (emptyFileChosen()) {
            showEmptyFile();
        }
    });
    // A new group takes inside it the elements that close the main section, or leaves them
    connect(m_takeBox, &QCheckBox::toggled, this, [this]() {
        if (const NewFileKind* entry = chosen();
            entry && entry->kind.kind->form == core::ElementForm::Group) {
            showGroup(*entry);
        }
    });
    connect(m_chooseButton, &QPushButton::clicked, this, &NewFileDialog::chooseMaterialFile);
    m_titleEdit->installEventFilter(this);
    m_materialLabel->installEventFilter(this);

    // New icon colors or sizes (a change of theme or icon settings) redraw the icons
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() {
                sizeTiles();
                updateKindIcon();
            });

    // The kind chosen at the start
    QListWidgetItem* start = nullptr;
    for (int row = 0; row < m_kindList->count() && !start; ++row) {
        QListWidgetItem* item = m_kindList->item(row);
        if (item->data(KIND_ROLE).isValid() && item->data(KIND_ROLE).toInt() == m_book.current) {
            start = item;
        }
    }
    if (start) {
        m_kindList->setCurrentItem(start);
    } else {
        onKindChanged();
    }

    // The size of a big screen, higher by the heading, or less on a smaller one
    utils::resizeWithinScreen(this, withHeading(DEFAULT_SIZE));

    // Typing replaces the title
    m_titleEdit->selectAll();
    m_titleEdit->setFocus();
}

core::KindRef NewFileDialog::kind() const
{
    const NewFileKind* entry = chosen();
    return entry ? entry->kind : core::KindRef{};
}

QString NewFileDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

core::ElementPlace NewFileDialog::place() const
{
    // An element of the Workshop goes to the place of its title
    const NewFileKind* entry = chosen();
    return entry && !entry->placeNote.isEmpty() ? entry->place : m_picker->place();
}

QString NewFileDialog::status() const
{
    return hasStatus(chosen()) ? m_statusBox->currentData().toString() : QString();
}

QStringList NewFileDialog::takeInside() const
{
    const NewFileKind* entry = chosen();
    const bool group = entry && entry->kind.kind->form == core::ElementForm::Group;
    return group && m_canTake && m_takeBox->isChecked() ? idsOf(m_closing) : QStringList();
}

QString NewFileDialog::materialFile() const
{
    return isMaterial(chosen()) ? m_materialFile : QString();
}

void NewFileDialog::changeEvent(QEvent* event)
{
    KalahariDialog::changeEvent(event);
    if (event->type() == QEvent::FontChange && m_kindList) {
        sizeTiles();
    }
}

void NewFileDialog::showEvent(QShowEvent* event)
{
    KalahariDialog::showEvent(event);
    // A material chosen at the start asks for its file once the window is in view
    if (!event->spontaneous() && isMaterial(chosen()) && m_materialFile.isEmpty()) {
        QTimer::singleShot(0, this, &NewFileDialog::chooseMaterialFile);
    }
}

bool NewFileDialog::eventFilter(QObject* watched, QEvent* event)
{
    // The path of the file of a material, shortened in its middle to the width of its label
    if (watched == m_materialLabel && event->type() == QEvent::Resize) {
        showMaterialFile();
    }

    // A click in the proposed title selects it, as the window selects it at the start, so
    // that typing replaces it; in the writer's own title the click puts the cursor. The
    // selection comes after the click, which puts the cursor first.
    if (watched == m_titleEdit && event->type() == QEvent::FocusIn && !m_titleChanged &&
        static_cast<QFocusEvent*>(event)->reason() == Qt::MouseFocusReason) {
        QTimer::singleShot(0, m_titleEdit, &QLineEdit::selectAll);
    }
    return KalahariDialog::eventFilter(watched, event);
}

QWidget* NewFileDialog::createDescriptionPanel(QWidget* parent)
{
    auto* panel = new QWidget(parent);
    panel->setFixedWidth(DESCRIPTION_WIDTH);
    auto* layout = new QVBoxLayout(panel);
    layout->setSpacing(11);
    layout->setContentsMargins(11, 11, 11, 11);

    m_iconLabel = new QLabel(panel);
    m_iconLabel->setFixedSize(DESCRIPTION_ICON_SIZE, DESCRIPTION_ICON_SIZE);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);

    m_nameLabel = new QLabel(panel);
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(NAME_POINT_SIZE);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    layout->addWidget(m_nameLabel);

    auto* separator = new QFrame(panel);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // The words in grey, what they say about the kind beside them
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);
    int rows = 0;
    QLabel* caption = nullptr;
    const auto addRow = [panel, grid, &rows, &caption](const QString& words) {
        const int row = rows++;
        caption = new QLabel(words, panel);
        caption->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(caption, row, 0, Qt::AlignTop);
        auto* value = new QLabel(panel);
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(value, row, 1, Qt::AlignTop);
        return value;
    };
    //: Where the elements of the chosen kind can be. In Polish: Gdzie:
    m_whereLabel = addRow(tr("Where:"));
    //: How many elements of the chosen kind the book can have. In Polish: Ile:
    m_countLabel = addRow(tr("How many:"));
    //: The file a new element of the chosen kind gets. In Polish: Plik:
    m_fileLabel = addRow(tr("File:"));
    //: The type of book the chosen kind belongs to. In Polish: Typ:
    m_typeLabel = addRow(tr("Type:"));
    m_typeCaption = caption;
    layout->addLayout(grid);
    layout->addStretch(1);
    return panel;
}

QWidget* NewFileDialog::createKindsGroup(QWidget* parent)
{
    //: The tiles of the kinds of elements of the book. In Polish: Rodzaje
    auto* group = new QGroupBox(tr("Kinds"), parent);
    auto* layout = new QVBoxLayout(group);
    m_kindList = new KindList(group);
    m_kindList->setItemDelegate(new KindDelegate(m_kindList));
    layout->addWidget(m_kindList);
    return group;
}

QLayout* NewFileDialog::createTitleRow(QWidget* parent)
{
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    //: In Polish: &Tytuł:
    auto* titleLabel = new QLabel(tr("&Title:"), parent);
    m_titleEdit = new QLineEdit(parent);
    titleLabel->setBuddy(m_titleEdit);
    row->addWidget(titleLabel);
    row->addWidget(m_titleEdit, 1);

    // A new text element of the book gets its status (Add Element), Draft at first
    //: The status of a new text element of the book. In Polish: &Stan:
    m_statusLabel = new QLabel(tr("&Status:"), parent);
    m_statusBox = new QComboBox(parent);
    //: A status of a text element. In Polish: Szkic
    m_statusBox->addItem(tr("Draft"), QStringLiteral("draft"));
    //: A status of a text element. In Polish: Poprawki
    m_statusBox->addItem(tr("Revision"), QStringLiteral("revision"));
    //: A status of a text element. In Polish: Gotowy
    m_statusBox->addItem(tr("Final"), QStringLiteral("final"));
    m_statusLabel->setBuddy(m_statusBox);
    m_statusLabel->hide();
    m_statusBox->hide();
    row->addSpacing(8);
    row->addWidget(m_statusLabel);
    row->addWidget(m_statusBox);
    return row;
}

QWidget* NewFileDialog::createMaterialRow(QWidget* parent)
{
    m_materialRow = new QWidget(parent);
    auto* row = new QHBoxLayout(m_materialRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    //: The file a new material is a copy of. In Polish: P&lik:
    auto* label = new QLabel(tr("Fi&le:"), m_materialRow);
    m_materialLabel = new QLabel(m_materialRow);
    m_materialLabel->setTextFormat(Qt::PlainText);
    // A long path does not widen the window: it is shortened in its middle
    m_materialLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    //: Chooses the file of a new material again. In Polish: Wy&bierz...
    m_chooseButton = new QPushButton(tr("C&hoose..."), m_materialRow);
    label->setBuddy(m_chooseButton);
    row->addWidget(label);
    row->addWidget(m_materialLabel, 1);
    row->addWidget(m_chooseButton);
    m_materialRow->hide();
    return m_materialRow;
}

QWidget* NewFileDialog::createPlaceGroup(QWidget* parent)
{
    //: The place of the new element in the book. In Polish: Miejsce
    auto* group = new QGroupBox(tr("Place"), parent);
    auto* layout = new QVBoxLayout(group);

    // The list of the part of the book with the element in its place, named by the group; an
    // empty text file has no place in it, and a note says where it goes instead. Both pages
    // keep their room, so a choice of a tile does not move the tiles or the title.
    // A new group (Add Element) can take inside it the elements that close the main section,
    // and the window notes what happens to them
    m_takeBox = new QCheckBox(group);
    m_takeBox->setChecked(true);
    m_takeBox->hide();
    layout->addWidget(m_takeBox);
    m_noticeBox = new QWidget(group);
    auto* noticeLayout = new QHBoxLayout(m_noticeBox);
    noticeLayout->setContentsMargins(0, 0, 0, 0);
    noticeLayout->setSpacing(8);
    m_noticeIcon = new QLabel(m_noticeBox);
    m_noticeIcon->setAlignment(Qt::AlignTop);
    noticeLayout->addWidget(m_noticeIcon);
    m_noticeText = new QLabel(m_noticeBox);
    m_noticeText->setTextFormat(Qt::PlainText);
    m_noticeText->setWordWrap(true);
    noticeLayout->addWidget(m_noticeText, 1);
    m_noticeBox->hide();
    layout->addWidget(m_noticeBox);

    m_placePages = new QStackedWidget(group);
    m_picker = new ElementPlacePicker(m_placePages);
    m_picker->setLabel(QString());
    // The list asks for the rows it keeps on a small screen and grows with the window, so
    // that in a low window the description of the kind keeps all its lines
    m_picker->setListRows(ElementPlacePicker::MINIMUM_LIST_ROWS);
    m_placePages->addWidget(m_picker);
    auto* notePage = new QWidget(m_placePages);
    auto* noteLayout = new QVBoxLayout(notePage);
    noteLayout->setContentsMargins(0, 0, 0, 0);

    // An empty text file goes outside the book, at first, or with it; the other tiles do not
    // have these buttons
    auto* emptyFilePlaces = new QHBoxLayout();
    emptyFilePlaces->setSpacing(16);
    //: The place of an empty text file: not in the book, in a tab of its own. In Polish:
    //: &Poza książką
    m_outsideButton = new QRadioButton(tr("&Outside the book"), notePage);
    //: The place of an empty text file: in the book, among its files outside its text
    //: ("Other Files" in the Navigator). In Polish: &W książce
    m_insideButton = new QRadioButton(tr("&In the book"), notePage);
    m_outsideButton->setChecked(true);
    emptyFilePlaces->addWidget(m_outsideButton);
    emptyFilePlaces->addWidget(m_insideButton);
    emptyFilePlaces->addStretch(1);
    noteLayout->addLayout(emptyFilePlaces);

    m_placeNote = new QLabel(notePage);
    m_placeNote->setTextFormat(Qt::PlainText);
    m_placeNote->setWordWrap(true);
    noteLayout->addWidget(m_placeNote);
    noteLayout->addStretch(1);
    m_placePages->addWidget(notePage);
    // The page not shown counts as well: the place keeps the room of the list whichever page
    // is shown
    for (QWidget* page : {static_cast<QWidget*>(m_picker), notePage}) {
        QSizePolicy policy = page->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        page->setSizePolicy(policy);
    }
    layout->addWidget(m_placePages);

    //: Under the list of a new group, which only shows the book with it. In Polish: Lista
    //: tylko pokazuje, jak będzie wyglądać książka.
    m_previewNote = new QLabel(tr("The list only shows how the book will look."), group);
    m_previewNote->setTextFormat(Qt::PlainText);
    m_previewNote->setWordWrap(true);
    m_previewNote->hide();
    layout->addWidget(m_previewNote);
    return group;
}

void NewFileDialog::fillKinds()
{
    m_kindList->clear();
    const QString language = programLanguage();
    for (qsizetype section = 0; section < m_book.sections.size(); ++section) {
        const core::BookPlace place = m_book.sections.at(section).place;
        const auto inSection = [place](const NewFileKind& entry) {
            return entry.section == place;
        };
        if (std::none_of(m_book.kinds.cbegin(), m_book.kinds.cend(), inSection)) {
            continue;
        }

        // The name of the part of the book, in a row of its own
        auto* heading = new QListWidgetItem(m_book.groupNames.value(section), m_kindList);
        heading->setData(HEADING_ROLE, true);
        heading->setFlags(Qt::NoItemFlags);

        // Its tiles; a greyed one says why in its tooltip
        for (qsizetype index = 0; index < m_book.kinds.size(); ++index) {
            const NewFileKind& entry = m_book.kinds.at(index);
            if (entry.section != place) {
                continue;
            }
            auto* tile = new QListWidgetItem(entry.kind.kind->name.text(language), m_kindList);
            tile->setData(KIND_ROLE, static_cast<int>(index));
            tile->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            if (entry.unavailable.isEmpty()) {
                tile->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            } else {
                tile->setFlags(Qt::NoItemFlags);
                tile->setToolTip(entry.unavailable);
            }
        }
    }

    // The files that are no kind of the book, after the parts of the book
    if (!m_book.untitled.isEmpty()) {
        //: The group of the tiles of files that are no kind of the book, after the parts of
        //: the book. In Polish: Inne pliki
        auto* heading = new QListWidgetItem(tr("Other Files"), m_kindList);
        heading->setData(HEADING_ROLE, true);
        heading->setFlags(Qt::NoItemFlags);
        auto* tile = new QListWidgetItem(emptyFileName(), m_kindList);
        tile->setData(EMPTY_FILE_ROLE, true);
        tile->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        tile->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }
    sizeTiles();
}

void NewFileDialog::sizeTiles()
{
    auto& artProvider = core::ArtProvider::getInstance();
    const int iconSize = artProvider.getIconSize(core::IconContext::Dialog);
    m_kindList->setIconSize(QSize(iconSize, iconSize));

    // As wide as two short words of the font of the list, and as high as the longest name
    // needs, two lines at least and three at most
    const QFont font = m_kindList->font();
    const QFontMetrics metrics(font);
    const int width = qRound(TILE_WIDTH_EMS * QFontInfo(font).pixelSize());
    int lines = TILE_TEXT_LINES;
    for (int row = 0; row < m_kindList->count(); ++row) {
        QListWidgetItem* item = m_kindList->item(row);
        if (item->data(HEADING_ROLE).toBool()) {
            continue;
        }
        const QString iconId =
            item->data(EMPTY_FILE_ROLE).toBool()
                ? EMPTY_FILE_ICON
                : NavigatorPanel::iconIdOf(m_book.kinds.at(item->data(KIND_ROLE).toInt()).kind);
        item->setIcon(artProvider.getIcon(iconId, core::IconContext::Dialog));
        const QRect text = metrics.boundingRect(
            QRect(0, 0, width - TILE_PADDING, std::numeric_limits<int>::max() / 2),
            Qt::TextWordWrap | Qt::AlignHCenter, item->text());
        lines = std::max(lines, (text.height() + metrics.lineSpacing() - 1) /
                                    std::max(1, metrics.lineSpacing()));
    }
    lines = std::min(lines, MAXIMUM_TEXT_LINES);
    const QSize tile(width, iconSize + lines * metrics.lineSpacing() + 2 * TILE_PADDING);
    for (int row = 0; row < m_kindList->count(); ++row) {
        QListWidgetItem* item = m_kindList->item(row);
        if (!item->data(HEADING_ROLE).toBool()) {
            item->setSizeHint(tile);
        }
    }

    // At least the name of a part of the book and a row of its tiles in view
    m_kindList->setMinimumHeight(KindDelegate::headingHeight(font) + tile.height() +
                                 3 * m_kindList->spacing() + 2 * m_kindList->frameWidth());
}

const NewFileKind* NewFileDialog::chosen() const
{
    const QListWidgetItem* item = m_kindList ? m_kindList->currentItem() : nullptr;
    if (!item || !item->data(KIND_ROLE).isValid()) {
        return nullptr;
    }
    const int index = item->data(KIND_ROLE).toInt();
    if (index < 0 || index >= m_book.kinds.size() ||
        !m_book.kinds.at(index).unavailable.isEmpty()) {
        return nullptr;
    }
    return &m_book.kinds.at(index);
}

QString NewFileDialog::emptyFileName()
{
    //: The tile of an empty text file in the New File window, and its name above its
    //: description. In Polish: Pusty plik tekstowy
    return tr("Empty Text File");
}

bool NewFileDialog::emptyFileChosen() const
{
    const QListWidgetItem* item = m_kindList ? m_kindList->currentItem() : nullptr;
    return item && item->data(EMPTY_FILE_ROLE).toBool();
}

bool NewFileDialog::outsideBook() const
{
    return emptyFileChosen() && !m_insideButton->isChecked();
}

bool NewFileDialog::emptyFileInBook() const
{
    return emptyFileChosen() && m_insideButton->isChecked();
}

void NewFileDialog::onKindChanged()
{
    const bool emptyFile = emptyFileChosen();
    const NewFileKind* entry = chosen();

    // An empty text file and an element of the Workshop have no place in the list of the
    // book: the note says where they go. Only an empty text file goes outside the book or
    // with it.
    const bool noted = emptyFile || (entry && !entry->placeNote.isEmpty());
    m_placePages->setCurrentWidget(noted ? m_placeNote->parentWidget()
                                         : static_cast<QWidget*>(m_picker));
    m_outsideButton->setVisible(emptyFile);
    m_insideButton->setVisible(emptyFile);
    const bool status = hasStatus(entry);
    m_statusLabel->setVisible(status);
    m_statusBox->setVisible(status);
    m_materialRow->setVisible(isMaterial(entry));
    const bool group = entry && entry->kind.kind->form == core::ElementForm::Group;
    if (!group) {
        m_takeBox->hide();
        showNotice(QString(), QString());
        m_previewNote->hide();
        m_picker->setPreview(false);
        m_picker->setLabel(QString());
    }
    updateHeading();
    if (emptyFile) {
        showEmptyFile();
        return;
    }
    m_typeCaption->show();
    m_typeLabel->show();

    updateKindIcon();
    m_nameLabel->setText(entry ? entry->kind.kind->name.text(programLanguage()) : QString());
    m_whereLabel->setText(entry ? entry->where : QString());
    m_countLabel->setText(entry ? entry->count : QString());
    m_fileLabel->setText(entry ? entry->file : QString());
    m_typeLabel->setText(entry ? entry->type : QString());
    if (!entry) {
        m_picker->setElement(core::KindRef{}, QString());
        updateCreateButton();
        return;
    }

    // The title follows the kind, or the file of a material, until the writer changes it
    if (!m_titleChanged) {
        m_titleEdit->setText(!isMaterial(entry)       ? entry->title
                             : m_materialFile.isEmpty() ? QString()
                                                        : QFileInfo(m_materialFile).completeBaseName());
    }
    if (isMaterial(entry)) {
        showMaterialFile();
        // The tile of a material asks for its file at once; at the start, once the window is
        // shown (showEvent())
        if (m_materialFile.isEmpty() && isVisible()) {
            QTimer::singleShot(0, this, &NewFileDialog::chooseMaterialFile);
        }
    }
    if (noted) {
        m_placeNote->setText(entry->placeNote);
    } else if (group) {
        showGroup(*entry);
    } else {
        showPlaces(*entry);
    }
    updateCreateButton();
}

void NewFileDialog::showPlaces(const NewFileKind& entry)
{
    // The places of the kind in the part of the book of its tile
    QList<PickerSection> sections;
    for (const PickerSection& section : std::as_const(m_book.sections)) {
        if (section.place == entry.section) {
            sections.append(section);
        }
    }
    m_picker->setBook(sections, *m_registry);
    m_picker->setElement(entry.kind, title());
    m_picker->setPlace(entry.place);
}

void NewFileDialog::showGroup(const NewFileKind& entry)
{
    const auto section =
        std::find_if(m_book.sections.cbegin(), m_book.sections.cend(),
                     [&entry](const PickerSection& part) { return part.place == entry.section; });
    if (section == m_book.sections.cend()) {
        return;
    }
    const QList<core::ProjectElement>& elements = section->elements;

    // The elements that close the main section at the end of the groups before the new group,
    // and whether the group can have them inside it
    m_closing.clear();
    m_canTake = false;
    if (entry.place.groupId.isEmpty()) {
        const qsizetype before = qMin(entry.place.index, elements.size());
        for (const core::ProjectElement* element :
             core::BookProject::closingElementsOf(*m_registry, elements)) {
            for (qsizetype i = 0; i < before; ++i) {
                if (findIn(elements.at(i).elements, element->id)) {
                    m_closing.append(*element);
                    break;
                }
            }
        }
        m_canTake = !m_closing.isEmpty() &&
                    std::all_of(m_closing.cbegin(), m_closing.cend(),
                                [this, &entry](const core::ProjectElement& element) {
                                    const core::KindRef inside =
                                        core::BookProject::kindOf(*m_registry, element);
                                    return inside && inside.kind->allowsInside(entry.kind.kind->id);
                                });
    }
    const QStringList titles = titlesOf(m_closing);
    const QString quoted = ElementPlacePicker::quoted(titles);
    const bool one = titles.size() == 1;
    m_takeBox->setText(
        one
            //: In Polish: Przenieś element „%1” na koniec
            //: {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive}
            ? named(tr("Move \"%1\" to the end of the new {kind}")).arg(titles.value(0))
            //: %1: the titles in quotes. In Polish: Przenieś %1 na koniec
            //: {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive}
            : named(tr("Move %1 to the end of the new {kind}")).arg(quoted));
    m_takeBox->setVisible(m_canTake);
    const bool taken = m_canTake && m_takeBox->isChecked();

    if (m_closing.isEmpty()) {
        showNotice(QString(), QString());
    } else if (taken) {
        showNotice(QStringLiteral("help.about"),
                   one
                       //: In Polish: Element „%1” jest teraz ostatni %2. Na końcu
                       //: {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive} nadal
                       //: będzie ostatni.
                       ? named(tr("\"%1\" is now the last element %2. At the end of the new "
                                  "{kind} it stays the last one."))
                             .arg(titles.first(), section->words.inPart)
                       //: In Polish: Elementy %1 są teraz ostatnie %2. Na końcu
                       //: {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive} nadal
                       //: będą ostatnie.
                       : named(tr("%1 are now the last elements %2. At the end of the new {kind} "
                                  "they stay the last ones."))
                             .arg(quoted, section->words.inPart));
    } else {
        showNotice(QStringLiteral("common.warning"),
                   one
                       //: {main:plural}: the main texts of the book, "chapters", "stories". In
                       //: Polish: Element „%1” zostanie na swoim miejscu, a {main:plural}
                       //: dodane do {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive}
                       //: znajdą się za nim.
                       ? named(tr("\"%1\" stays where it is, and the {main:plural} added to the "
                                  "new {kind} go after it."))
                             .arg(titles.first())
                       //: In Polish: Elementy %1 zostaną na swoim miejscu, a {main:plural}
                       //: dodane do {kind:m=nowego|f=nowej|n=nowego|p=nowych} {kind:genitive}
                       //: znajdą się za nimi.
                       : named(tr("%1 stay where they are, and the {main:plural} added to the "
                                  "new {kind} go after them."))
                             .arg(quoted));
    }

    // The part of the book as it will be: the group has no place to choose
    m_picker->setPreview(true);
    //: The list of a part of the book with a new group, e.g. a part. In Polish: Po dodaniu
    //: {kind:genitive}
    m_picker->setLabel(named(tr("After adding the {kind}")));
    m_picker->setBook({{section->place, section->name, section->words,
                        taken ? without(elements, idsOf(m_closing)) : elements}},
                      *m_registry);
    m_picker->setElement(entry.kind, title(), taken ? m_closing : QList<core::ProjectElement>());
    m_picker->setPlace(entry.place);
    m_previewNote->setVisible(!m_picker->places().isEmpty());
}

void NewFileDialog::showNotice(const QString& iconId, const QString& text)
{
    m_noticeText->setText(text);
    m_noticeIcon->setPixmap(iconId.isEmpty()
                                ? QPixmap()
                                : core::ArtProvider::getInstance().getIcon(iconId).pixmap(
                                      NOTICE_ICON_SIZE, NOTICE_ICON_SIZE));
    m_noticeBox->setVisible(!text.isEmpty());
}

QString NewFileDialog::named(const QString& text) const
{
    const NewFileKind* entry = chosen();
    return core::fillWords(text,
                           {{QStringLiteral("kind"), wordsOf(entry ? entry->kind : core::KindRef{})},
                            {QStringLiteral("main"), wordsOf(m_book.mainKind)}});
}

void NewFileDialog::updateHeading()
{
    if (!m_book.adding) {
        return;
    }
    const NewFileKind* entry = chosen();
    const bool workshop = std::any_of(
        m_book.sections.cbegin(), m_book.sections.cend(),
        [](const PickerSection& section) { return section.place == core::BookPlace::Workshop; });
    QString description;
    if (isMaterial(entry)) {
        //: The heading of the Add Element window for a material. In Polish: Wybierz plik
        //: {kind:genitive} i {kind:m=jego|f=jej|n=jego|p=ich} tytuł.
        description = named(tr("Choose the file of the {kind} and its title."));
    } else if (workshop) {
        //: The heading of the Add Element window in the Workshop. In Polish: Wybierz rodzaj
        //: nowego elementu Warsztatu i jego tytuł.
        description = tr("Choose the kind of the new element of the Workshop and its title.");
    } else if (entry && !hasStatus(entry)) {
        //: The heading of the Add Element window for an element without a status, e.g. a
        //: part. In Polish: Wybierz rodzaj nowego elementu, jego tytuł i miejsce.
        description = tr("Choose the kind of the new element, its title and place.");
    } else {
        //: The heading of the Add Element window. In Polish: Wybierz rodzaj nowego elementu,
        //: jego tytuł, stan i miejsce.
        description = tr("Choose the kind of the new element, its title, status and place.");
    }
    //: The window of a new element at the place of the Navigator or of the editor it was
    //: opened on. In Polish: Dodaj element
    setHeading(tr("Add Element"), description);
}

void NewFileDialog::chooseMaterialFile()
{
    if (m_choosingFile || !isMaterial(chosen())) {
        return;
    }
    m_choosingFile = true;
    const QString folder =
        m_materialFile.isEmpty()
            ? ProgramFolders::windowFolder(ProgramFolders::Operation::AddMaterial)
            : QFileInfo(m_materialFile).absolutePath();
    QString file;
    if (fileChooser()) {
        file = fileChooser()(folder);
    } else {
        //: The filters of the window of files of a new material: any file first. In Polish:
        //: Wszystkie pliki (*)
        const QStringList filters{tr("All Files (*)"),
                                  //: In Polish: Obrazy (*.png *.jpg *.jpeg *.gif *.bmp *.svg *.webp)
                                  tr("Images (*.png *.jpg *.jpeg *.gif *.bmp *.svg *.webp)"),
                                  //: In Polish: Pliki PDF (*.pdf)
                                  tr("PDF Files (*.pdf)")};
        //: The system's window of files of a new material. In Polish: Wybierz plik
        //: {kind:genitive}
        file = QFileDialog::getOpenFileName(this, named(tr("Choose the File of the {Kind}")),
                                            folder, filters.join(QStringLiteral(";;")));
    }
    m_choosingFile = false;
    if (file.isEmpty()) {
        return;
    }
    const QFileInfo chosenFile(file);
    m_materialFile = chosenFile.absoluteFilePath();
    ProgramFolders::remember(ProgramFolders::Operation::AddMaterial, chosenFile.absolutePath());
    if (!m_titleChanged) {
        m_titleEdit->setText(chosenFile.completeBaseName());
    }
    showMaterialFile();
    updateCreateButton();
}

void NewFileDialog::showMaterialFile()
{
    const QString path = QDir::toNativeSeparators(m_materialFile);
    m_materialLabel->setToolTip(path);
    m_materialLabel->setForegroundRole(m_materialFile.isEmpty() ? QPalette::PlaceholderText
                                                                : QPalette::WindowText);
    //: The file of a new material before the writer chooses one. In Polish: nie wybrano pliku
    const QString text = m_materialFile.isEmpty() ? tr("no file chosen") : path;
    m_materialLabel->setText(m_materialLabel->fontMetrics().elidedText(
        text, Qt::ElideMiddle, std::max(1, m_materialLabel->width())));
}

bool NewFileDialog::isMaterial(const NewFileKind* entry)
{
    return entry && entry->kind.kind->isMaterial();
}

bool NewFileDialog::hasStatus(const NewFileKind* entry) const
{
    return m_book.adding && entry && entry->kind.kind->form == core::ElementForm::Text &&
           entry->section != core::BookPlace::Workshop;
}

void NewFileDialog::showEmptyFile()
{
    const bool inBook = m_insideButton->isChecked();
    updateKindIcon();
    m_nameLabel->setText(emptyFileName());
    m_insideButton->setEnabled(!m_book.bookFile.isEmpty());
    m_typeCaption->hide();
    m_typeLabel->hide();
    //: How many empty text files there can be. In Polish: dowolnie wiele
    m_countLabel->setText(tr("any number"));
    if (inBook) {
        //: Where an empty text file in the book goes: to the files of the book outside its
        //: text, "Other Files" in the Navigator. In Polish: w książce, w grupie „Inne pliki”,
        //: poza jej treścią
        m_whereLabel->setText(tr("in the book, in \"Other Files\", outside its text"));
        m_fileLabel->setText(m_book.bookFile);
        //: Where an empty text file in the book goes, in place of the list of places. In
        //: Polish: Plik trafi do folderu książki i do grupy „Inne pliki” w Nawigatorze. Nie
        //: należy do treści książki, więc gotowa książka go nie zawiera.
        m_placeNote->setText(tr("The file goes to the book's folder and to \"Other Files\" in "
                                "the Navigator. It is not part of the text of the book, so the "
                                "finished book does not have it."));
    } else {
        //: Where an empty text file outside the book goes. In Polish: poza książką, w nowej
        //: karcie
        m_whereLabel->setText(tr("outside the book, in a tab of its own"));
        //: The file of an empty text file outside the book: the writer chooses it when saving
        //: it for the first time. In Polish: wybierzesz go przy pierwszym zapisie
        m_fileLabel->setText(tr("chosen when it is saved for the first time"));
        //: Where an empty text file outside the book goes, in place of the list of places. In
        //: Polish: Plik otworzy się w nowej karcie, poza książką. Miejsce i format pliku
        //: wybierzesz przy pierwszym zapisie.
        m_placeNote->setText(tr("The file opens in a new tab, outside the book. You choose its "
                                "place and format when you save it for the first time."));
    }

    // The title of an empty text file, "Untitled 2", unless the writer changed it
    if (!m_titleChanged) {
        m_titleEdit->setText(m_book.untitled);
    }
    updateCreateButton();
}

void NewFileDialog::updateKindIcon()
{
    const bool emptyFile = emptyFileChosen();
    const NewFileKind* entry = chosen();
    if (!emptyFile && !entry) {
        m_iconLabel->clear();
        return;
    }
    const QIcon icon = core::ArtProvider::getInstance().getThemedIcon(
        emptyFile ? EMPTY_FILE_ICON : NavigatorPanel::iconIdOf(entry->kind));
    m_iconLabel->setPixmap(icon.pixmap(QSize(DESCRIPTION_ICON_SIZE, DESCRIPTION_ICON_SIZE),
                                       devicePixelRatioF()));
}

void NewFileDialog::updateCreateButton()
{
    // An empty text file needs only its title, an element of the Workshop no place in the
    // list, and a material its file
    if (emptyFileChosen()) {
        acceptButton()->setEnabled(!title().isEmpty());
        return;
    }
    const NewFileKind* entry = chosen();
    acceptButton()->setEnabled(entry && !title().isEmpty() &&
                               (!isMaterial(entry) || !m_materialFile.isEmpty()) &&
                               (!entry->placeNote.isEmpty() || !m_picker->places().isEmpty()));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
