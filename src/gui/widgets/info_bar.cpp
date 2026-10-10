/// @file info_bar.cpp
/// @brief A message across the text area: an icon, the message, a button and a close button

#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

namespace kalahari {
namespace gui {

namespace {

constexpr int MINIMUM_HEIGHT = 40;  ///< Height of a bar with a message in one line
constexpr int ICON_SIZE = 20;

} // anonymous namespace

InfoBar::InfoBar(QWidget* parent)
    : QFrame(parent)
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

void InfoBar::updateStyling()
{
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

    // A subtle info color scheme; the theme resolves the light and dark values. The rule is
    // the bar's own: its labels (frames too) get no border of their own.
    setStyleSheet(QString("QFrame#infoBar { background-color: %1; border: 1px solid %2; "
                          "border-radius: 4px; }")
                      .arg(theme.colors.infoBarBackground.name(),
                           theme.colors.infoBarBorder.name()));

    m_messageLabel->setStyleSheet(QString("QLabel { color: %1; background: transparent; }")
                                      .arg(theme.palette.windowText.name()));

    m_actionButton->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; "
                                          "border: 1px solid %3; border-radius: 3px; "
                                          "padding: 4px 12px; } "
                                          "QPushButton:hover { background-color: %4; } "
                                          "QPushButton:pressed { background-color: %5; }")
                                      .arg(theme.palette.button.name(),
                                           theme.palette.buttonText.name(),
                                           theme.palette.mid.name(), theme.palette.light.name(),
                                           theme.palette.mid.name()));
}

void InfoBar::updateIcons()
{
    auto& artProvider = core::ArtProvider::getInstance();

    // The info icon (help.about uses info.svg)
    m_iconLabel->setPixmap(artProvider.getThemedIcon("help.about").pixmap(ICON_SIZE, ICON_SIZE));
    m_closeButton->setIcon(artProvider.getIcon("dock.close", core::IconContext::Button));
}

} // namespace gui
} // namespace kalahari
