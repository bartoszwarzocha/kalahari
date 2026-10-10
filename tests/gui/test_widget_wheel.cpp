/// @file test_widget_wheel.cpp
/// @brief The mouse wheel changes only the field with the focus, as on Windows

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/widget_wheel.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QProxyStyle>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <utility>
#include <vector>

using kalahari::gui::WidgetWheel;
namespace test = kalahari::test;

namespace {

/// The filter on the whole application, for one test
struct InstalledWheel {
    WidgetWheel filter;
    InstalledWheel() { qApp->installEventFilter(&filter); }
    ~InstalledWheel() { qApp->removeEventFilter(&filter); }
    InstalledWheel(const InstalledWheel&) = delete;
    InstalledWheel& operator=(const InstalledWheel&) = delete;
};

/// One notch of the wheel away from the user over the middle of @p widget; whether the
/// widget took it
bool turnWheel(QWidget& widget) {
    const QPointF middle = QRectF(widget.rect()).center();
    QWheelEvent wheel(middle, widget.mapToGlobal(middle), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&widget, &wheel);
    return wheel.isAccepted();
}

/// A window with a text field, a field of each kind the wheel turns and a scroll bar, each in
/// the middle of its range
struct Fields {
    QWidget window;
    QLineEdit* text = new QLineEdit(&window);
    QSpinBox* spinBox = new QSpinBox(&window);
    QComboBox* comboBox = new QComboBox(&window);
    QSlider* slider = new QSlider(Qt::Horizontal, &window);
    QScrollBar* scrollBar = new QScrollBar(Qt::Horizontal, &window);

    Fields() {
        auto* layout = new QVBoxLayout(&window);
        layout->addWidget(text);
        layout->addWidget(spinBox);
        layout->addWidget(comboBox);
        layout->addWidget(slider);
        layout->addWidget(scrollBar);
        spinBox->setRange(0, 10);
        spinBox->setValue(5);
        comboBox->addItems({QStringLiteral("One"), QStringLiteral("Two"),
                            QStringLiteral("Three")});
        comboBox->setCurrentIndex(1);
        slider->setRange(0, 10);
        slider->setValue(5);
        scrollBar->setRange(0, 10);
        scrollBar->setValue(5);
    }
};

/// The style of macOS for a combo box: the wheel does not turn it
class NoWheelComboBoxStyle : public QProxyStyle {
public:
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override {
        if (hint == SH_ComboBox_AllowWheelScrolling) {
            return 0;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

}  // anonymous namespace

TEST_CASE("Mouse wheel: only spin boxes, combo boxes and sliders are fields it turns",
          "[gui][wheel]") {
    Fields fields;
    CHECK(WidgetWheel::turnsValue(fields.spinBox));
    CHECK(WidgetWheel::turnsValue(fields.comboBox));
    CHECK(WidgetWheel::turnsValue(fields.slider));
    CHECK_FALSE(WidgetWheel::turnsValue(fields.scrollBar));
    CHECK_FALSE(WidgetWheel::turnsValue(fields.text));
    CHECK_FALSE(WidgetWheel::turnsValue(&fields.window));
    CHECK_FALSE(WidgetWheel::turnsValue(nullptr));
}

TEST_CASE("Mouse wheel: the fields take the focus from a click or Tab, not from the wheel",
          "[gui][wheel]") {
    InstalledWheel wheel;
    Fields fields;
    // Qt's own policies differ between the systems: on macOS a combo box and a slider take
    // the focus from Tab only, like buttons. A spin box takes it from the wheel everywhere.
    REQUIRE(fields.spinBox->focusPolicy() == Qt::WheelFocus);
    const std::vector<std::pair<QWidget*, Qt::FocusPolicy>> before = {
        {fields.spinBox, fields.spinBox->focusPolicy()},
        {fields.comboBox, fields.comboBox->focusPolicy()},
        {fields.slider, fields.slider->focusPolicy()},
        {fields.scrollBar, fields.scrollBar->focusPolicy()},
    };

    fields.window.ensurePolished();
    for (QWidget* widget : fields.window.findChildren<QWidget*>()) {
        widget->ensurePolished();
    }
    CHECK(fields.spinBox->focusPolicy() == Qt::StrongFocus);
    for (const auto& [widget, policy] : before) {
        // The fields the wheel turns no longer take the focus from it; the others, and the
        // policies without the wheel, stay as they were
        const bool fromWheel = WidgetWheel::turnsValue(widget) && policy == Qt::WheelFocus;
        CHECK(widget->focusPolicy() == (fromWheel ? Qt::StrongFocus : policy));
    }
}

TEST_CASE("Mouse wheel: over a field without the focus it goes on, with the focus it turns "
          "the value", "[gui][wheel]") {
    InstalledWheel wheel;
    Fields fields;
    fields.window.show();
    fields.window.activateWindow();
    REQUIRE(test::waitUntil([&fields] { return QApplication::activeWindow() == &fields.window; }));
    // The focus in the text field, as while one types
    fields.text->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.text->hasFocus());

    // Without the focus: not taken, so that Qt gives it to the widget around (a page that
    // scrolls); the values stay
    CHECK_FALSE(turnWheel(*fields.spinBox));
    CHECK(fields.spinBox->value() == 5);
    CHECK_FALSE(turnWheel(*fields.comboBox));
    CHECK(fields.comboBox->currentIndex() == 1);
    CHECK_FALSE(turnWheel(*fields.slider));
    CHECK(fields.slider->value() == 5);

    // A scroll bar scrolls as always
    CHECK(turnWheel(*fields.scrollBar));
    CHECK(fields.scrollBar->value() != 5);

    // With the focus, after a click or Tab, the wheel turns the value of the field
    fields.spinBox->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.spinBox->hasFocus());
    CHECK(turnWheel(*fields.spinBox));
    CHECK(fields.spinBox->value() == 6);

    // A combo box turns as the system's style lets it: not on macOS, where the wheel scrolls
    // the page around it even with the focus (Qt 6.4 keeps the wheel there and turns nothing,
    // so then only the value tells)
    QStyleOptionComboBox option;
    option.initFrom(fields.comboBox);
    const bool comboBoxTurns = fields.comboBox->style()->styleHint(
                                   QStyle::SH_ComboBox_AllowWheelScrolling, &option,
                                   fields.comboBox) != 0;
    fields.comboBox->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.comboBox->hasFocus());
    const bool comboBoxTook = turnWheel(*fields.comboBox);
    if (comboBoxTurns) {
        CHECK(comboBoxTook);
    }
    CHECK(fields.comboBox->currentIndex() == (comboBoxTurns ? 0 : 1));

    fields.slider->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.slider->hasFocus());
    CHECK(turnWheel(*fields.slider));
    CHECK(fields.slider->value() != 5);
}

TEST_CASE("Mouse wheel: a combo box as on macOS takes the focus from Tab, never the wheel",
          "[gui][wheel]") {
    // There a combo box takes the focus from Tab only, like a button, and its style does
    // not let the wheel turn it: the filter changes neither
    NoWheelComboBoxStyle style;  // outlives the combo box
    InstalledWheel wheel;
    Fields fields;
    fields.comboBox->setStyle(&style);
    fields.comboBox->setFocusPolicy(Qt::TabFocus);
    fields.window.show();
    fields.window.activateWindow();
    REQUIRE(test::waitUntil([&fields] { return QApplication::activeWindow() == &fields.window; }));
    CHECK(fields.comboBox->focusPolicy() == Qt::TabFocus);

    // Without the focus the wheel goes on to the widget around. With it the combo box gets
    // the wheel and turns nothing; whether it passes the wheel on depends on the Qt version,
    // so only the value tells.
    CHECK_FALSE(turnWheel(*fields.comboBox));
    fields.comboBox->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.comboBox->hasFocus());
    turnWheel(*fields.comboBox);
    CHECK(fields.comboBox->currentIndex() == 1);
}
