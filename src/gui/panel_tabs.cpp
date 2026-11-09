/// @file panel_tabs.cpp
/// @brief The tabs of the panels docked one over another

#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QEvent>
#include <QHash>
#include <QKeySequence>
#include <QMainWindow>
#include <QMetaObject>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QTabBar>
#include <QVBoxLayout>

#include <string>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// @brief Set on Qt's tabs while the program's own show their panels (hideQtTabs())
constexpr const char* QT_TABS_HIDDEN = "kalahariQtTabsHidden";

/// @brief Qt's own choices for its tabs, kept while they are hidden
constexpr const char* QT_TABS_SCROLL_BUTTONS = "kalahariQtTabsScrollButtons";
constexpr const char* QT_TABS_FOCUS = "kalahariQtTabsFocus";

/// @brief The style of Qt's tabs while they are hidden: tabs of no size, so the window keeps
/// no room for them (QDockAreaLayoutInfo gives its tabs the size their QTabBar asks for, and
/// none when that size is null)
class NoTabsStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget) const override {
        if (type == CT_TabBarTab) {
            return {0, 0};
        }
        return QProxyStyle::sizeFromContents(type, option, size, widget);
    }
};

/// @brief One NoTabsStyle for all the windows, as long as the application: a widget does not
/// own its style, and Qt's tabs go with their window after its PanelTabs
QStyle* noTabsStyle() {
    static QStyle* style = nullptr;
    if (style == nullptr) {
        style = new NoTabsStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        style->setParent(qApp);
        QObject::connect(style, &QObject::destroyed, [] { style = nullptr; });
    }
    return style;
}

} // anonymous namespace

PanelTabs::PanelTabs(QMainWindow* window, QObject* parent)
    : QObject(parent)
    , m_window(window)
{
    // Panels docked, moved, shown or closed change the window's layout, which asks to be laid
    // out again; new groups bring Qt new tabs, new children of the window
    m_window->installEventFilter(this);

    // The icons follow the theme and their size (Settings > Appearance > Icons > Tab Bar)
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() {
                for (Panel& panel : m_panels) {
                    panel.icon = core::ArtProvider::getInstance().getIcon(
                        QString::fromStdString(panel.commandId));
                }
                applySettings();
            });

    // The tooltips say the panels' keys
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, this,
            [this]() {
                m_refill = true;
                scheduleRefresh();
            });
}

void PanelTabs::addPanel(QDockWidget* dock, const std::string& commandId) {
    if (dock == nullptr || tabBar(dock) != nullptr) {
        return;
    }
    Panel& panel = m_panels.emplace_back(Panel{
        dock, commandId,
        core::ArtProvider::getInstance().getIcon(QString::fromStdString(commandId)), nullptr,
        nullptr, {}});

    // The content in a widget of its own, with room for the tabs under it
    QWidget* content = dock->widget();
    auto* body = new QWidget(dock);
    body->setObjectName(QStringLiteral("panelBody"));
    panel.body = new QVBoxLayout(body);
    panel.body->setContentsMargins(0, 0, 0, 0);
    panel.body->setSpacing(0);
    dock->setWidget(body);  // takes the content out of the dock, hidden
    body->show();
    if (content != nullptr) {
        panel.body->addWidget(content, 1);
        content->show();
    }

    panel.tabs = new PanelTabBar(body);
    panel.tabs->hide();  // until the panel is in a group
    panel.tabs->setLook(lookSetting());
    panel.tabs->setRows(rowsSetting());
    panel.tabs->setIconSize(
        core::ArtProvider::getInstance().getIconSize(core::IconContext::TabBar));
    placeTabs(panel, sideSetting());

    connect(panel.tabs, &PanelTabBar::tabChosen, this,
            [this, dock](int index) { choose(dock, index); });
    connect(panel.tabs, &PanelTabBar::tabMoveRequested, this,
            [this, dock](int from, int to) { move(dock, from, to); });

    // Docked onto another panel, floated, moved, shown, closed
    connect(dock, &QDockWidget::topLevelChanged, this, &PanelTabs::scheduleRefresh);
    connect(dock, &QDockWidget::dockLocationChanged, this, &PanelTabs::scheduleRefresh);
    connect(dock, &QDockWidget::visibilityChanged, this, &PanelTabs::scheduleRefresh);
    // Its title is the name on its tab
    dock->installEventFilter(this);

    m_refill = true;
    scheduleRefresh();
}

PanelTabBar::Look PanelTabs::lookSetting() {
    const auto look =
        core::SettingsManager::getInstance().get<std::string>(LOOK_KEY, LOOK_ICONS);
    if (look == LOOK_ICONS_AND_NAMES) {
        return PanelTabBar::Look::IconsAndNames;
    }
    if (look == LOOK_NAMES) {
        return PanelTabBar::Look::Names;
    }
    return PanelTabBar::Look::Icons;
}

PanelTabBar::Rows PanelTabs::rowsSetting() {
    return core::SettingsManager::getInstance().get<std::string>(ROWS_KEY, ROWS_WRAP) ==
                   ROWS_SINGLE
               ? PanelTabBar::Rows::Single
               : PanelTabBar::Rows::Wrap;
}

PanelTabBar::Side PanelTabs::sideSetting() {
    return core::SettingsManager::getInstance().get<std::string>(SIDE_KEY, SIDE_ABOVE) ==
                   SIDE_BELOW
               ? PanelTabBar::Side::Below
               : PanelTabBar::Side::Above;
}

void PanelTabs::applySettings() {
    const PanelTabBar::Look look = lookSetting();
    const PanelTabBar::Rows rows = rowsSetting();
    const PanelTabBar::Side side = sideSetting();
    const int iconSize = core::ArtProvider::getInstance().getIconSize(core::IconContext::TabBar);
    for (Panel& panel : m_panels) {
        if (panel.dock.isNull()) {
            continue;
        }
        panel.tabs->setLook(look);
        panel.tabs->setRows(rows);
        panel.tabs->setIconSize(iconSize);
        placeTabs(panel, side);
    }
    m_refill = true;
    refresh();
}

void PanelTabs::placeTabs(Panel& panel, PanelTabBar::Side side) {
    QWidget* titleBar = panel.dock->titleBarWidget();
    auto* titleLayout =
        titleBar != nullptr ? qobject_cast<QBoxLayout*>(titleBar->layout()) : nullptr;
    if (titleLayout == nullptr) {
        side = PanelTabBar::Side::Below;  // a title bar without room above it
    }

    QBoxLayout* place = side == PanelTabBar::Side::Above ? titleLayout : panel.body;
    if (place->indexOf(panel.tabs) < 0) {
        const bool shown = !panel.tabs->isHidden();
        if (titleLayout != nullptr) {
            titleLayout->removeWidget(panel.tabs);
        }
        panel.body->removeWidget(panel.tabs);
        if (side == PanelTabBar::Side::Above) {
            place->insertWidget(0, panel.tabs);  // above the title
        } else {
            place->addWidget(panel.tabs);  // under the content
        }
        panel.tabs->setVisible(shown);
    }
    panel.tabs->setSide(side);
}

void PanelTabs::scheduleRefresh() {
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;
    QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
}

QDockWidget* PanelTabs::dockOfTab(const QTabBar* bar, int index) const {
    // Qt keeps the address of a tab's dock as the tab's data (tabId() in qdockarealayout.cpp,
    // Qt 6.4 to 6.9); a tab of another dock, or other data, keeps Qt's tabs
    const auto key = qvariant_cast<quintptr>(bar->tabData(index));
    for (const Panel& panel : m_panels) {
        if (!panel.dock.isNull() && reinterpret_cast<quintptr>(panel.dock.data()) == key) {
            return panel.dock.data();
        }
    }
    return nullptr;
}

QTabBar* PanelTabs::qtTabsOf(const QDockWidget* dock) const {
    for (QTabBar* bar : m_window->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly)) {
        for (int i = 0; i < bar->count(); ++i) {
            if (dockOfTab(bar, i) == dock) {
                return bar;
            }
        }
    }
    return nullptr;
}

void PanelTabs::hideQtTabs(QTabBar* bar, bool hide) {
    if (bar->property(QT_TABS_HIDDEN).toBool() == hide) {
        return;
    }
    if (hide) {
        bar->setProperty(QT_TABS_SCROLL_BUTTONS, bar->usesScrollButtons());
        bar->setProperty(QT_TABS_FOCUS, static_cast<int>(bar->focusPolicy()));
        bar->setProperty(QT_TABS_HIDDEN, true);
        // Without scroll buttons the tabs' least size is theirs too: none
        bar->setUsesScrollButtons(false);
        bar->setFocusPolicy(Qt::NoFocus);
        bar->setStyle(noTabsStyle());
        return;
    }
    bar->setProperty(QT_TABS_HIDDEN, false);
    bar->setStyle(nullptr);  // the application's
    bar->setUsesScrollButtons(bar->property(QT_TABS_SCROLL_BUTTONS).toBool());
    bar->setFocusPolicy(static_cast<Qt::FocusPolicy>(bar->property(QT_TABS_FOCUS).toInt()));
}

void PanelTabs::refresh() {
    m_refreshPending = false;

    // The groups of panels Qt keeps, each in the order of its tabs
    QHash<const QDockWidget*, QList<QDockWidget*>> groups;
    for (QTabBar* bar : m_window->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly)) {
        QList<QDockWidget*> docks;
        bool own = true;
        for (int i = 0; i < bar->count() && own; ++i) {
            QDockWidget* dock = dockOfTab(bar, i);
            own = dock != nullptr;
            docks.append(dock);
        }
        // Tabs without panels yet (Qt fills them when the window shows) hide at once, so they
        // never take room; a group with a panel of another kind keeps Qt's tabs
        hideQtTabs(bar, own);
        if (!own || docks.size() < 2) {
            continue;
        }
        for (const QDockWidget* dock : std::as_const(docks)) {
            groups.insert(dock, docks);
        }
    }

    const bool refill = std::exchange(m_refill, false);
    for (Panel& panel : m_panels) {
        if (panel.dock.isNull()) {
            continue;
        }
        // A floating panel is a window of its own, without tabs
        const QList<QDockWidget*> group =
            panel.dock->isFloating() ? QList<QDockWidget*>() : groups.value(panel.dock.data());
        if (refill || group != panel.group) {
            panel.group = group;
            fill(panel);
        }
        const bool show = !group.isEmpty();
        if (panel.tabs->isHidden() == show) {
            panel.tabs->setVisible(show);
        }
    }
}

void PanelTabs::fill(Panel& panel) {
    QList<PanelTabBar::Tab> tabs;
    int current = -1;
    for (QDockWidget* dock : std::as_const(panel.group)) {
        const Panel* member = panelOf(dock);
        if (member == nullptr) {
            continue;
        }
        if (dock == panel.dock) {
            current = static_cast<int>(tabs.size());
        }
        tabs.append({dock, member->icon, dock->windowTitle(), toolTipOf(*member)});
    }
    panel.tabs->setTabs(tabs, current);
}

PanelTabs::Panel* PanelTabs::panelOf(const QDockWidget* dock) {
    for (Panel& panel : m_panels) {
        if (panel.dock == dock) {
            return &panel;
        }
    }
    return nullptr;
}

PanelTabBar* PanelTabs::tabBar(const QDockWidget* dock) const {
    for (const Panel& panel : m_panels) {
        if (!panel.dock.isNull() && panel.dock == dock) {
            return panel.tabs;
        }
    }
    return nullptr;
}

QString PanelTabs::toolTipOf(const Panel& panel) {
    const QString name = panel.dock->windowTitle();
    const Command* command = CommandRegistry::getInstance().getCommand(panel.commandId);
    const QString keys = command != nullptr
                             ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
                             : QString();
    return keys.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, keys);
}

void PanelTabs::choose(const QDockWidget* dock, int index) {
    Panel* panel = panelOf(dock);
    if (panel == nullptr || index < 0 || index >= panel->group.size()) {
        return;
    }
    const bool keys = panel->tabs->hasFocus();
    QDockWidget* chosen = panel->group.at(index);
    chosen->raise();
    // The keys stay on the tabs, which are the chosen panel's now
    if (keys) {
        if (Panel* front = panelOf(chosen)) {
            front->tabs->setFocus(Qt::TabFocusReason);
        }
    }
}

void PanelTabs::move(const QDockWidget* dock, int from, int to) {
    QTabBar* bar = qtTabsOf(dock);
    if (bar == nullptr || from == to || from < 0 || to < 0 || from >= bar->count() ||
        to >= bar->count()) {
        return;
    }
    // One place at a time: Qt reorders the group by the tabs on both sides of a move
    // (QDockAreaLayoutInfo::moveTab()), which gives the right order for neighbours only
    const int step = from < to ? 1 : -1;
    for (int i = from; i != to; i += step) {
        bar->moveTab(i, i + step);
    }
    refresh();
}

bool PanelTabs::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::LayoutRequest:
    case QEvent::ChildAdded:
        if (watched == m_window) {
            scheduleRefresh();
        }
        break;
    case QEvent::WindowTitleChange:
        if (watched != m_window) {
            m_refill = true;
            scheduleRefresh();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
