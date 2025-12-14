/// @file dashboard_panel.cpp
/// @brief Dashboard panel implementation - pure HTML rendering
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books
/// Redesign: Pure HTML rendering via QTextBrowser

#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QVBoxLayout>
#include <QTextBrowser>
#include <QFileInfo>
#include <QDateTime>

namespace kalahari {
namespace gui {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
    , m_browser(nullptr)
    , m_autoLoadCheckbox(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel constructor called");

    setupUI();

    // Connect to RecentBooksManager for auto-refresh
    connect(&core::RecentBooksManager::getInstance(), &core::RecentBooksManager::recentFilesChanged,
            this, &DashboardPanel::refreshContent);

    // Connect to ThemeManager for theme changes
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &DashboardPanel::onThemeChanged);

    logger.debug("DashboardPanel initialized with HTML rendering");
}

void DashboardPanel::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // QTextBrowser for HTML content
    m_browser = new QTextBrowser(this);
    m_browser->setOpenLinks(false);  // Handle links manually
    m_browser->setOpenExternalLinks(false);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setHtml(generateHtml());

    connect(m_browser, &QTextBrowser::anchorClicked,
            this, &DashboardPanel::onAnchorClicked);

    layout->addWidget(m_browser, 1);

    // Auto-load checkbox at the bottom
    m_autoLoadCheckbox = new QCheckBox(tr("Open last project on startup"), this);
    m_autoLoadCheckbox->setToolTip(tr("Automatically open the most recently used project when Kalahari starts"));
    m_autoLoadCheckbox->setContentsMargins(11, 6, 11, 11);

    // Load current setting
    auto& settings = core::SettingsManager::getInstance();
    m_autoLoadCheckbox->setChecked(settings.get<bool>("startup.autoLoadLastProject", false));

    // Connect to save setting when changed
    connect(m_autoLoadCheckbox, &QCheckBox::toggled, this, [](bool checked) {
        auto& settings = core::SettingsManager::getInstance();
        settings.set("startup.autoLoadLastProject", checked);
    });

    layout->addWidget(m_autoLoadCheckbox);
}

QString DashboardPanel::generateCss()
{
    // Get theme colors for dynamic styling
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

    // Primary color for headings and accents
    QString primaryColor = theme.colors.primary.name();
    // Text color for body text
    QString textColor = theme.colors.text.name();
    // Secondary/muted color for less important text
    QString mutedColor = theme.colors.secondary.name();
    // Background color
    QString bgColor = theme.colors.background.name();
    // Accent for highlights
    QString accentColor = theme.colors.accent.name();

    // Card background with transparency (works with both light and dark themes)
    QColor cardBg = theme.colors.primary;
    cardBg.setAlpha(25);  // ~10% opacity
    QString cardBgColor = QString("rgba(%1, %2, %3, 0.1)")
        .arg(cardBg.red()).arg(cardBg.green()).arg(cardBg.blue());

    QColor cardHoverBg = theme.colors.primary;
    cardHoverBg.setAlpha(51);  // ~20% opacity
    QString cardHoverBgColor = QString("rgba(%1, %2, %3, 0.2)")
        .arg(cardHoverBg.red()).arg(cardHoverBg.green()).arg(cardHoverBg.blue());

    // Code background for keyboard shortcuts
    QColor codeBg = theme.palette.base;
    QString codeBgColor = codeBg.name();

    return QString(
        "body {"
        "  font-family: 'Segoe UI', 'Arial', sans-serif;"
        "  padding: 40px;"
        "  background-color: %1;"
        "  color: %2;"
        "  max-width: 600px;"
        "  margin: 0 auto;"
        "}"
        ".welcome {"
        "  text-align: center;"
        "  margin-bottom: 30px;"
        "}"
        ".welcome h1 {"
        "  color: %3;"
        "  font-size: 28px;"
        "  margin-bottom: 10px;"
        "  font-weight: 600;"
        "}"
        ".welcome .subtitle {"
        "  color: %4;"
        "  font-size: 14px;"
        "  margin-bottom: 20px;"
        "}"
        ".welcome .description {"
        "  color: %2;"
        "  font-size: 12px;"
        "  margin-bottom: 15px;"
        "}"
        ".shortcuts {"
        "  text-align: center;"
        "  margin-bottom: 30px;"
        "}"
        ".shortcuts-title {"
        "  font-weight: bold;"
        "  margin-bottom: 10px;"
        "  color: %4;"
        "}"
        ".shortcut {"
        "  color: %4;"
        "  margin: 5px 0;"
        "  font-size: 11px;"
        "}"
        ".shortcut code {"
        "  background: %5;"
        "  padding: 2px 6px;"
        "  border-radius: 3px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "}"
        ".recent-section {"
        "  margin-top: 20px;"
        "}"
        ".recent-title {"
        "  color: %3;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  margin-bottom: 15px;"
        "}"
        ".book-card {"
        "  background: %6;"
        "  border-radius: 8px;"
        "  padding: 12px 15px;"
        "  margin-bottom: 10px;"
        "  border-left: 3px solid %3;"
        "}"
        ".book-title {"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "  margin-bottom: 4px;"
        "  color: %2;"
        "}"
        ".book-info {"
        "  color: %4;"
        "  font-size: 12px;"
        "}"
        "a {"
        "  text-decoration: none;"
        "  color: inherit;"
        "}"
        ".no-recent {"
        "  color: %4;"
        "  padding: 15px 0;"
        "  font-style: italic;"
        "}"
    ).arg(bgColor, textColor, primaryColor, mutedColor, codeBgColor, cardBgColor);
}

QString DashboardPanel::generateBookCardHtml(const QString& filePath)
{
    QFileInfo fileInfo(filePath);

    // Extract title (filename without extension)
    QString title = fileInfo.completeBaseName();
    if (title.endsWith(".klh", Qt::CaseInsensitive)) {
        title = title.left(title.length() - 4);
    }

    // Format date
    QString dateStr;
    if (fileInfo.exists()) {
        QDateTime lastModified = fileInfo.lastModified();
        dateStr = tr("Last modified: %1").arg(lastModified.toString("MMM d, yyyy"));
    } else {
        dateStr = tr("File not found");
    }

    // Create URL for the file (use file:// scheme)
    QUrl fileUrl = QUrl::fromLocalFile(filePath);

    return QString(
        "<a href=\"%1\">"
        "<div class=\"book-card\">"
        "<div class=\"book-title\">%2</div>"
        "<div class=\"book-info\">%3</div>"
        "</div>"
        "</a>"
    ).arg(fileUrl.toString().toHtmlEscaped(), title.toHtmlEscaped(), dateStr.toHtmlEscaped());
}

QString DashboardPanel::generateHtml()
{
    QString css = generateCss();

    // Build recent books section
    QString recentBooksHtml;
    const QStringList recentFiles = core::RecentBooksManager::getInstance().getRecentFiles();

    if (recentFiles.isEmpty()) {
        recentBooksHtml = QString(
            "<div class=\"no-recent\">%1</div>"
        ).arg(tr("No recent books. Create a new project or open an existing one."));
    } else {
        int count = 0;
        for (const QString& filePath : recentFiles) {
            if (count >= MAX_RECENT_BOOKS) {
                break;
            }
            recentBooksHtml += generateBookCardHtml(filePath);
            ++count;
        }
    }

    // Build complete HTML document
    return QString(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<style>%1</style>"
        "</head>"
        "<body>"
        "<div class=\"welcome\">"
        "<h1>%2</h1>"
        "<p class=\"subtitle\">%3</p>"
        "<p class=\"description\">%4</p>"
        "</div>"
        "<div class=\"shortcuts\">"
        "<div class=\"shortcuts-title\">%5</div>"
        "<div class=\"shortcut\">%6 <code>Ctrl+Shift+N</code></div>"
        "<div class=\"shortcut\">%7 <code>Ctrl+O</code></div>"
        "<div class=\"shortcut\">%8 <code>Ctrl+1</code></div>"
        "<div class=\"shortcut\">%9 <code>Ctrl+2</code></div>"
        "</div>"
        "<div class=\"recent-section\">"
        "<div class=\"recent-title\">%10</div>"
        "%11"
        "</div>"
        "</body>"
        "</html>"
    ).arg(
        css,
        tr("Welcome to Kalahari"),
        tr("A Writer's IDE for book authors"),
        tr("Create a new book or open an existing one to get started."),
        tr("Keyboard Shortcuts"),
        tr("New Book:"),
        tr("Open Book:"),
        tr("Toggle Navigator:"),
        tr("Toggle Properties:"),
        tr("Recent Books"),
        recentBooksHtml
    );
}

void DashboardPanel::onAnchorClicked(const QUrl& url)
{
    auto& logger = core::Logger::getInstance();

    if (url.scheme() == "file") {
        QString filePath = url.toLocalFile();
        logger.info("DashboardPanel: Recent book clicked: {}", filePath.toStdString());
        emit openRecentBookRequested(filePath);
    } else {
        logger.debug("DashboardPanel: Ignoring non-file URL: {}", url.toString().toStdString());
    }
}

void DashboardPanel::refreshContent()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::refreshContent called");

    m_browser->setHtml(generateHtml());
}

void DashboardPanel::onThemeChanged()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::onThemeChanged - regenerating HTML with new theme colors");

    // Regenerate HTML with updated theme colors
    m_browser->setHtml(generateHtml());
}

} // namespace gui
} // namespace kalahari
