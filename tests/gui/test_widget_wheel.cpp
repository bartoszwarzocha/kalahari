/// @file test_widget_wheel.cpp
/// @brief The mouse wheel changes only the field with the focus, as on Windows

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/widget_wheel.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

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
    // Qt's own: the wheel gives them the focus
    REQUIRE(fields.spinBox->focusPolicy() == Qt::WheelFocus);
    REQUIRE(fields.comboBox->focusPolicy() == Qt::WheelFocus);
    const Qt::FocusPolicy sliderPolicy = fields.slider->focusPolicy();
    const Qt::FocusPolicy scrollBarPolicy = fields.scrollBar->focusPolicy();

    fields.window.ensurePolished();
    for (QWidget* widget : fields.window.findChildren<QWidget*>()) {
        widget->ensurePolished();
    }
    CHECK(fields.spinBox->focusPolicy() == Qt::StrongFocus);
    CHECK(fields.comboBox->focusPolicy() == Qt::StrongFocus);
    // Those the wheel does not give the focus keep theirs
    CHECK(fields.slider->focusPolicy() == sliderPolicy);
    CHECK(fields.scrollBar->focusPolicy() == scrollBarPolicy);
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

    fields.comboBox->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.comboBox->hasFocus());
    CHECK(turnWheel(*fields.comboBox));
    CHECK(fields.comboBox->currentIndex() == 0);

    fields.slider->setFocus(Qt::TabFocusReason);
    REQUIRE(fields.slider->hasFocus());
    CHECK(turnWheel(*fields.slider));
    CHECK(fields.slider->value() != 5);
}
