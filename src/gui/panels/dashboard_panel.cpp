/// @file dashboard_panel.cpp
/// @brief Dashboard panel implementation
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books

#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/widgets/recent_book_card.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QGroupBox>
#include <QScrollArea>
#include <QFont>
#include <QFrame>

namespace kalahari {
namespace gui {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
    , m_welcomeLabel(nullptr)
    , m_recentBooksLayout(nullptr)
    , m_recentBooksContainer(nullptr)
    , m_noRecentBooksLabel(nullptr)
    , m_autoLoadCheckbox(nullptr)
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
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(0);

    // Top stretch to center welcome content
    mainLayout->addStretch(1);

    // Welcome section (centered, professional HTML styling)
    mainLayout->addWidget(createWelcomeHeader(), 0, Qt::AlignCenter);

    // Bottom stretch before recent books
    mainLayout->addStretch(1);

    // Separator line
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("QFrame { color: #bdc3c7; margin: 20px 0px; }");
    mainLayout->addWidget(separator);

    // Recent Books section (below, left-aligned)
    mainLayout->addWidget(createRecentBooksSection());

    // Checkbox at the very bottom
    mainLayout->addSpacing(15);
    m_autoLoadCheckbox = new QCheckBox(tr("Open last project on startup"), this);
    m_autoLoadCheckbox->setToolTip(tr("Automatically open the most recently used project when Kalahari starts"));

    // Load current setting
    auto& settings = core::SettingsManager::getInstance();
    m_autoLoadCheckbox->setChecked(settings.get<bool>("startup.autoLoadLastProject", false));

    // Connect to save setting when changed
    connect(m_autoLoadCheckbox, &QCheckBox::toggled, this, [](bool checked) {
        auto& settings = core::SettingsManager::getInstance();
        settings.set("startup.autoLoadLastProject", checked);
    });

    mainLayout->addWidget(m_autoLoadCheckbox);

    // Small bottom margin
    mainLayout->addSpacing(20);

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    // Initial population of recent books
    refreshRecentBooks();
}

QWidget* DashboardPanel::createWelcomeHeader()
{
    QWidget* headerWidget = new QWidget(this);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    // Create welcome label with professional HTML styling (restored original design)
    m_welcomeLabel = new QLabel(this);
    m_welcomeLabel->setText(
        tr("<h1 style='color: #2c3e50; font-size: 28px; margin-bottom: 10px;'>Welcome to Kalahari</h1>"
           "<p style='font-size: 14px; color: #7f8c8d; margin-bottom: 20px;'>"
           "A Writer's IDE for book authors"
           "</p>"
           "<p style='font-size: 12px; color: #34495e; margin-bottom: 15px;'>"
           "Create a new book or open an existing one to get started."
           "</p>"
           "<p style='font-size: 11px; color: #95a5a6; line-height: 1.6;'>"
           "<b>Keyboard Shortcuts:</b><br>"
           "&bull; New Book: <code>Ctrl+Shift+N</code><br>"
           "&bull; Open Book: <code>Ctrl+O</code><br>"
           "&bull; Toggle Navigator: <code>Ctrl+1</code><br>"
           "&bull; Toggle Properties: <code>Ctrl+2</code><br>"
           "</p>")
    );
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setWordWrap(true);
    m_welcomeLabel->setTextFormat(Qt::RichText);
    m_welcomeLabel->setMinimumWidth(400);

    headerLayout->addWidget(m_welcomeLabel);

    return headerWidget;
}

QWidget* DashboardPanel::createRecentBooksSection()
{
    // Create widget for recent books section (no group box for cleaner look)
    QWidget* recentWidget = new QWidget(this);
    QVBoxLayout* sectionLayout = new QVBoxLayout(recentWidget);
    sectionLayout->setContentsMargins(0, 10, 0, 0);
    sectionLayout->setSpacing(10);

    // Section title
    QLabel* titleLabel = new QLabel(tr("<b style='font-size: 14px; color: #2c3e50;'>Recent Books</b>"), this);
    titleLabel->setTextFormat(Qt::RichText);
    sectionLayout->addWidget(titleLabel);

    // Container for book cards
    m_recentBooksContainer = new QWidget(this);
    m_recentBooksLayout = new QVBoxLayout(m_recentBooksContainer);
    m_recentBooksLayout->setContentsMargins(0, 0, 0, 0);
    m_recentBooksLayout->setSpacing(6);

    // "No recent books" message (hidden by default)
    m_noRecentBooksLabel = new QLabel(tr("No recent books. Create a new project or open an existing one."), this);
    m_noRecentBooksLabel->setStyleSheet("color: #7f8c8d; padding: 15px 0px;");
    m_noRecentBooksLabel->setAlignment(Qt::AlignLeft);
    m_noRecentBooksLabel->hide();

    sectionLayout->addWidget(m_recentBooksContainer);
    sectionLayout->addWidget(m_noRecentBooksLabel);

    return recentWidget;
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
