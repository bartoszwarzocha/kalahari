/// @file shortcuts_page.cpp
/// @brief Settings page: Keyboard Shortcuts
///
/// The page keeps the user's keys (m_custom) apart from the program's (m_defaults) and
/// shows what CommandRegistry::resolveShortcuts() makes of them, so the list shows the
/// keys the commands will have after Apply. Every key given here passes ShortcutRules.

#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QAccessible>
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
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOption>
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

constexpr int KIND_ROLE = Qt::UserRole;       ///< ItemKind
constexpr int INDEX_ROLE = Qt::UserRole + 1;  ///< The command in m_entries, or the fixed group
constexpr int ROW_ROLE = Qt::UserRole + 2;    ///< The row of the fixed group

constexpr int LIST_MIN_ROWS = 3;          ///< On a small screen the list shrinks to this many rows
constexpr int KEYS_COLUMN_PADDING = 24;   ///< Room around the keys in their column
constexpr int FIELD_TEXT_ROOM = 6;        ///< Room a line edit keeps beside its text
constexpr int MESSAGE_ICON_SIZE = 20;
constexpr int SKIPPED_SHOWN = 8;          ///< The skipped keys of an import named one by one
constexpr qint64 MAX_FILE_SIZE = 1024 * 1024;  ///< A file of shortcuts is far smaller
constexpr float MESSAGE_TINT = 0.12F;     ///< How much of its color the message's background has

/// The mark of a changed shortcut, before its keys
const QString CHANGED_MARK = QStringLiteral("● ");
/// Between a menu and its command, as in "File › Save"
const QString PATH_SEPARATOR = QStringLiteral(" › ");

ItemKind kindOf(const QTreeWidgetItem* item) {
    return static_cast<ItemKind>(item->data(0, KIND_ROLE).toInt());
}

QKeyCombination combinationOf(const KeyboardShortcut& keys) {
    return QKeyCombination(keys.modifiers, static_cast<Qt::Key>(keys.keyCode));
}

KeyboardShortcut shortcutFrom(QKeyCombination keys) {
    return KeyboardShortcut(static_cast<int>(keys.key()), keys.keyboardModifiers());
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

/// Whether the keys, as the system writes them or in the portable form, contain the text
bool keysContain(QKeyCombination keys, const QString& query) {
    const QKeySequence sequence(keys);
    return searchForm(sequence.toString(QKeySequence::NativeText)).contains(query) ||
           searchForm(sequence.toString(QKeySequence::PortableText)).contains(query);
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
};

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
{
    createSearch();
    createList();
    createDetail();
    createButtons();
    buildList();

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
    updateIcons();
    refresh();
}

void ShortcutsPage::createSearch() {
    auto* row = new QHBoxLayout();
    auto* label = new QLabel(tr("&Search:"));
    m_search = new ShortcutRecorder();
    m_search->setObjectName(QStringLiteral("shortcutsSearch"));
    m_search->setPlaceholderText(textSearchPrompt());
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    label->setBuddy(m_search);

    m_byKeysButton = new QPushButton(tr("By &Keys"));
    m_byKeysButton->setObjectName(QStringLiteral("shortcutsByKeys"));
    m_byKeysButton->setCheckable(true);
    m_byKeysButton->setToolTip(tr("Press a shortcut to see what it does"));

    m_onlyChanged = new QCheckBox(tr("&Only Changed"));
    m_onlyChanged->setObjectName(QStringLiteral("shortcutsOnlyChanged"));
    m_onlyChanged->setToolTip(tr("Show only the shortcuts you changed"));

    row->addWidget(label);
    row->addWidget(m_search, 1);
    row->addWidget(m_byKeysButton);
    row->addWidget(m_onlyChanged);
    pageLayout()->addLayout(row);

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
    pageLayout()->addWidget(m_list, 1);

    connect(m_list, &QTreeWidget::currentItemChanged, this, [this]() {
        hideMessage();
        showDetail();
    });
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (entryOf(item) != nullptr) {
            startChange();
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
    auto* messageLayout = new QHBoxLayout(m_message);
    messageLayout->setContentsMargins(8, 6, 8, 6);
    m_messageIcon = new QLabel();
    messageLayout->addWidget(m_messageIcon, 0, Qt::AlignTop);
    auto* messageColumn = new QVBoxLayout();
    m_messageText = new QLabel();
    m_messageText->setObjectName(QStringLiteral("shortcutMessageText"));
    m_messageText->setTextFormat(Qt::RichText);
    m_messageText->setWordWrap(true);
    messageColumn->addWidget(m_messageText);
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
    messageColumn->addLayout(messageButtons);
    messageLayout->addLayout(messageColumn, 1);
    m_message->hide();
    layout->addWidget(m_message);

    pageLayout()->addWidget(detail);
    // A new width may move the buttons of the keys
    detail->installEventFilter(this);

    connect(m_keysField, &ShortcutRecorder::recorded, this, &ShortcutsPage::assignKeys);
    connect(m_keysField, &ShortcutRecorder::recordingStopped, this, [this](bool escape) {
        showDetail();
        if (escape) {
            m_list->setFocus(Qt::OtherFocusReason);
        }
    });
    connect(m_changeButton, &QPushButton::clicked, this, &ShortcutsPage::startChange);
    connect(m_removeButton, &QPushButton::clicked, this, &ShortcutsPage::removeKeys);
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
        m_list->setFocus(Qt::OtherFocusReason);
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
    auto& registry = CommandRegistry::getInstance();
    m_defaults = registry.defaultShortcuts();
    const std::vector<Command> commands = registry.getAllCommands();

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
        QFont groupFont = m_list->font();
        groupFont.setBold(true);
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
                    entry.item->setText(0, entry.available
                                               ? entry.label
                                               : tr("%1 (not available yet)").arg(entry.label));
                    entry.item->setData(0, KIND_ROLE, static_cast<int>(ItemKind::Command));
                    entry.item->setData(0, INDEX_ROLE, static_cast<int>(m_entries.size()));
                    m_entries.push_back(std::move(entry));
                }
            };
        addItems(menu, QString());
    }

    // The keys no command can have, last: so it is seen what is taken
    const QColor muted =
        core::ThemeManager::getInstance().getCurrentTheme().palette.placeholderText;
    for (size_t groupIndex = 0; groupIndex < m_fixedGroups.size(); ++groupIndex) {
        const FixedKeyGroup& fixedGroup = m_fixedGroups[groupIndex];
        auto* group = new QTreeWidgetItem(m_list);
        group->setText(0, fixedGroup.system ? tr("%1 – keys of the system").arg(fixedGroup.title)
                                            : tr("%1 – fixed keys").arg(fixedGroup.title));
        group->setData(0, KIND_ROLE, static_cast<int>(ItemKind::FixedGroup));
        group->setData(0, INDEX_ROLE, static_cast<int>(groupIndex));
        group->setFirstColumnSpanned(true);
        group->setForeground(0, muted);
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
            item->setForeground(0, muted);
            item->setForeground(1, muted);
        }
    }

    // What the list says when the search leaves nothing in it
    m_noMatchItem = new QTreeWidgetItem(m_list);
    m_noMatchItem->setData(0, KIND_ROLE, static_cast<int>(ItemKind::NoMatch));
    m_noMatchItem->setFlags(Qt::ItemIsEnabled);
    m_noMatchItem->setFirstColumnSpanned(true);
    m_noMatchItem->setForeground(0, muted);
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
    m_list->scrollToItem(entry->item);
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
        m_list->setFocus(Qt::OtherFocusReason);
        return;
    }

    KeyCheck check = m_rules.check(pressed, id);
    if (check.result == KeyCheck::Result::Refused) {
        refresh();
        showMessage(MessageKind::Error, check.reason.toHtmlEscaped());
        m_list->setFocus(Qt::OtherFocusReason);
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
        m_list->setFocus(Qt::OtherFocusReason);
        return;
    }

    // Keys another command has, or keys some desktops take: the user decides
    QStringList lines;
    if (!owner.empty()) {
        lines.append(tr("%1 is the shortcut of <b>%2</b>: if you assign it here, that command "
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
                                    tr("<b>%1</b> is now without a shortcut.")
                                        .arg(titleOf(owner).toHtmlEscaped()));
                    } else {
                        hideMessage();
                    }
                    m_list->setFocus(Qt::OtherFocusReason);
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
    const QByteArray data = QByteArray::fromStdString(shortcutsFile(m_custom).dump(2) + "\n");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
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
                tr("%n changed shortcut(s) saved to %1.", nullptr, count)
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
    const QByteArray data = file.readAll();
    const nlohmann::json json =
        nlohmann::json::parse(data.constBegin(), data.constEnd(), nullptr, false);
    const std::optional<nlohmann::json> object =
        json.is_discarded() ? std::nullopt : shortcutsOfFile(json);
    if (!object) {
        showMessage(MessageKind::Error, notShortcuts);
        return false;
    }

    // Every key as if it was pressed here, but without questions: what cannot be is skipped
    QStringList unreadable;
    const CommandRegistry::ShortcutMap fromFile = shortcutsFromJson(*object, &unreadable);
    QStringList skipped;
    for (const QString& id : unreadable) {
        skipped.append(tr("%1: the keys cannot be read").arg(titleOf(id.toStdString())));
    }
    CommandRegistry::ShortcutMap imported;
    std::map<KeyboardShortcut, std::string> owners;
    int read = 0;
    for (const auto& [id, keys] : fromFile) {
        if (entryById(id) == nullptr) {
            skipped.append(
                tr("“%1”: Kalahari has no such command").arg(QString::fromStdString(id)));
            continue;
        }
        if (!keys.isEmpty()) {
            const KeyCheck check = m_rules.check(combinationOf(keys), id);
            if (check.result == KeyCheck::Result::Refused) {
                skipped.append(
                    tr("%1 for %2: %3").arg(nativeText(keys), titleOf(id), check.reason));
                continue;
            }
            const auto [owner, first] = owners.emplace(keys, id);
            if (!first) {
                skipped.append(tr("%1 for %2: the file gives it to %3 too")
                                   .arg(nativeText(keys), titleOf(id), titleOf(owner->second)));
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
    QString text = tr("%n shortcut(s) read from %1.", nullptr, read).arg(name.toHtmlEscaped());
    if (!skipped.isEmpty()) {
        QStringList shown;
        for (const QString& line : skipped.mid(0, SKIPPED_SHOWN)) {
            shown.append(line.toHtmlEscaped());
        }
        text += QStringLiteral(" ") +
                tr("Skipped %n:", nullptr, static_cast<int>(skipped.size())) +
                QStringLiteral("<br>") + shown.join(QStringLiteral("<br>"));
        if (skipped.size() > SKIPPED_SHOWN) {
            text += QStringLiteral("<br>") +
                    tr("and %n more.", nullptr, static_cast<int>(skipped.size()) - SKIPPED_SHOWN);
        }
    }
    text += QStringLiteral("<br>") + tr("Apply or OK saves the changes, Cancel drops them.");
    showMessage(skipped.isEmpty() ? MessageKind::Information : MessageKind::Warning, text);
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
    bool anyChanged = false;
    for (const Entry& entry : m_entries) {
        const auto found = keysNow.find(entry.id);
        const KeyboardShortcut keys = found != keysNow.end() ? found->second : KeyboardShortcut();
        const KeyboardShortcut programKeys = defaultOf(entry.id);
        const bool changed = keys != programKeys;
        anyChanged = anyChanged || changed;

        QString text = nativeText(keys);
        if (changed) {
            text = CHANGED_MARK + (keys.isEmpty() ? tr("none") : text);
        }
        entry.item->setText(1, text);
        entry.item->setFont(1, changed ? bold : normal);
        entry.item->setToolTip(
            1, changed ? tr("Default: %1")
                             .arg(programKeys.isEmpty() ? tr("none") : nativeText(programKeys))
                       : QString());
        // Screen readers say "changed" instead of reading the mark
        entry.item->setData(1, Qt::AccessibleTextRole,
                            changed ? tr("%1, changed").arg(keys.isEmpty() ? tr("none")
                                                                           : nativeText(keys))
                                    : QVariant());
        widest = std::max(widest, (changed ? boldMetrics : normalMetrics).horizontalAdvance(text));
    }
    // As wide as the keys of the commands; the long lists of the fixed keys are cut short
    m_list->setColumnWidth(1, widest + KEYS_COLUMN_PADDING);

    // The field under the list shows any of them whole (arrangeKeysRow)
    const QFontMetrics fieldMetrics(m_keysField->font());
    m_widestKeys = 0;
    for (const auto& [id, keys] : keysNow) {
        m_widestKeys = std::max(m_widestKeys, fieldMetrics.horizontalAdvance(nativeText(keys)));
    }
    for (const FixedKeyGroup& group : m_fixedGroups) {
        for (const FixedKeys& fixed : group.rows) {
            m_widestKeys = std::max(m_widestKeys, fieldMetrics.horizontalAdvance(fixed.keysText()));
        }
    }
    arrangeKeysRow();

    m_restoreAllButton->setEnabled(anyChanged || !m_custom.empty());
    m_exportButton->setEnabled(!m_custom.empty());
    applyFilter();
    showDetail();
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
               (!keys.isEmpty() && keysContain(combinationOf(keys), query));
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
    if (searchForm(group->title + fixed->label).contains(query) ||
        searchForm(fixed->anyKeyText).contains(query) ||
        searchForm(fixed->summary).contains(query)) {
        return true;
    }
    return std::any_of(fixed->keys.begin(), fixed->keys.end(),
                       [&query](QKeyCombination keys) { return keysContain(keys, query); });
}

void ShortcutsPage::applyFilter() {
    const CommandRegistry::ShortcutMap keysNow = resolved();
    // In the search by keys the field shows the keys, not a text to search for
    const QString query = m_keySearch ? QString() : searchForm(m_search->text());
    const bool filtering = m_onlyChanged->isChecked() || m_keySearch || !query.isEmpty();
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
        if (m_keysField->isRecording()) {
            // The field is too narrow for the whole prompt
            m_detailInfo->setText(tr("Press the new shortcut of the command. Esc cancels."));
            return;
        }
        m_keysField->setPlaceholderText(tr("No shortcut"));
        m_keysField->setText(nativeText(keys));
        m_keysField->setCursorPosition(0);

        QStringList info;
        if (!entry->available) {
            info.append(tr("The command is not available yet; its shortcut is kept for it."));
        }
        if (programKeys.isEmpty()) {
            info.append(tr("Default: no shortcut."));
        } else if (const std::string owner = ownerOf(programKeys, entry->id); !owner.empty()) {
            info.append(tr("Default: %1 (now the shortcut of %2).")
                            .arg(nativeText(programKeys), titleOf(owner)));
        } else {
            info.append(tr("Default: %1.").arg(nativeText(programKeys)));
        }
        if (entry->id == "file.exit" && m_rules.platform() != ShortcutPlatform::MacOS) {
            info.append(
                tr("Alt+F4 closes the window also without this shortcut: the system does it."));
        }
        m_detailInfo->setText(info.join(QLatin1Char(' ')));
        return;
    }

    // Fixed keys, a group or nothing: nothing to change
    m_keysField->setEnabled(false);
    m_keysField->setPlaceholderText(QString());
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
        m_detailTitle->setText(item->text(0));
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
    // The field as wide as the longest keys, with its frame
    QStyleOptionFrame option;
    option.initFrom(m_keysField);
    option.lineWidth =
        m_keysField->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, m_keysField);
    const QMargins textMargins = m_keysField->textMargins();
    const QSize text(m_widestKeys + FIELD_TEXT_ROOM + textMargins.left() + textMargins.right(),
                     m_keysField->sizeHint().height());
    const int fieldWidth = m_keysField->style()
                               ->sizeFromContents(QStyle::CT_LineEdit, &option, text, m_keysField)
                               .width();

    const int spacing = std::max(m_keysRow->horizontalSpacing(), 0);
    const int besideWidth = m_keysLabel->sizeHint().width() + spacing + fieldWidth + spacing +
                            m_keyButtons->sizeHint().width();
    const QMargins margins = m_detail->layout()->contentsMargins();
    const int room = m_detail->contentsRect().width() - margins.left() - margins.right();
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

QString ShortcutsPage::explanationOf(const FixedKeyGroup& group, const FixedKeys* row) const {
    if (group.local) {
        return tr("These keys work only while the list of the panel is active, so a command "
                  "may have them as well.");
    }
    if (!group.system) {
        return tr("Fixed keys: they work here as in other programs, so no command can have them.");
    }
    if (row != nullptr) {
        // The system's own reason, where it names one key
        if (!row->reason.contains(QLatin1String("%1"))) {
            return row->reason;
        }
        if (row->keys.isEmpty() && !row->anyKeyText.isEmpty()) {
            return row->reason.arg(row->anyKeyText);
        }
        if (row->keys.size() == 1 && row->anyKeyWith == Qt::NoModifier) {
            return row->reason.arg(row->keysText());
        }
    }
    return tr("The system or the windows of the program use these keys, so no command can "
              "have them.");
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
    QString icon;
    switch (m_messageKind) {
    case MessageKind::Information:
        line = theme.palette.highlight;
        background = mixed(theme.palette.base, line, MESSAGE_TINT);
        icon = QStringLiteral("help.about");
        break;
    case MessageKind::Warning:
        line = theme.colors.infoBarBorder;
        background = theme.colors.infoBarBackground;
        icon = QStringLiteral("common.warning");
        break;
    case MessageKind::Error:
        line = theme.log.error;
        background = mixed(theme.palette.base, line, MESSAGE_TINT);
        icon = QStringLiteral("common.error");
        break;
    }
    m_message->setStyleSheet(
        QStringLiteral("QFrame#shortcutMessage { background-color: %1; border: none; "
                       "border-left: 3px solid %2; border-radius: 2px; } "
                       "QLabel { background: transparent; color: %3; }")
            .arg(background.name(), line.name(), theme.palette.windowText.name()));
    m_messageIcon->setPixmap(core::ArtProvider::getInstance().getThemedIcon(icon).pixmap(
        QSize(MESSAGE_ICON_SIZE, MESSAGE_ICON_SIZE)));
}

void ShortcutsPage::updateIcons() {
    const QIcon lock = core::ArtProvider::getInstance().getIcon(QStringLiteral("common.lock"),
                                                                core::IconContext::TreeView);
    for (int top = 0; top < m_list->topLevelItemCount(); ++top) {
        QTreeWidgetItem* group = m_list->topLevelItem(top);
        if (kindOf(group) == ItemKind::FixedGroup) {
            group->setIcon(0, lock);
        }
    }
    if (!m_message->isHidden()) {
        styleMessage();
    }
}

// ============================================================================
// Actions
// ============================================================================

void ShortcutsPage::startChange() {
    if (currentEntry() == nullptr) {
        return;
    }
    hideMessage();
    m_keysField->startRecording(tr("Press the keys"));
    showDetail();
}

void ShortcutsPage::removeKeys() {
    const Entry* entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    hideMessage();
    setKeys(entry->id, KeyboardShortcut());
    refresh();
    m_list->setFocus(Qt::OtherFocusReason);
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
        m_list->setFocus(Qt::OtherFocusReason);
        return;
    }
    showMessage(MessageKind::Warning,
                tr("The default shortcut %1 is now the shortcut of <b>%2</b>: if you restore it, "
                   "that command will be left without a shortcut.")
                    .arg(nativeText(programKeys).toHtmlEscaped(), titleOf(owner).toHtmlEscaped()),
                tr("Restore A&nyway"), [this, id, programKeys, owner]() {
                    setKeys(owner, KeyboardShortcut());
                    setKeys(id, programKeys);
                    refresh();
                    showMessage(MessageKind::Information,
                                tr("<b>%1</b> is now without a shortcut.")
                                    .arg(titleOf(owner).toHtmlEscaped()));
                    m_list->setFocus(Qt::OtherFocusReason);
                });
}

void ShortcutsPage::restoreAllDefaults() {
    hideMessage();
    m_custom.clear();
    refresh();
    showMessage(MessageKind::Information,
                tr("All the commands have their default shortcuts again. Apply or OK saves it, "
                   "Cancel drops it."));
    m_list->setFocus(Qt::OtherFocusReason);
}

void ShortcutsPage::chooseExportFile() {
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
        m_textQuery = m_search->text();
        recordSearchKeys();
    } else {
        if (m_search->isRecording()) {
            m_search->stopRecording();
        }
        const QSignalBlocker blocker(m_search);
        m_search->setReadOnly(false);
        m_search->setPlaceholderText(textSearchPrompt());
        m_search->setToolTip(QString());
        m_search->setText(m_textQuery);
    }
    applyFilter();
}

void ShortcutsPage::recordSearchKeys() {
    // The field as it is for the text, so recording restores it so when it ends
    {
        const QSignalBlocker blocker(m_search);
        m_search->setReadOnly(false);
        m_search->setPlaceholderText(textSearchPrompt());
        m_search->setText(m_textQuery);
    }
    m_searchKeys.reset();
    // The field is too narrow for more than the prompt
    m_search->setToolTip(tr("Esc goes back to the search by text."));
    m_search->startRecording(keySearchPrompt(), true);
    applyFilter();
}

QString ShortcutsPage::textSearchPrompt() {
    return tr("Command name or keys, e.g. %1")
        .arg(ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_F)));
}

QString ShortcutsPage::keySearchPrompt() {
    return tr("Press a shortcut...");
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
                    startChange();
                } else if (enter && item != nullptr && item->childCount() > 0) {
                    item->setExpanded(!item->isExpanded());
                }
                return true;
            }
            if (key->key() == Qt::Key_Delete && currentEntry() != nullptr &&
                m_removeButton->isEnabled()) {
                removeKeys();
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
            m_list->setFocus(Qt::OtherFocusReason);
            return true;
        }
    } else if (event->type() == QEvent::FocusIn && watched == m_search) {
        // Back in the field while searching by keys: press the next keys
        if (m_keySearch && !m_search->isRecording()) {
            recordSearchKeys();
        }
    } else if (event->type() == QEvent::MouseButtonRelease && watched == m_keysField) {
        if (currentEntry() != nullptr && !m_keysField->isRecording()) {
            startChange();
        }
    } else if (event->type() == QEvent::Resize && watched == m_detail) {
        arrangeKeysRow();
    } else if (event->type() == QEvent::Resize && watched == m_list) {
        // The list gets lower when what is under it grows (a message, a longer note): the
        // selected row stays in sight if it was. The rows have not moved: was it in the
        // part of the list that is gone?
        const auto* resize = static_cast<QResizeEvent*>(event);
        const int lost = resize->oldSize().height() - resize->size().height();
        const QTreeWidgetItem* current = m_list->currentItem();
        if (current != nullptr && lost > 0) {
            const QRect row = m_list->visualItemRect(current);
            if (row.isValid() && row.top() >= 0 &&
                row.top() < m_list->viewport()->height() + lost) {
                QTimer::singleShot(0, m_list, [this]() {
                    if (QTreeWidgetItem* item = m_list->currentItem()) {
                        m_list->scrollToItem(item);
                    }
                });
            }
        }
    }
    return SettingsPage::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
