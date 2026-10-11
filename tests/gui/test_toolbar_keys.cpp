/// @file test_toolbar_keys.cpp
/// @brief After a command given in a toolbar the keys go back where they were (ToolbarKeys)

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/toolbar_keys.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The window's tests need an active window; the offscreen platform has one without a window
/// on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

/// Whether a window is active with the keys in this widget, after the events that give them
bool keysIn(QWidget* window, QWidget* widget) {
    return test::waitUntil([window, widget] {
        return QApplication::activeWindow() == window && QApplication::focusWidget() == widget;
    });
}

/// Whether the keys stay in this widget of the window while the events that might move them
/// come
bool keysStayIn(QWidget* window, QWidget* widget) {
    test::runEventLoop(50);
    return keysIn(window, widget);
}

/// Press a key in the widget with the keys
void press(Qt::Key key, const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text);
    QApplication::sendEvent(QApplication::focusWidget(), &event);
}

/// Type digits or letters in the widget with the keys
void type(const QString& text) {
    for (const QChar c : text) {
        const auto key = c.isDigit() ? static_cast<Qt::Key>(Qt::Key_0 + c.digitValue())
                                     : static_cast<Qt::Key>(Qt::Key_A + c.toUpper().unicode() - 'A');
        press(key, QString(c));
    }
}

/// An editable font box, as the toolbar's (QFontComboBox) is from the start
QComboBox* fontBoxIn(QToolBar* toolbar) {
    auto* box = new QComboBox(toolbar);
    box->setEditable(true);
    box->addItems({QStringLiteral("Serif"), QStringLiteral("Sans"), QStringLiteral("Mono")});
    return box;
}

/// The window with the keys in its text, and a toolbar with a button and two fields
struct Desk {
    QMainWindow window;
    QWidget* central = new QWidget(&window);
    QLineEdit* title = new QLineEdit(central);
    QLineEdit* text = new QLineEdit(central);
    QToolBar* toolbar = new QToolBar(QStringLiteral("Format"), &window);
    QAction* bold = toolbar->addAction(QStringLiteral("Bold"));
    QComboBox* font = fontBoxIn(toolbar);
    QSpinBox* size = new QSpinBox(toolbar);
    QToolButton* button = nullptr;
    ToolbarKeys keys{&window};
    int bolds = 0;
    bool toolbarHadKeys = false;  ///< The floating toolbar's window had the keys at some time

    /// @param floating Floating, a window of its own, or docked in the window
    explicit Desk(bool floating = true) {
        auto* layout = new QVBoxLayout(central);
        layout->addWidget(title);
        layout->addWidget(text);
        window.setCentralWidget(central);
        window.setGeometry(0, 0, 400, 300);
        QObject::connect(bold, &QAction::triggered, [this]() { ++bolds; });
        size->setRange(6, 72);
        size->setValue(12);
        toolbar->addWidget(font);
        toolbar->addWidget(size);
        button = qobject_cast<QToolButton*>(toolbar->widgetForAction(bold));

        if (floating) {
            // As QToolBar makes it: a window of its own, beside the main one
            toolbar->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
            toolbar->move(450, 50);
        } else {
            window.addToolBar(toolbar);
        }
        window.show();
        window.activateWindow();
        REQUIRE(test::waitUntil([this] { return QApplication::activeWindow() == &window; }));
        QCursor::setPos(text->mapToGlobal(text->rect().center()));  // away from the toolbar
        text->setFocus(Qt::OtherFocusReason);
        REQUIRE(keysIn(&window, text));
        if (floating) {
            toolbar->show();
            REQUIRE(toolbar->isFloating());
        }
        REQUIRE(button != nullptr);
        REQUIRE(keysStayIn(&window, text));  // the system may give a window it shows the keys

        QObject::connect(qApp, &QGuiApplication::focusWindowChanged, toolbar,
                         [this](QWindow* focused) {
                             toolbarHadKeys = toolbarHadKeys || focused == toolbar->windowHandle();
                         });
    }

    /// The window with the toolbar
    QWidget* toolbarWindow() const { return toolbar->window(); }

    /// The middle of the size field's arrows, outside its text
    QPoint sizeArrows() const {
        const auto* sizeText = size->findChild<QLineEdit*>();
        REQUIRE(sizeText != nullptr);
        const int left = sizeText->geometry().right() + 1;
        REQUIRE(left < size->width());
        return size->mapToGlobal(QPoint((left + size->width()) / 2, size->height() / 4));
    }

    /// The system gives the floating toolbar the keys at a click at this point
    void clickActivates(const QPoint& point) {
        QCursor::setPos(point);
        toolbar->activateWindow();
        REQUIRE(test::waitUntil([this] { return toolbarHadKeys; }));
    }

    /// The system gives the floating toolbar the keys at a click on this widget
    void clickActivates(QWidget* widget) {
        clickActivates(widget->mapToGlobal(widget->rect().center()));
    }

    /// A click at the point gives the field the keys: the toolbar's window takes them first,
    /// when it is floating
    void clickField(QWidget* field, const QPoint& point) {
        if (toolbar->isFloating() && QApplication::activeWindow() != toolbar) {
            clickActivates(point);
        } else {
            QCursor::setPos(point);
        }
        field->setFocus(Qt::MouseFocusReason);
    }

    /// Open the font list, over everything without taking the keys: the system's activation
    /// of the list is ignored while it is open
    void openList() {
        font->showPopup();
        REQUIRE(test::waitUntil([this] { return font->view()->isVisible(); }));
        test::runEventLoop(50);
    }

    /// A click on the field's text gives it the keys
    void clickField(QWidget* field) {
        clickField(field, field->mapToGlobal(field->rect().center()));
        REQUIRE(keysIn(toolbarWindow(), field));
    }
};

}  // namespace

TEST_CASE("Floating toolbar: a click on a button gives the keys back where they were",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk;

    desk.clickActivates(desk.button);
    CHECK(keysIn(&desk.window, desk.text));

    desk.button->click();
    CHECK(desk.bolds == 1);
    CHECK(keysIn(&desk.window, desk.text));
}

TEST_CASE("Floating toolbar: one types in its fields; a command gives the keys back",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk;

    desk.clickField(desk.size);
    REQUIRE(keysStayIn(desk.toolbar, desk.size));
    desk.size->selectAll();
    type(QStringLiteral("18"));
    CHECK(desk.size->value() == 18);

    SECTION("the keys move between its fields with Tab") {
        press(Qt::Key_Tab);
        CHECK(keysStayIn(desk.toolbar, desk.font));
    }

    SECTION("a command given there") {
        desk.button->click();
        CHECK(desk.bolds == 1);
        CHECK(keysIn(&desk.window, desk.text));
    }
}

TEST_CASE("Floating toolbar: the keys go back when the system gives them to it",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk;

    desk.clickActivates(desk.text);  // the pointer over the main window
    CHECK(keysIn(&desk.window, desk.text));
}

TEST_CASE("Floating toolbar: the keys go back to a floating panel that had them",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk;
    QWidget panel(&desk.window, Qt::Tool);  // as a floating panel
    auto* filter = new QLineEdit(&panel);
    panel.setGeometry(0, 350, 300, 100);
    panel.show();
    panel.activateWindow();
    filter->setFocus(Qt::OtherFocusReason);
    REQUIRE(keysIn(&panel, filter));

    desk.clickActivates(desk.button);
    CHECK(keysIn(&panel, filter));
}

TEST_CASE("Toolbar fields: Enter and Escape give the keys back where they were",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    const bool floating = GENERATE(false, true);
    CAPTURE(floating);
    Desk desk(floating);

    SECTION("Enter takes the size") {
        desk.clickField(desk.size);
        desk.size->selectAll();
        type(QStringLiteral("18"));
        press(Qt::Key_Return, QStringLiteral("\r"));
        CHECK(desk.size->value() == 18);
        CHECK(keysIn(&desk.window, desk.text));
    }

    SECTION("Enter takes the font") {
        desk.clickField(desk.font);
        desk.font->lineEdit()->selectAll();
        type(QStringLiteral("Mono"));
        press(Qt::Key_Enter, QStringLiteral("\r"));
        CHECK(desk.font->currentText() == QStringLiteral("Mono"));
        CHECK(keysIn(&desk.window, desk.text));
    }

    SECTION("Escape leaves what was typed in the font field") {
        desk.clickField(desk.font);
        desk.font->lineEdit()->selectAll();
        type(QStringLiteral("Mo"));
        press(Qt::Key_Escape);
        CHECK(desk.font->currentText() == QStringLiteral("Serif"));
        CHECK(keysIn(&desk.window, desk.text));
    }

    SECTION("the keys go back to the element that had them") {
        desk.title->setFocus(Qt::OtherFocusReason);
        REQUIRE(keysIn(&desk.window, desk.title));
        desk.clickField(desk.size);
        press(Qt::Key_Escape);
        CHECK(keysIn(&desk.window, desk.title));
    }
}

TEST_CASE("Toolbar fields: a choice in the font list gives the keys back; the arrows keep them",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    const bool floating = GENERATE(false, true);
    CAPTURE(floating);
    Desk desk(floating);
    desk.clickField(desk.font);

    // The arrows in the field go through the fonts, the keys there
    press(Qt::Key_Down);
    CHECK(desk.font->currentIndex() == 1);
    CHECK(keysStayIn(desk.toolbarWindow(), desk.font));

    // The list closed without a choice leaves them there too
    desk.openList();
    desk.font->hidePopup();
    REQUIRE(test::waitUntil([&desk] { return !desk.font->view()->isVisible(); }));
    CHECK(keysStayIn(desk.toolbarWindow(), desk.font));

    // A choice in the list
    desk.openList();
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(desk.font->view(), &down);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
    QApplication::sendEvent(desk.font->view(), &enter);
    CHECK(desk.font->currentIndex() == 2);
    CHECK(keysIn(&desk.window, desk.text));
}

TEST_CASE("Toolbar fields: a click on the size arrows leaves the keys where they were",
          "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    const bool floating = GENERATE(false, true);
    CAPTURE(floating);
    Desk desk(floating);

    SECTION("in the text") {
        desk.clickField(desk.size, desk.sizeArrows());
        CHECK(keysIn(&desk.window, desk.text));
    }

    SECTION("in the font field") {
        desk.clickField(desk.font);
        desk.clickField(desk.size, desk.sizeArrows());
        CHECK(keysStayIn(desk.toolbarWindow(), desk.font));
    }

    SECTION("a click on its text gives them to it") {
        desk.clickField(desk.size);
        CHECK(keysStayIn(desk.toolbarWindow(), desk.size));
    }
}

TEST_CASE("Docked toolbar: a command gives the keys back from its field", "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk(false);
    desk.clickField(desk.size);

    desk.button->click();
    CHECK(desk.bolds == 1);
    CHECK(keysIn(&desk.window, desk.text));
}

TEST_CASE("Toolbar fields made later give the keys back too", "[gui][toolbar]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    Desk desk(false);
    auto* spacing = new QSpinBox(desk.toolbar);  // as the toolbar is built again
    desk.toolbar->addWidget(spacing);
    REQUIRE(test::waitUntil([spacing] { return spacing->isVisible(); }));

    desk.clickField(spacing);
    press(Qt::Key_Return, QStringLiteral("\r"));
    CHECK(keysIn(&desk.window, desk.text));
}
