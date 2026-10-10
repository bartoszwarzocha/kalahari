/// @file test_widget_keys.cpp
/// @brief Qt's fields, lists and buttons with the keys of Windows, also on Linux (WidgetKeys)
///
/// The tests give the widgets the filter of Linux on every system. Where Qt already gives
/// a widget the keys of Windows (on Windows, and on Linux with the offscreen platform the
/// tests run on) they check that the filter keeps them; under X11 with a desktop's keys
/// (QT_QPA_PLATFORM=xcb, XDG_CURRENT_DESKTOP=GNOME) they check what it changes.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/widget_keys.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDialog>
#include <QGroupBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStringList>
#include <QTextCursor>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <memory>
#include <utility>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

constexpr Qt::KeyboardModifiers NONE = Qt::NoModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers ALT = Qt::AltModifier;

/// macOS keeps the keys of its own fields: the program gives them no filter there
bool onMacOS() {
    return currentShortcutPlatform() == ShortcutPlatform::MacOS;
}

/// The filter of the keys of a system on the application while a test runs
class InstalledKeys {
public:
    explicit InstalledKeys(ShortcutPlatform platform)
        : m_keys(platform) {
        QCoreApplication::instance()->installEventFilter(&m_keys);
    }
    ~InstalledKeys() { QCoreApplication::instance()->removeEventFilter(&m_keys); }
    InstalledKeys(const InstalledKeys&) = delete;
    InstalledKeys& operator=(const InstalledKeys&) = delete;

private:
    WidgetKeys m_keys;
};

/// A key pressed and released in a widget (and on to its parents while none takes it);
/// whether a widget took the press
bool press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = NONE,
           const QString& text = QString()) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers, text);
    QApplication::sendEvent(widget, &released);
    return pressed.isAccepted();
}

/// Whether a widget with the keys keeps a key from the window's shortcuts
bool keepsFromShortcuts(QWidget* widget, int key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();  // as the shortcuts ask
    QApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// The whole text of a menu's item, found by its label without the keys
QString itemText(const QMenu& menu, const QString& label) {
    for (const QAction* action : menu.actions()) {
        if (action->text().section(QLatin1Char('\t'), 0, 0) == label) {
            return action->text();
        }
    }
    return QString();
}

/// An item of Qt's menus of the fields with its keys after a tab
QString withKeys(const char* context, const char* label, QKeyCombination keys) {
    return QCoreApplication::translate(context, label) + QLatin1Char('\t') +
           ShortcutRules::keysText(keys);
}

/// The items of the menus of the fields that have keys, with the keys of Windows
void checkMenuKeys(const QMenu& menu, const char* context) {
    const std::pair<const char*, QKeyCombination> items[] = {
        {"&Undo", QKeyCombination(CTRL, Qt::Key_Z)}, {"&Redo", QKeyCombination(CTRL, Qt::Key_Y)},
        {"Cu&t", QKeyCombination(CTRL, Qt::Key_X)},  {"&Copy", QKeyCombination(CTRL, Qt::Key_C)},
        {"&Paste", QKeyCombination(CTRL, Qt::Key_V)},
        {"Select All", QKeyCombination(CTRL, Qt::Key_A)}};
    for (const auto& [label, keys] : items) {
        CAPTURE(label);
        CHECK(itemText(menu, QCoreApplication::translate(context, label)) ==
              withKeys(context, label, keys));
    }
    // An item without keys stays as it is
    const QString deleteLabel = QCoreApplication::translate(context, "Delete");
    CHECK(itemText(menu, deleteLabel) == deleteLabel);
}

bool clipboardWorks() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return false;
    }
    clipboard->setText(QStringLiteral("__kalahari_widget_keys__"));
    QCoreApplication::processEvents();
    return clipboard->text() == QStringLiteral("__kalahari_widget_keys__");
}

/// A line that counts its context menus instead of showing them
class MenuLine : public QLineEdit {
public:
    int menus = 0;
    QContextMenuEvent::Reason reason = QContextMenuEvent::Other;
    QPoint position;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override {
        ++menus;
        reason = event->reason();
        position = event->pos();
        event->accept();
    }
};

/// A widget that takes Shift+F10 itself and opens its menu with it, as the book editor does
class MenuKeyWidget : public QWidget {
public:
    int keys = 0;
    int menus = 0;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_F10 && event->modifiers() == SHIFT) {
            ++keys;
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent* event) override {
        ++menus;
        event->accept();
    }
};

/// A line that goes while it handles Shift+F10
class VanishingLine : public QLineEdit {
public:
    explicit VanishingLine(int& menus)
        : m_menus(menus) {}

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_F10) {
            event->ignore();
            delete this;
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent* event) override {
        ++m_menus;
        event->accept();
    }

private:
    int& m_menus;
};

/// A filter that takes Enter on a button itself, as the frame of an annotation does
class EnterFilter : public QObject {
public:
    int enters = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return) {
            ++enters;
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

}  // namespace

TEST_CASE("The program gives the widgets the keys of Windows on Windows and Linux",
          "[gui][widget_keys]") {
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    REQUIRE(app != nullptr);
    installWidgetKeys(*app);
    auto* installed = app->findChild<WidgetKeys*>(QString(), Qt::FindDirectChildrenOnly);
    CHECK((installed != nullptr) == !onMacOS());
    delete installed;  // each test installs the filter it checks
}

TEST_CASE("Fields undo and redo with the keys of Windows", "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);

    SECTION("A line: Alt+Backspace undoes, Ctrl+Y and Alt+Shift+Backspace redo") {
        QLineEdit line;
        line.insert(QStringLiteral("kot"));
        // The line's, not the window's Undo and Redo
        CHECK(keepsFromShortcuts(&line, Qt::Key_Y, CTRL));
        CHECK(keepsFromShortcuts(&line, Qt::Key_Backspace, ALT));
        CHECK(keepsFromShortcuts(&line, Qt::Key_Backspace, ALT | SHIFT));

        CHECK(press(&line, Qt::Key_Backspace, ALT));
        CHECK(line.text().isEmpty());
        CHECK(press(&line, Qt::Key_Y, CTRL));
        CHECK(line.text() == QStringLiteral("kot"));
        press(&line, Qt::Key_Z, CTRL);
        CHECK(line.text().isEmpty());
        press(&line, Qt::Key_Backspace, ALT | SHIFT);
        CHECK(line.text() == QStringLiteral("kot"));
    }

    SECTION("A text: the same keys") {
        QPlainTextEdit plain;
        QTextEdit rich;
        const auto undoAndRedo = [](auto& text) {
            text.insertPlainText(QStringLiteral("kot"));
            CHECK(keepsFromShortcuts(&text, Qt::Key_Y, CTRL));
            CHECK(keepsFromShortcuts(&text, Qt::Key_Backspace, ALT));
            press(&text, Qt::Key_Backspace, ALT);
            CHECK(text.toPlainText().isEmpty());
            press(&text, Qt::Key_Y, CTRL);
            CHECK(text.toPlainText() == QStringLiteral("kot"));
            press(&text, Qt::Key_Backspace, ALT);
            press(&text, Qt::Key_Backspace, ALT | SHIFT);
            CHECK(text.toPlainText() == QStringLiteral("kot"));
        };
        undoAndRedo(plain);
        undoAndRedo(rich);
    }

    SECTION("A number field and a list one types into: the keys go to their line") {
        QSpinBox spin;
        spin.setRange(0, 999);
        spin.setValue(5);
        press(&spin, Qt::Key_End);
        press(&spin, Qt::Key_7, NONE, QStringLiteral("7"));
        REQUIRE(spin.text() == QStringLiteral("57"));
        CHECK(keepsFromShortcuts(&spin, Qt::Key_Y, CTRL));
        press(&spin, Qt::Key_Backspace, ALT);
        CHECK(spin.text() == QStringLiteral("5"));
        press(&spin, Qt::Key_Y, CTRL);
        CHECK(spin.text() == QStringLiteral("57"));

        QComboBox combo;
        combo.setEditable(true);
        combo.lineEdit()->insert(QStringLiteral("kot"));
        CHECK(keepsFromShortcuts(&combo, Qt::Key_Y, CTRL));
        press(&combo, Qt::Key_Backspace, ALT);
        CHECK(combo.currentText().isEmpty());
        press(&combo, Qt::Key_Y, CTRL);
        CHECK(combo.currentText() == QStringLiteral("kot"));
    }

    SECTION("Read-only: a line takes the keys and does nothing, a text passes them on") {
        QLineEdit line(QStringLiteral("kot"));
        line.setReadOnly(true);
        QPlainTextEdit text(QStringLiteral("kot"));
        text.setReadOnly(true);
        for (QWidget* field : {static_cast<QWidget*>(&line), static_cast<QWidget*>(&text)}) {
            // The window's Undo and Redo, as on Windows
            CHECK_FALSE(keepsFromShortcuts(field, Qt::Key_Y, CTRL));
            CHECK_FALSE(keepsFromShortcuts(field, Qt::Key_Backspace, ALT));
        }
        CHECK(press(&line, Qt::Key_Y, CTRL));
        CHECK(press(&line, Qt::Key_Backspace, ALT));
        CHECK(line.text() == QStringLiteral("kot"));
        CHECK_FALSE(press(&text, Qt::Key_Y, CTRL));
        CHECK_FALSE(press(&text, Qt::Key_Backspace, ALT));
        CHECK(text.toPlainText() == QStringLiteral("kot"));
    }
}

TEST_CASE("The keys Qt gives the fields on Linux only do nothing there",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);
    const QString sentence = QStringLiteral("Ala ma kota");

    QLineEdit line;
    QPlainTextEdit text;
    QTreeWidget list;
    new QTreeWidgetItem(&list, QStringList{sentence});
    list.setCurrentItem(list.topLevelItem(0));
    const bool clipboard = clipboardWorks();
    const QString onClipboard = QStringLiteral("schowek");

    for (const QKeyCombination pressed : WidgetKeys::linuxOnlyKeys()) {
        CAPTURE(ShortcutRules::keysText(pressed).toStdString());
        const int key = pressed.key();
        const Qt::KeyboardModifiers modifiers = pressed.keyboardModifiers();
        if (clipboard) {
            QGuiApplication::clipboard()->setText(onClipboard);
        }

        // A line: the window's shortcuts get them (Ctrl+E centers the paragraph, Ctrl+U
        // underlines); with none, nothing moves, is deleted, pasted or deselected
        line.setText(sentence);
        line.setSelection(4, 2);  // "ma"
        CHECK_FALSE(keepsFromShortcuts(&line, key, modifiers));
        CHECK_FALSE(press(&line, key, modifiers));
        CHECK(line.text() == sentence);
        CHECK(line.selectedText() == QStringLiteral("ma"));
        CHECK(line.cursorPosition() == 6);

        // A text
        text.setPlainText(sentence);
        QTextCursor cursor = text.textCursor();
        cursor.setPosition(4);
        cursor.setPosition(6, QTextCursor::KeepAnchor);
        text.setTextCursor(cursor);
        CHECK_FALSE(press(&text, key, modifiers));
        CHECK(text.toPlainText() == sentence);
        CHECK(text.textCursor().selectedText() == QStringLiteral("ma"));

        // A list
        CHECK_FALSE(press(&list, key, modifiers));

        // F16 copied, F20 cut
        if (clipboard) {
            CHECK(QGuiApplication::clipboard()->text() == onClipboard);
        }
    }

    // A number field: Ctrl+U clears it under X11 only
    QSpinBox spin;
    spin.setRange(0, 999);
    spin.setValue(57);
    CHECK_FALSE(keepsFromShortcuts(&spin, Qt::Key_U, CTRL));
    press(&spin, Qt::Key_U, CTRL);
    CHECK(spin.text() == QStringLiteral("57"));
}

TEST_CASE("Left and Right over a selection move the cursor from where it is",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);

    QLineEdit line(QStringLiteral("Ala ma kota"));
    line.setSelection(4, 2);  // "ma", the cursor after it
    press(&line, Qt::Key_Left);
    CHECK_FALSE(line.hasSelectedText());
    CHECK(line.cursorPosition() == 5);

    line.setSelection(6, -2);  // "ma", the cursor before it
    press(&line, Qt::Key_Right);
    CHECK_FALSE(line.hasSelectedText());
    CHECK(line.cursorPosition() == 5);

    // What the line completed in itself is kept or dropped whole, as on Windows
    QCompleter completer(QStringList{QStringLiteral("kotara")});
    completer.setCompletionMode(QCompleter::InlineCompletion);
    line.setCompleter(&completer);
    line.setText(QStringLiteral("kotara"));
    line.setSelection(3, 3);  // "ara" completed after "kot"
    press(&line, Qt::Key_Left);
    CHECK(line.cursorPosition() == 3);
    line.setCompleter(nullptr);
}

TEST_CASE("Read-only texts and selectable labels copy and select what is in them",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const ShortcutPlatform platform =
        GENERATE(ShortcutPlatform::Windows, ShortcutPlatform::Linux);
    CAPTURE(static_cast<int>(platform));
    const InstalledKeys keys(platform);

    QPlainTextEdit plain(QStringLiteral("Dziennik"));
    plain.setReadOnly(true);
    QTextEdit rich;
    rich.setPlainText(QStringLiteral("Dziennik"));
    rich.setReadOnly(true);
    QLabel label(QStringLiteral("Wiadomość"));
    label.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QLineEdit line(QStringLiteral("Pole"));
    line.setReadOnly(true);

    // Not the window's Copy and Select All: they copied and selected the book's text
    for (QWidget* widget : {static_cast<QWidget*>(&plain), static_cast<QWidget*>(&rich),
                            static_cast<QWidget*>(&label), static_cast<QWidget*>(&line)}) {
        CHECK(keepsFromShortcuts(widget, Qt::Key_C, CTRL));
        CHECK(keepsFromShortcuts(widget, Qt::Key_Insert, CTRL));
        CHECK(keepsFromShortcuts(widget, Qt::Key_A, CTRL));
        CHECK_FALSE(keepsFromShortcuts(widget, Qt::Key_V, CTRL));  // nothing to paste into
    }
    QLabel plainLabel(QStringLiteral("Etykieta"));
    CHECK_FALSE(keepsFromShortcuts(&plainLabel, Qt::Key_C, CTRL));

    press(&plain, Qt::Key_A, CTRL);
    CHECK(plain.textCursor().selectedText() == QStringLiteral("Dziennik"));
    press(&label, Qt::Key_A, CTRL);
    CHECK(label.selectedText() == QStringLiteral("Wiadomość"));
    if (clipboardWorks()) {
        press(&plain, Qt::Key_C, CTRL);
        CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("Dziennik"));
        press(&label, Qt::Key_Insert, CTRL);
        CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("Wiadomość"));
    }
}

TEST_CASE("Enter goes to the window's default button, Space presses the button",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);

    QDialog dialog;
    auto* layout = new QVBoxLayout(&dialog);
    auto* check = new QCheckBox(QStringLiteral("Check"), &dialog);
    auto* option = new QRadioButton(QStringLiteral("Option"), &dialog);
    auto* group = new QGroupBox(QStringLiteral("Group"), &dialog);
    group->setCheckable(true);
    group->setChecked(false);
    auto* choice = new QComboBox(&dialog);
    choice->addItems({QStringLiteral("a"), QStringLiteral("b")});
    auto* side = new QPushButton(QStringLiteral("Side"), &dialog);
    side->setAutoDefault(false);  // as the side buttons of the program's dialogs
    auto* ok = new QPushButton(QStringLiteral("OK"), &dialog);
    ok->setDefault(true);
    layout->addWidget(check);
    layout->addWidget(option);
    layout->addWidget(group);
    layout->addWidget(choice);
    layout->addWidget(side);
    layout->addWidget(ok);
    int okClicks = 0;
    int sideClicks = 0;
    QObject::connect(ok, &QPushButton::clicked, [&okClicks] { ++okClicks; });
    QObject::connect(side, &QPushButton::clicked, [&sideClicks] { ++sideClicks; });
    dialog.show();  // the window gives Enter to a visible default button only
    REQUIRE(test::waitUntil([&dialog] { return dialog.isVisible(); }));

    SECTION("Enter on a check box, an option, a group, a list or a side button") {
        for (QWidget* widget : {static_cast<QWidget*>(check), static_cast<QWidget*>(option),
                                static_cast<QWidget*>(group), static_cast<QWidget*>(choice),
                                static_cast<QWidget*>(side)}) {
            press(widget, Qt::Key_Return);
        }
        press(check, Qt::Key_Enter, Qt::KeypadModifier);
        CHECK(okClicks == 6);
        CHECK(sideClicks == 0);
        CHECK_FALSE(check->isChecked());
        CHECK_FALSE(option->isChecked());
        CHECK_FALSE(group->isChecked());
        CHECK_FALSE(choice->view()->isVisible());
    }

    SECTION("Space presses the check box and the button with the keys") {
        press(check, Qt::Key_Space, NONE, QStringLiteral(" "));
        CHECK(check->isChecked());
        press(side, Qt::Key_Space, NONE, QStringLiteral(" "));
        CHECK(sideClicks == 1);
        CHECK(okClicks == 0);
    }

    SECTION("A filter of the button gets Enter first (the frame of an annotation saves)") {
        EnterFilter filter;
        side->installEventFilter(&filter);
        press(side, Qt::Key_Return);
        CHECK(filter.enters == 1);
        CHECK(okClicks == 0);
        CHECK(sideClicks == 0);
    }
}

TEST_CASE("Shift+F10 opens the context menu, as on Windows", "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);

    SECTION("Of a field, at its cursor, as the Menu key") {
        MenuLine line;
        line.setText(QStringLiteral("Ala ma kota"));
        line.setCursorPosition(4);
        CHECK(press(&line, Qt::Key_F10, SHIFT));
        CHECK(line.menus == 1);
        CHECK(line.reason == QContextMenuEvent::Keyboard);
        CHECK(line.position == line.inputMethodQuery(Qt::ImCursorRectangle).toRect().center());
    }

    SECTION("A widget that takes Shift+F10 opens its menu itself, once") {
        MenuKeyWidget widget;
        CHECK(press(&widget, Qt::Key_F10, SHIFT));
        CHECK(widget.keys == 1);
        CHECK(widget.menus == 0);
    }

    SECTION("A widget that goes while it handles the key gets no menu") {
        int menus = 0;
        auto* line = new VanishingLine(menus);
        line->setText(QStringLiteral("Ala ma kota"));
        QKeyEvent pressed(QEvent::KeyPress, Qt::Key_F10, SHIFT);
        QApplication::sendEvent(line, &pressed);
        CHECK(menus == 0);
    }

    SECTION("Of a list, at its current item") {
        QTreeWidget list;
        list.setContextMenuPolicy(Qt::CustomContextMenu);
        new QTreeWidgetItem(&list, QStringList{QStringLiteral("a")});
        auto* second = new QTreeWidgetItem(&list, QStringList{QStringLiteral("b")});
        list.setCurrentItem(second);
        list.resize(200, 200);
        list.show();
        REQUIRE(test::waitUntil([&list] { return list.isVisible(); }));
        QTreeWidgetItem* chosen = nullptr;
        QObject::connect(&list, &QWidget::customContextMenuRequested,
                         [&list, &chosen](const QPoint& position) {
                             chosen = list.itemAt(position);
                         });
        press(&list, Qt::Key_F10, SHIFT);
        CHECK(chosen == second);
    }
}

TEST_CASE("The context menus of fields show the keys of their items", "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const ShortcutPlatform platform =
        GENERATE(ShortcutPlatform::Windows, ShortcutPlatform::Linux);
    CAPTURE(static_cast<int>(platform));
    const InstalledKeys keys(platform);

    SECTION("A line") {
        QLineEdit line;
        const std::unique_ptr<QMenu> menu(line.createStandardContextMenu());
        menu->ensurePolished();  // the menu takes its size after it is polished
        checkMenuKeys(*menu, "QLineEdit");
    }

    SECTION("A text, whose menu Qt gives its viewport") {
        QPlainTextEdit text;
        QMenu menu(text.viewport());
        for (const char* label :
             {"&Undo", "&Redo", "Cu&t", "&Copy", "&Paste", "Delete", "Select All"}) {
            menu.addAction(QCoreApplication::translate("QWidgetTextControl", label));
        }
        menu.ensurePolished();
        checkMenuKeys(menu, "QWidgetTextControl");
    }

    SECTION("A text whose menu the program asks for") {
        QTextEdit text;
        const std::unique_ptr<QMenu> menu(text.createStandardContextMenu());
        menu->ensurePolished();
        checkMenuKeys(*menu, "QWidgetTextControl");
    }

    SECTION("Another menu keeps its items as they are") {
        QWidget widget;
        QMenu menu(&widget);
        const QString copyLabel = QCoreApplication::translate("QLineEdit", "&Copy");
        QAction* copy = menu.addAction(copyLabel);
        menu.ensurePolished();
        CHECK(copy->text() == copyLabel);
    }
}

TEST_CASE("Fields show their keys also where the window's commands have them",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: "
             "run with QT_QPA_PLATFORM=offscreen");
    }
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* line = new QLineEdit(&window);
    auto* text = new QPlainTextEdit(&window);
    layout->addWidget(line);
    layout->addWidget(text);
    auto* copy = new QAction(QStringLiteral("Copy"), &window);
    copy->setShortcut(QKeySequence(QKeyCombination(CTRL, Qt::Key_C)));
    window.addAction(copy);
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return window.isActiveWindow(); }));

    // Qt leaves out the keys the window has
    const QString copyLabel = QCoreApplication::translate("QLineEdit", "&Copy");
    {
        const std::unique_ptr<QMenu> menu(line->createStandardContextMenu());
        menu->ensurePolished();
        REQUIRE(itemText(*menu, copyLabel) == copyLabel);
    }

    const InstalledKeys keys(ShortcutPlatform::Linux);
    const QKeyCombination copyKeys(CTRL, Qt::Key_C);
    {
        const std::unique_ptr<QMenu> menu(line->createStandardContextMenu());
        menu->ensurePolished();
        CHECK(itemText(*menu, copyLabel) == withKeys("QLineEdit", "&Copy", copyKeys));
    }

    // The menu of a text as the Menu key opens it
    const QPoint position(5, 5);
    QContextMenuEvent event(QContextMenuEvent::Keyboard, position, text->mapToGlobal(position));
    QApplication::sendEvent(text, &event);
    auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    REQUIRE(popup != nullptr);
    CHECK(itemText(*popup, QCoreApplication::translate("QWidgetTextControl", "&Copy")) ==
          withKeys("QWidgetTextControl", "&Copy", copyKeys));
    popup->close();
}

TEST_CASE("A field that records shortcuts gets every key while it records",
          "[gui][widget_keys]") {
    if (onMacOS()) {
        SKIP("macOS keeps the keys of its own fields");
    }
    const InstalledKeys keys(ShortcutPlatform::Linux);

    ShortcutRecorder recorder;
    QList<QKeyCombination> recorded;
    QObject::connect(&recorder, &ShortcutRecorder::recorded,
                     [&recorded](QKeyCombination pressed) { recorded.append(pressed); });
    recorder.startRecording(QStringLiteral("Press the keys"), true);

    const QList<QKeyCombination> pressedKeys = {
        QKeyCombination(CTRL, Qt::Key_Y), QKeyCombination(CTRL, Qt::Key_E),
        QKeyCombination(SHIFT, Qt::Key_F10), QKeyCombination(ALT, Qt::Key_Backspace),
        QKeyCombination(NONE, Qt::Key_Return)};
    for (const QKeyCombination pressed : pressedKeys) {
        CAPTURE(ShortcutRules::keysText(pressed).toStdString());
        CHECK(keepsFromShortcuts(&recorder, pressed.key(), pressed.keyboardModifiers()));
        press(&recorder, pressed.key(), pressed.keyboardModifiers());
    }
    CHECK(recorded == pressedKeys);
    CHECK(recorder.isRecording());
}

TEST_CASE("The filter of macOS changes nothing", "[gui][widget_keys]") {
    QLineEdit line;
    const std::unique_ptr<QMenu> before(line.createStandardContextMenu());
    before->ensurePolished();

    const InstalledKeys keys(ShortcutPlatform::MacOS);
    const std::unique_ptr<QMenu> after(line.createStandardContextMenu());
    after->ensurePolished();
    REQUIRE(after->actions().size() == before->actions().size());
    for (qsizetype i = 0; i < after->actions().size(); ++i) {
        CHECK(after->actions().at(i)->text() == before->actions().at(i)->text());
    }
}
