/// @file shortcuts_page.cpp
/// @brief Settings page: Keyboard Shortcuts
///
/// The page keeps the user's keys (m_custom) apart from the program's (m_defaults) and
/// shows what CommandRegistry::resolveShortcuts() makes of them, so the list shows the
/// keys the commands will have after Apply. Every key given here passes ShortcutRules.

#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// What an item of the list is
enum class ItemKind { Menu, Command, FixedGroup, FixedRow, NoMatch };

constexpr int KIND_ROLE = Qt::UserRole;            ///< ItemKind
constexpr int INDEX_ROLE = Qt::UserRole + 1;       ///< The command in m_entries, or the group
constexpr int ROW_ROLE = Qt::UserRole + 2;         ///< The row of the fixed group
constexpr int NAME_ROLE = Qt::UserRole + 3;        ///< The name the list paints before a note
constexpr int NOTE_ROLE = Qt::UserRole + 4;        ///< A muted note after the name
constexpr int NOTE_COLOR_ROLE = Qt::UserRole + 5;  ///< The color of the note
constexpr int LOCK_ROLE = Qt::UserRole + 6;        ///< A lock after the name: fixed keys (QIcon)

constexpr int LIST_MIN_ROWS = 3;          ///< On a small screen the list shrinks to this many rows
constexpr int KEYS_COLUMN_PADDING = 24;   ///< Room around the keys in their column
constexpr double KEYS_COLUMN_SHARE = 0.38;  ///< The keys' column has at least this much of the list
constexpr int FIELD_TEXT_ROOM = 6;        ///< Room a line edit keeps beside its text
constexpr int LOCK_GAP = 4;               ///< Between a name and its lock
constexpr double LOCK_OPACITY = 0.7;      ///< The lock is quieter than the name
constexpr int SKIPPED_SHOWN = 8;          ///< The skipped keys of an import named one by one
constexpr qint64 MAX_FILE_SIZE = 1024 * 1024;  ///< A file of shortcuts is far smaller
constexpr float MESSAGE_TINT = 0.12F;     ///< How much of its color the message's background has

/// The mark of a changed shortcut, before its keys
const QString CHANGED_MARK = QStringLiteral("●");
/// Between a menu and its command, as in "File › Save"
const QString PATH_SEPARATOR = QStringLiteral(" › ");

ItemKind kindOf(const QTreeWidgetItem* item) {
    return static_cast<ItemKind>(item->data(0, KIND_ROLE).toInt());
}

// The key codes as numbers: a key press can have a value Qt::Key does not name
QKeyCombination combinationOf(const KeyboardShortcut& keys) {
    return QKeyCombination::fromCombined(keys.keyCode | static_cast<int>(keys.modifiers.toInt()));
}

KeyboardShortcut shortcutFrom(QKeyCombination keys) {
    return KeyboardShortcut(keys.toCombined() & ~int(Qt::KeyboardModifierMask),
                            keys.keyboardModifiers());
}

/// Keys as the system writes them; empty for none
QString nativeText(const KeyboardShortcut& keys) {
    return keys.isEmpty() ? QString() : ShortcutRules::keysText(combinationOf(keys));
}

/// A text as the search compares it: case folded, without spaces
QString searchForm(const QString& text) {
    QString form = text.toCaseFolded();
    form.remove(QLatin1Char(' '));
    return form;
}

/// Whether a character of a text of keys stands between two key names: "+" between the
/// keys, "," between the keys of a list, the symbols of the modifiers on macOS
bool isKeyNameBoundary(QChar character) {
    return QStringView(u"+,\u2303\u2325\u21E7\u2318").contains(character);
}

/// Whether a name is the whole name of a key ("h", "f1", "home"), not its beginning ("ho")
bool isWholeKeyName(const QString& name) {
    if (name.isEmpty()) {
        return false;
    }
    for (const auto format : {QKeySequence::PortableText, QKeySequence::NativeText}) {
        const QKeySequence keys = QKeySequence::fromString(name, format);
        if (keys.count() == 1 && keys[0].key() != Qt::Key_unknown &&
            keys[0].keyboardModifiers() == Qt::NoModifier &&
            searchForm(keys.toString(format)) == name) {
            return true;
        }
    }
    return false;
}

/// The key name a search ends with: after its last "+" ("+" itself, the key, after "++"),
/// without the symbols of the modifiers on macOS
QString lastKeyName(const QString& query) {
    QString name = query.endsWith(QLatin1String("++")) || query == QLatin1String("+")
                       ? QStringLiteral("+")
                       : query.mid(query.lastIndexOf(QLatin1Char('+')) + 1);
    while (!name.isEmpty() && name.front() != QLatin1Char('+') &&
           isKeyNameBoundary(name.front())) {
        name.remove(0, 1);
    }
    return name;
}

/// Whether a text of keys ("Ctrl+F4", "Ctrl+Home, Ctrl+End") has the search from the
/// beginning of a key name on; with toNameEnd also to the end of a key name, as for a search
/// that ends with a whole key name: "ctrl+h" then finds Ctrl+H and not Ctrl+Home, "f1"
/// Shift+F1 and not F12
bool keysTextHas(const QString& keysText, const QString& query, bool toNameEnd) {
    if (query.isEmpty()) {
        return true;
    }
    const QString text = searchForm(keysText);
    for (qsizetype at = text.indexOf(query); at >= 0; at = text.indexOf(query, at + 1)) {
        const qsizetype end = at + query.size();
        const bool begins =
            at == 0 || isKeyNameBoundary(text.at(at - 1)) || isKeyNameBoundary(query.front());
        const bool ends = !toNameEnd || end == text.size() || isKeyNameBoundary(text.at(end));
        if (begins && ends) {
            return true;
        }
    }
    return false;
}

/// Whether the keys, as the list writes them, as the system does or in the portable form,
/// have the search (see keysTextHas())
bool keysContain(QKeyCombination keys, const QString& query, bool toNameEnd) {
    const QKeySequence sequence(keys);
    return keysTextHas(ShortcutRules::keysText(keys), query, toNameEnd) ||
           keysTextHas(sequence.toString(QKeySequence::NativeText), query, toNameEnd) ||
           keysTextHas(sequence.toString(QKeySequence::PortableText), query, toNameEnd);
}

/// A color between two
QColor mixed(const QColor& base, const QColor& tint, float amount) {
    const auto between = [amount](float from, float to) { return from + (to - from) * amount; };
    return QColor::fromRgbF(between(base.redF(), tint.redF()),
                            between(base.greenF(), tint.greenF()),
                            between(base.blueF(), tint.blueF()));
}

/// The list of the commands. It asks only for its smallest height and takes the room the
/// page leaves it: a scroll area gives a page with wrapped labels its preferred height, so
/// on a small screen the list gives up its rows before the page has to scroll
class CommandList : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    [[nodiscard]] QSize sizeHint() const override {
        return {QTreeWidget::sizeHint().width(), minimumHeight()};
    }

    /// Scrolled by pixels (the style of macOS), Qt leaves the last pixel of a row it brings
    /// up from below under the edge of the list: the row is shown whole. (scrollToItem()
    /// goes past this: the page scrolls with scrollTo().)
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override {
        QTreeWidget::scrollTo(index, hint);
        if (verticalScrollMode() != ScrollPerPixel ||
            (hint != EnsureVisible && hint != PositionAtBottom)) {
            return;
        }
        const QRect row = visualRect(index);
        const int hidden = row.bottom() - (viewport()->height() - 1);
        if (row.isValid() && hidden > 0 && row.top() - hidden >= 0) {
            verticalScrollBar()->setValue(verticalScrollBar()->value() + hidden);
        }
    }

protected:
    /// The rows get less room (what is under the list grows, a scroll bar comes, the header
    /// gets higher): the selected row stays in sight if it was
    void resizeEvent(QResizeEvent* event) override {
        // The size is the rows' room (the viewport's), and the base fits the scroll bar to it
        QTreeWidget::resizeEvent(event);
        const int before = event->oldSize().height();
        QTreeWidgetItem* current = currentItem();
        if (current == nullptr || before <= event->size().height()) {
            return;
        }
        // The rows have not moved: was the row in the room the list had?
        const QRect row = visualItemRect(current);
        if (row.isValid() && row.bottom() >= 0 && row.top() < before) {
            scrollTo(indexFromItem(current));
        }
    }
};

/// Paints a command's name with a muted note after it ("not available yet"), and a group of
/// fixed keys with a lock after its name; every other item as usual
class NameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const QString note = index.data(NOTE_ROLE).toString();
        const QIcon lock = index.data(LOCK_ROLE).value<QIcon>();
        if (index.column() != 0 || (note.isEmpty() && lock.isNull())) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget* widget = item.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        // The row, its selection and its focus, without the text
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        // The text where the style puts it
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        QRect room = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget)
                         .adjusted(margin, 0, -margin, 0);
        const QPalette::ColorGroup colors = !item.state.testFlag(QStyle::State_Enabled)
                                                ? QPalette::Disabled
                                            : item.state.testFlag(QStyle::State_Active)
                                                ? QPalette::Normal
                                                : QPalette::Inactive;
        const bool selected = item.state.testFlag(QStyle::State_Selected);
        const QColor text =
            item.palette.color(colors, selected ? QPalette::HighlightedText : QPalette::Text);
        const QFontMetrics metrics(item.font);
        const int lockSize = metrics.ascent();
        const int lockRoom = lock.isNull() ? 0 : LOCK_GAP + lockSize;

        painter->save();
        painter->setClipRect(room);
        painter->setFont(item.font);
        painter->setPen(text);
        const QString name = metrics.elidedText(index.data(NAME_ROLE).toString(), Qt::ElideRight,
                                                std::max(room.width() - lockRoom, 0));
        painter->drawText(room, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, name);
        room.setLeft(room.left() + metrics.horizontalAdvance(name));

        if (!lock.isNull()) {
            const QRect lockRect(room.left() + LOCK_GAP, room.center().y() - lockSize / 2 + 1,
                                 lockSize, lockSize);
            painter->setOpacity(LOCK_OPACITY);
            lock.paint(painter, lockRect, Qt::AlignCenter,
                       selected ? QIcon::Selected : QIcon::Normal);
            painter->setOpacity(1.0);
        }
        if (!note.isEmpty() && room.width() > 0) {
            QFont noteFont = item.font;
            noteFont.setBold(false);
            const QColor noteColor = index.data(NOTE_COLOR_ROLE).value<QColor>();
            painter->setFont(noteFont);
            painter->setPen(selected || !noteColor.isValid() ? text : noteColor);
            const QString shown = QFontMetrics(noteFont).elidedText(
                QStringLiteral(" (%1)").arg(note), Qt::ElideRight, room.width());
            painter->drawText(room, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, shown);
        }
        painter->restore();
    }
};

/// The width a line edit needs to show a text of a width whole
int lineEditWidth(const QLineEdit* field, int textWidth) {
    QStyleOptionFrame option;
    option.initFrom(field);
    option.lineWidth = field->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, field);
    const QMargins textMargins = field->textMargins();
    const QSize text(textWidth + FIELD_TEXT_ROOM + textMargins.left() + textMargins.right(),
                     field->sizeHint().height());
    return field->style()->sizeFromContents(QStyle::CT_LineEdit, &option, text, field).width();
}

/// Whether the item or its group is hidden by the search
bool isHiddenInList(const QTreeWidgetItem* item) {
    return item->isHidden() || (item->parent() != nullptr && item->parent()->isHidden());
}

/// A part of a menu: a submenu with its items, or a command
struct MenuNode {
    std::string name;                  ///< A submenu's part of Command::menuPath
    const Command* command = nullptr;  ///< A command
    std::vector<MenuNode> children;    ///< A submenu's items, in the menu's order
};

/// Put a command in its submenu; a submenu stands where its first command is, as in the menu
void addToMenu(MenuNode& menu, const std::vector<std::string>& submenus, size_t level,
               const Command* command) {
    if (level == submenus.size()) {
        menu.children.push_back({{}, command, {}});
        return;
    }
    for (MenuNode& child : menu.children) {
        if (child.command == nullptr && child.name == submenus[level]) {
            addToMenu(child, submenus, level + 1, command);
            return;
        }
    }
    menu.children.push_back({submenus[level], nullptr, {}});
    addToMenu(menu.children.back(), submenus, level + 1, command);
}

/// The parts of a menu path ("FILE/Open/Open File..." → FILE, Open, Open File...)
std::vector<std::string> pathParts(const std::string& menuPath) {
    std::vector<std::string> parts;
    std::istringstream stream(menuPath);
    std::string part;
    while (std::getline(stream, part, '/')) {
        parts.push_back(part);
    }
    return parts;
}

}  // namespace

// ============================================================================
// Construction
// ============================================================================

ShortcutsPage::ShortcutsPage(QWidget* parent)
    : SettingsPage(parent)
    , m_fixedGroups(m_rules.fixedGroups())
    , m_defaults(CommandRegistry::getInstance().defaultShortcuts())
{
    createSearch();
    createList();
    createDetail();
    createButtons();
    buildList();

    // The keys go through the page as they are seen: the search, the list, the selected
    // command and its message, the buttons under it
    setTabOrder(m_search, m_byKeysButton);
    setTabOrder(m_byKeysButton, m_onlyChanged);
    setTabOrder(m_onlyChanged, m_list);
    setTabOrder(m_list, m_changeButton);
    setTabOrder(m_changeButton, m_removeButton);
    setTabOrder(m_removeButton, m_restoreButton);
    setTabOrder(m_restoreButton, m_messageAcceptButton);
    setTabOrder(m_messageAcceptButton, m_messageCancelButton);
    setTabOrder(m_messageCancelButton, m_restoreAllButton);
    setTabOrder(m_restoreAllButton, m_importButton);
    setTabOrder(m_importButton, m_exportButton);

    // Apply and OK write only the keys the user changed
    auto& settings = core::SettingsManager::getInstance();
    Binding binding;
    binding.key = []() { return std::string(SHORTCUTS_SETTING); };
    binding.stored = [&settings]() { return settings.get<nlohmann::json>(SHORTCUTS_SETTING); };
    binding.shown = [this]() { return shortcutsToJson(m_custom); };
    binding.show = [this](const nlohmann::json& value) {
        // As the program reads them at startup: without the keys no command can have
        m_custom = withoutDefaults(validShortcuts(shortcutsFromJson(value), m_rules));
        hideMessage();
        refresh();
    };
    binding.store = [&settings](const nlohmann::json& value) {
        settings.set<nlohmann::json>(SHORTCUTS_SETTING, value);
    };
    bindCustom(std::move(binding));

    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &ShortcutsPage::updateIcons);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            [this]() {
                updateColors();
                refresh();
            });
    updateIcons();
    updateColors();
    refresh();
}

void ShortcutsPage::createSearch() {
    // The options beside the field, or under it on a narrow page (arrangeSearchRow)
    m_searchRow = new QGridLayout();
    m_searchRow->setColumnStretch(1, 1);
    m_searchLabel = new QLabel(tr("&Search:"));
    m_search = new ShortcutRecorder();
    m_search->setObjectName(QStringLiteral("shortcutsSearch"));
    m_search->setPlaceholderText(textSearchPrompt());
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    m_searchLabel->setBuddy(m_search);

    m_byKeysButton = new QPushButton(tr("Search by &Keys"));
    m_byKeysButton->setObjectName(QStringLiteral("shortcutsByKeys"));
    m_byKeysButton->setCheckable(true);
    m_byKeysButton->setToolTip(tr("Press a shortcut to see what it does"));

    m_onlyChanged = new QCheckBox(tr("&Only Changed"));
    m_onlyChanged->setObjectName(QStringLiteral("shortcutsOnlyChanged"));
    m_onlyChanged->setToolTip(tr("Show only the shortcuts you changed"));

    m_searchOptions = new QWidget();
    auto* options = new QHBoxLayout(m_searchOptions);
    options->setContentsMargins(0, 0, 0, 0);
    options->addWidget(m_byKeysButton);
    options->addWidget(m_onlyChanged);
    m_searchRow->addWidget(m_searchLabel, 0, 0);
    m_searchRow->addWidget(m_search, 0, 1);
    m_searchRow->addWidget(m_searchOptions, 0, 2);
    pageLayout()->addLayout(m_searchRow);

    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        if (!m_keySearch) {
            applyFilter();
        }
    });
    connect(m_search, &ShortcutRecorder::recorded, this, [this](QKeyCombination keys) {
        m_searchKeys = keys;
        applyFilter();
    });
    connect(m_search, &ShortcutRecorder::recordingStopped, this, [this](bool escape) {
        if (escape) {
            setKeySearch(false);
            return;
        }
        // The field was left: the keys pressed last stay searched for, and the field
        // records again when it gets the keys back
        m_search->setReadOnly(true);
        if (!m_searchKeys) {
            m_search->setPlaceholderText(keySearchPrompt());
        }
    });
    connect(m_byKeysButton, &QPushButton::toggled, this, &ShortcutsPage::setKeySearch);
    connect(m_onlyChanged, &QCheckBox::toggled, this, &ShortcutsPage::applyFilter);
}

void ShortcutsPage::createList() {
    m_list = new CommandList();
    m_list->setObjectName(QStringLiteral("shortcutsList"));
    m_list->setColumnCount(2);
    m_list->setHeaderLabels({tr("Command"), tr("Shortcut")});
    m_list->setAccessibleName(tr("Commands and their shortcuts"));
    m_list->setItemDelegateForColumn(0, new NameDelegate(m_list));
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    // A double click changes a command's keys (a group's opens or closes it: itemDoubleClicked)
    m_list->setExpandsOnDoubleClick(false);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    // On a small screen the list gives up its rows first (CommandList); the page scrolls
    // only below this
    const int rowHeight = m_list->fontMetrics().height() + 4;
    m_list->setMinimumHeight(m_list->header()->sizeHint().height() + LIST_MIN_ROWS * rowHeight +
                             2 * m_list->frameWidth());
    m_list->installEventFilter(this);
    // A new width gives the keys' column its share again (fitKeysColumn)
    m_list->viewport()->installEventFilter(this);
    pageLayout()->addWidget(m_list, 1);

    connect(m_list, &QTreeWidget::currentItemChanged, this, [this]() {
        hideMessage();
        showDetail();
    });
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (entryOf(item) != nullptr) {
            startChange(m_list);
        } else if (item->childCount() > 0) {
            item->setExpanded(!item->isExpanded());
        }
    });
}

void ShortcutsPage::createDetail() {
    auto* detail = new QGroupBox();
    detail->setObjectName(QStringLiteral("shortcutDetail"));
    m_detail = detail;
    auto* layout = new QVBoxLayout(detail);

    m_detailTitle = new QLabel();
    m_detailTitle->setObjectName(QStringLiteral("shortcutTitle"));
    m_detailTitle->setTextFormat(Qt::PlainText);
    m_detailTitle->setWordWrap(true);
    QFont titleFont = m_detailTitle->font();
    titleFont.setBold(true);
    m_detailTitle->setFont(titleFont);
    layout->addWidget(m_detailTitle);

    // The keys and the buttons that change them, beside the field or under it
    // (arrangeKeysRow)
    m_keysRow = new QGridLayout();
    m_keysRow->setColumnStretch(1, 1);
    m_keysLabel = new QLabel(tr("Shortcut:"));
    m_keysField = new ShortcutRecorder();
    m_keysField->setObjectName(QStringLiteral("shortcutKeys"));
    m_keysField->setReadOnly(true);
    // Shows the keys; Change... (or a click) records new ones into it
    m_keysField->setFocusPolicy(Qt::NoFocus);
    m_keysField->setAccessibleName(tr("Shortcut"));
    m_keysField->installEventFilter(this);
    m_changeButton = new QPushButton(tr("&Change..."));
    m_changeButton->setToolTip(tr("Press the new shortcut of the command"));
    m_removeButton = new QPushButton(tr("&Remove"));
    m_removeButton->setToolTip(tr("Leave the command without a shortcut"));
    m_restoreButton = new QPushButton(tr("Restore &Default"));
    m_restoreButton->setToolTip(tr("Give the command its default shortcut"));
    m_keyButtons = new QWidget();
    auto* buttonsLayout = new QHBoxLayout(m_keyButtons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addWidget(m_changeButton);
    buttonsLayout->addWidget(m_removeButton);
    buttonsLayout->addWidget(m_restoreButton);
    m_keysRow->addWidget(m_keysLabel, 0, 0);
    m_keysRow->addWidget(m_keysField, 0, 1);
    m_keysRow->addWidget(m_keyButtons, 0, 2);
    layout->addLayout(m_keysRow);

    m_detailInfo = addNote(layout, QString());
    m_detailInfo->setObjectName(QStringLiteral("shortcutInfo"));
    m_detailInfo->setTextFormat(Qt::PlainText);

    // The message about the keys pressed, and the buttons that go on when it asks
    m_message = new QFrame();
    m_message->setObjectName(QStringLiteral("shortcutMessage"));
    auto* messageLayout = new QVBoxLayout(m_message);
    messageLayout->setContentsMargins(8, 6, 8, 6);
    m_messageText = new QLabel();
    m_messageText->setObjectName(QStringLiteral("shortcutMessageText"));
    m_messageText->setTextFormat(Qt::RichText);
    m_messageText->setWordWrap(true);
    messageLayout->addWidget(m_messageText);
    auto* messageButtons = new QHBoxLayout();
    m_messageAcceptButton = new QPushButton();
    m_messageAcceptButton->setObjectName(QStringLiteral("shortcutMessageAccept"));
    m_messageCancelButton = new QPushButton(tr("Cancel"));
    m_messageCancelButton->setObjectName(QStringLiteral("shortcutMessageCancel"));
    // Esc on them closes the message, not the window
    m_messageAcceptButton->installEventFilter(this);
    m_messageCancelButton->installEventFilter(this);
    messageButtons->addWidget(m_messageAcceptButton);
    messageButtons->addWidget(m_messageCancelButton);
    messageButtons->addStretch();
    messageLayout->addLayout(messageButtons);
    m_message->hide();
    layout->addWidget(m_message);

    pageLayout()->addWidget(detail);
    // A new width may move the buttons of the keys
    detail->installEventFilter(this);

    connect(m_keysField, &ShortcutRecorder::recorded, this, &ShortcutsPage::assignKeys);
    connect(m_keysField, &ShortcutRecorder::recordingStopped, this, [this](bool escape) {
        showDetail();
        if (escape) {
            returnFocus();
        }
    });
    connect(m_changeButton, &QPushButton::clicked, this,
            [this]() { startChange(m_changeButton); });
    connect(m_removeButton, &QPushButton::clicked, this,
            [this]() { removeKeys(m_changeButton); });
    connect(m_restoreButton, &QPushButton::clicked, this, &ShortcutsPage::restoreDefault);
    connect(m_messageAcceptButton, &QPushButton::clicked, this, [this]() {
        // What the button does may show the next message
        std::function<void()> accept = std::move(m_messageAccept);
        m_messageAccept = nullptr;
        if (accept) {
            accept();
        }
    });
    connect(m_messageCancelButton, &QPushButton::clicked, this, [this]() {
        hideMessage();
        returnFocus();
    });
}

void ShortcutsPage::createButtons() {
    auto* row = new QHBoxLayout();
    m_restoreAllButton = new QPushButton(tr("Restore &All Defaults"));
    m_restoreAllButton->setToolTip(tr("Give every command its default shortcut"));
    m_importButton = new QPushButton(tr("&Import..."));
    m_importButton->setToolTip(tr("Read shortcuts from a file in place of your changes"));
    m_exportButton = new QPushButton(tr("&Export..."));
    m_exportButton->setToolTip(
        tr("Save your changed shortcuts to a file, e.g. for another computer"));
    row->addWidget(m_restoreAllButton);
    row->addStretch();
    row->addWidget(m_importButton);
    row->addWidget(m_exportButton);
    pageLayout()->addLayout(row);

    connect(m_restoreAllButton, &QPushButton::clicked, this, &ShortcutsPage::restoreAllDefaults);
    connect(m_importButton, &QPushButton::clicked, this, &ShortcutsPage::chooseImportFile);
    connect(m_exportButton, &QPushButton::clicked, this, &ShortcutsPage::chooseExportFile);
}

void ShortcutsPage::buildList() {
    const std::vector<Command> commands = CommandRegistry::getInstance().getAllCommands();
    QFont groupFont = m_list->font();
    groupFont.setBold(true);

    // The commands the menus show and the program runs (or keeps keys for, as F1 for the
    // help to come), by their menu
    std::map<std::string, std::vector<const Command*>> byMenu;
    for (const Command& command : commands) {
        if (!command.showInMenu || command.menuPath.empty() || !command.isShortcutCustomizable) {
            continue;
        }
        if (!command.canExecute() && defaultOf(command.id).isEmpty()) {
            continue;
        }
        byMenu[command.menuPath.substr(0, command.menuPath.find('/'))].push_back(&command);
    }

    for (const std::string& top : MenuBuilder::topLevelMenus()) {
        const auto found = byMenu.find(top);
        if (found == byMenu.end()) {
            continue;
        }
        std::vector<const Command*> menuCommands = found->second;
        std::stable_sort(menuCommands.begin(), menuCommands.end(),
                         [](const Command* a, const Command* b) {
                             return a->menuOrder != b->menuOrder ? a->menuOrder < b->menuOrder
                                                                 : a->id < b->id;
                         });
        MenuNode menu;
        for (const Command* command : menuCommands) {
            // The path without the menu and the command's own name: the submenus
            const std::vector<std::string> parts = pathParts(command->menuPath);
            std::vector<std::string> submenus;
            if (parts.size() > 2) {
                submenus.assign(parts.begin() + 1, parts.end() - 1);
            }
            addToMenu(menu, submenus, 0, command);
        }

        const QString menuTitle = MenuBuilder::menuTitle(top);
        auto* group = new QTreeWidgetItem(m_list);
        group->setText(0, menuTitle);
        group->setData(0, KIND_ROLE, static_cast<int>(ItemKind::Menu));
        group->setFirstColumnSpanned(true);
        group->setFont(0, groupFont);

        // Depth first: the items of a submenu where the submenu is
        std::function<void(const MenuNode&, const QString&)> addItems =
            [&](const MenuNode& node, const QString& prefix) {
                for (const MenuNode& child : node.children) {
                    if (child.command == nullptr) {
                        addItems(child,
                                 prefix + MenuBuilder::menuTitle(child.name) + PATH_SEPARATOR);
                        continue;
                    }
                    Entry entry;
                    entry.id = child.command->id;
                    entry.menu = menuTitle;
                    entry.label = prefix + QString::fromStdString(child.command->label);
                    entry.available = child.command->canExecute();
                    entry.item = new QTreeWidgetItem(group);
                    entry.item->setText(0, entry.label);
                    if (!entry.available) {
                        // Muted after the name (NameDelegate); a screen reader reads both
                        const QString note =
                            child.command->unavailableNote.empty()
                                ? tr("in preparation")
                                : QString::fromStdString(child.command->unavailableNote);
                        entry.item->setText(0, QStringLiteral("%1 (%2)").arg(entry.label, note));
                        entry.item->setData(0, NAME_ROLE, entry.label);
                        entry.item->setData(0, NOTE_ROLE, note);
                    }
                    entry.item->setData(0, KIND_ROLE, static_cast<int>(ItemKind::Command));
                    entry.item->setData(0, INDEX_ROLE, static_cast<int>(m_entries.size()));
                    m_entries.push_back(std::move(entry));
                }
            };
        addItems(menu, QString());
    }

    // The keys no command can have, last and muted (updateColors), the groups with a lock
    // (updateIcons): so it is seen what is taken
    for (size_t groupIndex = 0; groupIndex < m_fixedGroups.size(); ++groupIndex) {
        const FixedKeyGroup& fixedGroup = m_fixedGroups[groupIndex];
        auto* group = new QTreeWidgetItem(m_list);
        group->setText(0, fixedGroup.title);
        group->setData(0, NAME_ROLE, fixedGroup.title);
        group->setData(0, KIND_ROLE, static_cast<int>(ItemKind::FixedGroup));
        group->setData(0, INDEX_ROLE, static_cast<int>(groupIndex));
        group->setFirstColumnSpanned(true);
        group->setFont(0, groupFont);
        group->setToolTip(0, explanationOf(fixedGroup, nullptr));
        for (size_t rowIndex = 0; rowIndex < fixedGroup.rows.size(); ++rowIndex) {
            const FixedKeys& fixed = fixedGroup.rows[rowIndex];
            auto* item = new QTreeWidgetItem(group);
            item->setText(0, fixed.label);
            item->setText(1, fixed.keysText());
            item->setToolTip(1, fixed.keysText());
            item->setData(0, KIND_ROLE, static_cast<int>(ItemKind::FixedRow));
            item->setData(0, INDEX_ROLE, static_cast<int>(groupIndex));
            item->setData(0, ROW_ROLE, static_cast<int>(rowIndex));
        }
    }

    // What the list says when the search leaves nothing in it
    m_noMatchItem = new QTreeWidgetItem(m_list);
    m_noMatchItem->setData(0, KIND_ROLE, static_cast<int>(ItemKind::NoMatch));
    m_noMatchItem->setFlags(Qt::ItemIsEnabled);
    m_noMatchItem->setFirstColumnSpanned(true);
    m_noMatchItem->setHidden(true);

    m_list->expandAll();
}

// ============================================================================
// Public
// ============================================================================

bool ShortcutsPage::selectCommand(const std::string& commandId) {
    const Entry* entry = entryById(commandId);
    if (entry == nullptr) {
        return false;
    }
    if (isHiddenInList(entry->item)) {
        setKeySearch(false);
        m_search->clear();
        m_onlyChanged->setChecked(false);
    }
    entry->item->parent()->setExpanded(true);
    m_list->setCurrentItem(entry->item);
    m_list->scrollTo(m_list->indexFromItem(entry->item));
    return true;
}

void ShortcutsPage::assignKeys(QKeyCombination pressed) {
    const Entry* entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    const std::string id = entry->id;
    const KeyboardShortcut keys = shortcutFrom(pressed);
    hideMessage();

    if (shortcutOf(id) == keys) {
        refresh();
        returnFocus();
        return;
    }

    KeyCheck check = m_rules.check(pressed, id);
    if (check.result == KeyCheck::Result::Refused) {
        refresh();
        showMessage(MessageKind::Error, check.reason.toHtmlEscaped());
        returnFocus();
        return;
    }
    // The program's own keys of the command come back as Restore Default brings them
    if (keys == defaultOf(id)) {
        check = {};
    }

    const std::string owner = ownerOf(keys, id);
    if (owner.empty() && check.result == KeyCheck::Result::Allowed) {
        setKeys(id, keys);
        refresh();
        returnFocus();
        return;
    }

    // Keys another command has, or keys some desktops take: the user decides
    QStringList lines;
    if (!owner.empty()) {
        lines.append(tr("%1 already belongs to <b>%2</b>: if you assign it here, that command "
                        "will be left without a shortcut.")
                         .arg(nativeText(keys).toHtmlEscaped(), titleOf(owner).toHtmlEscaped()));
    }
    if (check.result == KeyCheck::Result::Warning) {
        lines.append(check.reason.toHtmlEscaped());
    }
    refresh();
    showMessage(MessageKind::Warning, lines.join(QStringLiteral("<br>")), tr("Assi&gn Anyway"),
                [this, id, keys, owner]() {
                    if (!owner.empty()) {
                        setKeys(owner, KeyboardShortcut());
                    }
                    setKeys(id, keys);
                    refresh();
                    if (!owner.empty()) {
                        showMessage(MessageKind::Information,
                                    tr("%1 now has no shortcut.")
                                        .arg(titleOf(owner).toHtmlEscaped()));
                    } else {
                        hideMessage();
                    }
                    returnFocus();
                });
}

KeyboardShortcut ShortcutsPage::shortcutOf(const std::string& commandId) const {
    const CommandRegistry::ShortcutMap keysNow = resolved();
    const auto found = keysNow.find(commandId);
    return found != keysNow.end() ? found->second : KeyboardShortcut();
}

bool ShortcutsPage::exportShortcuts(const QString& path) {
    hideMessage();
    const QString name = QDir::toNativeSeparators(path);
    const QByteArray contents =
        QByteArray::fromStdString(shortcutsFile(m_custom).dump(2) + "\n");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() ||
        !file.commit()) {
        core::Logger::getInstance().warn("Keyboard shortcuts: could not export to {}: {}",
                                         path.toStdString(), file.errorString().toStdString());
        showMessage(MessageKind::Error, tr("The shortcuts could not be saved to %1: %2")
                                            .arg(name.toHtmlEscaped(),
                                                 file.errorString().toHtmlEscaped()));
        return false;
    }
    const int count = static_cast<int>(m_custom.size());
    core::Logger::getInstance().info("Keyboard shortcuts: {} exported to {}", count,
                                     path.toStdString());
    showMessage(MessageKind::Information,
                tr("Saved %n changed shortcut(s) to the file %1.", nullptr, count)
                    .arg(name.toHtmlEscaped()));
    return true;
}

bool ShortcutsPage::importShortcuts(const QString& path) {
    hideMessage();
    const QString name = QDir::toNativeSeparators(path);
    const QString notShortcuts = tr("%1 is not a file of Kalahari keyboard shortcuts.")
                                     .arg(name.toHtmlEscaped());

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showMessage(MessageKind::Error, tr("%1 could not be read: %2")
                                            .arg(name.toHtmlEscaped(),
                                                 file.errorString().toHtmlEscaped()));
        return false;
    }
    if (file.size() > MAX_FILE_SIZE) {
        showMessage(MessageKind::Error, notShortcuts);
        return false;
    }
    const QByteArray contents = file.readAll();
    const nlohmann::json json =
        nlohmann::json::parse(contents.constBegin(), contents.constEnd(), nullptr, false);
    const std::optional<nlohmann::json> object =
        json.is_discarded() ? std::nullopt : shortcutsOfFile(json);
    if (!object) {
        showMessage(MessageKind::Error, notShortcuts);
        return false;
    }

    // Every key as if it was pressed here, but without questions: what cannot be is skipped,
    // each with why in a few words
    QStringList unreadable;
    const CommandRegistry::ShortcutMap fromFile = shortcutsFromJson(*object, &unreadable);
    QStringList skipped;
    for (const QString& id : unreadable) {
        skipped.append(tr("“%1” (the keys cannot be read)").arg(labelOf(id.toStdString())));
    }
    CommandRegistry::ShortcutMap imported;
    std::map<KeyboardShortcut, std::string> owners;
    int read = 0;
    for (const auto& [id, keys] : fromFile) {
        if (entryById(id) == nullptr) {
            skipped.append(
                tr("“%1” (Kalahari has no such command)").arg(QString::fromStdString(id)));
            continue;
        }
        if (!keys.isEmpty()) {
            const KeyCheck check = m_rules.check(combinationOf(keys), id);
            if (check.result == KeyCheck::Result::Refused) {
                skipped.append(
                    tr("%1 for “%2” (%3)").arg(nativeText(keys), labelOf(id), check.brief));
                continue;
            }
            const auto [owner, first] = owners.emplace(keys, id);
            if (!first) {
                skipped.append(tr("%1 for “%2” (the file gives it to “%3” too)")
                                   .arg(nativeText(keys), labelOf(id), labelOf(owner->second)));
                continue;
            }
        }
        imported[id] = keys;
        ++read;
    }
    m_custom = withoutDefaults(imported);
    refresh();

    core::Logger::getInstance().info("Keyboard shortcuts: {} imported from {}, {} skipped", read,
                                     path.toStdString(), skipped.size());
    QString text =
        tr("Read %n shortcut(s) from the file %1.", nullptr, read).arg(name.toHtmlEscaped());
    if (!skipped.isEmpty()) {
        QStringList shown;
        for (const QString& line : skipped.mid(0, SKIPPED_SHOWN)) {
            shown.append(line.toHtmlEscaped());
        }
        if (skipped.size() > SKIPPED_SHOWN) {
            shown.append(
                tr("and %n more", nullptr, static_cast<int>(skipped.size()) - SKIPPED_SHOWN));
        }
        text += QLatin1Char(' ') + tr("Skipped %n: %1.", nullptr, static_cast<int>(skipped.size()))
                                       .arg(shown.join(QStringLiteral("; ")));
    }
    text += QLatin1Char(' ') + tr("Apply or OK will save the changes.");
    showMessage(MessageKind::Information, text);
    return true;
}

// ============================================================================
// Keys
// ============================================================================

const ShortcutsPage::Entry* ShortcutsPage::entryOf(const QTreeWidgetItem* item) const {
    if (item == nullptr || kindOf(item) != ItemKind::Command) {
        return nullptr;
    }
    const int index = item->data(0, INDEX_ROLE).toInt();
    return index >= 0 && index < static_cast<int>(m_entries.size()) ? &m_entries[index] : nullptr;
}

const ShortcutsPage::Entry* ShortcutsPage::currentEntry() const {
    return entryOf(m_list->currentItem());
}

const ShortcutsPage::Entry* ShortcutsPage::entryById(const std::string& id) const {
    const auto found = std::find_if(m_entries.begin(), m_entries.end(),
                                    [&id](const Entry& entry) { return entry.id == id; });
    return found != m_entries.end() ? &*found : nullptr;
}

const FixedKeys* ShortcutsPage::fixedKeysOf(const QTreeWidgetItem* item,
                                            const FixedKeyGroup** group) const {
    if (group != nullptr) {
        *group = nullptr;
    }
    if (item == nullptr) {
        return nullptr;
    }
    const ItemKind kind = kindOf(item);
    if (kind != ItemKind::FixedGroup && kind != ItemKind::FixedRow) {
        return nullptr;
    }
    const int groupIndex = item->data(0, INDEX_ROLE).toInt();
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_fixedGroups.size())) {
        return nullptr;
    }
    const FixedKeyGroup& fixedGroup = m_fixedGroups[groupIndex];
    if (group != nullptr) {
        *group = &fixedGroup;
    }
    if (kind == ItemKind::FixedGroup) {
        return nullptr;
    }
    const int rowIndex = item->data(0, ROW_ROLE).toInt();
    return rowIndex >= 0 && rowIndex < static_cast<int>(fixedGroup.rows.size())
               ? &fixedGroup.rows[rowIndex]
               : nullptr;
}

QString ShortcutsPage::titleOf(const std::string& commandId) const {
    if (const Entry* entry = entryById(commandId)) {
        return entry->menu + PATH_SEPARATOR + entry->label;
    }
    if (const Command* command = CommandRegistry::getInstance().getCommand(commandId)) {
        return QString::fromStdString(command->label);
    }
    return QString::fromStdString(commandId);
}

QString ShortcutsPage::labelOf(const std::string& commandId) const {
    if (const Entry* entry = entryById(commandId)) {
        return entry->label;
    }
    return titleOf(commandId);
}

KeyboardShortcut ShortcutsPage::defaultOf(const std::string& commandId) const {
    const auto found = m_defaults.find(commandId);
    return found != m_defaults.end() ? found->second : KeyboardShortcut();
}

CommandRegistry::ShortcutMap ShortcutsPage::resolved() const {
    return CommandRegistry::resolveShortcuts(m_defaults, m_custom);
}

std::string ShortcutsPage::ownerOf(const KeyboardShortcut& keys,
                                   const std::string& exceptId) const {
    if (keys.isEmpty()) {
        return {};
    }
    for (const auto& [id, commandKeys] : resolved()) {
        if (id != exceptId && commandKeys == keys) {
            return id;
        }
    }
    return {};
}

CommandRegistry::ShortcutMap ShortcutsPage::withoutDefaults(
    const CommandRegistry::ShortcutMap& custom) const {
    CommandRegistry::ShortcutMap changed;
    for (const auto& [id, keys] : custom) {
        const auto programKeys = m_defaults.find(id);
        if (programKeys == m_defaults.end() || programKeys->second != keys) {
            changed[id] = keys;
        }
    }
    return changed;
}

void ShortcutsPage::setKeys(const std::string& commandId, const KeyboardShortcut& keys) {
    const auto programKeys = m_defaults.find(commandId);
    if (programKeys != m_defaults.end() && programKeys->second == keys) {
        m_custom.erase(commandId);
    } else {
        m_custom[commandId] = keys;
    }
}

// ============================================================================
// The list
// ============================================================================

void ShortcutsPage::refresh() {
    const CommandRegistry::ShortcutMap keysNow = resolved();
    const QFont normal = m_list->font();
    QFont bold = normal;
    bold.setBold(true);
    const QFontMetrics normalMetrics(normal);
    const QFontMetrics boldMetrics(bold);

    int widest = m_list->header()->fontMetrics().horizontalAdvance(m_list->headerItem()->text(1));
    for (const Entry& entry : m_entries) {
        const auto found = keysNow.find(entry.id);
        const KeyboardShortcut keys = found != keysNow.end() ? found->second : KeyboardShortcut();
        const KeyboardShortcut programKeys = defaultOf(entry.id);
        const bool changed = keys != programKeys;

        // A changed shortcut stands out, the mark alone where it was removed
        QString text = nativeText(keys);
        if (changed) {
            text = keys.isEmpty() ? CHANGED_MARK : CHANGED_MARK + QLatin1Char(' ') + text;
        }
        entry.item->setText(1, text);
        entry.item->setFont(1, changed ? bold : normal);
        entry.item->setData(1, Qt::ForegroundRole,
                            changed ? QVariant(QBrush(m_changedColor)) : QVariant());
        const QString programText = !changed              ? QString()
                                    : programKeys.isEmpty() ? tr("Default: no shortcut")
                                                            : tr("Default: %1").arg(
                                                                  nativeText(programKeys));
        entry.item->setToolTip(0, programText);
        entry.item->setToolTip(1, programText);
        // Screen readers say "changed" instead of reading the mark
        entry.item->setData(1, Qt::AccessibleTextRole,
                            changed ? tr("%1, changed").arg(keys.isEmpty() ? tr("none")
                                                                           : nativeText(keys))
                                    : QVariant());
        widest = std::max(widest, (changed ? boldMetrics : normalMetrics).horizontalAdvance(text));
    }
    // As wide as the keys of the commands; the long lists of the fixed keys are cut short
    m_keysColumnWidth = widest + KEYS_COLUMN_PADDING;
    fitKeysColumn();

    // The field under the list shows the keys of any command whole, and what it asks for
    // while it records them
    const QFontMetrics fieldMetrics(m_keysField->font());
    m_widestKeys = std::max(fieldMetrics.horizontalAdvance(changePrompt()),
                            fieldMetrics.horizontalAdvance(tr("No shortcut")));
    for (const auto& [id, keys] : keysNow) {
        m_widestKeys = std::max(m_widestKeys, fieldMetrics.horizontalAdvance(nativeText(keys)));
    }
    arrangeKeysRow();

    applyFilter();
    showDetail();
}

void ShortcutsPage::updateColors() {
    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    // Readable on the list as the text of the annotations' cards is on theirs
    m_mutedColor =
        readableColor(theme.palette.placeholderText, theme.palette.base, MIN_TEXT_CONTRAST);
    m_changedColor = readableColor(theme.palette.link, theme.palette.base, MIN_TEXT_CONTRAST);

    for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
        QTreeWidgetItem* group = m_list->topLevelItem(top);
        if (kindOf(group) == ItemKind::FixedGroup) {
            group->setForeground(0, m_mutedColor);
            for (int child = 0; child < group->childCount(); ++child) {
                group->child(child)->setForeground(0, m_mutedColor);
                group->child(child)->setForeground(1, m_mutedColor);
            }
        }
    }
    for (const Entry& entry : m_entries) {
        if (!entry.available) {
            entry.item->setData(0, NOTE_COLOR_ROLE, m_mutedColor);
        }
    }
    m_noMatchItem->setForeground(0, m_mutedColor);
}

bool ShortcutsPage::isShown(const QTreeWidgetItem* item,
                            const CommandRegistry::ShortcutMap& keysNow,
                            const QString& query) const {
    const bool byText = !query.isEmpty();

    if (const Entry* entry = entryOf(item)) {
        const auto found = keysNow.find(entry->id);
        const KeyboardShortcut keys = found != keysNow.end() ? found->second : KeyboardShortcut();
        if (m_onlyChanged->isChecked() && keys == defaultOf(entry->id)) {
            return false;
        }
        if (m_keySearch) {
            return !m_searchKeys || (!keys.isEmpty() && keys == shortcutFrom(*m_searchKeys));
        }
        if (!byText) {
            return true;
        }
        return searchForm(entry->menu + entry->label).contains(query) ||
               keysOfItemHave(item, keysNow, query, m_wholeKeyName);
    }

    const FixedKeyGroup* group = nullptr;
    const FixedKeys* fixed = fixedKeysOf(item, &group);
    if (fixed == nullptr || m_onlyChanged->isChecked()) {
        return false;
    }
    if (m_keySearch) {
        return !m_searchKeys || fixed->covers(*m_searchKeys);
    }
    if (!byText) {
        return true;
    }
    return searchForm(group->title + fixed->label).contains(query) ||
           keysOfItemHave(item, keysNow, query, m_wholeKeyName);
}

bool ShortcutsPage::keysOfItemHave(const QTreeWidgetItem* item,
                                   const CommandRegistry::ShortcutMap& keysNow,
                                   const QString& query, bool toNameEnd) const {
    if (const Entry* entry = entryOf(item)) {
        const auto found = keysNow.find(entry->id);
        return found != keysNow.end() && !found->second.isEmpty() &&
               keysContain(combinationOf(found->second), query, toNameEnd);
    }
    const FixedKeys* fixed = fixedKeysOf(item, nullptr);
    if (fixed == nullptr) {
        return false;
    }
    return keysTextHas(fixed->anyKeyText, query, toNameEnd) ||
           keysTextHas(fixed->summary, query, toNameEnd) ||
           std::any_of(fixed->keys.begin(), fixed->keys.end(), [&](QKeyCombination keys) {
               return keysContain(keys, query, toNameEnd);
           });
}

void ShortcutsPage::applyFilter() {
    const CommandRegistry::ShortcutMap keysNow = resolved();
    // In the search by keys the field shows the keys, not a text to search for
    const QString query = m_keySearch ? QString() : searchForm(m_search->text());
    const bool filtering = m_onlyChanged->isChecked() || m_keySearch || !query.isEmpty();
    // A whole key name finds that key: "f1" is not F12. While no keys have it, it is a name
    // being written and finds the keys it begins: "shift+f1" finds Shift+F12 on the way
    const auto someKeysHave = [&]() {
        for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
            const QTreeWidgetItem* group = m_list->topLevelItem(top);
            for (int child = 0; child < group->childCount(); ++child) {
                if (keysOfItemHave(group->child(child), keysNow, query, true)) {
                    return true;
                }
            }
        }
        return false;
    };
    m_wholeKeyName =
        !query.isEmpty() && isWholeKeyName(lastKeyName(query)) && someKeysHave();
    bool anyShown = false;
    for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
        QTreeWidgetItem* group = m_list->topLevelItem(top);
        if (group == m_noMatchItem) {
            continue;
        }
        bool groupShown = false;
        for (int child = 0; child < group->childCount(); ++child) {
            QTreeWidgetItem* item = group->child(child);
            const bool shown = isShown(item, keysNow, query);
            item->setHidden(!shown);
            groupShown = groupShown || shown;
        }
        group->setHidden(!groupShown);
        // What the search found is seen, also in a group closed before
        if (groupShown && filtering) {
            group->setExpanded(true);
        }
        anyShown = anyShown || groupShown;
    }

    m_noMatchItem->setHidden(anyShown);
    if (!anyShown) {
        m_noMatchItem->setText(0, noMatchText());
        m_noMatchItem->setToolTip(0, m_noMatchItem->text(0));
    }

    // The selection stays on what the list shows
    QTreeWidgetItem* current = m_list->currentItem();
    if (current == nullptr || isHiddenInList(current)) {
        QTreeWidgetItem* first = firstShownItem();
        if (first != current) {
            m_list->setCurrentItem(first);
        }
        if (first == nullptr) {
            showDetail();
        }
    }
}

QString ShortcutsPage::noMatchText() const {
    if (m_onlyChanged->isChecked()) {
        const bool searching = m_keySearch ? m_searchKeys.has_value()
                                           : !searchForm(m_search->text()).isEmpty();
        return searching ? tr("No changed shortcut matches the search.")
                         : tr("You have not changed any shortcut.");
    }
    if (m_keySearch && m_searchKeys) {
        const QString keys = ShortcutRules::keysText(*m_searchKeys);
        const KeyCheck check = m_rules.check(*m_searchKeys, std::string());
        switch (check.result) {
        case KeyCheck::Result::Refused:
            return check.reason;
        case KeyCheck::Result::Warning:
            return tr("No command has %1.").arg(keys) + QStringLiteral(" ") + check.reason;
        case KeyCheck::Result::Allowed:
            break;
        }
        return tr("No command has %1: the shortcut is free.").arg(keys);
    }
    return tr("Nothing matches the search.");
}

QTreeWidgetItem* ShortcutsPage::firstShownItem() const {
    for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
        const QTreeWidgetItem* group = m_list->topLevelItem(top);
        if (group->isHidden()) {
            continue;
        }
        for (int child = 0; child < group->childCount(); ++child) {
            if (!group->child(child)->isHidden()) {
                return group->child(child);
            }
        }
    }
    return nullptr;
}

void ShortcutsPage::fitKeysColumn() {
    const auto share = static_cast<int>(m_list->viewport()->width() * KEYS_COLUMN_SHARE);
    const int width = std::max(m_keysColumnWidth, share);
    if (m_list->columnWidth(1) != width) {
        m_list->setColumnWidth(1, width);
    }
}

// ============================================================================
// The selected command
// ============================================================================

void ShortcutsPage::showDetail() {
    const QTreeWidgetItem* item = m_list->currentItem();
    if (const Entry* entry = entryOf(item)) {
        const KeyboardShortcut keys = shortcutOf(entry->id);
        const KeyboardShortcut programKeys = defaultOf(entry->id);
        m_detailTitle->setText(entry->menu + PATH_SEPARATOR + entry->label);
        m_keysField->setEnabled(true);
        m_keysField->setToolTip(QString());
        m_changeButton->setEnabled(true);
        m_removeButton->setEnabled(!keys.isEmpty());
        m_restoreButton->setEnabled(keys != programKeys);
        if (!m_keysField->isRecording()) {
            m_keysField->setText(keys.isEmpty() ? tr("No shortcut") : nativeText(keys));
            m_keysField->setCursorPosition(0);
        }

        QStringList info;
        if (!entry->available) {
            info.append(tr("The command is not available yet: its shortcut is reserved."));
        }
        if (programKeys.isEmpty()) {
            info.append(tr("Default: no shortcut."));
        } else if (const std::string owner = ownerOf(programKeys, entry->id); !owner.empty()) {
            info.append(tr("Default: %1 (now the shortcut of %2).")
                            .arg(nativeText(programKeys), titleOf(owner)));
        } else {
            info.append(tr("Default: %1.").arg(nativeText(programKeys)));
        }
        if (entry->id == "file.exit") {
            switch (m_rules.platform()) {
            case ShortcutPlatform::Windows:
                info.append(tr("Alt+F4 closes the window also without this shortcut: Windows "
                               "does it."));
                break;
            case ShortcutPlatform::Linux:
                info.append(tr("Alt+F4 closes the window also without this shortcut: the "
                               "desktop does it."));
                break;
            case ShortcutPlatform::MacOS:
                break;
            }
        }
        m_detailInfo->setText(info.join(QLatin1Char(' ')));
        return;
    }

    // Fixed keys, a group or nothing: nothing to change
    m_keysField->setEnabled(false);
    m_changeButton->setEnabled(false);
    m_removeButton->setEnabled(false);
    m_restoreButton->setEnabled(false);

    const FixedKeyGroup* group = nullptr;
    const FixedKeys* fixed = fixedKeysOf(item, &group);
    if (fixed != nullptr) {
        m_detailTitle->setText(group->title + PATH_SEPARATOR + fixed->label);
        m_keysField->setText(fixed->keysText());
        m_keysField->setCursorPosition(0);
        m_keysField->setToolTip(fixed->keysText());
        m_detailInfo->setText(explanationOf(*group, fixed));
        return;
    }
    m_keysField->clear();
    m_keysField->setToolTip(QString());
    if (group != nullptr) {
        m_detailTitle->setText(group->title);
        m_detailInfo->setText(explanationOf(*group, nullptr));
    } else if (item != nullptr && kindOf(item) == ItemKind::Menu) {
        m_detailTitle->setText(item->text(0));
        m_detailInfo->setText(tr("Choose a command of the menu to see or change its shortcut."));
    } else {
        // The list says why nothing is shown
        m_detailTitle->clear();
        m_detailInfo->clear();
    }
}

void ShortcutsPage::arrangeKeysRow() {
    // The field as wide as the longest text it shows
    const int fieldWidth = lineEditWidth(m_keysField, m_widestKeys);
    const int spacing = std::max(m_keysRow->horizontalSpacing(), 0);
    const int besideWidth = m_keysLabel->sizeHint().width() + spacing + fieldWidth + spacing +
                            m_keyButtons->sizeHint().width();
    const QMargins margins = m_detail->layout()->contentsMargins();
    const int room =
        m_detail->contentsRect().width() - margins.left() - margins.right() - scrollBarRoom();
    const bool under = room < besideWidth;
    if (under == m_keyButtonsUnder) {
        return;
    }
    m_keyButtonsUnder = under;
    m_keysRow->removeWidget(m_keyButtons);
    if (under) {
        m_keysRow->addWidget(m_keyButtons, 1, 1, Qt::AlignLeft);
    } else {
        m_keysRow->addWidget(m_keyButtons, 0, 2);
    }
}

void ShortcutsPage::arrangeSearchRow() {
    // The field as wide as the longer of its prompts. They show while the field is empty,
    // and then QLineEdit keeps no room for its clear button.
    const QFontMetrics metrics(m_search->font());
    const int prompts = std::max(metrics.horizontalAdvance(textSearchPrompt()),
                                 metrics.horizontalAdvance(keySearchPrompt()));
    const int fieldWidth = lineEditWidth(m_search, prompts);
    const int spacing = std::max(m_searchRow->horizontalSpacing(), 0);
    const int besideWidth = m_searchLabel->sizeHint().width() + spacing + fieldWidth + spacing +
                            m_searchOptions->sizeHint().width();
    const QMargins margins = pageLayout()->contentsMargins();
    const int room = contentsRect().width() - margins.left() - margins.right() - scrollBarRoom();
    const bool under = room < besideWidth;
    if (under == m_searchOptionsUnder) {
        return;
    }
    m_searchOptionsUnder = under;
    m_searchRow->removeWidget(m_searchOptions);
    if (under) {
        m_searchRow->addWidget(m_searchOptions, 1, 1, Qt::AlignLeft);
    } else {
        m_searchRow->addWidget(m_searchOptions, 0, 2);
    }
}

int ShortcutsPage::scrollBarRoom() const {
    // The page of the dialog is in a scroll area: the room its scroll bar takes when the
    // page gets taller than the window (a message), as QAbstractScrollArea gives it
    for (const QWidget* parent = parentWidget(); parent != nullptr;
         parent = parent->parentWidget()) {
        const auto* scrollArea = qobject_cast<const QScrollArea*>(parent);
        if (scrollArea == nullptr) {
            continue;
        }
        const QScrollBar* bar = scrollArea->verticalScrollBar();
        const QStyle* style = scrollArea->style();
        if (bar->isVisibleTo(scrollArea) ||
            scrollArea->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff ||
            style->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) != 0) {
            // Shown already (the page is narrower), never shown, or over the page
            return 0;
        }
        int room = bar->sizeHint().width();
        if (style->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents, nullptr,
                             scrollArea) != 0) {
            room += style->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing, nullptr,
                                       scrollArea);
        }
        return room;
    }
    return 0;
}

QString ShortcutsPage::explanationOf(const FixedKeyGroup& group, const FixedKeys* row) const {
    if (group.local) {
        return tr("These keys work only while the list of the panel is active, so a command "
                  "may have them as well.");
    }
    if (group.system) {
        return row != nullptr
                   ? tr("A key of the system: the program does not get it or cannot change it.")
                   : tr("Keys of the system: the program does not get them or cannot change "
                        "them.");
    }
    return row != nullptr
               ? tr("A fixed key: it works as in other programs and cannot be changed.")
               : tr("Fixed keys: they work as in other programs and cannot be changed.");
}

// ============================================================================
// Messages
// ============================================================================

void ShortcutsPage::showMessage(MessageKind kind, const QString& html, const QString& acceptText,
                                std::function<void()> accept) {
    m_messageKind = kind;
    m_messageAccept = std::move(accept);
    m_messageText->setText(html);
    const bool question = !acceptText.isEmpty();
    m_messageAcceptButton->setText(acceptText);
    m_messageAcceptButton->setVisible(question);
    m_messageCancelButton->setVisible(question);
    styleMessage();
    // The message takes the place of the note under the keys, which it is more important than
    m_detailInfo->hide();
    m_message->show();
    revealMessage();

    // Screen readers read it out
    m_message->setAccessibleName(QTextDocumentFragment::fromHtml(html).toPlainText());
    QAccessibleEvent alert(m_message, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);

    if (question) {
        m_messageAcceptButton->setFocus(Qt::OtherFocusReason);
    }
}

void ShortcutsPage::hideMessage() {
    m_message->hide();
    m_detailInfo->show();
    m_messageAccept = nullptr;
}

void ShortcutsPage::revealMessage() {
    // A page taller than a small screen scrolls: the message must not stay below the view.
    // The layouts settle first.
    QTimer::singleShot(0, this, [this]() {
        if (m_message->isHidden()) {
            return;
        }
        for (QWidget* parent = parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
            auto* scrollArea = qobject_cast<QScrollArea*>(parent);
            if (scrollArea == nullptr || scrollArea->widget() == nullptr) {
                continue;
            }
            // Up or down only: the message starts at the left of the page. Its bottom, then
            // its top, which wins when it is taller than the view.
            const int left = scrollArea->horizontalScrollBar()->value();
            const int top = m_message->mapTo(scrollArea->widget(), QPoint(0, 0)).y();
            scrollArea->ensureVisible(left, top + m_message->height(), 0, 0);
            scrollArea->ensureVisible(left, top, 0, 0);
            return;
        }
    });
}

void ShortcutsPage::styleMessage() {
    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    QColor background;
    QColor line;
    switch (m_messageKind) {
    case MessageKind::Information:
        line = theme.palette.highlight;
        background = mixed(theme.palette.base, line, MESSAGE_TINT);
        break;
    case MessageKind::Warning:
        line = theme.colors.infoBarBorder;
        background = theme.colors.infoBarBackground;
        break;
    case MessageKind::Error:
        line = theme.log.error;
        background = mixed(theme.palette.base, line, MESSAGE_TINT);
        break;
    }
    m_message->setStyleSheet(
        QStringLiteral("QFrame#shortcutMessage { background-color: %1; border: none; "
                       "border-left: 3px solid %2; border-radius: 2px; } "
                       "QLabel { background: transparent; color: %3; }")
            .arg(background.name(), line.name(), theme.palette.windowText.name()));
}

void ShortcutsPage::updateIcons() {
    const QIcon lock = core::ArtProvider::getInstance().getIcon(QStringLiteral("common.lock"),
                                                                core::IconContext::TreeView);
    // After the name (NameDelegate): the name comes first, as in the menus
    for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
        QTreeWidgetItem* group = m_list->topLevelItem(top);
        if (kindOf(group) == ItemKind::FixedGroup) {
            group->setData(0, LOCK_ROLE, lock);
        }
    }
    if (!m_message->isHidden()) {
        styleMessage();
    }
}

// ============================================================================
// Actions
// ============================================================================

void ShortcutsPage::startChange(QWidget* returnTo) {
    if (currentEntry() == nullptr) {
        return;
    }
    m_returnFocus = returnTo != nullptr ? returnTo : m_list;
    hideMessage();
    m_keysField->startRecording(changePrompt());
    showDetail();
}

void ShortcutsPage::removeKeys(QWidget* returnTo) {
    const Entry* entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    hideMessage();
    setKeys(entry->id, KeyboardShortcut());
    refresh();
    m_returnFocus = returnTo;
    returnFocus();
}

void ShortcutsPage::restoreDefault() {
    const Entry* entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    const std::string id = entry->id;
    const KeyboardShortcut programKeys = defaultOf(id);
    hideMessage();

    const std::string owner = ownerOf(programKeys, id);
    if (owner.empty()) {
        setKeys(id, programKeys);
        refresh();
        // The button is off now: the keys go to the change
        m_returnFocus = m_changeButton;
        returnFocus();
        return;
    }
    // Cancel gives the keys back to the button
    m_returnFocus = m_restoreButton;
    showMessage(MessageKind::Warning,
                tr("The default shortcut %1 now belongs to <b>%2</b>: if you restore it, that "
                   "command will be left without a shortcut.")
                    .arg(nativeText(programKeys).toHtmlEscaped(), titleOf(owner).toHtmlEscaped()),
                tr("Restore A&nyway"), [this, id, programKeys, owner]() {
                    setKeys(owner, KeyboardShortcut());
                    setKeys(id, programKeys);
                    refresh();
                    showMessage(MessageKind::Information,
                                tr("%1 now has no shortcut.").arg(titleOf(owner).toHtmlEscaped()));
                    m_returnFocus = m_changeButton;
                    returnFocus();
                });
}

void ShortcutsPage::restoreAllDefaults() {
    hideMessage();
    if (m_custom.empty()) {
        showMessage(MessageKind::Information,
                    tr("All the commands already have their default shortcuts."));
        return;
    }
    m_custom.clear();
    refresh();
    showMessage(MessageKind::Information,
                tr("All the commands have their default shortcuts again. Apply or OK will save "
                   "this change, Cancel will discard it."));
}

void ShortcutsPage::chooseExportFile() {
    hideMessage();
    if (m_custom.empty()) {
        showMessage(MessageKind::Information,
                    tr("There are no changed shortcuts: the file would be empty. Change a "
                       "shortcut, then export it."));
        return;
    }
    const QString suggested =
        QDir(QDir::homePath()).filePath(tr("kalahari-shortcuts") + QStringLiteral(".json"));
    QString path = QFileDialog::getSaveFileName(this, tr("Export Keyboard Shortcuts"), suggested,
                                                tr("Keyboard shortcuts (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".json");
    }
    exportShortcuts(path);
}

void ShortcutsPage::chooseImportFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Keyboard Shortcuts"), QDir::homePath(),
        tr("Keyboard shortcuts (*.json)") + QStringLiteral(";;") + tr("All files (*)"));
    if (!path.isEmpty()) {
        importShortcuts(path);
    }
}

// ============================================================================
// Search
// ============================================================================

void ShortcutsPage::setKeySearch(bool on) {
    if (on == m_keySearch) {
        if (on && !m_search->isRecording()) {
            recordSearchKeys();
        }
        return;
    }
    m_keySearch = on;
    m_searchKeys.reset();
    {
        const QSignalBlocker blocker(m_byKeysButton);
        m_byKeysButton->setChecked(on);
    }
    if (on) {
        recordSearchKeys();
    } else {
        // The search by text starts again from an empty field
        if (m_search->isRecording()) {
            m_search->stopRecording();
        }
        const QSignalBlocker blocker(m_search);
        m_search->setReadOnly(false);
        m_search->setPlaceholderText(textSearchPrompt());
        m_search->clear();
    }
    applyFilter();
}

void ShortcutsPage::recordSearchKeys() {
    // The field as it is for the text, empty, so recording leaves it so when it ends
    {
        const QSignalBlocker blocker(m_search);
        m_search->setReadOnly(false);
        m_search->setPlaceholderText(textSearchPrompt());
        m_search->clear();
    }
    m_searchKeys.reset();
    m_search->startRecording(keySearchPrompt(), true);
    applyFilter();
}

QString ShortcutsPage::textSearchPrompt() const {
    // Keys the list has, as an example: those of Find and Replace (⌥⌘F on macOS), or else
    // those of Find, the same on every system
    const KeyboardShortcut example = defaultOf("edit.findReplace");
    return tr("Command name or keys, e.g. %1")
        .arg(example.isEmpty()
                 ? ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_F))
                 : nativeText(example));
}

QString ShortcutsPage::keySearchPrompt() {
    return tr("Press a shortcut... (Esc – normal search)");
}

QString ShortcutsPage::changePrompt() {
    return tr("Press the new shortcut... (Esc – cancel)");
}

void ShortcutsPage::focusList() {
    QTreeWidgetItem* item = m_list->currentItem();
    if (item == nullptr || isHiddenInList(item)) {
        if (QTreeWidgetItem* first = firstShownItem()) {
            m_list->setCurrentItem(first);
        }
    }
    m_list->setFocus(Qt::OtherFocusReason);
}

void ShortcutsPage::returnFocus() {
    QWidget* target = m_returnFocus;
    if (target == nullptr || !target->isEnabled() || target->isHidden()) {
        target = m_list;
    }
    target->setFocus(Qt::OtherFocusReason);
}

bool ShortcutsPage::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool plain = (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;

        if (watched == m_list && plain) {
            // Enter changes the keys, it does not press OK
            if (enter || key->key() == Qt::Key_F2) {
                QTreeWidgetItem* item = m_list->currentItem();
                if (currentEntry() != nullptr) {
                    startChange(m_list);
                } else if (enter && item != nullptr && item->childCount() > 0) {
                    item->setExpanded(!item->isExpanded());
                }
                return true;
            }
            if (key->key() == Qt::Key_Delete && currentEntry() != nullptr &&
                m_removeButton->isEnabled()) {
                removeKeys(m_list);
                return true;
            }
        }
        if (watched == m_search && plain && !m_search->isRecording() &&
            (enter || key->key() == Qt::Key_Down)) {
            focusList();
            return true;
        }
        if ((watched == m_messageAcceptButton || watched == m_messageCancelButton) && plain &&
            key->key() == Qt::Key_Escape) {
            hideMessage();
            returnFocus();
            return true;
        }
    } else if (event->type() == QEvent::FocusIn && watched == m_search) {
        // Back in the field while searching by keys: press the next keys
        if (m_keySearch && !m_search->isRecording()) {
            recordSearchKeys();
        }
    } else if (event->type() == QEvent::MouseButtonRelease && watched == m_keysField) {
        if (currentEntry() != nullptr && !m_keysField->isRecording()) {
            startChange(m_list);
        }
    } else if (event->type() == QEvent::Resize && watched == m_detail) {
        arrangeKeysRow();
    } else if (event->type() == QEvent::Resize && watched == m_list->viewport()) {
        fitKeysColumn();
    }
    return SettingsPage::eventFilter(watched, event);
}

void ShortcutsPage::resizeEvent(QResizeEvent* event) {
    SettingsPage::resizeEvent(event);
    arrangeSearchRow();
    // The page settles after the message came (a scroll bar, a line more of the message):
    // the message stays in sight
    if (!m_message->isHidden()) {
        revealMessage();
    }
}

} // namespace gui
} // namespace kalahari
