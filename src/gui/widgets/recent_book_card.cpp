/// @file recent_book_card.cpp
/// @brief Recent book card widget implementation
///
/// OpenSpec #00036: Enhanced Dashboard with recent books cards

#include "kalahari/gui/widgets/recent_book_card.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDateTime>
#include <QMouseEvent>
#include <QFont>

namespace kalahari {
namespace gui {

RecentBookCard::RecentBookCard(const QString& filePath, QWidget* parent)
    : QFrame(parent)
    , m_filePath(filePath)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_statsLabel(nullptr)
    , m_dateLabel(nullptr)
{
    setupUI();
    setToolTip(filePath);
    setCursor(Qt::PointingHandCursor);
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
}

void RecentBookCard::setupUI()
{
    // Main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(11, 11, 11, 11);
    mainLayout->setSpacing(12);

    // Icon label (48x48)
    m_iconLabel = new QLabel(this);
    QPixmap iconPixmap = core::ArtProvider::getInstance().getPixmap("file.open.project", 48);
    m_iconLabel->setPixmap(iconPixmap);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);

    // Right side: vertical layout with title, stats, date
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(4);

    // Title label (bold, larger)
    m_titleLabel = new QLabel(extractTitle(), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    infoLayout->addWidget(m_titleLabel);

    // Stats label
    m_statsLabel = new QLabel(extractStats(), this);
    m_statsLabel->setStyleSheet("color: gray;");
    infoLayout->addWidget(m_statsLabel);

    // Date label
    m_dateLabel = new QLabel(formatDate(), this);
    m_dateLabel->setStyleSheet("color: gray; font-size: 10px;");
    infoLayout->addWidget(m_dateLabel);

    mainLayout->addLayout(infoLayout, 1);

    setLayout(mainLayout);

    // Connect to theme changes to update icon
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, [this]() {
        QPixmap iconPixmap = core::ArtProvider::getInstance().getPixmap("file.open.project", 48);
        m_iconLabel->setPixmap(iconPixmap);
    });
}

QString RecentBookCard::extractTitle() const
{
    QFileInfo fileInfo(m_filePath);
    // Get filename without extension
    QString baseName = fileInfo.completeBaseName();
    // Remove .klh if still present (for .klh.zip files etc)
    if (baseName.endsWith(".klh", Qt::CaseInsensitive)) {
        baseName = baseName.left(baseName.length() - 4);
    }
    return baseName;
}

QString RecentBookCard::extractStats() const
{
    // Placeholder - actual stats would require loading the project
    // Could be enhanced later to read project metadata
    return tr("Project file");
}

QString RecentBookCard::formatDate() const
{
    QFileInfo fileInfo(m_filePath);
    if (fileInfo.exists()) {
        QDateTime lastModified = fileInfo.lastModified();
        return tr("Last modified: %1").arg(lastModified.toString("MMM d, yyyy"));
    }
    return tr("File not found");
}

void RecentBookCard::updateHoverState(bool hovered)
{
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

    if (hovered) {
        // Use theme highlight color for hover
        QColor hoverColor = theme.palette.highlight;
        hoverColor.setAlpha(30); // Semi-transparent
        setStyleSheet(QString("RecentBookCard { background-color: %1; border-radius: 4px; }")
                     .arg(hoverColor.name(QColor::HexArgb)));
    } else {
        // Reset to transparent
        setStyleSheet("RecentBookCard { background-color: transparent; border-radius: 4px; }");
    }
}

void RecentBookCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_filePath);
    }
    QFrame::mousePressEvent(event);
}

void RecentBookCard::enterEvent(QEnterEvent* event)
{
    updateHoverState(true);
    QFrame::enterEvent(event);
}

void RecentBookCard::leaveEvent(QEvent* event)
{
    updateHoverState(false);
    QFrame::leaveEvent(event);
}

} // namespace gui
} // namespace kalahari
