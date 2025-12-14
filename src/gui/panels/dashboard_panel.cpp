/// @file dashboard_panel.cpp
/// @brief Dashboard panel implementation
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books

#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/widgets/recent_book_card.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QGroupBox>
#include <QScrollArea>
#include <QFont>

namespace kalahari {
namespace gui {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
    , m_welcomeLabel(nullptr)
    , m_subtitleLabel(nullptr)
    , m_recentBooksLayout(nullptr)
    , m_recentBooksContainer(nullptr)
    , m_noRecentBooksLabel(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel constructor called");

    setupUI();

    // Connect to RecentBooksManager for auto-refresh
    connect(&core::RecentBooksManager::getInstance(), &core::RecentBooksManager::recentFilesChanged,
            this, &DashboardPanel::refreshRecentBooks);

    logger.debug("DashboardPanel initialized");
}

void DashboardPanel::setupUI()
{
    // Create main layout with scroll area
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* scrollContent = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->setSpacing(20);

    // Add sections
    mainLayout->addWidget(createWelcomeHeader());
    mainLayout->addWidget(createRecentBooksSection());
    mainLayout->addWidget(createQuickStartSection());
    mainLayout->addStretch(1);

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    // Initial population of recent books
    refreshRecentBooks();
}

QWidget* DashboardPanel::createWelcomeHeader()
{
    QWidget* headerWidget = new QWidget(this);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 10);
    headerLayout->setSpacing(8);

    // Welcome title
    m_welcomeLabel = new QLabel(tr("Welcome to Kalahari"), this);
    QFont titleFont = m_welcomeLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    m_welcomeLabel->setFont(titleFont);
    headerLayout->addWidget(m_welcomeLabel);

    // Subtitle
    m_subtitleLabel = new QLabel(tr("Your creative writing environment"), this);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPointSize(12);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet("color: gray;");
    headerLayout->addWidget(m_subtitleLabel);

    return headerWidget;
}

QWidget* DashboardPanel::createRecentBooksSection()
{
    // Create group box for recent books
    QGroupBox* recentGroup = new QGroupBox(tr("Recent Books"), this);
    QVBoxLayout* groupLayout = new QVBoxLayout(recentGroup);
    groupLayout->setContentsMargins(11, 11, 11, 11);
    groupLayout->setSpacing(6);

    // Container for book cards
    m_recentBooksContainer = new QWidget(this);
    m_recentBooksLayout = new QVBoxLayout(m_recentBooksContainer);
    m_recentBooksLayout->setContentsMargins(0, 0, 0, 0);
    m_recentBooksLayout->setSpacing(6);

    // "No recent books" message (hidden by default)
    m_noRecentBooksLabel = new QLabel(tr("No recent books. Create a new project or open an existing one."), this);
    m_noRecentBooksLabel->setStyleSheet("color: gray; padding: 20px;");
    m_noRecentBooksLabel->setAlignment(Qt::AlignCenter);
    m_noRecentBooksLabel->hide();

    groupLayout->addWidget(m_recentBooksContainer);
    groupLayout->addWidget(m_noRecentBooksLabel);

    return recentGroup;
}

QWidget* DashboardPanel::createQuickStartSection()
{
    QGroupBox* quickStartGroup = new QGroupBox(tr("Quick Start"), this);
    QVBoxLayout* groupLayout = new QVBoxLayout(quickStartGroup);
    groupLayout->setContentsMargins(11, 11, 11, 11);
    groupLayout->setSpacing(4);

    // Quick start items
    struct QuickStartItem {
        const char* text;
        const char* shortcut;
    };

    QuickStartItem items[] = {
        {QT_TR_NOOP("New Book"), "Ctrl+Shift+N"},
        {QT_TR_NOOP("Open Book"), "Ctrl+O"},
        {QT_TR_NOOP("Toggle Navigator"), "Ctrl+1"},
        {QT_TR_NOOP("Toggle Properties"), "Ctrl+2"}
    };

    for (const auto& item : items) {
        QLabel* label = new QLabel(QString("%1 (%2)")
                                   .arg(tr(item.text))
                                   .arg(item.shortcut), this);
        label->setStyleSheet("padding: 4px 0px;");
        groupLayout->addWidget(label);
    }

    return quickStartGroup;
}

void DashboardPanel::clearRecentBookCards()
{
    // Remove all existing cards
    for (RecentBookCard* card : m_bookCards) {
        m_recentBooksLayout->removeWidget(card);
        card->deleteLater();
    }
    m_bookCards.clear();
}

void DashboardPanel::refreshRecentBooks()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::refreshRecentBooks called");

    // Clear existing cards
    clearRecentBookCards();

    // Get recent files from manager
    const QStringList recentFiles = core::RecentBooksManager::getInstance().getRecentFiles();

    if (recentFiles.isEmpty()) {
        m_noRecentBooksLabel->show();
        m_recentBooksContainer->hide();
        return;
    }

    m_noRecentBooksLabel->hide();
    m_recentBooksContainer->show();

    // Create cards for recent files (max MAX_RECENT_BOOKS)
    int count = 0;
    for (const QString& filePath : recentFiles) {
        if (count >= MAX_RECENT_BOOKS) {
            break;
        }

        RecentBookCard* card = new RecentBookCard(filePath, this);
        connect(card, &RecentBookCard::clicked,
                this, &DashboardPanel::onRecentBookClicked);

        m_recentBooksLayout->addWidget(card);
        m_bookCards.append(card);
        ++count;
    }

    logger.debug("DashboardPanel: Displayed {} recent books", count);
}

void DashboardPanel::onRecentBookClicked(const QString& filePath)
{
    auto& logger = core::Logger::getInstance();
    logger.info("DashboardPanel: Recent book clicked: {}", filePath.toStdString());

    emit openRecentBookRequested(filePath);
}

} // namespace gui
} // namespace kalahari
