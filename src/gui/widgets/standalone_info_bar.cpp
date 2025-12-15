/// @file standalone_info_bar.cpp
/// @brief Implementation of StandaloneInfoBar widget
///
/// OpenSpec #00033 Phase F: Info bar for standalone files

#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

namespace kalahari {
namespace gui {

StandaloneInfoBar::StandaloneInfoBar(QWidget* parent)
    : QFrame(parent)
    , m_iconLabel(nullptr)
    , m_messageLabel(nullptr)
    , m_addButton(nullptr)
    , m_closeButton(nullptr)
{
    setupUI();
    createConnections();
    updateStyling();
    updateIcons();
}

void StandaloneInfoBar::setupUI() {
    // Frame styling
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setFixedHeight(40);

    // Main horizontal layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(11, 6, 11, 6);
    layout->setSpacing(8);

    // Info icon
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    m_iconLabel->setScaledContents(true);
    layout->addWidget(m_iconLabel);

    // Message label
    m_messageLabel = new QLabel(tr("This file is not part of a project. Limited features available."), this);
    m_messageLabel->setWordWrap(false);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_messageLabel, 1);

    // "Add to Project" button
    m_addButton = new QPushButton(tr("Add to Project"), this);
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setToolTip(tr("Add this file to a project for full features"));
    layout->addWidget(m_addButton);

    // Close/dismiss button
    m_closeButton = new QToolButton(this);
    m_closeButton->setAutoRaise(true);
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolTip(tr("Dismiss this message"));
    layout->addWidget(m_closeButton);
}

void StandaloneInfoBar::createConnections() {
    // Button clicks
    connect(m_addButton, &QPushButton::clicked,
            this, &StandaloneInfoBar::addToProjectClicked);
    connect(m_closeButton, &QToolButton::clicked,
            this, &StandaloneInfoBar::onDismiss);

    // Theme changes
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &StandaloneInfoBar::onThemeChanged);

    // Icon theme/color changes
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, &StandaloneInfoBar::updateIcons);
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
}

void StandaloneInfoBar::setMessage(const QString& message) {
    m_messageLabel->setText(message);
}

void StandaloneInfoBar::onDismiss() {
    emit dismissed();
}

void StandaloneInfoBar::onThemeChanged() {
    updateStyling();
    updateIcons();
}

void StandaloneInfoBar::updateStyling() {
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

    // Use a subtle warning/info color scheme
    // Light theme: light yellow/cream background
    // Dark theme: dark amber/brown background
    QColor bgColor;
    QColor borderColor;
    QColor textColor = theme.palette.windowText;

    // Determine background based on theme luminance
    // Light themes have brighter backgrounds
    int luminance = theme.palette.window.lightness();

    if (luminance > 128) {
        // Light theme - use light yellow/cream
        bgColor = QColor(255, 248, 225);      // Light cream
        borderColor = QColor(255, 213, 79);   // Amber border
    } else {
        // Dark theme - use dark amber
        bgColor = QColor(62, 50, 30);         // Dark amber
        borderColor = QColor(120, 94, 30);    // Darker amber border
    }

    // Apply frame styling
    QString styleSheet = QString(
        "QFrame { "
        "  background-color: %1; "
        "  border: 1px solid %2; "
        "  border-radius: 4px; "
        "}"
    ).arg(bgColor.name()).arg(borderColor.name());

    setStyleSheet(styleSheet);

    // Style the message label
    m_messageLabel->setStyleSheet(QString(
        "QLabel { "
        "  color: %1; "
        "  background: transparent; "
        "  border: none; "
        "}"
    ).arg(textColor.name()));

    // Style the "Add to Project" button
    QString buttonStyle = QString(
        "QPushButton { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  border-radius: 3px; "
        "  padding: 4px 12px; "
        "} "
        "QPushButton:hover { "
        "  background-color: %4; "
        "} "
        "QPushButton:pressed { "
        "  background-color: %5; "
        "}"
    ).arg(theme.palette.button.name())
     .arg(theme.palette.buttonText.name())
     .arg(theme.palette.mid.name())
     .arg(theme.palette.light.name())
     .arg(theme.palette.mid.name());

    m_addButton->setStyleSheet(buttonStyle);
}

void StandaloneInfoBar::updateIcons() {
    auto& artProvider = core::ArtProvider::getInstance();

    // Info icon (using help.about which uses info.svg) - use getThemedIcon for auto-refresh
    QIcon infoIcon = artProvider.getThemedIcon("help.about");
    m_iconLabel->setPixmap(infoIcon.pixmap(20, 20));

    // Close button icon
    m_closeButton->setIcon(artProvider.getIcon("dock.close", core::IconContext::Button));
}

} // namespace gui
} // namespace kalahari
