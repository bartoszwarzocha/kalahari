/// @file color_config_widget.cpp
/// @brief Implementation of ColorConfigWidget

#include "kalahari/gui/widgets/color_config_widget.h"

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
    // Create horizontal layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Label
    m_label = new QLabel(label, this);
    m_label->setMinimumWidth(80);
    layout->addWidget(m_label);

    // Color button - 24x24 with color as background
    m_colorButton = new QPushButton(this);
    m_colorButton->setFixedSize(24, 24);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("Click to select color"));
    connect(m_colorButton, &QPushButton::clicked,
            this, &ColorConfigWidget::onColorButtonClicked);
    layout->addWidget(m_colorButton);

    // Hex label
    m_hexLabel = new QLabel(this);
    m_hexLabel->setMinimumWidth(60);
    m_hexLabel->setStyleSheet("font-family: monospace;");
    layout->addWidget(m_hexLabel);

    // Add stretch to push everything to the left
    layout->addStretch();

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
    // Update button background
    QString buttonStyle = QString(
        "background-color: %1; "
        "border: 1px solid #888; "
        "border-radius: 2px;"
    ).arg(m_color.name());
    m_colorButton->setStyleSheet(buttonStyle);

    // Update hex label
    m_hexLabel->setText(m_color.name().toUpper());
}

} // namespace gui
} // namespace kalahari
