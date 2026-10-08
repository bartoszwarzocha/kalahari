/// @file test_distraction_free_layout.cpp
/// @brief The main window's layout for Distraction-Free writing: the parts it hides and
///        brings back, Esc, the menu shortcuts while the menu bar is hidden, and the menus
///        at the top edge

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/utils/distraction_free_layout.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <functional>

using kalahari::gui::utils::DistractionFreeLayout;

namespace {

/// A widget that uses Esc, as the find bar does
class EscapeUser : public QWidget {
public:
    using QWidget::QWidget;
    int escapes = 0;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            ++escapes;
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }
};

/// A main window like the program's: a menu bar with shortcuts (also in a submenu), two
/// toolbars (one hidden), three panels (two in tabs, one closed), the chapter tabs and a
/// status bar
struct TestWindow {
    QMainWindow window;
    QMenuBar* menuBar = nullptr;
    QAction* save = nullptr;        ///< File > Save, Ctrl+S
    QAction* zoomIn = nullptr;      ///< View > Zoom > Zoom In, Ctrl+=
    QAction* about = nullptr;       ///< Help > About, no shortcut
    QAction* quit = nullptr;        ///< File > Quit, Ctrl+Q, also the window's own
    QToolBar* fileBar = nullptr;
    QToolBar* hiddenBar = nullptr;  ///< Hidden before
    QDockWidget* navigator = nullptr;
    QDockWidget* properties = nullptr;  ///< In a tab with the navigator
    QDockWidget* log = nullptr;         ///< Closed before
    QStatusBar* status = nullptr;
    QTabWidget* tabs = nullptr;
    QWidget* text = nullptr;         ///< In the first tab, uses no keys
    EscapeUser* findBar = nullptr;   ///< In the first tab, uses Esc

    TestWindow() {
        menuBar = new QMenuBar(&window);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setNativeMenuBar(false);
        window.setMenuBar(menuBar);
        QMenu* file = menuBar->addMenu(QStringLiteral("File"));
        save = file->addAction(QStringLiteral("Save"));
        save->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
        quit = new QAction(QStringLiteral("Quit"), &window);
        quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
        file->addAction(quit);
        window.addAction(quit);
        QMenu* zoom = menuBar->addMenu(QStringLiteral("View"))->addMenu(QStringLiteral("Zoom"));
        zoomIn = zoom->addAction(QStringLiteral("Zoom In"));
        zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
        about = menuBar->addMenu(QStringLiteral("Help"))->addAction(QStringLiteral("About"));

        fileBar = window.addToolBar(QStringLiteral("File"));
        fileBar->setObjectName(QStringLiteral("fileBar"));
        fileBar->addAction(save);
        hiddenBar = window.addToolBar(QStringLiteral("Hidden"));
        hiddenBar->setObjectName(QStringLiteral("hiddenBar"));
        hiddenBar->hide();

        navigator = addPanel(QStringLiteral("Navigator"), Qt::LeftDockWidgetArea);
        properties = addPanel(QStringLiteral("Properties"), Qt::LeftDockWidgetArea);
        window.tabifyDockWidget(navigator, properties);
        log = addPanel(QStringLiteral("Log"), Qt::BottomDockWidgetArea);
        log->hide();

        tabs = new QTabWidget(&window);
        tabs->tabBar()->setObjectName(QStringLiteral("tabBar"));
        auto* chapter = new QWidget(tabs);
        text = new QWidget(chapter);
        text->setFocusPolicy(Qt::StrongFocus);
        findBar = new EscapeUser(chapter);
        findBar->setFocusPolicy(Qt::StrongFocus);
        tabs->addTab(chapter, QStringLiteral("Chapter 1"));
        tabs->addTab(new QLabel(QStringLiteral("Chapter 2"), tabs), QStringLiteral("Chapter 2"));
        window.setCentralWidget(tabs);

        status = window.statusBar();
        status->setObjectName(QStringLiteral("statusBar"));
        status->showMessage(QStringLiteral("Ready"));
        window.resize(640, 480);
    }

    QDockWidget* addPanel(const QString& name, Qt::DockWidgetArea area) {
        auto* dock = new QDockWidget(name, &window);
        dock->setObjectName(name);
        dock->setWidget(new QLabel(name, dock));
        window.addDockWidget(area, dock);
        return dock;
    }

    /// The parts around the text that Distraction-Free hides
    [[nodiscard]] QList<QWidget*> parts() const {
        return {menuBar, fileBar, navigator, properties, status, tabs->tabBar()};
    }
};

/// A key pressed in @p widget: the shortcuts first, then the widget and its parents
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = {}) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// A mouse event at @p pos in @p window, sent to the widget there
void sendMouse(QMainWindow& window, QEvent::Type type, const QPoint& pos, Qt::MouseButton button,
               Qt::MouseButtons buttons) {
    QWidget* target = window.childAt(pos);
    if (target == nullptr) {
        target = &window;
    }
    QMouseEvent event(type, target->mapFrom(&window, QPointF(pos)), window.mapToGlobal(QPointF(pos)),
                      button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &event);
}

/// The mouse moved to @p pos in @p window (with @p buttons held)
void moveMouse(QMainWindow& window, const QPoint& pos, Qt::MouseButtons buttons = Qt::NoButton) {
    sendMouse(window, QEvent::MouseMove, pos, Qt::NoButton, buttons);
}

/// A click at @p pos in @p window
void clickMouse(QMainWindow& window, const QPoint& pos) {
    sendMouse(window, QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton);
    sendMouse(window, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton);
}

/// The menus Distraction-Free shows at the top edge of @p window (none while it is off)
QMenuBar* topMenus(const QMainWindow& window) {
    return window.findChild<QMenuBar*>(QStringLiteral("distractionFreeMenuBar"));
}

/// Process events until @p done returns true or about two seconds pass
bool waitFor(const std::function<bool()>& done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return done();
}

/// Title of the tab on top of the tab group holding @p dock (empty without a group)
QString raisedTab(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return tabBar->tabText(tabBar->currentIndex());
            }
        }
    }
    return {};
}

}  // namespace

TEST_CASE("Distraction-Free layout: hides the window's parts and brings the layout back",
          "[gui][distraction-free]") {
    // Full screen only where no window reaches a real screen
    const bool offscreen = QGuiApplication::platformName() == QStringLiteral("offscreen");
    TestWindow w;
    w.window.move(100, 100);  // the whole window on the screen, so it comes back in place
    w.window.show();
    w.navigator->raise();  // not the tab added last
    QApplication::processEvents();
    REQUIRE(raisedTab(w.window, w.navigator) == QStringLiteral("Navigator"));
    const QByteArray state = w.window.saveState();
    const QRect geometry = w.window.geometry();

    DistractionFreeLayout layout(&w.window);
    QList<bool> changes;
    QObject::connect(&layout, &DistractionFreeLayout::activeChanged,
                     [&changes](bool active) { changes.append(active); });
    layout.setActive(true, {w.tabs->tabBar()}, offscreen);
    QApplication::processEvents();
    REQUIRE(layout.isActive());
    REQUIRE(changes == QList<bool>{true});

    // Only the text is left
    for (QWidget* part : w.parts()) {
        INFO(part->objectName().toStdString());
        CHECK(part->isHidden());
    }
    CHECK_FALSE(w.tabs->isHidden());
    CHECK_FALSE(w.text->isHidden());
    CHECK(w.window.isFullScreen() == offscreen);

    // Meanwhile the window carries the menu commands with a shortcut, also from submenus
    CHECK(w.window.actions().contains(w.save));
    CHECK(w.window.actions().contains(w.zoomIn));
    CHECK_FALSE(w.window.actions().contains(w.about));
    CHECK(w.window.actions().count(w.quit) == 1);

    // Turning it on again changes nothing
    layout.setActive(true, {}, offscreen);
    CHECK(changes.size() == 1);

    layout.setActive(false);
    QApplication::processEvents();
    REQUIRE_FALSE(layout.isActive());
    REQUIRE(changes == QList<bool>{true, false});

    for (QWidget* part : w.parts()) {
        INFO(part->objectName().toStdString());
        CHECK_FALSE(part->isHidden());
    }
    // What was hidden before stays hidden
    CHECK(w.hiddenBar->isHidden());
    CHECK(w.log->isHidden());
    // The toolbars and panels in their places and tabs, the window as it was
    CHECK(w.window.saveState() == state);
    CHECK(raisedTab(w.window, w.navigator) == QStringLiteral("Navigator"));
    CHECK_FALSE(w.window.isFullScreen());
    CHECK(w.window.geometry() == geometry);
    // The menu commands are the menu bar's only again; the window keeps its own
    CHECK_FALSE(w.window.actions().contains(w.save));
    CHECK_FALSE(w.window.actions().contains(w.zoomIn));
    CHECK(w.window.actions().count(w.quit) == 1);
}

TEST_CASE("Distraction-Free layout: a part deleted meanwhile is simply gone",
          "[gui][distraction-free]") {
    TestWindow w;
    w.window.show();
    QApplication::processEvents();
    DistractionFreeLayout layout(&w.window);
    layout.setActive(true, {w.tabs->tabBar()}, false);
    REQUIRE(w.window.actions().contains(w.zoomIn));

    // A toolbar it hid and a menu command it gave to the window
    delete w.fileBar;
    delete w.zoomIn;
    layout.setActive(false);
    QApplication::processEvents();

    CHECK_FALSE(w.menuBar->isHidden());
    CHECK_FALSE(w.navigator->isHidden());
    CHECK_FALSE(w.status->isHidden());
    CHECK_FALSE(w.tabs->tabBar()->isHidden());
    CHECK_FALSE(w.window.actions().contains(w.save));
    CHECK(w.window.actions().count(w.quit) == 1);
}

TEST_CASE("Distraction-Free layout: Esc that no widget uses turns it off", "[gui][distraction-free]") {
    TestWindow w;
    DistractionFreeLayout layout(&w.window);
    layout.setActive(true, {w.tabs->tabBar()}, false);
    REQUIRE(layout.isActive());

    SECTION("a widget that uses Esc keeps it on") {
        press(w.findBar, Qt::Key_Escape);
        CHECK(w.findBar->escapes == 1);
        CHECK(layout.isActive());
    }

    SECTION("Esc with a modifier keeps it on") {
        press(w.text, Qt::Key_Escape, Qt::ShiftModifier);
        CHECK(layout.isActive());
    }

    SECTION("Esc the text does not use turns it off") {
        press(w.text, Qt::Key_Escape);
        CHECK_FALSE(layout.isActive());
        CHECK_FALSE(w.menuBar->isHidden());
        CHECK_FALSE(w.tabs->tabBar()->isHidden());
    }
}

TEST_CASE("Distraction-Free layout: the menu shortcuts work while the menu bar is hidden",
          "[gui][distraction-free]") {
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    TestWindow w;
    w.window.show();
    w.window.activateWindow();
    REQUIRE(waitFor([&w] { return QApplication::activeWindow() == &w.window; }));
    w.text->setFocus();
    int saved = 0;
    int zoomed = 0;
    QObject::connect(w.save, &QAction::triggered, [&saved] { ++saved; });
    QObject::connect(w.zoomIn, &QAction::triggered, [&zoomed] { ++zoomed; });

    // A hidden menu bar takes the shortcuts of the commands only it shows with it
    w.menuBar->hide();
    press(w.text, Qt::Key_Equal, Qt::ControlModifier);
    REQUIRE(zoomed == 0);
    w.menuBar->show();
    press(w.text, Qt::Key_Equal, Qt::ControlModifier);
    REQUIRE(zoomed == 1);

    // With the toolbars hidden too, also the commands of the toolbars
    DistractionFreeLayout layout(&w.window);
    layout.setActive(true, {}, false);
    REQUIRE(w.menuBar->isHidden());
    REQUIRE(w.fileBar->isHidden());
    press(w.text, Qt::Key_S, Qt::ControlModifier);
    press(w.text, Qt::Key_Equal, Qt::ControlModifier);
    CHECK(saved == 1);
    CHECK(zoomed == 2);

    // Back on the menu bar, a shortcut triggers its command once
    layout.setActive(false);
    press(w.text, Qt::Key_S, Qt::ControlModifier);
    press(w.text, Qt::Key_Equal, Qt::ControlModifier);
    CHECK(saved == 2);
    CHECK(zoomed == 3);
}

TEST_CASE("Distraction-Free layout: the menus show over the text at the top edge",
          "[gui][distraction-free]") {
    TestWindow w;
    w.window.show();
    QApplication::processEvents();
    DistractionFreeLayout layout(&w.window);
    layout.setActive(true, {w.tabs->tabBar()}, false);
    QApplication::processEvents();
    const QRect text = w.tabs->geometry();

    QMenuBar* menus = topMenus(w.window);
    REQUIRE(menus != nullptr);
    CHECK(menus->isHidden());

    // The mouse over the text changes nothing
    moveMouse(w.window, QPoint(320, 200));
    CHECK(menus->isHidden());

    // At the top edge the menus of the menu bar show over the text, as wide as the window;
    // the text stays in its place
    moveMouse(w.window, QPoint(320, 0));
    REQUIRE_FALSE(menus->isHidden());
    CHECK(menus->actions() == w.menuBar->actions());
    CHECK(menus->geometry() == QRect(0, 0, w.window.width(), menus->sizeHint().height()));
    CHECK(w.menuBar->isHidden());
    QApplication::processEvents();
    CHECK(w.tabs->geometry() == text);

    // Over them and a little below them they stay
    moveMouse(w.window, QPoint(100, menus->height() - 1));
    moveMouse(w.window, QPoint(100, menus->height() + 5));
    CHECK_FALSE(menus->isHidden());

    // A window of another size: the menus as wide
    w.window.resize(700, 500);
    CHECK(waitFor([menus] { return menus->width() == 700; }));

    // Away from them they hide
    moveMouse(w.window, QPoint(320, 200));
    CHECK(menus->isHidden());

    // Also a click in the text hides them
    moveMouse(w.window, QPoint(320, 0));
    REQUIRE_FALSE(menus->isHidden());
    clickMouse(w.window, QPoint(320, 200));
    CHECK(menus->isHidden());
    CHECK(layout.isActive());

    // Not for a selection dragged to the top
    moveMouse(w.window, QPoint(320, 0), Qt::LeftButton);
    CHECK(menus->isHidden());

    // Turned off, they are gone and the menu bar is back in its place
    moveMouse(w.window, QPoint(320, 0));
    REQUIRE_FALSE(menus->isHidden());
    layout.setActive(false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(topMenus(w.window) == nullptr);
    CHECK_FALSE(w.menuBar->isHidden());
}

TEST_CASE("Distraction-Free layout: an open menu keeps the menus at the top",
          "[gui][distraction-free]") {
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("opens a menu in an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    TestWindow w;
    w.window.show();
    w.window.activateWindow();
    REQUIRE(waitFor([&w] { return QApplication::activeWindow() == &w.window; }));
    w.text->setFocus();
    DistractionFreeLayout layout(&w.window);
    layout.setActive(true, {w.tabs->tabBar()}, false);
    QApplication::processEvents();
    moveMouse(w.window, QPoint(320, 0));
    QMenuBar* menus = topMenus(w.window);
    REQUIRE(menus != nullptr);
    REQUIRE_FALSE(menus->isHidden());

    // A click on File opens its menu, as on the menu bar
    QAction* file = w.menuBar->actions().constFirst();
    const QPoint onFile = menus->actionGeometry(file).center();
    clickMouse(w.window, onFile);
    REQUIRE(waitFor([file] { return QApplication::activePopupWidget() == file->menu(); }));

    // The mouse far below the menus while File is open
    moveMouse(w.window, QPoint(320, 200));
    CHECK_FALSE(menus->isHidden());

    SECTION("closed, the menus hide when the mouse moves away") {
        file->menu()->hide();
        REQUIRE(QApplication::activePopupWidget() == nullptr);
        moveMouse(w.window, QPoint(320, 200));
        CHECK(menus->isHidden());
        CHECK(w.text->hasFocus());
    }

    SECTION("closed with Esc, the keys come back to the text when the mouse moves away") {
        // The keys move through the menus after Esc closed one of them
        press(file->menu(), Qt::Key_Escape);
        REQUIRE(QApplication::activePopupWidget() == nullptr);
        REQUIRE(menus->hasFocus());
        moveMouse(w.window, QPoint(320, 200));
        CHECK(menus->isHidden());
        CHECK(w.text->hasFocus());
        CHECK(layout.isActive());
    }
}
