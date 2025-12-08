/// @file color_config_widget.cpp
/// @brief Implementation of ColorConfigWidget

#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/core/theme_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>

namespace kalahari {
namespace gui {

ColorConfigWidget::ColorConfigWidget(const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_label(nullptr)
    , m_colorButton(nullptr)
    , m_hexLabel(nullptr)
    , m_color(Qt::black)
{
    // Create horizontal layout with 40/60 split for label/controls
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Label (40% width via stretch factor 2)
    m_label = new QLabel(label, this);
    m_label->setMinimumWidth(100);
    m_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(m_label, 2);  // stretch factor 2 = 40%

    // Controls container (60% width via stretch factor 3)
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);

    // Color button - 24x24 with color as background
    m_colorButton = new QPushButton(this);
    m_colorButton->setFixedSize(24, 24);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("Click to select color"));
    connect(m_colorButton, &QPushButton::clicked,
            this, &ColorConfigWidget::onColorButtonClicked);
    controlsLayout->addWidget(m_colorButton);

    // Hex label
    m_hexLabel = new QLabel(this);
    m_hexLabel->setMinimumWidth(60);
    m_hexLabel->setStyleSheet("font-family: monospace;");
    controlsLayout->addWidget(m_hexLabel);

    // Push controls to the left within the 60% area
    controlsLayout->addStretch();

    layout->addLayout(controlsLayout, 3);  // stretch factor 3 = 60%

    // Initialize display
    updateColorDisplay();
}

void ColorConfigWidget::setColor(const QColor& color) {
    if (m_color != color) {
        m_color = color;
        updateColorDisplay();
    }
}

QColor ColorConfigWidget::color() const {
    return m_color;
}

void ColorConfigWidget::onColorButtonClicked() {
    // Open color dialog without alpha channel (standard RGB)
    QColor newColor = QColorDialog::getColor(
        m_color,
        this,
        tr("Select Color"),
        QColorDialog::DontUseNativeDialog
    );

    if (newColor.isValid() && newColor != m_color) {
        m_color = newColor;
        updateColorDisplay();
        emit colorChanged(m_color);
    }
}

void ColorConfigWidget::updateColorDisplay() {
    // Update button background - use theme-aware border color
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    QString buttonStyle = QString(
        "background-color: %1; "
        "border: 1px solid %2; "
        "border-radius: 2px;"
    ).arg(m_color.name()).arg(theme.palette.mid.name());
    m_colorButton->setStyleSheet(buttonStyle);

    // Update hex label
    m_hexLabel->setText(m_color.name().toUpper());
}

} // namespace gui
} // namespace kalahari
