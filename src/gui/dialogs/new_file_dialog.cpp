/// @file new_file_dialog.cpp
/// @brief New File: a new element of the open book, of the kind, with the title and at the
/// place the writer chooses

#include "kalahari/gui/dialogs/new_file_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileInfo>
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
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
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
constexpr QSize DEFAULT_SIZE{880, 740};    ///< The tiles of the main part and the details in view
constexpr int DESCRIPTION_WIDTH = 250;     ///< Of the description at the left, as in New Book
constexpr int DESCRIPTION_ICON_SIZE = 64;  ///< Of the icon above the description
constexpr int NAME_POINT_SIZE = 12;        ///< Of the name of the kind above its description
constexpr int KIND_ROLE = Qt::UserRole;          ///< Index of the kind of a tile in Book::kinds
constexpr int HEADING_ROLE = Qt::UserRole + 1;   ///< The item names a part of the book
constexpr qreal TILE_WIDTH_EMS = 7.5;      ///< Room for a name of two short words in a line
constexpr int TILE_TEXT_LINES = 2;         ///< Lines of a name the tiles have room for at least
constexpr int MAXIMUM_TEXT_LINES = 3;      ///< Lines of the longest name the tiles make room for
constexpr int TILE_PADDING = 8;            ///< Around the icon and the name of a tile
constexpr int TILE_SPACING = 6;            ///< Between the tiles and their rows
constexpr int HEADING_TOP = 6;             ///< Above the name of a part of the book
constexpr int HEADING_BOTTOM = 3;          ///< Between the name of a part and its line

/// The parts of the book, in reading order
constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                     core::BookPlace::Back};

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
    /// the part of the book of that tile, so that no name is cut in half above the tiles
    void doItemsLayout() override
    {
        QListWidget::doItemsLayout();
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

NewFileDialog::Book NewFileDialog::openBook()
{
    Book book;
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    if (!project || !projectBook) {
        return book;
    }
    const core::BookTypeRegistry& registry = pm.bookTypes();
    const QString language = programLanguage();

    // The kinds of the groups of the book (a part, a cycle), for the words that say where the
    // elements of a kind can be: "in parts"
    QList<core::KindRef> groups;
    for (const core::BookPlace place : BOOK_PLACES) {
        for (const core::KindRef& kind : project->kindsIn(registry, place)) {
            const auto same = [&kind](const core::KindRef& group) {
                return group.kind == kind.kind;
            };
            if (kind.kind->form == core::ElementForm::Group &&
                std::none_of(groups.cbegin(), groups.cend(), same)) {
                groups.append(kind);
            }
        }
    }

    // Where the elements of a kind can be: in the parts of the book and in groups
    const auto whereOf = [&groups, projectBook](const core::KindRef& kind) {
        QStringList places;
        for (const core::BookPlace place : BOOK_PLACES) {
            if (kind.kind->allows(place)) {
                places << SectionWords::forPart(projectBook, place).inPart;
            }
        }
        for (const core::KindRef& group : std::as_const(groups)) {
            if (kind.kind->allowsInside(group.kind->id)) {
                //: Where the elements of a kind can be: in the groups of a kind, e.g. in parts.
                //: In Polish: w {group:locativePlural}
                places << core::fillWords(tr("in {group:plural}"),
                                          {{QStringLiteral("group"), wordsOf(group)}});
            }
        }
        if (places.size() < 2) {
            return places.value(0);
        }
        const QString last = places.takeLast();
        //: The places of a kind in a sentence: %1 is the places but the last one, joined with
        //: commas; %2 is the last one. In Polish: %1 i %2
        return tr("%1 and %2").arg(places.join(QStringLiteral(", ")), last);
    };

    // How many elements of a kind the book can have
    const auto countOf = [](const core::KindRef& kind) {
        if (kind.kind->limit <= 0) {
            //: How many elements of a kind the book can have. In Polish: dowolnie wiele
            return tr("any number");
        }
        if (kind.kind->limit == 1) {
            //: How many elements of a kind the book can have, e.g. one title page. In Polish:
            //: {kind:m=jeden|f=jedna|n=jedno|p=jedne} w książce
            return core::fillWords(tr("one {kind} in the book"),
                                   {{QStringLiteral("kind"), wordsOf(kind)}});
        }
        //: How many elements of a kind the book can have; %1: the number. In Polish:
        //: najwyżej %1 w książce
        return tr("at most %1 in the book").arg(kind.kind->limit);
    };

    const core::KindRef main = pm.mainTextKind();
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
            if (kind.kind->form == core::ElementForm::Group || !kind.kind->allows(place)) {
                continue;
            }
            const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(kind)}};
            NewFileKind entry;
            entry.kind = kind;
            entry.section = place;
            entry.where = whereOf(kind);
            entry.count = countOf(kind);
            // The type of the book; in a user project, the type each kind comes from
            entry.type = project->type ? typeNameOf(registry, *project)
                                       : kind.package->name.text(language);
            const auto same = [&kind](const core::KindRef& other) {
                return other.kind == kind.kind;
            };
            if (kind.kind->form == core::ElementForm::Window) {
                //: The tooltip of a greyed kind that opens in a window of its own, e.g. a
                //: table of contents. In Polish: Program nie ma jeszcze edytora
                //: {kind:genitive}.
                entry.unavailable = core::fillWords(
                    tr("The program does not have an editor for the {kind} yet."), nouns);
            } else if (std::none_of(offered.cbegin(), offered.cend(), same)) {
                entry.unavailable =
                    kind.kind->limit == 1
                        //: The tooltip of a greyed kind of which a book can have one element,
                        //: e.g. a title page. In Polish: Książka ma już {kind:accusative}.
                        ? core::fillWords(tr("The book already has {kind:indefinite}."), nouns)
                        //: The tooltip of a greyed kind; %1: how many elements of the kind a
                        //: book can have. In Polish: Książka ma już tyle elementów tego rodzaju,
                        //: ile może mieć: %1.
                        : tr("The book already has as many elements of this kind as it can "
                             "have: %1.")
                              .arg(kind.kind->limit);
            } else {
                entry.title = pm.defaultTitle(kind);
                entry.place = pm.newPlaceOf(kind, place);
                const QFileInfo file(pm.newChapterFile(kind, place));
                //: The file of a new element: %1 is its name, %2 the folder of the book it is
                //: in. In Polish: %1 w folderze „%2”
                entry.file = tr("%1 in the folder \"%2\"").arg(file.fileName(), file.path());
                if (book.current < 0 && place == core::BookPlace::Main && kind.kind == main.kind) {
                    book.current = static_cast<int>(book.kinds.size());
                }
            }
            book.kinds.append(entry);
        }
    }

    // Without the main text kind, the first kind of which an element can be made
    if (book.current < 0) {
        const auto first = std::find_if(
            book.kinds.cbegin(), book.kinds.cend(),
            [](const NewFileKind& entry) { return entry.unavailable.isEmpty(); });
        book.current =
            first != book.kinds.cend() ? static_cast<int>(first - book.kinds.cbegin()) : -1;
    }
    return book;
}

NewFileDialog::NewFileDialog(const Book& book, const core::BookTypeRegistry& registry,
                             QWidget* parent)
    : QDialog(parent)
    , m_book(book)
    , m_registry(&registry)
{
    //: The window of a new element of the open book. In Polish: Nowy plik
    setWindowTitle(tr("New File"));

    // As wide as the description and three tiles in a row; the height of the content can be
    // lower: on a small screen (1366x768 at 150%) the content scrolls above the buttons
    setMinimumWidth(MINIMUM_WIDTH);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(11);
    mainLayout->setContentsMargins(11, 11, 11, 11);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setSpacing(11);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    auto* top = new QHBoxLayout();
    top->setSpacing(11);
    top->addWidget(createDescriptionPanel(content));
    top->addWidget(createKindsGroup(content), 1);
    contentLayout->addLayout(top, 1);
    contentLayout->addWidget(createDetailsGroup(content));
    mainLayout->addWidget(new FittingScrollArea(content, this), 1);

    auto* buttons = new QDialogButtonBox(this);
    //: The button that makes the new element. In Polish: Utwórz
    m_createButton = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    fillKinds();
    connect(m_kindList, &QListWidget::currentItemChanged, this, &NewFileDialog::onKindChanged);
    connect(m_kindList, &QListWidget::itemDoubleClicked, this, [this]() {
        if (m_createButton->isEnabled()) {
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

    // The size of a big screen, or less on a smaller one
    utils::resizeWithinScreen(this, DEFAULT_SIZE);

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
    return m_picker->place();
}

void NewFileDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::FontChange && m_kindList) {
        sizeTiles();
    }
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
    const auto addRow = [panel, grid, &rows](const QString& words) {
        const int row = rows++;
        auto* label = new QLabel(words, panel);
        label->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(label, row, 0, Qt::AlignTop);
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

QWidget* NewFileDialog::createDetailsGroup(QWidget* parent)
{
    //: The title and the place of the new element. In Polish: Szczegóły
    auto* group = new QGroupBox(tr("Details"), parent);
    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);
    grid->setContentsMargins(11, 11, 11, 11);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(1, 1);

    //: In Polish: &Tytuł:
    auto* titleLabel = new QLabel(tr("&Title:"), group);
    m_titleEdit = new QLineEdit(group);
    titleLabel->setBuddy(m_titleEdit);
    grid->addWidget(titleLabel, 0, 0);
    grid->addWidget(m_titleEdit, 0, 1);

    // The list of the part of the book with the element in its place, named beside it
    //: The place of the new element in the book. In Polish: Miejsce:
    auto* placeLabel = new QLabel(tr("Place:"), group);
    m_picker = new ElementPlacePicker(group);
    m_picker->setLabel(QString());
    grid->addWidget(placeLabel, 1, 0, Qt::AlignTop);
    grid->addWidget(m_picker, 1, 1);
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
        const NewFileKind& entry = m_book.kinds.at(item->data(KIND_ROLE).toInt());
        item->setIcon(artProvider.getIcon(NavigatorPanel::iconIdOf(entry.kind),
                                          core::IconContext::Dialog));
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

void NewFileDialog::onKindChanged()
{
    const NewFileKind* entry = chosen();
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

    // The title follows the kind until the writer changes it
    if (!m_titleChanged) {
        m_titleEdit->setText(entry->title);
    }

    // The places of the kind in the part of the book of its tile
    QList<PickerSection> sections;
    for (const PickerSection& section : std::as_const(m_book.sections)) {
        if (section.place == entry->section) {
            sections.append(section);
        }
    }
    m_picker->setBook(sections, *m_registry);
    m_picker->setElement(entry->kind, title());
    m_picker->setPlace(entry->place);
    updateCreateButton();
}

void NewFileDialog::updateKindIcon()
{
    const NewFileKind* entry = chosen();
    if (!entry) {
        m_iconLabel->clear();
        return;
    }
    const QIcon icon =
        core::ArtProvider::getInstance().getThemedIcon(NavigatorPanel::iconIdOf(entry->kind));
    m_iconLabel->setPixmap(icon.pixmap(QSize(DESCRIPTION_ICON_SIZE, DESCRIPTION_ICON_SIZE),
                                       devicePixelRatioF()));
}

void NewFileDialog::updateCreateButton()
{
    m_createButton->setEnabled(chosen() && !title().isEmpty() && !m_picker->places().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
