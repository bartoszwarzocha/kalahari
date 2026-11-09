/// @file special_character_dialog.cpp
/// @brief Insert > Special Character...: the characters by group or in a list with their names,
///        found by a name or a code, inserted in the text at the cursor

#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/special_character_choices.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int SPACING = 12;            ///< Between the parts of a view
constexpr int SMALL_SPACING = 6;       ///< Between the lines of the details, the buttons
constexpr qreal GLYPH_MARK_SCALE = 1.4;  ///< The mark of the details, against the dialog's font
constexpr int GRID_COLUMNS = 10;       ///< The cells of a row of the grid at first
constexpr int GRID_ROWS = 6;           ///< The rows of the grid at first
constexpr int MINIMUM_GRID_COLUMNS = 6;
constexpr int MINIMUM_GRID_ROWS = 2;   ///< On a small screen
constexpr int LIST_ROWS = 12;          ///< The rows of the list at first
constexpr int DETAILS_WIDTH = 17;      ///< The details, in widths of the letter "x" of the font
constexpr int GROUPS_PADDING = 24;     ///< Beside the longest name of a group
constexpr int MINIMUM_GROUPS_WIDTH = 12;  ///< The groups, in widths of the letter "x"

constexpr int CODE_ROLE = Qt::UserRole;  ///< The code of the character of a line of the list

const QString FAVORITES = QStringLiteral("favorites");
const QString RECENT = QStringLiteral("recent");
const QString ALL = QStringLiteral("all");

/// The columns of the list
enum ListColumn { GLYPH_COLUMN, NAME_COLUMN, CODE_COLUMN, KEYS_COLUMN, FAVORITE_COLUMN,
                  COLUMN_COUNT };

/// The star of a favorite in the list, and of a character that is not one
const QString FAVORITE_STAR = QStringLiteral("★");
const QString NO_STAR = QStringLiteral("☆");

/// @brief The list of the characters, as high as LIST_ROWS rows at first
class CharacterList : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    QSize sizeHint() const override
    {
        const QSize hint = QTreeWidget::sizeHint();
        const int row = sizeHintForRow(0) > 0 ? sizeHintForRow(0) : fontMetrics().height() + 8;
        return {hint.width(), header()->sizeHint().height() + LIST_ROWS * row + 2 * frameWidth()};
    }
};

/// @brief A tooltip of a button with its keys: Alt and its access key, where the system has
///        access keys
QString withAccessKeys(const QString& tip, const QString& text)
{
    const QKeySequence keys = QKeySequence::mnemonic(text);
    return keys.isEmpty() ? tip
                          : QStringLiteral("%1 (%2)").arg(
                                tip, keys.toString(QKeySequence::NativeText));
}

/// @brief The group buttons of the list: only the one pressed takes Tab (one stop for all of
///        them; Left and Right move to the next one and press it)
void setGroupButtonFocus(QButtonGroup* buttons)
{
    // None pressed (a search): the first one takes Tab
    const QList<QAbstractButton*> all = buttons->buttons();
    const bool anyChecked = std::any_of(all.begin(), all.end(),
                                        [](const QAbstractButton* b) { return b->isChecked(); });
    for (QAbstractButton* button : all) {
        const bool tabStop = anyChecked ? button->isChecked() : button == all.first();
        button->setFocusPolicy(tabStop ? Qt::StrongFocus : Qt::ClickFocus);
    }
}

} // namespace

SpecialCharacterDialog::SpecialCharacterDialog(TargetOf targetOf, KeysOf keysOf,
                                               AutoCorrectOf autoCorrectOf, QWidget* parent)
    : KalahariDialog(parent)
    , m_targetOf(std::move(targetOf))
    , m_keysOf(std::move(keysOf))
    , m_autoCorrectOf(std::move(autoCorrectOf))
{
    setHeading(tr("Special Character"),
               tr("Enter inserts the character at the cursor. Esc closes the window."));
    setHeadingIcon(QStringLiteral("insert.specialChar"));
    setCompactHeading(true);
    setAcceptText(tr("&Insert"));
    cancelButton()->setText(tr("Close"));

    createCharacterMenu();

    // The search over both views; one view shown: the hidden one asks for no room (a stack
    // would ask for the room of both)
    contentLayout()->addLayout(createSearchRow());
    m_gridView = createGridView();
    m_listView = createListView();
    contentLayout()->addWidget(m_gridView, 1);
    contentLayout()->addWidget(m_listView, 1);

    // Whether the window stays open, and why it cannot insert
    m_keepOpen = new QCheckBox(tr("&Keep the window open after inserting"), this);
    m_keepOpen->setChecked(
        core::SettingsManager::getInstance().get<bool>(KEEP_CHARACTER_WINDOW_OPEN_SETTING, true));
    connect(m_keepOpen, &QCheckBox::toggled, this, [](bool keep) {
        core::SettingsManager::getInstance().set<bool>(KEEP_CHARACTER_WINDOW_OPEN_SETTING, keep);
    });
    contentLayout()->addWidget(m_keepOpen);
    m_whyNot = new QLabel(this);
    m_whyNot->setWordWrap(true);
    m_whyNot->setTextFormat(Qt::PlainText);
    m_whyNot->setForegroundRole(QPalette::PlaceholderText);
    m_whyNot->hide();
    contentLayout()->addWidget(m_whyNot);

    // The buttons of the list view: in the grid view they are in the details
    m_listFavoriteButton = addSideButton(tr("Add to &Favorites"));
    m_listShortcutButton = addSideButton(tr("Assign &Shortcut..."));
    m_listReplacementButton = addSideButton(tr("Add &Replacement..."));
    connect(m_listFavoriteButton, &QPushButton::clicked, this,
            &SpecialCharacterDialog::toggleFavorite);
    connect(m_listShortcutButton, &QPushButton::clicked, this,
            [this]() { emit shortcutRequested(m_current); });
    connect(m_listReplacementButton, &QPushButton::clicked, this,
            [this]() { emit replacementRequested(m_current); });

    // Ctrl+F: back to the search, as in the find bar of the text
    auto* find = new QShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_F), this);
    connect(find, &QShortcut::activated, this, [this]() {
        m_search->setFocus(Qt::ShortcutFocusReason);
        m_search->selectAll();
    });

    m_target = m_targetOf ? m_targetOf() : CharacterTarget{};
    updateFonts();

    const QList<char32_t> favorites = favoriteCharacters();
    m_gridGroup = favorites.isEmpty() ? m_characters.groups().first().id : FAVORITES;
    m_listGroup = ALL;
    m_current = favorites.isEmpty() ? m_characters.groups().first().characters.first()
                                    : favorites.first();
    // The view shown last; the grid at first, as in Word and LibreOffice
    setView(core::SettingsManager::getInstance().get<std::string>(CHARACTER_WINDOW_VIEW_SETTING,
                                                                   "grid") == "list"
                ? View::List
                : View::Grid);
}

// ============================================================================
// The views
// ============================================================================

QLayout* SpecialCharacterDialog::createSearchRow()
{
    auto* row = new QHBoxLayout();
    row->setSpacing(SPACING);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Find: a name or a code, e.g. em dash, arrow, euro, 2014"));
    m_search->setAccessibleName(tr("Find a character"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textEdited, this, &SpecialCharacterDialog::setSearchText);
    row->addWidget(m_search, 1);

    // The views as two buttons side by side, one of them pressed: Left and Right move between
    // them as between options
    auto* views = new QWidget(this);
    views->setAccessibleName(tr("View"));
    auto* viewsLayout = new QHBoxLayout(views);
    viewsLayout->setContentsMargins(0, 0, 0, 0);
    viewsLayout->setSpacing(0);
    const auto newViewButton = [this, views, viewsLayout](const QString& text,
                                                          const QString& tip, View view) {
        auto* button = new QToolButton(views);
        button->setText(text);
        button->setToolTip(withAccessKeys(tip, text));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setFocusPolicy(Qt::StrongFocus);
        connect(button, &QToolButton::clicked, this, [this, view]() { setView(view); });
        viewsLayout->addWidget(button);
        return button;
    };
    m_gridButton = newViewButton(tr("&Grid"), tr("The characters in a grid, by group"),
                                 View::Grid);
    m_listButton = newViewButton(tr("&List"), tr("The characters in a list, with their names"),
                                 View::List);
    updateViewIcons();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &SpecialCharacterDialog::updateViewIcons);
    row->addWidget(views);
    return row;
}

void SpecialCharacterDialog::updateViewIcons()
{
    auto& artProvider = core::ArtProvider::getInstance();
    m_gridButton->setIcon(artProvider.getIcon(QStringLiteral("common.gridView"),
                                              core::IconContext::Button));
    m_listButton->setIcon(artProvider.getIcon(QStringLiteral("common.listView"),
                                              core::IconContext::Button));
}

void SpecialCharacterDialog::createCharacterMenu()
{
    m_menu = new QMenu(this);
    m_insertAction = m_menu->addAction(tr("&Insert"));
    m_menu->setDefaultAction(m_insertAction);
    m_menu->addSeparator();
    m_favoriteAction = m_menu->addAction(tr("Add to &Favorites"));
    m_shortcutAction = m_menu->addAction(tr("Assign &Shortcut..."));
    m_replacementAction = m_menu->addAction(tr("Add &Replacement..."));
    connect(m_insertAction, &QAction::triggered, this, &SpecialCharacterDialog::accept);
    connect(m_favoriteAction, &QAction::triggered, this, &SpecialCharacterDialog::toggleFavorite);
    connect(m_shortcutAction, &QAction::triggered, this,
            [this]() { emit shortcutRequested(m_current); });
    connect(m_replacementAction, &QAction::triggered, this,
            [this]() { emit replacementRequested(m_current); });
}

void SpecialCharacterDialog::showCharacterMenu(QAbstractItemView* view, const QPoint& position)
{
    // The right button chooses the character under it first; the keys open the menu of the
    // character selected
    const QModelIndex index = view->indexAt(position);
    if (index.isValid()) {
        view->setCurrentIndex(index);
    }
    if (m_current == 0) {
        return;
    }
    m_menu->popup(view->viewport()->mapToGlobal(position));
}

CharacterGrid* SpecialCharacterDialog::newCharacterGrid(const QString& accessibleName,
                                                        bool oneRow)
{
    auto* grid = new CharacterGrid(GRID_COLUMNS, oneRow ? 1 : GRID_ROWS, this);
    grid->setAccessibleName(accessibleName);
    if (!oneRow) {
        grid->setMinimumCells(MINIMUM_GRID_COLUMNS, MINIMUM_GRID_ROWS);
    }
    connect(grid, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* item) {
                if (!m_filling && item != nullptr) {
                    onCharacterChosen(CharacterGrid::codeOf(item));
                }
            });
    grid->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(grid, &QWidget::customContextMenuRequested, this,
            [this, grid](const QPoint& position) { showCharacterMenu(grid, position); });
    if (oneRow) {
        // A click inserts a character used last at once; the second click of a double click
        // inserts nothing more
        connect(grid, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            onCharacterChosen(CharacterGrid::codeOf(item));
            if (m_recentInserted.isValid() &&
                m_recentInserted.elapsed() < QApplication::doubleClickInterval()) {
                return;
            }
            m_recentInserted.start();
            accept();
        });
        return grid;
    }
    // A click on the selected character selects it again, after the row of those used last
    connect(grid, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) { onCharacterChosen(CharacterGrid::codeOf(item)); });
    // Enter reaches the dialog's Insert; a double click inserts too
    connect(grid, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        onCharacterChosen(CharacterGrid::codeOf(item));
        accept();
    });
    return grid;
}

QWidget* SpecialCharacterDialog::createGridView()
{
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(SPACING);

    auto* main = new QHBoxLayout();
    main->setSpacing(SPACING);

    // The groups: the favorites and the characters used last, then those of the program
    m_groups = new QListWidget(view);
    m_groups->setAccessibleName(tr("Groups"));
    m_groups->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* favorites = new QListWidgetItem(tr("Favorites"), m_groups);
    favorites->setData(Qt::UserRole, FAVORITES);
    auto* recent = new QListWidgetItem(tr("Recently Used"), m_groups);
    recent->setData(Qt::UserRole, RECENT);
    for (const SpecialCharacterGroup& group : m_characters.groups()) {
        auto* item = new QListWidgetItem(group.name, m_groups);
        item->setData(Qt::UserRole, group.id);
    }
    int widest = 0;
    for (int row = 0; row < m_groups->count(); ++row) {
        widest = std::max(widest,
                          m_groups->fontMetrics().horizontalAdvance(m_groups->item(row)->text()));
    }
    // As wide as the longest name, narrower on a small screen
    m_groups->setMaximumWidth(widest + GROUPS_PADDING + 2 * m_groups->frameWidth());
    m_groups->setMinimumWidth(MINIMUM_GROUPS_WIDTH *
                              m_groups->fontMetrics().horizontalAdvance(QLatin1Char('x')));
    connect(m_groups, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (m_filling || item == nullptr) {
            return;
        }
        m_gridGroup = item->data(Qt::UserRole).toString();
        if (!m_search->text().isEmpty()) {
            m_search->clear();
        }
        const QList<char32_t> characters = charactersOf(m_gridGroup);
        if (!characters.contains(m_current) && !characters.isEmpty()) {
            m_current = characters.first();
        }
        fillCharacters();
    });
    main->addWidget(m_groups, 0);

    auto* gridColumn = new QVBoxLayout();
    gridColumn->setSpacing(SMALL_SPACING);
    m_grid = newCharacterGrid(tr("Characters"), false);
    gridColumn->addWidget(m_grid, 1);
    m_nothingFound = new QLabel(view);
    m_nothingFound->setWordWrap(true);
    m_nothingFound->setTextFormat(Qt::PlainText);
    m_nothingFound->setForegroundRole(QPalette::PlaceholderText);
    m_nothingFound->hide();
    gridColumn->addWidget(m_nothingFound);
    main->addLayout(gridColumn, 1);

    // The character selected, large, with what is known of it
    auto* details = new QWidget(view);
    auto* detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(SMALL_SPACING);
    m_glyph = new QLabel(details);
    m_glyph->setAlignment(Qt::AlignCenter);
    m_glyph->setTextFormat(Qt::PlainText);
    m_glyph->setFrameShape(QFrame::StyledPanel);
    m_glyph->setAutoFillBackground(true);
    m_glyph->setBackgroundRole(QPalette::Base);
    m_glyph->setForegroundRole(QPalette::Text);
    detailsLayout->addWidget(m_glyph);
    m_name = new QLabel(details);
    m_name->setWordWrap(true);
    m_name->setTextFormat(Qt::PlainText);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont nameFont = m_name->font();
    nameFont.setBold(true);
    m_name->setFont(nameFont);
    detailsLayout->addWidget(m_name);
    for (QLabel** label : {&m_code, &m_keys, &m_autoCorrect}) {
        *label = new QLabel(details);
        (*label)->setWordWrap(true);
        (*label)->setTextFormat(label == &m_autoCorrect ? Qt::RichText : Qt::PlainText);
        (*label)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        detailsLayout->addWidget(*label);
    }
    m_fontNote = new QLabel(details);
    m_fontNote->setWordWrap(true);
    m_fontNote->setTextFormat(Qt::PlainText);
    m_fontNote->setForegroundRole(QPalette::PlaceholderText);
    detailsLayout->addWidget(m_fontNote);

    m_favoriteButton = new QPushButton(tr("Add to &Favorites"), details);
    m_shortcutButton = new QPushButton(tr("Assign &Shortcut..."), details);
    m_replacementButton = new QPushButton(tr("Add &Replacement..."), details);
    for (QPushButton* button : {m_favoriteButton, m_shortcutButton, m_replacementButton}) {
        button->setAutoDefault(false);
        detailsLayout->addWidget(button);
    }
    m_shortcutButton->setToolTip(tr("Give the character keys that insert it"));
    m_replacementButton->setToolTip(
        tr("Add a text that auto-correct replaces with the character as you type"));
    connect(m_favoriteButton, &QPushButton::clicked, this,
            &SpecialCharacterDialog::toggleFavorite);
    connect(m_shortcutButton, &QPushButton::clicked, this,
            [this]() { emit shortcutRequested(m_current); });
    connect(m_replacementButton, &QPushButton::clicked, this,
            [this]() { emit replacementRequested(m_current); });
    detailsLayout->addStretch(1);
    const int letter = details->fontMetrics().horizontalAdvance(QLatin1Char('x'));
    details->setFixedWidth(DETAILS_WIDTH * letter + 2 * SPACING);
    main->addWidget(details, 0);
    layout->addLayout(main, 1);

    // The characters used last, the last first, in a row beside their label
    auto* recentRow = new QHBoxLayout();
    recentRow->setSpacing(SPACING);
    auto* recentLabel = new QLabel(tr("Recently &used"), view);
    m_recent = newCharacterGrid(tr("Recently used"), true);
    recentLabel->setBuddy(m_recent);
    m_noRecent = new QLabel(tr("The characters you insert appear here."), view);
    m_noRecent->setForegroundRole(QPalette::PlaceholderText);
    recentRow->addWidget(recentLabel);
    recentRow->addWidget(m_recent, 1);
    recentRow->addWidget(m_noRecent, 1);
    layout->addLayout(recentRow);

    setTabOrder(m_listButton, m_groups);
    setTabOrder(m_groups, m_grid);
    setTabOrder(m_grid, m_favoriteButton);
    setTabOrder(m_favoriteButton, m_shortcutButton);
    setTabOrder(m_shortcutButton, m_replacementButton);
    setTabOrder(m_replacementButton, m_recent);
    return view;
}

QWidget* SpecialCharacterDialog::createListView()
{
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(SPACING);

    // The groups as buttons, in lines as many as the width needs: the width of the window
    // comes from the list
    auto* groups = new QWidget(view);
    groups->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    groups->setAccessibleName(tr("Groups"));
    auto* groupsLayout = new FlowLayout(groups, SMALL_SPACING, SMALL_SPACING);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    // One pressed, none while the search shows what it found (the window presses them)
    m_groupButtons = new QButtonGroup(this);
    m_groupButtons->setExclusive(false);
    const auto addGroupButton = [this, groups, groupsLayout](const QString& id,
                                                             const QString& name) {
        auto* button = new QToolButton(groups);
        button->setText(name);
        button->setCheckable(true);
        button->setAutoRaise(false);
        button->setProperty("groupId", id);
        button->installEventFilter(this);
        m_groupButtons->addButton(button);
        groupsLayout->addWidget(button);
    };
    addGroupButton(ALL, tr("All"));
    addGroupButton(FAVORITES, tr("Favorites"));
    addGroupButton(RECENT, tr("Recently Used"));
    for (const SpecialCharacterGroup& group : m_characters.groups()) {
        addGroupButton(group.id, group.name);
    }
    connect(m_groupButtons, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        m_listGroup = button->property("groupId").toString();
        if (!m_search->text().isEmpty()) {
            m_search->clear();
        }
        const QList<char32_t> characters = charactersOf(m_listGroup);
        if (!characters.contains(m_current) && !characters.isEmpty()) {
            m_current = characters.first();
        }
        fillCharacters();
    });
    layout->addWidget(groups);

    // The characters with their names, codes, keys and auto-correct, and the star
    m_list = new CharacterList(view);
    m_list->setAccessibleName(tr("Characters"));
    m_list->setColumnCount(COLUMN_COUNT);
    m_list->setHeaderLabels({tr("Character"), tr("Name"), tr("Code"),
                             tr("Shortcut, auto-correct"), QString()});
    m_list->headerItem()->setToolTip(FAVORITE_COLUMN, tr("Favorite"));
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(NAME_COLUMN, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(GLYPH_COLUMN, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(CODE_COLUMN, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(KEYS_COLUMN, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(FAVORITE_COLUMN, QHeaderView::ResizeToContents);
    connect(m_list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (!m_filling && item != nullptr) {
            onCharacterChosen(static_cast<char32_t>(item->data(0, CODE_ROLE).toUInt()));
        }
    });
    connect(m_list, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        onCharacterChosen(static_cast<char32_t>(item->data(0, CODE_ROLE).toUInt()));
        if (column == FAVORITE_COLUMN) {
            toggleFavorite();
        }
    });
    connect(m_list, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int column) {
                if (column == FAVORITE_COLUMN) {
                    return;
                }
                onCharacterChosen(static_cast<char32_t>(item->data(0, CODE_ROLE).toUInt()));
                accept();
            });
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) { showCharacterMenu(m_list, position); });
    layout->addWidget(m_list, 1);
    m_listNothingFound = new QLabel(view);
    m_listNothingFound->setWordWrap(true);
    m_listNothingFound->setTextFormat(Qt::PlainText);
    m_listNothingFound->setForegroundRole(QPalette::PlaceholderText);
    m_listNothingFound->hide();
    layout->addWidget(m_listNothingFound);
    return view;
}

// ============================================================================
// State
// ============================================================================

void SpecialCharacterDialog::setView(View view)
{
    m_view = view;
    m_gridView->setVisible(view == View::Grid);
    m_listView->setVisible(view == View::List);
    m_gridButton->setChecked(view == View::Grid);
    m_listButton->setChecked(view == View::List);
    for (QPushButton* button :
         {m_listFavoriteButton, m_listShortcutButton, m_listReplacementButton}) {
        button->setVisible(view == View::List);
    }
    // The window opens in it next time
    auto& settings = core::SettingsManager::getInstance();
    const std::string name = view == View::Grid ? "grid" : "list";
    if (settings.get<std::string>(CHARACTER_WINDOW_VIEW_SETTING, "grid") != name) {
        settings.set<std::string>(CHARACTER_WINDOW_VIEW_SETTING, name);
    }
    // The same character, in its group when the group of the view has it not
    if (m_current != 0) {
        selectCharacter(m_current);
    } else {
        fillCharacters();
    }
}

void SpecialCharacterDialog::selectCharacter(char32_t code)
{
    m_current = code;
    if (m_search->text().isEmpty() && !shownCharacters().contains(code)) {
        // The group of the character, or all of them
        const QString group = m_characters.character(code).group;
        if (m_view == View::Grid) {
            m_gridGroup = group.isEmpty() ? m_gridGroup : group;
        } else {
            m_listGroup = group.isEmpty() ? ALL : group;
        }
    }
    fillCharacters();
}

void SpecialCharacterDialog::setSearchText(const QString& text)
{
    if (m_search->text() != text) {
        m_search->setText(text);
    }
    fillCharacters();
}

QList<char32_t> SpecialCharacterDialog::charactersOf(const QString& group) const
{
    if (group == FAVORITES) {
        return favoriteCharacters();
    }
    if (group == RECENT) {
        return recentCharacters();
    }
    QList<char32_t> characters;
    for (const SpecialCharacterGroup& each : m_characters.groups()) {
        if (group == ALL || each.id == group) {
            characters.append(each.characters);
        }
    }
    return characters;
}

QList<char32_t> SpecialCharacterDialog::shownCharacters() const
{
    const QString search = m_search->text();
    if (!search.trimmed().isEmpty()) {
        return m_characters.find(search);
    }
    return charactersOf(m_view == View::Grid ? m_gridGroup : m_listGroup);
}

void SpecialCharacterDialog::fillCharacters()
{
    const QList<char32_t> characters = shownCharacters();
    // The character selected stays, where the search or the group shows it
    if (!characters.contains(m_current) && !characters.isEmpty()) {
        m_current = characters.first();
    }
    if (characters.isEmpty() && !m_search->text().trimmed().isEmpty()) {
        m_current = 0;
    }

    const QString search = m_search->text().trimmed();
    const QString nothing =
        tr("Nothing matches “%1”. Type another name, or a code such as U+2014.").arg(search);
    if (m_view == View::Grid) {
        m_filling = true;
        for (int row = 0; row < m_groups->count(); ++row) {
            QListWidgetItem* item = m_groups->item(row);
            if (search.isEmpty() && item->data(Qt::UserRole).toString() == m_gridGroup) {
                m_groups->setCurrentItem(item);
            }
        }
        if (!search.isEmpty()) {
            m_groups->clearSelection();
            m_groups->setCurrentItem(nullptr);
        }
        m_filling = false;
        const auto toolTipOf = [this](char32_t code) { return this->toolTipOf(code); };
        m_grid->setCharacters(characters, m_characters, m_fonts, toolTipOf);
        m_nothingFound->setText(nothing);
        m_nothingFound->setVisible(characters.isEmpty() && !search.isEmpty());
        const QList<char32_t> recent = recentCharacters();
        m_recent->setCharacters(recent, m_characters, m_fonts, toolTipOf);
        m_recent->setVisible(!recent.isEmpty());
        m_noRecent->setVisible(recent.isEmpty());
    } else {
        for (QAbstractButton* button : m_groupButtons->buttons()) {
            button->setChecked(search.isEmpty() &&
                               button->property("groupId").toString() == m_listGroup);
        }
        setGroupButtonFocus(m_groupButtons);
        fillList(characters);
        m_listNothingFound->setText(nothing);
        m_listNothingFound->setVisible(characters.isEmpty() && !search.isEmpty());
    }
    showCurrent();
}

void SpecialCharacterDialog::fillList(const QList<char32_t>& characters)
{
    m_filling = true;
    m_list->clear();
    const QList<char32_t> favorites = favoriteCharacters();
    const QBrush muted = m_list->palette().brush(QPalette::PlaceholderText);
    QList<QTreeWidgetItem*> items;
    items.reserve(characters.size());
    for (const char32_t code : characters) {
        const SpecialCharacter character = m_characters.character(code);
        auto* item = new QTreeWidgetItem();
        item->setData(0, CODE_ROLE, static_cast<uint>(code));
        item->setText(GLYPH_COLUMN, shownCharacterText(character));
        item->setTextAlignment(GLYPH_COLUMN, Qt::AlignCenter);
        item->setFont(GLYPH_COLUMN, character.mark.isEmpty() ? m_fonts.cell() : m_fonts.mark());
        if (!character.mark.isEmpty() || !m_fonts.has(code)) {
            item->setForeground(GLYPH_COLUMN, muted);
        }
        item->setText(NAME_COLUMN, character.name);
        item->setText(CODE_COLUMN, SpecialCharacters::codeText(code));
        QStringList keys;
        if (m_keysOf) {
            const QString shortcut = m_keysOf(code);
            if (!shortcut.isEmpty()) {
                keys.append(shortcut);
            }
        }
        if (m_autoCorrectOf) {
            for (const QString& typed : m_autoCorrectOf(code)) {
                //: In the list of the characters: what auto-correct replaces with one
                keys.append(tr("auto-correct %1").arg(typed));
            }
        }
        item->setText(KEYS_COLUMN, keys.join(QStringLiteral(" · ")));
        const bool favorite = favorites.contains(code);
        item->setText(FAVORITE_COLUMN, favorite ? FAVORITE_STAR : NO_STAR);
        item->setToolTip(FAVORITE_COLUMN, favorite ? tr("A favorite: click to remove it")
                                                   : tr("Click to add it to the favorites"));
        item->setToolTip(GLYPH_COLUMN, toolTipOf(code));
        items.append(item);
    }
    m_list->addTopLevelItems(items);
    m_filling = false;
}

void SpecialCharacterDialog::showCurrent()
{
    // In the view: selected and in sight
    m_filling = true;
    if (m_view == View::Grid) {
        m_grid->selectCharacter(m_current);
        m_recent->clearSelection();
    } else {
        QTreeWidgetItem* selected = nullptr;
        for (int row = 0; row < m_list->topLevelItemCount(); ++row) {
            QTreeWidgetItem* item = m_list->topLevelItem(row);
            if (static_cast<char32_t>(item->data(0, CODE_ROLE).toUInt()) == m_current) {
                selected = item;
                break;
            }
        }
        m_list->setCurrentItem(selected);
        if (selected == nullptr) {
            m_list->clearSelection();
        } else {
            m_list->scrollToItem(selected);
        }
    }
    m_filling = false;

    // The details of the grid view
    const bool any = m_current != 0;
    const SpecialCharacter character = m_characters.character(m_current);
    if (any) {
        const bool mark = !character.mark.isEmpty();
        QFont glyphMark = m_fonts.mark();
        glyphMark.setPointSizeF(font().pointSizeF() * GLYPH_MARK_SCALE);
        m_glyph->setFont(mark ? glyphMark : m_fonts.glyph());
        m_glyph->setForegroundRole(mark || !m_fonts.has(m_current) ? QPalette::PlaceholderText
                                                                   : QPalette::Text);
        m_glyph->setText(shownCharacterText(character));
        m_name->setText(character.name);
        m_code->setText(tr("Code: %1").arg(SpecialCharacters::codeText(m_current)));
        const QString keys = m_keysOf ? m_keysOf(m_current) : QString();
        m_keys->setText(tr("Shortcut: %1").arg(keys));
        m_keys->setVisible(!keys.isEmpty());
        const QStringList typed = m_autoCorrectOf ? m_autoCorrectOf(m_current) : QStringList();
        // In a font of the same widths: "->" stays two characters, where a font of the system
        // draws it as an arrow
        QStringList typedHtml;
        for (const QString& each : typed) {
            typedHtml.append(QStringLiteral("<code>%1</code>").arg(each.toHtmlEscaped()));
        }
        //: %1: what auto-correct replaces with the character, as "(c)" or "--"
        m_autoCorrect->setText(
            tr("Auto-correct: %1").toHtmlEscaped().arg(typedHtml.join(QStringLiteral(", "))));
        m_autoCorrect->setVisible(!typed.isEmpty());
        const bool missing = !mark && !m_fonts.has(m_current);
        //: %1 is the name of a font
        m_fontNote->setText(missing ? tr("The font %1 has no such character: in the text "
                                         "another font shows it.")
                                          .arg(m_target.fontFamily)
                                    : QString());
        m_fontNote->setVisible(missing);
    } else {
        m_glyph->clear();
        m_name->setText(tr("No character"));
        m_code->clear();
        m_keys->hide();
        m_autoCorrect->hide();
        m_fontNote->hide();
    }
    m_code->setVisible(any);

    const QString favoriteText = any && isFavoriteCharacter(m_current)
                                     ? tr("Remove from &Favorites")
                                     : tr("Add to &Favorites");
    for (QPushButton* button : {m_favoriteButton, m_listFavoriteButton}) {
        button->setText(favoriteText);
        button->setEnabled(any);
    }
    m_favoriteAction->setText(favoriteText);
    for (QPushButton* button : {m_shortcutButton, m_replacementButton, m_listShortcutButton,
                                m_listReplacementButton}) {
        button->setEnabled(any);
    }
    acceptButton()->setEnabled(any && m_target.whyNot.isEmpty());
}

void SpecialCharacterDialog::onCharacterChosen(char32_t code)
{
    if (code == 0) {
        return;
    }
    m_current = code;
    showCurrent();
}

void SpecialCharacterDialog::moveSelection(int step)
{
    const QList<char32_t> characters = shownCharacters();
    if (characters.isEmpty()) {
        return;
    }
    const qsizetype index = characters.indexOf(m_current);
    const qsizetype next =
        index < 0 ? 0 : std::clamp<qsizetype>(index + step, 0, characters.size() - 1);
    m_current = characters.at(next);
    showCurrent();
}

void SpecialCharacterDialog::toggleFavorite()
{
    if (m_current == 0) {
        return;
    }
    setFavoriteCharacter(m_current, !isFavoriteCharacter(m_current));
    refresh();
}

void SpecialCharacterDialog::refresh()
{
    // The favorites and the characters used last may show other characters now
    fillCharacters();
}

// ============================================================================
// Where the window inserts, the fonts
// ============================================================================

void SpecialCharacterDialog::updateTarget()
{
    const CharacterTarget target = m_targetOf ? m_targetOf() : CharacterTarget{};
    const bool newFont = target.fontFamily != m_target.fontFamily;
    m_target = target;
    if (newFont) {
        updateFonts();
        fillCharacters();
    }
    //: %1 says why, as "No chapter is open"
    m_whyNot->setText(m_target.whyNot.isEmpty()
                          ? QString()
                          : tr("Insert is greyed out: %1").arg(m_target.whyNot));
    m_whyNot->setVisible(!m_target.whyNot.isEmpty());
    acceptButton()->setToolTip(m_target.whyNot);
    acceptButton()->setEnabled(m_current != 0 && m_target.whyNot.isEmpty());
    m_insertAction->setEnabled(m_target.whyNot.isEmpty());
    m_insertAction->setToolTip(m_target.whyNot);
}

void SpecialCharacterDialog::updateFonts()
{
    m_fonts.setFamily(m_target.fontFamily, font());
    m_grid->setCellSide(m_fonts.cellSide());
    m_recent->setCellSide(m_fonts.cellSide());
    m_glyph->setMinimumHeight(QFontMetrics(m_fonts.glyph()).height() + 2 * SMALL_SPACING);
}

QString SpecialCharacterDialog::toolTipOf(char32_t code) const
{
    const SpecialCharacter character = m_characters.character(code);
    QString tip = QStringLiteral("%1 (%2)").arg(character.name, SpecialCharacters::codeText(code));
    if (character.mark.isEmpty() && !m_fonts.has(code)) {
        tip += QLatin1Char('\n') +
               tr("Not in the font %1: in the text another font shows it.")
                   .arg(m_target.fontFamily);
    }
    return tip;
}

// ============================================================================
// Insert, events
// ============================================================================

void SpecialCharacterDialog::accept()
{
    updateTarget();
    if (m_current == 0 || !m_target.whyNot.isEmpty()) {
        return;
    }
    emit insertRequested(m_current);
    if (!m_keepOpen->isChecked()) {
        KalahariDialog::accept();
    }
}

void SpecialCharacterDialog::showEvent(QShowEvent* event)
{
    updateTarget();
    refresh();
    KalahariDialog::showEvent(event);
    m_search->setFocus(Qt::OtherFocusReason);
    m_search->selectAll();
}

void SpecialCharacterDialog::changeEvent(QEvent* event)
{
    KalahariDialog::changeEvent(event);
    switch (event->type()) {
        case QEvent::ActivationChange:
            if (isActiveWindow()) {
                updateTarget();
            }
            break;
        case QEvent::FontChange:
        case QEvent::PaletteChange:
            if (m_grid != nullptr) {
                updateFonts();
                fillCharacters();
            }
            break;
        default:
            break;
    }
}

bool SpecialCharacterDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return KalahariDialog::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    // Up and Down in the search field move the selection of the characters
    if (watched == m_search && key->modifiers() == Qt::NoModifier) {
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
            moveSelection(key->key() == Qt::Key_Down ? 1 : -1);
            return true;
        }
    }
    // Left and Right on a group button of the list press the next one
    auto* button = qobject_cast<QAbstractButton*>(watched);
    if (button != nullptr && m_groupButtons->buttons().contains(button) &&
        (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right ||
         key->key() == Qt::Key_Home || key->key() == Qt::Key_End)) {
        const QList<QAbstractButton*> buttons = m_groupButtons->buttons();
        qsizetype index = buttons.indexOf(button);
        if (key->key() == Qt::Key_Left) {
            index = std::max<qsizetype>(0, index - 1);
        } else if (key->key() == Qt::Key_Right) {
            index = std::min<qsizetype>(buttons.size() - 1, index + 1);
        } else {
            index = key->key() == Qt::Key_Home ? 0 : buttons.size() - 1;
        }
        QAbstractButton* next = buttons.at(index);
        next->click();
        next->setFocus(Qt::TabFocusReason);
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
