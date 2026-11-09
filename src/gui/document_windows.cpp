/// @file document_windows.cpp
/// @brief The texts of the book window in its tabs and in the windows of their own the tabs are
/// moved to

#include "kalahari/gui/document_windows.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/utils/tab_focus.h"
#include "kalahari/gui/widgets/status_bar_statistics.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMouseEvent>
#include <QScreen>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// The data of a tab being dragged between the bars of tabs of the windows of a book
constexpr const char* TAB_MIME = "application/x-kalahari-tab";

/// The window title of the book window without the name of the program: "Rain"
QString withoutProgramName(QString title) {
    for (const QString& suffix : {QStringLiteral(" - Kalahari"), QStringLiteral("Kalahari")}) {
        if (title.endsWith(suffix)) {
            title.chop(suffix.size());
            break;
        }
    }
    return title.trimmed();
}

/// Bring @p window to the front, with the keys in the page in front of @p tabs
void bringToFront(QWidget* window, QTabWidget& tabs) {
    if (window->isMinimized()) {
        window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    window->show();
    window->raise();
    window->activateWindow();
    utils::focusPageInFront(tabs);
}

/// The commands of the Window menu
constexpr const char* MOVE_TAB_COMMAND = "window.moveTabToNewWindow";
constexpr const char* RETURN_TAB_COMMAND = "window.returnTab";
constexpr const char* NEXT_WINDOW_COMMAND = "window.next";
constexpr const char* PREVIOUS_WINDOW_COMMAND = "window.previous";
constexpr const char* SETTINGS_COMMAND = "window.settings";

}  // namespace

// ============================================================================
// DocumentWindows
// ============================================================================

DocumentWindows& DocumentWindows::of(QTabWidget& bookTabs) {
    if (auto* found = bookTabs.findChild<DocumentWindows*>(QString(), Qt::FindDirectChildrenOnly)) {
        return *found;
    }
    return *new DocumentWindows(bookTabs);
}

DocumentWindows::DocumentWindows(QTabWidget& bookTabs)
    : QObject(&bookTabs), m_bookTabs(&bookTabs), m_frontTabs(&bookTabs) {
    setObjectName(QStringLiteral("documentWindows"));
    watch(bookTabs);
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        connect(app, &QApplication::focusChanged, this,
                [this](QWidget* /*old*/, QWidget* now) { onFocusChanged(now); });
    }
}

DocumentWindows::Look DocumentWindows::look() {
    const std::string look =
        core::SettingsManager::getInstance().get<std::string>(LOOK_KEY, "full");
    return look == "writing" ? Look::Writing : Look::Full;
}

DocumentWindows::Closing DocumentWindows::closing() {
    const std::string closing =
        core::SettingsManager::getInstance().get<std::string>(CLOSING_KEY, "return");
    return closing == "close" ? Closing::Close : Closing::Return;
}

bool DocumentWindows::isWhole() const {
    // Being destroyed, they are no longer a QTabWidget: their members must not be called
    return qobject_cast<const QTabWidget*>(m_bookTabs) != nullptr;
}

QList<QTabWidget*> DocumentWindows::allTabs() const {
    QList<QTabWidget*> tabs;
    if (isWhole()) {
        tabs.append(m_bookTabs);
    }
    for (DetachedWindow* window : windows()) {
        tabs.append(&window->tabs());
    }
    return tabs;
}

QList<DetachedWindow*> DocumentWindows::windows() const {
    QList<DetachedWindow*> open;
    const QList<DetachedWindow*> children =
        m_bookTabs->window()->findChildren<DetachedWindow*>(QString(), Qt::FindDirectChildrenOnly);
    for (DetachedWindow* window : children) {
        if (window->isOpen()) {
            open.append(window);
        }
    }
    return open;
}

QList<QWidget*> DocumentWindows::pages() const {
    QList<QWidget*> pages;
    for (QTabWidget* tabs : allTabs()) {
        for (int i = 0; i < tabs->count(); ++i) {
            pages.append(tabs->widget(i));
        }
    }
    return pages;
}

QTabWidget* DocumentWindows::frontTabs() const {
    // The tabs of a window of its own while it is open
    if (m_frontTabs == m_bookTabs || !allTabs().contains(m_frontTabs)) {
        return m_bookTabs;
    }
    return m_frontTabs;
}

QWidget* DocumentWindows::frontPage() const {
    return isWhole() ? frontTabs()->currentWidget() : nullptr;
}

bool DocumentWindows::isDetached(const QWidget* page) const {
    QTabWidget* tabs = tabsOf(page);
    return tabs != nullptr && tabs != m_bookTabs && allTabs().contains(tabs);
}

bool DocumentWindows::canDetach(const QWidget* page) {
    return qobject_cast<const EditorPanel*>(page) != nullptr;
}

QTabWidget* DocumentWindows::tabsOf(const QWidget* page) {
    // A page is in the stack of its tabs (QTabWidget > QStackedWidget > page)
    for (QWidget* parent = page ? page->parentWidget() : nullptr; parent != nullptr;
         parent = parent->parentWidget()) {
        if (auto* tabs = qobject_cast<QTabWidget*>(parent)) {
            return tabs->indexOf(const_cast<QWidget*>(page)) >= 0 ? tabs : nullptr;
        }
    }
    return nullptr;
}

QString DocumentWindows::tabText(const QWidget* page) {
    QTabWidget* tabs = tabsOf(page);
    return tabs ? tabs->tabText(tabs->indexOf(const_cast<QWidget*>(page))) : QString();
}

void DocumentWindows::setTabText(QWidget* page, const QString& text) {
    if (QTabWidget* tabs = tabsOf(page)) {
        tabs->setTabText(tabs->indexOf(page), text);
        if (auto* window = qobject_cast<DetachedWindow*>(tabs->window())) {
            window->followFront();  // its title names the text in front
        }
    }
}

void DocumentWindows::setTabIcon(QWidget* page, const QIcon& icon) {
    if (QTabWidget* tabs = tabsOf(page)) {
        tabs->setTabIcon(tabs->indexOf(page), icon);
    }
}

void DocumentWindows::removePage(QWidget* page) {
    if (QTabWidget* tabs = tabsOf(page)) {
        utils::removeTabKeepingFocus(*tabs, tabs->indexOf(page));
    }
}

void DocumentWindows::showPage(QWidget* page) {
    QTabWidget* tabs = tabsOf(page);
    if (tabs == nullptr) {
        return;
    }
    tabs->setCurrentWidget(page);
    setFrontTabs(tabs);
    QWidget* window = tabs->window();
    if (window->isMinimized()) {
        window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    window->raise();
    window->activateWindow();
}

// ----------------------------------------------------------------------------
// The page in front
// ----------------------------------------------------------------------------

void DocumentWindows::watch(QTabWidget& tabs) {
    QTabBar* bar = tabs.tabBar();
    bar->installEventFilter(this);
    bar->setAcceptDrops(true);
    connect(&tabs, &QTabWidget::currentChanged, this,
            [this, tabs = &tabs](int /*index*/) { setFrontTabs(tabs); });
}

void DocumentWindows::setFrontTabs(QTabWidget* tabs) {
    m_frontTabs = tabs;
    updateFront();
}

void DocumentWindows::updateFront() {
    if (!isWhole()) {
        return;  // the book window is being destroyed
    }
    QWidget* page = frontPage();
    if (m_frontSaid && page == m_frontPage) {
        return;
    }
    m_frontPage = page;
    m_frontSaid = true;
    // The Window menu follows the page in front
    CommandRegistry& registry = CommandRegistry::getInstance();
    if (registry.getCommand(MOVE_TAB_COMMAND) != nullptr) {
        registry.updateActionState(MOVE_TAB_COMMAND);
        registry.updateActionState(RETURN_TAB_COMMAND);
    }
    emit frontChanged(page);
}

void DocumentWindows::onFocusChanged(QWidget* now) {
    if (now == nullptr || !isWhole()) {
        return;
    }
    for (QTabWidget* tabs : allTabs()) {
        QWidget* page = tabs->currentWidget();
        if (page != nullptr && (page == now || page->isAncestorOf(now))) {
            setFrontTabs(tabs);
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Moving the tabs
// ----------------------------------------------------------------------------

DetachedWindow* DocumentWindows::detach(QWidget* page, std::optional<QPoint> at) {
    QTabWidget* from = tabsOf(page);
    auto* bookWindow = qobject_cast<QMainWindow*>(m_bookTabs->window());
    if (!canDetach(page) || from == nullptr || bookWindow == nullptr) {
        return nullptr;
    }
    // Alone in a window of its own already: it stays there
    if (from != m_bookTabs && from->count() == 1) {
        return qobject_cast<DetachedWindow*>(from->window());
    }

    auto* window = new DetachedWindow(*this, *bookWindow);
    watch(window->tabs());
    connect(window, &QObject::destroyed, this, [this]() {
        updateFront();
        emit windowsChanged();
    });

    // Two thirds of the window it comes from, on the screen of the place it goes to, whole
    QWidget* fromWindow = from->window();
    const QPoint place = at ? *at : fromWindow->frameGeometry().topLeft() + QPoint(48, 48);
    QScreen* screen = QGuiApplication::screenAt(place);
    if (screen == nullptr) {
        screen = fromWindow->screen();
    }
    const QRect available = screen->availableGeometry();
    const QSize size(std::min(std::max(fromWindow->width() * 2 / 3, 640), available.width()),
                     std::min(std::max(fromWindow->height() * 3 / 4, 480), available.height()));
    window->resize(size);
    // The title bar under the mouse, a little left of it, as where a tab was dropped
    QPoint topLeft = at ? place - QPoint(size.width() / 4, 12) : place;
    topLeft.setX(std::clamp(topLeft.x(), available.left(), available.right() - size.width()));
    topLeft.setY(std::clamp(topLeft.y(), available.top(), available.bottom() - size.height()));
    window->move(topLeft);

    moveTo(page, window->tabs());
    window->show();
    bringToFront(window, window->tabs());
    core::Logger::getInstance().info("A text went to a window of its own: {}",
                                     tabText(page).toStdString());
    emit windowsChanged();
    return window;
}

void DocumentWindows::moveTo(QWidget* page, QTabWidget& tabs, int index) {
    QTabWidget* from = tabsOf(page);
    if (from == nullptr) {
        return;
    }
    if (from == &tabs) {
        if (index >= 0) {
            from->tabBar()->moveTab(from->indexOf(page), std::min(index, from->count() - 1));
        }
        from->setCurrentWidget(page);
        setFrontTabs(from);
        return;
    }

    const int at = from->indexOf(page);
    const QString text = from->tabText(at);
    const QIcon icon = from->tabIcon(at);
    const QString toolTip = from->tabToolTip(at);
    utils::removeTabKeepingFocus(*from, at);
    const int to = tabs.insertTab(index < 0 || index > tabs.count() ? tabs.count() : index, page,
                                  icon, text);
    tabs.setTabToolTip(to, toolTip);
    tabs.setCurrentIndex(to);
    setFrontTabs(&tabs);
    if (auto* window = qobject_cast<DetachedWindow*>(tabs.window())) {
        window->followFront();
    }
}

void DocumentWindows::returnToBook(QWidget* page) {
    if (!isDetached(page)) {
        return;
    }
    moveTo(page, *m_bookTabs);
    bringToFront(m_bookTabs->window(), *m_bookTabs);
}

QList<QWidget*> DocumentWindows::bookWindows() const {
    QList<QWidget*> list{m_bookTabs->window()};
    for (DetachedWindow* window : windows()) {
        list.append(window);
    }
    return list;
}

void DocumentWindows::activateNextWindow(int step) {
    const QList<QWidget*> list = bookWindows();
    if (list.size() < 2) {
        return;
    }
    QWidget* active = QApplication::activeWindow();
    qsizetype at = list.indexOf(active);
    if (at < 0) {
        at = list.indexOf(frontTabs()->window());
    }
    const qsizetype count = list.size();
    QWidget* next = list.at(((std::max<qsizetype>(at, 0) + step) % count + count) % count);
    auto* window = qobject_cast<DetachedWindow*>(next);
    bringToFront(next, window ? window->tabs() : *m_bookTabs);
}

// ----------------------------------------------------------------------------
// The bars of the tabs: dragging a tab off, dropping it, its menu
// ----------------------------------------------------------------------------

bool DocumentWindows::eventFilter(QObject* watched, QEvent* event) {
    // The bar of tabs of the book; none while the bar or its tabs are being destroyed
    auto* bar = qobject_cast<QTabBar*>(watched);
    auto* tabs = bar != nullptr ? qobject_cast<QTabWidget*>(bar->parentWidget()) : nullptr;
    if (tabs == nullptr || !allTabs().contains(tabs)) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_pressedBar = bar;
            m_pressedIndex = bar->tabAt(mouse->position().toPoint());
            m_pressPos = mouse->position().toPoint();
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (m_pressedBar != bar || m_pressedIndex < 0 || !(mouse->buttons() & Qt::LeftButton)) {
            break;
        }
        // Off the bar, further than a tab is high: the tab leaves it
        const QPoint pos = mouse->position().toPoint();
        const int margin = std::max(QApplication::startDragDistance() * 2, bar->height());
        if (bar->rect().adjusted(-margin, -margin, margin, margin).contains(pos)) {
            break;
        }
        // The tab pressed is the one in front, wherever the bar moved it
        const int index = tabs->currentIndex();
        if (!canDetach(tabs->widget(index))) {
            break;
        }
        dragTab(*tabs, index);
        return true;
    }
    case QEvent::MouseButtonRelease:
        m_pressedBar = nullptr;
        m_pressedIndex = -1;
        break;
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto* drag = static_cast<QDragMoveEvent*>(event);
        if (drag->mimeData()->hasFormat(QString::fromLatin1(TAB_MIME)) && m_draggedPage) {
            drag->setDropAction(Qt::MoveAction);
            drag->accept();
            return true;
        }
        break;
    }
    case QEvent::Drop: {
        auto* drop = static_cast<QDropEvent*>(event);
        if (!drop->mimeData()->hasFormat(QString::fromLatin1(TAB_MIME)) || !m_draggedPage) {
            break;
        }
        QWidget* page = m_draggedPage;
        m_dropped = true;
        drop->setDropAction(Qt::MoveAction);
        drop->accept();
        // Where it was dropped among the tabs; after the last one past them
        moveTo(page, *tabs, bar->tabAt(drop->position().toPoint()));
        bringToFront(tabs->window(), *tabs);
        return true;
    }
    case QEvent::ContextMenu: {
        auto* menu = static_cast<QContextMenuEvent*>(event);
        int index = bar->tabAt(menu->pos());
        if (index < 0 && menu->reason() == QContextMenuEvent::Keyboard) {
            index = bar->currentIndex();
        }
        if (index >= 0) {
            showTabMenu(*tabs, index, menu->globalPos());
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void DocumentWindows::dragTab(QTabWidget& tabs, int index) {
    QWidget* page = tabs.widget(index);
    QTabBar* bar = tabs.tabBar();
    // Closed while it is dragged: nothing moves
    bool gone = false;
    const QMetaObject::Connection watching =
        connect(page, &QObject::destroyed, this, [this, &gone]() {
            gone = true;
            m_draggedPage = nullptr;
        });

    // The bar stops moving the tab among its tabs, as if the button was released where it was
    // pressed
    const QPoint pressPos = m_pressPos;
    m_pressedBar = nullptr;
    m_pressedIndex = -1;
    QMouseEvent release(QEvent::MouseButtonRelease, pressPos, bar->mapToGlobal(pressPos),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &release);

    auto* drag = new QDrag(bar);
    auto* data = new QMimeData;
    data->setData(QString::fromLatin1(TAB_MIME), QByteArray());
    drag->setMimeData(data);
    const int at = tabs.indexOf(page);
    if (at >= 0) {
        drag->setPixmap(bar->grab(bar->tabRect(at)));
    }
    m_draggedPage = page;
    m_dropped = false;
    drag->exec(Qt::MoveAction);
    m_draggedPage = nullptr;
    disconnect(watching);
    if (m_dropped || gone) {
        return;  // on a bar of tabs: moved there
    }

    // Dropped elsewhere: a window of its own where the mouse is; alone in one, it goes there
    QTabWidget* from = tabsOf(page);
    if (from == nullptr) {
        return;
    }
    auto* window = qobject_cast<DetachedWindow*>(from->window());
    if (window != nullptr && from->count() == 1) {
        window->move(QCursor::pos() - QPoint(window->width() / 4, 12));
        bringToFront(window, *from);
        return;
    }
    detach(page, QCursor::pos());
}

void DocumentWindows::showTabMenu(QTabWidget& tabs, int index, const QPoint& globalPos) {
    QWidget* page = tabs.widget(index);
    // Closed while its menu is open: the menu does nothing
    bool gone = false;
    const QMetaObject::Connection watching =
        connect(page, &QObject::destroyed, this, [&gone]() { gone = true; });
    QMenu menu(tabs.tabBar());
    //: The menu of a tab of a text: it goes to a window of its own. In Polish: &Przenieś do
    //: nowego okna
    QAction* toWindow = menu.addAction(tr("&Move to New Window"));
    toWindow->setEnabled(canDetach(page) && !(isDetached(page) && tabs.count() == 1));
    //: The menu of a tab in a window of its own: the text goes back to the book window. In
    //: Polish: Przy&wróć do okna książki
    QAction* back = menu.addAction(tr("&Return to Book Window"));
    back->setEnabled(isDetached(page));
    menu.addSeparator();
    //: The menu of a tab: closes it, as its cross does. In Polish: &Zamknij kartę
    QAction* close = menu.addAction(tr("&Close Tab"));

    // The item chosen with the mouse, its access key or a trigger() of its own, which exec()
    // does not return
    QAction* chosen = nullptr;
    connect(&menu, &QMenu::triggered, &menu, [&chosen](QAction* action) { chosen = action; });
    menu.exec(globalPos);
    disconnect(watching);
    if (chosen == nullptr || gone) {
        return;
    }
    if (chosen == toWindow) {
        detach(page);
    } else if (chosen == back) {
        returnToBook(page);
    } else if (chosen == close) {
        if (QTabWidget* in = tabsOf(page)) {
            emit in->tabCloseRequested(in->indexOf(page));
        }
    }
}

// ----------------------------------------------------------------------------
// The Window menu
// ----------------------------------------------------------------------------

int DocumentWindows::registerCommands(CommandRegistry& registry,
                                      const std::function<DocumentWindows*()>& windows) {
    int count = 0;
    const auto add = [&](const char* id, const char* menuText, int order, bool separatorAfter,
                         KeyboardShortcut shortcut, std::function<void()> execute,
                         std::function<bool()> enabled) {
        const QString text = tr(menuText);
        Command cmd;
        cmd.id = id;
        cmd.menuText = text.toStdString();
        cmd.label = withoutAccessKey(text).toStdString();
        cmd.tooltip = cmd.label;
        cmd.category = "WINDOW";
        cmd.menuPath = std::string("WINDOW/") + id;
        cmd.menuOrder = order;
        cmd.addSeparatorAfter = separatorAfter;
        cmd.showInMenu = true;
        cmd.shortcut = shortcut;
        cmd.execute = std::move(execute);
        cmd.isEnabled = std::move(enabled);
        registry.registerCommand(cmd);
        ++count;
    };

    add(MOVE_TAB_COMMAND,
        //: Window menu: the text in front goes to a window of its own. In Polish: &Przenieś
        //: kartę do nowego okna
        QT_TR_NOOP("&Move Tab to New Window"), 10, false, KeyboardShortcut(),
        [windows]() {
            if (DocumentWindows* w = windows()) w->detach(w->frontPage());
        },
        [windows]() {
            DocumentWindows* w = windows();
            QWidget* page = w ? w->frontPage() : nullptr;
            return canDetach(page) && !(w->isDetached(page) && w->frontTabs()->count() == 1);
        });
    add(RETURN_TAB_COMMAND,
        //: Window menu: the text in front, in a window of its own, goes back to the book
        //: window. In Polish: Przy&wróć kartę do okna książki
        QT_TR_NOOP("&Return Tab to Book Window"), 20, true, KeyboardShortcut(),
        [windows]() {
            if (DocumentWindows* w = windows()) w->returnToBook(w->frontPage());
        },
        [windows]() {
            DocumentWindows* w = windows();
            return w != nullptr && w->isDetached(w->frontPage());
        });
    add(NEXT_WINDOW_COMMAND,
        //: Window menu: the next window of the program comes to the front. In Polish:
        //: &Następne okno
        QT_TR_NOOP("&Next Window"), 30, false, KeyboardShortcut(Qt::Key_F6, Qt::ControlModifier),
        [windows]() {
            if (DocumentWindows* w = windows()) w->activateNextWindow(+1);
        },
        nullptr);
    add(PREVIOUS_WINDOW_COMMAND,
        //: Window menu: the previous window of the program comes to the front. In Polish:
        //: Popr&zednie okno
        QT_TR_NOOP("&Previous Window"), 40, true,
        KeyboardShortcut(Qt::Key_F6, Qt::ControlModifier | Qt::ShiftModifier),
        [windows]() {
            if (DocumentWindows* w = windows()) w->activateNextWindow(-1);
        },
        nullptr);
    add(SETTINGS_COMMAND,
        //: Window menu: Settings > General at the options of the windows. In Polish:
        //: &Ustawienia okien...
        QT_TR_NOOP("Window &Settings..."), 100, false, KeyboardShortcut(),
        [windows]() {
            if (DocumentWindows* w = windows()) emit w->settingsRequested();
        },
        nullptr);
    return count;
}

void DocumentWindows::listWindowsIn(QMenu* windowMenu) {
    if (windowMenu == nullptr) {
        return;
    }
    connect(windowMenu, &QMenu::aboutToShow, this, [this, windowMenu]() {
        qDeleteAll(m_windowActions);
        m_windowActions.clear();

        // Before Window Settings...: the book window, then its windows of their own
        QAction* settings =
            CommandRegistry::getInstance().getAction(QString::fromLatin1(SETTINGS_COMMAND));
        QAction* before = windowMenu->actions().contains(settings) ? settings : nullptr;
        QWidget* active = QApplication::activeWindow();
        const QList<QWidget*> list = bookWindows();
        for (qsizetype i = 0; i < list.size(); ++i) {
            QWidget* window = list.at(i);
            QString title = withoutProgramName(window->windowTitle());
            title.replace(QLatin1Char('&'), QStringLiteral("&&"));
            if (title.isEmpty()) {
                title = QStringLiteral("Kalahari");
            }
            // An open window in the Window menu: its number, its access key, and its title
            const QString text = i < 9 ? QStringLiteral("&%1 %2").arg(i + 1).arg(title)
                                       : QStringLiteral("%1 %2").arg(i + 1).arg(title);
            auto* action = new QAction(text, windowMenu);
            action->setCheckable(true);
            action->setChecked(window == active);
            // Gone with the window
            connect(action, &QAction::triggered, window, [this, window]() {
                auto* detached = qobject_cast<DetachedWindow*>(window);
                bringToFront(window, detached ? detached->tabs() : *m_bookTabs);
            });
            windowMenu->insertAction(before, action);
            m_windowActions.append(action);
        }
        QAction* separator = new QAction(windowMenu);
        separator->setSeparator(true);
        windowMenu->insertAction(before, separator);
        m_windowActions.append(separator);
    });
}

// ============================================================================
// DetachedWindow
// ============================================================================

DetachedWindow::DetachedWindow(DocumentWindows& owner, QMainWindow& bookWindow)
    : QMainWindow(&bookWindow, Qt::Window),
      m_owner(owner),
      m_bookWindow(bookWindow),
      m_tabs(new QTabWidget(this)),
      m_pageLabel(new QLabel(this)),
      m_zoomLabel(new QLabel(this)),
      m_statistics(nullptr) {
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_QuitOnClose, false);  // the program ends with the book window
    setObjectName(QStringLiteral("detachedWindow"));

    // Tabs as those of the book window
    m_tabs->setObjectName(QStringLiteral("detachedTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);
    utils::focusPageOnTabClick(*m_tabs);

    // Its settings: the gear at the end of the bar of the tabs
    auto* gear = new QToolButton(m_tabs);
    gear->setObjectName(QStringLiteral("windowSettingsButton"));
    gear->setAutoRaise(true);
    //: The gear at the end of the tabs of a window of its own: Settings > General, the
    //: options of the windows. In Polish: Ustawienia okien
    gear->setToolTip(tr("Window Settings"));
    gear->setAccessibleName(gear->toolTip());
    gear->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings")));
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, gear,
            [gear]() {
                gear->setIcon(
                    core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings")));
            });
    connect(gear, &QToolButton::clicked, &m_owner, &DocumentWindows::settingsRequested);
    m_tabs->setCornerWidget(gear, Qt::TopRightCorner);

    // Its tabs close as those of the book window: the book window asks about the changes
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (QWidget* page = m_tabs->widget(index)) {
            emit m_owner.closeRequested(page);
        }
    });
    // The last tab gone: the window closes
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index < 0 && !m_closing && !m_closingTexts) {
            close();
            return;
        }
        followFront();
    });

    // The page, the zoom and the counts of the text in front, as in the book window
    m_pageLabel->setMinimumWidth(110);
    m_pageLabel->hide();
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->hide();
    statusBar()->addPermanentWidget(m_pageLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    m_statistics = new StatusBarStatistics(statusBar(), this);

    // The commands of the book window, so their keys work here too
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const Command& command : registry.getAllCommands()) {
        if (command.canExecute()) {
            if (QAction* action = registry.getAction(command.id)) {
                addAction(action);
            }
        }
    }

    connect(&m_bookWindow, &QWidget::windowTitleChanged, this, &DetachedWindow::followFront);
    applyLook();
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [this](const std::string& key) {
            // Unsubscribed by the destructor; the settings may change on another thread
            if (key == DocumentWindows::LOOK_KEY) {
                QMetaObject::invokeMethod(this, "applyLook", Qt::QueuedConnection);
            }
        });
}

DetachedWindow::~DetachedWindow() {
    m_closing = true;  // no longer one of the windows of the book, nor its tabs in front
    // Its tabs and its editors are destroyed after it is no longer a DetachedWindow
    m_tabs->disconnect(this);
    for (const QMetaObject::Connection& connection : std::as_const(m_followConnections)) {
        disconnect(connection);
    }
    if (m_settingsListener >= 0) {
        core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    }
}

QMenu* DetachedWindow::createPopupMenu() {
    return nullptr;  // its toolbars are those of the book window, shown there
}

void DetachedWindow::applyLook() {
    const bool full = DocumentWindows::look() == DocumentWindows::Look::Full;

    // The menus of the book window
    QMenuBar* bar = menuBar();
    bar->clear();
    if (full) {
        for (QAction* menu : m_bookWindow.menuBar()->actions()) {
            bar->addAction(menu);
        }
    }
    bar->setVisible(full);

    // Its toolbars that are shown, or the Format toolbar, row after row, with the same
    // commands (not the boxes of the font and its size)
    for (QToolBar* toolbar : std::as_const(m_toolbars)) {
        removeToolBar(toolbar);
        toolbar->deleteLater();
    }
    m_toolbars.clear();
    QList<QToolBar*> sources;
    const QList<QToolBar*> bookToolbars =
        m_bookWindow.findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    for (QToolBar* source : bookToolbars) {
        if (full ? !source->isHidden() : source->objectName() == QLatin1String("format")) {
            sources.append(source);
        }
    }
    std::sort(sources.begin(), sources.end(), [](const QToolBar* a, const QToolBar* b) {
        return a->y() != b->y() ? a->y() < b->y() : a->x() < b->x();
    });
    const QToolBar* previous = nullptr;
    for (QToolBar* source : std::as_const(sources)) {
        if (previous != nullptr && previous->y() != source->y()) {
            addToolBarBreak();
        }
        auto* copy = addToolBar(source->windowTitle());
        copy->setObjectName(source->objectName());
        copy->setMovable(false);
        copy->setIconSize(source->iconSize());
        copy->setToolButtonStyle(source->toolButtonStyle());
        for (QAction* action : source->actions()) {
            if (action->isSeparator()) {
                copy->addSeparator();
            } else if (qobject_cast<QWidgetAction*>(action) == nullptr) {
                copy->addAction(action);
            }
        }
        copy->show();  // at once, also in a window shown already
        m_toolbars.append(copy);
        previous = source;
    }
}

void DetachedWindow::followFront() {
    QWidget* page = m_tabs->currentWidget();

    // "Chapter 3 - Rain - Kalahari": the text in front and the book window
    QString text = DocumentWindows::tabText(page);
    if (text.startsWith(QLatin1Char('*'))) {
        text.remove(0, 1);
    }
    const QString book = m_bookWindow.windowTitle();
    setWindowTitle(text.isEmpty()   ? book
                   : book.isEmpty() ? text
                                    : text + QStringLiteral(" - ") + book);

    // The status bar follows its editor
    auto* editor = qobject_cast<EditorPanel*>(page);
    editor::BookEditor* bookEditor = editor ? editor->getBookEditor() : nullptr;
    if (m_followed != page) {
        // Its connections go, also when the editor followed is gone
        for (const QMetaObject::Connection& connection : std::as_const(m_followConnections)) {
            disconnect(connection);
        }
        m_followConnections.clear();
        m_followed = page;
        if (bookEditor != nullptr) {
            m_followConnections = {
                connect(bookEditor, &editor::BookEditor::currentPageChanged, this,
                        [this](int) { updatePageStatus(); }),
                connect(bookEditor, &editor::BookEditor::totalPagesChanged, this,
                        [this](int) { updatePageStatus(); }),
                connect(bookEditor, &editor::BookEditor::zoomChanged, this,
                        [this](double) { updatePageStatus(); }),
                connect(bookEditor, &editor::BookEditor::viewModeChanged, this,
                        [this](editor::ViewMode) { updatePageStatus(); })};
        }
        m_statistics->setEditor(bookEditor);
    }
    updatePageStatus();
}

void DetachedWindow::updatePageStatus() {
    auto* editor = qobject_cast<EditorPanel*>(m_tabs->currentWidget());
    editor::BookEditor* bookEditor = editor ? editor->getBookEditor() : nullptr;
    if (bookEditor == nullptr) {
        m_pageLabel->hide();
        m_zoomLabel->hide();
        return;
    }
    const int total = bookEditor->totalPages();
    m_pageLabel->setVisible(total > 0);
    if (total > 0) {
        //: The status bar of a window of its own, as that of the book window. In Polish:
        //: Strona %1 z %2
        m_pageLabel->setText(tr("Page %1 of %2").arg(bookEditor->currentPage()).arg(total));
    }
    m_zoomLabel->setText(tr("%1%").arg(qRound(bookEditor->zoomFactor() * 100.0)));
    m_zoomLabel->show();
}

void DetachedWindow::closeEvent(QCloseEvent* event) {
    if (m_tabs->count() > 0 && DocumentWindows::closing() == DocumentWindows::Closing::Close) {
        // Each text closes as its tab would; a question about its changes can keep it, and
        // the window with it
        m_closingTexts = true;
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            if (QWidget* page = m_tabs->widget(i)) {
                emit m_owner.closeRequested(page);
            }
        }
        m_closingTexts = false;
        if (m_tabs->count() > 0) {
            event->ignore();
            return;
        }
    }

    m_closing = true;
    // The texts go back to the book window, the one in front last
    QWidget* front = m_tabs->currentWidget();
    while (m_tabs->count() > 0) {
        m_owner.moveTo(m_tabs->widget(0), m_owner.bookTabs());
    }
    if (front != nullptr) {
        m_owner.showPage(front);
    }
    event->accept();
}

} // namespace gui
} // namespace kalahari
