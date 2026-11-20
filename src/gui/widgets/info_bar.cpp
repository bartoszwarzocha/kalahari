// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file info_bar.cpp
/// @brief A message across the text area: an icon, the message, a button and a close button

#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QToolButton>

namespace kalahari::gui {

namespace {

constexpr int MINIMUM_HEIGHT = 40;  ///< Height of a bar with a message in one line
constexpr int ICON_SIZE = 20;

} // anonymous namespace

InfoBar::InfoBar(QWidget* parent)
    : QFrame(parent)
    , m_iconId(QStringLiteral("help.about"))
{
    setObjectName(QStringLiteral("infoBar"));
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setMinimumHeight(MINIMUM_HEIGHT);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(11, 6, 11, 6);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_iconLabel->setScaledContents(true);
    layout->addWidget(m_iconLabel);

    // In a narrow bar (a small screen) the message takes more lines
    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_messageLabel, 1);

    m_actionButton = new QPushButton(this);
    m_actionButton->setCursor(Qt::PointingHandCursor);
    m_actionButton->hide();
    layout->addWidget(m_actionButton);

    m_closeButton = new QToolButton(this);
    m_closeButton->setAutoRaise(true);
    m_closeButton->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolTip(tr("Dismiss this message"));
    layout->addWidget(m_closeButton);

    connect(m_actionButton, &QPushButton::clicked, this, &InfoBar::actionClicked);
    connect(m_closeButton, &QToolButton::clicked, this, &InfoBar::dismissed);

    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this, [this]() {
        updateStyling();
        updateIcons();
    });
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &InfoBar::updateIcons);
    // The colors of the "editor" section the writer changes (setColorKeys())
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            [this]() {
                if (!m_backgroundKey.empty()) {
                    updateStyling();
                }
            });

    updateStyling();
    updateIcons();
}

void InfoBar::setMessage(const QString& message)
{
    m_messageLabel->setText(message);
}

QString InfoBar::message() const
{
    return m_messageLabel->text();
}

void InfoBar::setActionText(const QString& text)
{
    m_actionButton->setText(text);
    m_actionButton->setVisible(!text.isEmpty());
}

void InfoBar::setActionToolTip(const QString& toolTip)
{
    m_actionButton->setToolTip(toolTip);
}

void InfoBar::setActionEnabled(bool enabled)
{
    m_actionButton->setEnabled(enabled);
}

void InfoBar::setIconId(const QString& iconId)
{
    if (iconId == m_iconId) {
        return;
    }
    m_iconId = iconId;
    updateIcons();
}

void InfoBar::showSettingsButton(const QString& toolTip)
{
    if (m_settingsButton == nullptr) {
        m_settingsButton = new QToolButton(this);
        m_settingsButton->setObjectName(QStringLiteral("infoBarSettings"));
        m_settingsButton->setAutoRaise(true);
        m_settingsButton->setFixedSize(ICON_SIZE, ICON_SIZE);
        m_settingsButton->setCursor(Qt::PointingHandCursor);
        // Before the close button, which ends the bar
        if (auto* row = qobject_cast<QHBoxLayout*>(layout())) {
            row->insertWidget(row->indexOf(m_closeButton), m_settingsButton);
        }
        connect(m_settingsButton, &QToolButton::clicked, this, &InfoBar::settingsClicked);
        updateIcons();
    }
    m_settingsButton->setToolTip(toolTip);
    m_settingsButton->setAccessibleName(toolTip);
}

void InfoBar::setColorKeys(const std::string& backgroundKey, const std::string& borderKey)
{
    m_backgroundKey = backgroundKey;
    m_borderKey = borderKey;
    updateStyling();
}

void InfoBar::updateStyling()
{
    const auto& themes = core::ThemeManager::getInstance();
    const auto& theme = themes.getCurrentTheme();

    // A subtle info color scheme; the theme resolves the light and dark values, or the colors
    // of setColorKeys() do, which the writer can change. The rule is the bar's own: its labels
    // (frames too) get no border of their own. It names the class, not the object: a bar with
    // a name of its own (the notifications, the plot board) keeps its colors.
    const QColor background = m_backgroundKey.empty()
                                  ? theme.colors.infoBarBackground
                                  : themes.editorColor(m_backgroundKey,
                                                       theme.colors.infoBarBackground);
    const QColor border = m_borderKey.empty()
                              ? theme.colors.infoBarBorder
                              : themes.editorColor(m_borderKey, theme.colors.infoBarBorder);
    setStyleSheet(QString("kalahari--gui--InfoBar { background-color: %1; "
                          "border: 1px solid %2; border-radius: 4px; }")
                      .arg(background.name(), border.name()));

    m_messageLabel->setStyleSheet(QString("QLabel { color: %1; background: transparent; }")
                                      .arg(theme.palette.windowText.name()));

    // Greyed out as the theme greys out every button (Theme::Palette::toQPalette): the style
    // sheet takes the place of the palette, and without its own colors a disabled button
    // looked the same as one that can be clicked
    const QPalette palette = theme.palette.toQPalette();
    const auto disabled = [&palette](QPalette::ColorRole role) {
        const QColor color = palette.color(QPalette::Disabled, role);
        return QString("rgba(%1, %2, %3, %4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
    };
    m_actionButton->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; "
                                          "border: 1px solid %3; border-radius: 3px; "
                                          "padding: 4px 12px; } "
                                          "QPushButton:hover { background-color: %4; } "
                                          "QPushButton:pressed { background-color: %5; } "
                                          "QPushButton:disabled { background-color: %6; "
                                          "color: %7; border-color: %8; }")
                                      .arg(theme.palette.button.name(),
                                           theme.palette.buttonText.name(),
                                           theme.palette.mid.name(), theme.palette.light.name(),
                                           theme.palette.mid.name())
                                      .arg(disabled(QPalette::Button),
                                           disabled(QPalette::ButtonText),
                                           disabled(QPalette::Mid)));
}

void InfoBar::updateIcons()
{
    auto& artProvider = core::ArtProvider::getInstance();

    // The info icon by default (help.about uses info.svg)
    m_iconLabel->setPixmap(artProvider.getThemedIcon(m_iconId).pixmap(ICON_SIZE, ICON_SIZE));
    m_closeButton->setIcon(artProvider.getIcon("dock.close", core::IconContext::Button));
    if (m_settingsButton != nullptr) {
        m_settingsButton->setIcon(artProvider.getIcon("dock.settings", core::IconContext::Button));
    }
}

} // namespace kalahari::gui
