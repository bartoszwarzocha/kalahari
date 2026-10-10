/// @file dashboard_panel.cpp
/// @brief Dashboard panel implementation - native Qt widgets
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books
/// Redesign: Native Qt widgets (QGridLayout, QLabel, QFrame)

#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/dashboard_layouts.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/path_label.h"
#include "kalahari/gui/widgets/wrapping_label.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/resource_paths.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QFileInfo>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QTimer>
#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

constexpr double LOGO_SHARE_OF_HEIGHT = 0.4;  ///< The logo's share of the panel's height at most

} // namespace

/// @brief Clickable card widget with hover effect
class ClickableCard : public QFrame {
public:
    explicit ClickableCard(QWidget* parent = nullptr) : QFrame(parent) {
        setCursor(Qt::PointingHandCursor);
        setAutoFillBackground(true);
        // Allow card to expand vertically to fit content (wrapped path text)
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    std::function<void()> onClick;

    /// @brief Set the hover background color
    void setHoverColor(const QColor& color) {
        m_hoverColor = color;
    }

    /// @brief Set the border color for the card
    void setBorderColor(const QColor& color) {
        m_borderColor = color;
        // Apply normal style with border initially
        applyNormalStyle();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onClick) {
            onClick();
        }
        QFrame::mousePressEvent(event);
    }

    void enterEvent(QEnterEvent* event) override {
        // Highlight on hover - keep border visible
        if (m_hoverColor.isValid()) {
            QString borderStyle = m_borderColor.isValid()
                ? QString("border: 1px solid %1;").arg(m_borderColor.name())
                : QString();
            setStyleSheet(QString("QFrame#recentFileCard { background: %1; border-radius: 0px; %2 }")
                .arg(m_hoverColor.name(), borderStyle));
        }
        QFrame::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        // Return to normal with subtle border
        applyNormalStyle();
        QFrame::leaveEvent(event);
    }

private:
    void applyNormalStyle() {
        QString borderStyle = m_borderColor.isValid()
            ? QString("border: 1px solid %1;").arg(m_borderColor.name())
            : QString();
        setStyleSheet(QString("QFrame#recentFileCard { background: transparent; border-radius: 0px; %1 }")
            .arg(borderStyle));
    }

    QColor m_hoverColor;
    QColor m_borderColor;
};

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
    , m_scrollArea(nullptr)
    , m_contentWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_logoLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_taglineLabel(nullptr)
    , m_shortcutsFrame(nullptr)
    , m_shortcutsTitleLabel(nullptr)
    , m_columnsWidget(nullptr)
    , m_newsColumn(nullptr)
    , m_recentFilesColumn(nullptr)
    , m_columnDivider(nullptr)
    , m_newsIcon(nullptr)
    , m_newsTitle(nullptr)
    , m_filesIcon(nullptr)
    , m_filesTitle(nullptr)
    , m_newsListLayout(nullptr)
    , m_filesListLayout(nullptr)
    , m_newsListWidget(nullptr)
    , m_filesListWidget(nullptr)
    , m_autoLoadCheckbox(nullptr)
    , m_singleColumnMode(false)
    , m_columnsGridLayout(nullptr)
    , m_showNews(true)
    , m_showRecentFiles(true)
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

    // Connect to ArtProvider for icon theme/color changes
    // This ensures dashboard refreshes when user changes icon style or colors in Settings
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, &DashboardPanel::onThemeChanged);

    logger.debug("DashboardPanel initialized with native Qt widgets");
}

void DashboardPanel::setupUI()
{
    // Main layout for the panel
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Scroll area for content
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    // The content wraps to the panel's width; only a panel narrower than its longest word
    // scrolls sideways, so that nothing is cut off
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Content widget inside scroll area
    m_contentWidget = new QWidget(m_scrollArea);
    m_scrollArea->setWidget(m_contentWidget);

    // Main layout - the content in the middle, at 75% of the width (more where it likes more,
    // and on a narrow panel all of it but a margin on each side)
    DashboardContentLayout* centeringLayout = new DashboardContentLayout(m_contentWidget);

    // Content container
    QWidget* centeredContent = new QWidget(m_contentWidget);
    QVBoxLayout* contentLayout = new QVBoxLayout(centeredContent);
    contentLayout->setContentsMargins(0, 48, 0, 32);  // Top and bottom padding
    contentLayout->setSpacing(32);

    // 1. Header section
    QWidget* headerWidget = createHeaderSection(centeredContent);
    contentLayout->addWidget(headerWidget);

    // 2. Shortcuts section
    QWidget* shortcutsWidget = createShortcutsSection(centeredContent);
    contentLayout->addWidget(shortcutsWidget);

    // 3. Main content (News + Recent Files)
    QWidget* mainContent = createMainContentSection(centeredContent);
    contentLayout->addWidget(mainContent, 1);

    // 4. Auto-load checkbox - with extra bottom padding for visibility. Its text is a label
    //    beside it, which wraps on a narrow panel (the text of a QCheckBox cannot)
    QWidget* checkboxContainer = new QWidget(centeredContent);
    QHBoxLayout* checkboxLayout = new QHBoxLayout(checkboxContainer);
    checkboxLayout->setContentsMargins(0, 24, 0, 32);  // Extra bottom margin
    checkboxLayout->setSpacing(style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing));

    const QString autoLoadText = tr("Open last project on startup");
    const QString autoLoadTip =
        tr("Automatically open the most recently used project when Kalahari starts");
    m_autoLoadCheckbox = new QCheckBox(checkboxContainer);
    m_autoLoadCheckbox->setAccessibleName(autoLoadText);
    m_autoLoadCheckbox->setToolTip(autoLoadTip);
    m_autoLoadLabel = new WrappingLabel(autoLoadText, checkboxContainer);
    m_autoLoadLabel->setBuddy(m_autoLoadCheckbox);
    m_autoLoadLabel->setToolTip(autoLoadTip);
    m_autoLoadLabel->installEventFilter(this);

    checkboxLayout->addStretch();
    checkboxLayout->addWidget(m_autoLoadCheckbox, 0, Qt::AlignTop);
    checkboxLayout->addWidget(m_autoLoadLabel);
    checkboxLayout->addStretch();

    contentLayout->addWidget(checkboxContainer);

    centeringLayout->addWidget(centeredContent);

    outerLayout->addWidget(m_scrollArea);

    // Load current setting for auto-load checkbox
    // Note: SettingsManager::load() must be called before DashboardPanel is created (done in main.cpp)
    auto& settingsMgr = core::SettingsManager::getInstance();
    bool autoLoadSetting = settingsMgr.get<bool>("startup.autoLoadLastProject");
    core::Logger::getInstance().info("Dashboard: startup.autoLoadLastProject from settings = {}", autoLoadSetting);
    m_autoLoadCheckbox->setChecked(autoLoadSetting);
    core::Logger::getInstance().info("Dashboard: checkbox setChecked({}) done, isChecked() = {}",
                                     autoLoadSetting, m_autoLoadCheckbox->isChecked());

    // Connect checkbox to save setting
    connect(m_autoLoadCheckbox, &QCheckBox::toggled, this, [](bool checked) {
        auto& settings = core::SettingsManager::getInstance();
        settings.set("startup.autoLoadLastProject", checked);
    });

    // Apply initial theme colors
    applyThemeColors();

    // Apply column visibility from settings (News/Recent Files show/hide)
    updateColumnVisibility();

    // Populate content
    populateNewsColumn();
    updateRecentFilesList();
}

QWidget* DashboardPanel::createHeaderSection(QWidget* parent)
{
    QWidget* headerWidget = new QWidget(parent);

    // Logo, as large as the header makes it (at most 256x256 logical pixels), drawn from the
    // whole image so that it is sharp at any size and scaling
    m_logoLabel = new QLabel(headerWidget);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    // The header gives it its size, whatever the image's own
    m_logoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    QString logoPath = core::ResourcePaths::getInstance().getResourcesDir() + "/images/app_logo.png";
    QPixmap logoPixmap(logoPath);
    if (!logoPixmap.isNull()) {
        m_logoLabel->setScaledContents(true);
        m_logoLabel->setPixmap(logoPixmap);
    } else {
        core::Logger::getInstance().warn("DashboardPanel: Could not load logo from {}", logoPath.toStdString());
        m_logoLabel->setText(tr("Logo"));
    }

    // Title and tagline: beside the logo each fits in its line; under it they are centered
    // and wrap where the panel is narrower than their line
    m_titleLabel = new WrappingLabel(tr("Welcome to Kalahari"), headerWidget);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setWeight(QFont::Light);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_taglineLabel = new WrappingLabel(tr("A Comprehensive Writer's IDE"), headerWidget);
    QFont taglineFont = m_taglineLabel->font();
    taglineFont.setPointSize(12);
    taglineFont.setWeight(QFont::Light);
    m_taglineLabel->setFont(taglineFont);
    m_taglineLabel->setAlignment(Qt::AlignCenter);

    // [Logo] | [Title / Tagline], or the logo above them
    m_headerLayout = new DashboardHeaderLayout(headerWidget);
    m_headerLayout->addWidget(m_logoLabel);
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addWidget(m_taglineLabel);

    return headerWidget;
}

QWidget* DashboardPanel::createShortcutsSection(QWidget* parent)
{
    m_shortcutsFrame = new QFrame(parent);
    m_shortcutsFrame->setFrameShape(QFrame::StyledPanel);
    m_shortcutsFrame->setObjectName("shortcutsFrame");

    QVBoxLayout* layout = new QVBoxLayout(m_shortcutsFrame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // Title
    m_shortcutsTitleLabel = new WrappingLabel(tr("KEYBOARD SHORTCUTS"), m_shortcutsFrame);
    QFont titleFont = m_shortcutsTitleLabel->font();
    titleFont.setPointSize(9);
    titleFont.setWeight(QFont::Light);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
    m_shortcutsTitleLabel->setFont(titleFont);
    m_shortcutsTitleLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_shortcutsTitleLabel);

    // Shortcuts in a row, or one under another where the panel is narrow
    QWidget* shortcutsRow = new QWidget(m_shortcutsFrame);
    m_shortcutsLayout = new DashboardHintsLayout(shortcutsRow);
    layout->addWidget(shortcutsRow);

    updateShortcutLabels();
    // The keys the user gives the commands in Settings > Keyboard Shortcuts
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, this,
            &DashboardPanel::updateShortcutLabels);

    return m_shortcutsFrame;
}

void DashboardPanel::updateShortcutLabels()
{
    for (QLabel* label : m_shortcutLabels) {
        m_shortcutsLayout->removeWidget(label);
        delete label;
    }
    m_shortcutLabels.clear();

    // The commands to start with, named as in the menus and with the keys they have
    // (written here a second time, they drifted apart: Ctrl+N was shown as New Chapter)
    const CommandRegistry& registry = CommandRegistry::getInstance();
    for (const char* id : {"file.new.project", "file.open", "file.open.file"}) {
        const Command* command = registry.getCommand(id);
        if (command == nullptr || command->shortcut.isEmpty()) {
            continue;
        }
        QString name = QString::fromStdString(command->label);
        if (name.endsWith(QStringLiteral("..."))) {
            name.chop(3);
        } else if (name.endsWith(QChar(0x2026))) {  // an ellipsis
            name.chop(1);
        }
        const QString keys = command->shortcut.toQKeySequence().toString(QKeySequence::NativeText);

        // The keys in bold and the command beside them, or under them on a narrow panel
        QWidget* row = m_shortcutsLayout->parentWidget();
        QLabel* keysLabel = new QLabel(row);
        keysLabel->setTextFormat(Qt::RichText);
        keysLabel->setText(
            QString("<span style='font-weight: bold;'>%1</span>").arg(keys.toHtmlEscaped()));
        QLabel* nameLabel = new WrappingLabel(name, row);
        nameLabel->setTextFormat(Qt::PlainText);
        nameLabel->setAlignment(Qt::AlignCenter);
        m_shortcutsLayout->addHint(keysLabel, nameLabel);
        m_shortcutLabels.push_back(keysLabel);
        m_shortcutLabels.push_back(nameLabel);
    }
    styleShortcutLabels();
    // Without a command with keys the section has nothing to show
    m_shortcutsFrame->setVisible(!m_shortcutLabels.empty());
}

void DashboardPanel::styleShortcutLabels()
{
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    for (QLabel* label : m_shortcutLabels) {
        label->setStyleSheet(QString("color: %1; background: transparent;")
                                 .arg(theme.palette.windowText.name()));
    }
}

QWidget* DashboardPanel::createMainContentSection(QWidget* parent)
{
    m_columnsWidget = new QWidget(parent);

    // Use QGridLayout for reliable 50/50 split
    m_columnsGridLayout = new QGridLayout(m_columnsWidget);
    m_columnsGridLayout->setContentsMargins(0, 0, 0, 0);
    m_columnsGridLayout->setSpacing(0);
    // Set equal column stretch for 50/50 split
    m_columnsGridLayout->setColumnStretch(0, 1);  // News column
    m_columnsGridLayout->setColumnStretch(1, 0);  // Divider (fixed)
    m_columnsGridLayout->setColumnStretch(2, 1);  // Files column

    // News column (left)
    m_newsColumn = new QFrame(m_columnsWidget);
    m_newsColumn->setObjectName("newsColumn");
    m_newsColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // Ignored allows grid to control width
    QVBoxLayout* newsLayout = new QVBoxLayout(m_newsColumn);
    newsLayout->setContentsMargins(0, 0, 24, 0);
    newsLayout->setSpacing(16);
    newsLayout->setAlignment(Qt::AlignTop);

    // News header
    QWidget* newsHeader = new QWidget(m_newsColumn);
    QHBoxLayout* newsHeaderLayout = new QHBoxLayout(newsHeader);
    newsHeaderLayout->setContentsMargins(0, 0, 0, 16);
    newsHeaderLayout->setSpacing(12);

    // News icon with background
    //int headerIconSize = 24;
    //int iconSize = getIconSize();
    //QFrame* newsIconFrame = new QFrame(newsHeader);
    //newsIconFrame->setObjectName("newsIconFrame");
    //newsIconFrame->setFixedSize(iconSize + 8, iconSize + 8);  // padding
    //QHBoxLayout* newsIconLayout = new QHBoxLayout(newsIconFrame);
    //newsIconLayout->setContentsMargins(0, 0, 0, 0);
    //newsIconLayout->setAlignment(Qt::AlignCenter);

    //m_newsIcon = new QLabel(newsIconFrame);
    //m_newsIcon->setFixedSize(headerIconSize, headerIconSize);
    //m_newsIcon->setScaledContents(true);  // NO SCALING - pixmap size = label size
    // News icon uses infoPrimary/infoSecondary colors for distinct appearance
    const auto& currentTheme = core::ThemeManager::getInstance().getCurrentTheme();
    //QIcon newsIcon = core::ArtProvider::getInstance().getThemedIcon(
    //    "book.newChapter",
    //    currentTheme.colors.infoPrimary,
    //    currentTheme.colors.infoSecondary);
    //m_newsIcon->setPixmap(newsIcon.pixmap(headerIconSize, headerIconSize));
    //newsIconLayout->addWidget(m_newsIcon);

    m_newsTitle = new WrappingLabel(tr("Kalahari News"), newsHeader);
    QFont newsTitleFont = m_newsTitle->font();
    newsTitleFont.setPointSize(13);
    newsTitleFont.setWeight(QFont::Medium);
    m_newsTitle->setFont(newsTitleFont);

    //newsHeaderLayout->addWidget(newsIconFrame);
    newsHeaderLayout->addWidget(m_newsTitle);
    newsHeaderLayout->addStretch();

    newsLayout->addWidget(newsHeader);

    // News list container
    m_newsListWidget = new QWidget(m_newsColumn);
    m_newsListLayout = new QVBoxLayout(m_newsListWidget);
    m_newsListLayout->setContentsMargins(0, 0, 0, 0);
    m_newsListLayout->setSpacing(4);
    // NOTE: Do NOT use setAlignment(Qt::AlignTop) here - it prevents items from expanding vertically

    newsLayout->addWidget(m_newsListWidget);
    newsLayout->addStretch();

    // Vertical divider - QWidget with background color from theme palette
    m_columnDivider = new QWidget(m_columnsWidget);
    m_columnDivider->setFixedWidth(1);
    m_columnDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_columnDivider->setStyleSheet(QString("background-color: %1;").arg(currentTheme.palette.placeholderText.name()));
    m_columnDivider->setObjectName("columnDivider");

    // Recent files column (right)
    m_recentFilesColumn = new QFrame(m_columnsWidget);
    m_recentFilesColumn->setObjectName("recentFilesColumn");
    m_recentFilesColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // Ignored allows grid to control width
    QVBoxLayout* filesLayout = new QVBoxLayout(m_recentFilesColumn);
    filesLayout->setContentsMargins(24, 0, 0, 0);
    filesLayout->setSpacing(16);
    filesLayout->setAlignment(Qt::AlignTop);

    // Files header
    QWidget* filesHeader = new QWidget(m_recentFilesColumn);
    QHBoxLayout* filesHeaderLayout = new QHBoxLayout(filesHeader);
    filesHeaderLayout->setContentsMargins(0, 0, 0, 16);
    filesHeaderLayout->setSpacing(12);

    // Files icon with background
    //QFrame* filesIconFrame = new QFrame(filesHeader);
    //filesIconFrame->setObjectName("filesIconFrame");
    //filesIconFrame->setFixedSize(iconSize + 8, iconSize + 8);  // padding
    //QHBoxLayout* filesIconLayout = new QHBoxLayout(filesIconFrame);
    //filesIconLayout->setContentsMargins(0, 0, 0, 0);
    //filesIconLayout->setAlignment(Qt::AlignCenter);

    //m_filesIcon = new QLabel(filesIconFrame);
    //m_filesIcon->setFixedSize(headerIconSize, headerIconSize);
    //m_filesIcon->setScaledContents(true);  // NO SCALING - pixmap size = label size
    // Recent Files icon uses dashboardPrimary/dashboardSecondary colors for distinct appearance
    //QIcon filesIcon = core::ArtProvider::getInstance().getThemedIcon(
    //    "file.open",
    //    currentTheme.colors.dashboardPrimary,
    //    currentTheme.colors.dashboardSecondary);
    //m_filesIcon->setPixmap(filesIcon.pixmap(headerIconSize, headerIconSize));
    //filesIconLayout->addWidget(m_filesIcon);

    m_filesTitle = new WrappingLabel(tr("Recent Files"), filesHeader);
    QFont filesTitleFont = m_filesTitle->font();
    filesTitleFont.setPointSize(13);
    filesTitleFont.setWeight(QFont::Medium);
    m_filesTitle->setFont(filesTitleFont);

    //filesHeaderLayout->addWidget(filesIconFrame);
    filesHeaderLayout->addWidget(m_filesTitle);
    filesHeaderLayout->addStretch();

    filesLayout->addWidget(filesHeader);

    // Files list container
    m_filesListWidget = new QWidget(m_recentFilesColumn);
    m_filesListLayout = new QVBoxLayout(m_filesListWidget);
    m_filesListLayout->setContentsMargins(0, 0, 0, 0);
    m_filesListLayout->setSpacing(4);
    // NOTE: Do NOT use setAlignment(Qt::AlignTop) here - it prevents cards from expanding vertically

    filesLayout->addWidget(m_filesListWidget);
    filesLayout->addStretch();

    // Add to grid layout with 50/50 split (row 0, columns 0, 1, 2)
    m_columnsGridLayout->addWidget(m_newsColumn, 0, 0);
    m_columnsGridLayout->addWidget(m_columnDivider, 0, 1);
    m_columnsGridLayout->addWidget(m_recentFilesColumn, 0, 2);

    return m_columnsWidget;
}

QWidget* DashboardPanel::createRecentFileCard(const QString& filePath, QWidget* parent)
{
    QFileInfo fileInfo(filePath);
    QString title = fileInfo.completeBaseName();
    QString author = tr("Unknown Author");
    QString bookType = tr("Novel");
    QString genre = "fiction";  // Default genre for icon selection
    QString dateStr;

    // Try to read metadata from .klh file
    if (fileInfo.exists() && filePath.endsWith(".klh", Qt::CaseInsensitive)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray fileData = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(fileData);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                // Read from "document" object (correct .klh format)
                if (obj.contains("document")) {
                    QJsonObject docObj = obj["document"].toObject();
                    if (docObj.contains("author") && !docObj["author"].toString().isEmpty()) {
                        author = docObj["author"].toString();
                    }
                    if (docObj.contains("title") && !docObj["title"].toString().isEmpty()) {
                        title = docObj["title"].toString();
                    }
                    if (docObj.contains("genre") && !docObj["genre"].toString().isEmpty()) {
                        genre = docObj["genre"].toString().toLower();
                        // Capitalize first letter for display
                        bookType = genre.at(0).toUpper() + genre.mid(1);
                    }
                }
            }
        }

        QDateTime lastModified = fileInfo.lastModified();
        dateStr = lastModified.toString("yyyy-MM-dd hh:mm");
    } else if (fileInfo.exists()) {
        QDateTime lastModified = fileInfo.lastModified();
        dateStr = lastModified.toString("yyyy-MM-dd hh:mm");
    } else {
        dateStr = tr("File not found");
    }

    // Create clickable card
    ClickableCard* card = new ClickableCard(parent);
    card->setObjectName("recentFileCard");
    card->setProperty("filePath", filePath);

    // Set colors from current theme
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    card->setHoverColor(theme.palette.alternateBase);
    // Card border needs good contrast but not overwhelming
    // Blend mid with text color (30% text) for better visibility in all themes
    QColor borderColor = theme.palette.button;
    //int r = borderColor.red() * 0.7 + theme.palette.text.red() * 0.3;
    //int g = borderColor.green() * 0.7 + theme.palette.text.green() * 0.3;
    //int b = borderColor.blue() * 0.7 + theme.palette.text.blue() * 0.3;
    //card->setBorderColor(QColor(r, g, b));
    card->setBorderColor(borderColor);

    // The icon beside the texts, or above them in a narrow card
    DashboardCardLayout* cardLayout = new DashboardCardLayout(card);

    // Book icon - use auto_stories from twotone style
    int iconSize = getIconSize();
    QFrame* iconFrame = new QFrame(card);
    iconFrame->setObjectName("fileIconFrame");
    iconFrame->setFixedSize(iconSize + 8, iconSize + 8);  // padding
    QHBoxLayout* iconLayout = new QHBoxLayout(iconFrame);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setAlignment(Qt::AlignCenter);

    QLabel* iconLabel = new QLabel(iconFrame);
    iconLabel->setObjectName("cardBookIcon");  // Named for theme refresh
    iconLabel->setFixedSize(iconSize, iconSize);
    iconLabel->setScaledContents(false);  // NO SCALING - pixmap size = label size

    // Select icon based on genre using centralized ArtProvider mapping
    QString iconActionId = core::ArtProvider::getInstance().getGenreIconId(genre);
    // Store genre for theme refresh
    iconLabel->setProperty("genre", genre);

    // Recent Files book icons use dashboardPrimary/dashboardSecondary colors for distinct appearance
    QIcon bookIcon = core::ArtProvider::getInstance().getThemedIcon(
        iconActionId,
        theme.colors.dashboardPrimary,
        theme.colors.dashboardSecondary);
    iconLabel->setPixmap(bookIcon.pixmap(iconSize, iconSize));
    iconLayout->addWidget(iconLabel);

    cardLayout->addWidget(iconFrame);

    // Content - MUST be transparent for hover to work!
    QWidget* contentWidget = new QWidget(card);
    contentWidget->setStyleSheet("background: transparent;");
    // Allow content to expand to fit wrapped text
    contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(2);

    // Title
    QLabel* titleLabel = new QLabel(title, contentWidget);
    titleLabel->setObjectName("cardTitle");
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setWordWrap(true);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);

    // Description (author | type)
    QLabel* descLabel = new QLabel(QString("%1 | %2").arg(author, bookType), contentWidget);
    descLabel->setObjectName("cardDescription");
    descLabel->setTextFormat(Qt::PlainText);
    descLabel->setWordWrap(true);
    QFont descFont = descLabel->font();
    descFont.setPointSize(11);
    descLabel->setFont(descFont);

    // Path - it wraps after its separators, and inside a name longer than the line
    QLabel* pathLabel = new PathLabel(filePath, contentWidget);
    pathLabel->setObjectName("cardPath");
    QFont pathFont = pathLabel->font();
    pathFont.setPointSize(8);
    pathLabel->setFont(pathFont);

    // Date
    QLabel* dateLabel = new QLabel(dateStr, contentWidget);
    dateLabel->setObjectName("cardDate");
    dateLabel->setWordWrap(true);
    QFont dateFont = dateLabel->font();
    dateFont.setPointSize(8);
    dateLabel->setFont(dateFont);

    contentLayout->addWidget(titleLabel);
    contentLayout->addWidget(descLabel);
    contentLayout->addWidget(pathLabel);
    contentLayout->addWidget(dateLabel);

    cardLayout->addWidget(contentWidget);

    // Set click handler
    card->onClick = [this, filePath]() {
        core::Logger::getInstance().info("DashboardPanel: Recent book clicked: {}", filePath.toStdString());
        emit openRecentBookRequested(filePath);
    };

    // Store card reference for later
    m_fileCards.push_back({card, filePath});

    return card;
}

void DashboardPanel::updateRecentFilesList()
{
    // Clear existing cards
    m_fileCards.clear();
    gui::utils::clearLayout(m_filesListLayout);

    // Get recent files
    const QStringList recentFiles = core::RecentBooksManager::getInstance().getRecentFiles();

    if (recentFiles.isEmpty()) {
        QLabel* noFilesLabel = new QLabel(tr("No recent projects yet."), m_filesListWidget);
        noFilesLabel->setObjectName("noItemsLabel");
        noFilesLabel->setWordWrap(true);
        QFont font = noFilesLabel->font();
        font.setItalic(true);
        noFilesLabel->setFont(font);
        noFilesLabel->setContentsMargins(12, 12, 12, 12);
        m_filesListLayout->addWidget(noFilesLabel);
    } else {
        int count = 0;
        int maxItems = getMaxItems();
        for (const QString& filePath : recentFiles) {
            if (count >= maxItems) break;
            QWidget* card = createRecentFileCard(filePath, m_filesListWidget);
            m_filesListLayout->addWidget(card);
            ++count;
        }
    }

    // Apply theme colors to new widgets
    applyThemeColors();
}

void DashboardPanel::populateNewsColumn()
{
    // Clear existing items
    gui::utils::clearLayout(m_newsListLayout);

    // Placeholder for news
    QLabel* noNewsLabel = new QLabel(tr("No news yet."), m_newsListWidget);
    noNewsLabel->setObjectName("noItemsLabel");
    noNewsLabel->setWordWrap(true);
    QFont font = noNewsLabel->font();
    font.setItalic(true);
    noNewsLabel->setFont(font);
    noNewsLabel->setContentsMargins(12, 12, 12, 12);
    m_newsListLayout->addWidget(noNewsLabel);
}

void DashboardPanel::applyThemeColors()
{
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

    // Background color
    QString bgColor = theme.palette.window.name();
    QString bgCard = theme.palette.base.name();
    QString bgHover = theme.palette.alternateBase.name();
    QString borderColor = theme.palette.mid.name();
    QString primaryColor = theme.colors.infoHeader.name();
    QString textColor = theme.palette.windowText.name();
    QString mutedColor = theme.palette.placeholderText.name();

    // Apply to scroll area
    m_scrollArea->setStyleSheet(QString("QScrollArea { background: %1; border: none; }").arg(bgColor));
    m_contentWidget->setStyleSheet(QString("background: %1;").arg(bgColor));

    // Title styling
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("color: %1;").arg(primaryColor));
    }

    // Tagline styling
    if (m_taglineLabel) {
        m_taglineLabel->setStyleSheet(QString("color: %1;").arg(mutedColor));
    }

    // Shortcuts frame styling - uniform background for entire frame
    if (m_shortcutsFrame) {
        m_shortcutsFrame->setStyleSheet(QString(
            "QFrame#shortcutsFrame { background: %1; border: 0px solid %2; border-radius: 0px; }"
            "QFrame#shortcutsFrame QLabel { background: transparent; }"
            "QFrame#shortcutsFrame QWidget { background: transparent; }"
        ).arg(bgCard, borderColor));
    }

    // Shortcuts title
    if (m_shortcutsTitleLabel) {
        m_shortcutsTitleLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(mutedColor));
    }

    // Shortcut labels - ensure transparent background
    styleShortcutLabels();

    // Column divider - simple vertical line with explicit background color
    if (m_columnDivider) {
        // Use palette.placeholderText - visible in both light and dark themes
        QString dividerColor = theme.palette.placeholderText.name();
        m_columnDivider->setStyleSheet(QString("background-color: %1;").arg(dividerColor));
    }

    // Column titles
    if (m_newsTitle) {
        m_newsTitle->setStyleSheet(QString("color: %1;").arg(textColor));
    }
    if (m_filesTitle) {
        m_filesTitle->setStyleSheet(QString("color: %1;").arg(textColor));
    }

    // Icon frames - NO background, icons use theme colors directly
    QString transparentFrameStyle = "background: transparent; border: none;";

    // Apply transparent style to all icon frames
    QList<QFrame*> newsIconFrames = m_newsColumn->findChildren<QFrame*>("newsIconFrame");
    for (auto* frame : newsIconFrames) {
        frame->setStyleSheet(transparentFrameStyle);
    }

    QList<QFrame*> filesIconFrames = m_recentFilesColumn->findChildren<QFrame*>("filesIconFrame");
    for (auto* frame : filesIconFrames) {
        frame->setStyleSheet(transparentFrameStyle);
    }

    // File cards - all children must have transparent background for hover to work
    QList<QFrame*> fileIconFrames = m_filesListWidget->findChildren<QFrame*>("fileIconFrame");
    for (auto* frame : fileIconFrames) {
        frame->setStyleSheet(transparentFrameStyle);
    }

    // Update card border colors on theme change
    // Note: We iterate over m_fileCards instead of using findChildren<ClickableCard*>()
    // because ClickableCard is defined in this .cpp file without Q_OBJECT macro,
    // and findChildren requires MOC-generated metadata.
    // Card border: blend mid with text color (30% text) for better visibility
    QColor cardBorderColor = theme.palette.midlight;
    //int br = cardBorderColor.red() * 0.7 + theme.palette.text.red() * 0.3;
    //int bg = cardBorderColor.green() * 0.7 + theme.palette.text.green() * 0.3;
    //int bb = cardBorderColor.blue() * 0.7 + theme.palette.text.blue() * 0.3;
    //QColor blendedBorderColor(br, bg, bb);
    for (const auto& cardPair : m_fileCards) {
        if (ClickableCard* card = static_cast<ClickableCard*>(cardPair.first)) {
            card->setHoverColor(theme.palette.alternateBase);
            card->setBorderColor(cardBorderColor);
        }
    }

    // Card text styling - ALL must have transparent background for hover to work!
    QString cardTitleStyle = QString("color: %1; background: transparent;").arg(textColor);
    QString cardDescStyle = QString("color: %1; background: transparent;").arg(mutedColor);
    QString cardPathStyle = QString("color: %1; background: transparent;").arg(mutedColor);
    QString cardDateStyle = QString("color: %1; background: transparent;").arg(mutedColor);

    QList<QLabel*> titles = m_filesListWidget->findChildren<QLabel*>("cardTitle");
    for (auto* label : titles) {
        label->setStyleSheet(cardTitleStyle);
    }

    QList<QLabel*> descs = m_filesListWidget->findChildren<QLabel*>("cardDescription");
    for (auto* label : descs) {
        label->setStyleSheet(cardDescStyle);
    }

    QList<QLabel*> paths = m_filesListWidget->findChildren<QLabel*>("cardPath");
    for (auto* label : paths) {
        label->setStyleSheet(cardPathStyle);
    }

    QList<QLabel*> dates = m_filesListWidget->findChildren<QLabel*>("cardDate");
    for (auto* label : dates) {
        label->setStyleSheet(cardDateStyle);
    }

    // No items labels
    QString noItemsStyle = QString("color: %1;").arg(mutedColor);
    QList<QLabel*> noItemsLabels = findChildren<QLabel*>("noItemsLabel");
    for (auto* label : noItemsLabels) {
        label->setStyleSheet(noItemsStyle);
    }

    // Checkbox text - only its color, Qt draws the indicator
    if (m_autoLoadLabel) {
        m_autoLoadLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    }

    // Refresh icons with current theme colors using getThemedIcon API
    int iconSize = getIconSize();

    // News icon uses infoPrimary/infoSecondary for distinct appearance
    if (m_newsIcon) {
        QIcon newsIcon = core::ArtProvider::getInstance().getThemedIcon(
            "book.newChapter",
            theme.colors.infoPrimary,
            theme.colors.infoSecondary);
        m_newsIcon->setPixmap(newsIcon.pixmap(iconSize, iconSize));
    }

    // Files icon uses dashboardPrimary/dashboardSecondary colors for distinct appearance
    if (m_filesIcon) {
        QIcon filesIcon = core::ArtProvider::getInstance().getThemedIcon(
            "file.open",
            theme.colors.dashboardPrimary,
            theme.colors.dashboardSecondary);
        m_filesIcon->setPixmap(filesIcon.pixmap(iconSize, iconSize));
    }

    // Refresh book icons in file cards - uses dashboardPrimary/dashboardSecondary colors
    // Icons are selected based on genre property stored on each label
    QList<QLabel*> bookIcons = m_filesListWidget->findChildren<QLabel*>("cardBookIcon");
    for (auto* iconLabel : bookIcons) {
        QString genre = iconLabel->property("genre").toString();
        QString iconActionId = core::ArtProvider::getInstance().getGenreIconId(genre);
        QIcon bookIcon = core::ArtProvider::getInstance().getThemedIcon(
            iconActionId,
            theme.colors.dashboardPrimary,
            theme.colors.dashboardSecondary);
        iconLabel->setPixmap(bookIcon.pixmap(iconSize, iconSize));
    }
}

void DashboardPanel::refreshContent()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::refreshContent called");

    updateRecentFilesList();
}

void DashboardPanel::onThemeChanged()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::onThemeChanged - applying new theme colors");

    applyThemeColors();
}

void DashboardPanel::onSettingsChanged()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::onSettingsChanged - refreshing content with updated settings");

    // Sync auto-load checkbox with settings (in case changed from Settings dialog)
    if (m_autoLoadCheckbox) {
        auto& settings = core::SettingsManager::getInstance();
        bool autoLoad = settings.get<bool>("startup.autoLoadLastProject");
        if (m_autoLoadCheckbox->isChecked() != autoLoad) {
            m_autoLoadCheckbox->blockSignals(true);
            m_autoLoadCheckbox->setChecked(autoLoad);
            m_autoLoadCheckbox->blockSignals(false);
            logger.debug("DashboardPanel: Auto-load checkbox synced to {}", autoLoad);
        }
    }

    // Update column visibility based on settings (News/Recent Files show/hide)
    updateColumnVisibility();

    // Refresh recent files list (if visible)
    if (m_showRecentFiles) {
        updateRecentFilesList();
    }
}

void DashboardPanel::onRecentFileClicked()
{
    // This slot is kept for compatibility but actual clicks are handled
    // by ClickableCard's onClick callback
}

void DashboardPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // On a low screen the logo leaves room for the shortcuts and the recent books
    if (m_headerLayout) {
        m_headerLayout->setLargestLogo(
            static_cast<int>(event->size().height() * LOGO_SHARE_OF_HEIGHT));
    }

    // Check if we need to switch layout mode based on width threshold
    bool shouldBeSingleColumn = event->size().width() < SINGLE_COLUMN_THRESHOLD;

    if (shouldBeSingleColumn != m_singleColumnMode) {
        m_singleColumnMode = shouldBeSingleColumn;
        reorganizeLayout(m_singleColumnMode);
    }
}

void DashboardPanel::reorganizeLayout(bool singleColumn)
{
    if (!m_columnsGridLayout || !m_newsColumn || !m_recentFilesColumn || !m_columnDivider) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel::reorganizeLayout - switching to {} column mode",
                 singleColumn ? "single" : "dual");

    // One column under the other is as wide as its cards need at least (a panel narrower
    // than that scrolls sideways instead of cutting them off); two share the width equally
    const QSizePolicy::Policy columnWidth =
        singleColumn ? QSizePolicy::Preferred : QSizePolicy::Ignored;
    m_newsColumn->setSizePolicy(columnWidth, QSizePolicy::Preferred);
    m_recentFilesColumn->setSizePolicy(columnWidth, QSizePolicy::Preferred);

    // Remove all widgets from grid layout first
    m_columnsGridLayout->removeWidget(m_newsColumn);
    m_columnsGridLayout->removeWidget(m_columnDivider);
    m_columnsGridLayout->removeWidget(m_recentFilesColumn);

    if (singleColumn) {
        // Single column: News on row 0, Files on row 1, hide divider
        // (divider is always hidden in single column mode)
        m_columnDivider->hide();

        // Reset column stretch - single column uses full width
        m_columnsGridLayout->setColumnStretch(0, 1);
        m_columnsGridLayout->setColumnStretch(1, 0);
        m_columnsGridLayout->setColumnStretch(2, 0);

        // Set row stretch for equal distribution
        m_columnsGridLayout->setRowStretch(0, 0);  // News - natural height
        m_columnsGridLayout->setRowStretch(1, 0);  // Files - natural height

        // Add vertical spacing between rows
        m_columnsGridLayout->setVerticalSpacing(24);

        // News column adjustments for single column
        QVBoxLayout* newsLayout = qobject_cast<QVBoxLayout*>(m_newsColumn->layout());
        if (newsLayout) {
            newsLayout->setContentsMargins(0, 0, 0, 0);
        }

        // Files column adjustments for single column
        QVBoxLayout* filesLayout = qobject_cast<QVBoxLayout*>(m_recentFilesColumn->layout());
        if (filesLayout) {
            filesLayout->setContentsMargins(0, 0, 0, 0);
        }

        // Add widgets in vertical stack
        m_columnsGridLayout->addWidget(m_newsColumn, 0, 0);
        m_columnsGridLayout->addWidget(m_recentFilesColumn, 1, 0);
    } else {
        // Dual column: News left, Divider center, Files right
        // Show divider ONLY if both columns are visible
        bool showDivider = m_showNews && m_showRecentFiles;
        m_columnDivider->setVisible(showDivider);

        // Reset column stretch based on visibility
        int newsStretch = m_showNews ? 1 : 0;
        int filesStretch = m_showRecentFiles ? 1 : 0;
        m_columnsGridLayout->setColumnStretch(0, newsStretch);
        m_columnsGridLayout->setColumnStretch(1, 0);
        m_columnsGridLayout->setColumnStretch(2, filesStretch);

        // Reset row stretch
        m_columnsGridLayout->setRowStretch(0, 0);
        m_columnsGridLayout->setRowStretch(1, 0);

        // Reset vertical spacing
        m_columnsGridLayout->setVerticalSpacing(0);

        // News column adjustments for dual column
        QVBoxLayout* newsLayout = qobject_cast<QVBoxLayout*>(m_newsColumn->layout());
        if (newsLayout) {
            newsLayout->setContentsMargins(0, 0, 24, 0);
        }

        // Files column adjustments for dual column
        QVBoxLayout* filesLayout = qobject_cast<QVBoxLayout*>(m_recentFilesColumn->layout());
        if (filesLayout) {
            filesLayout->setContentsMargins(24, 0, 0, 0);
        }

        // Add widgets in horizontal arrangement
        m_columnsGridLayout->addWidget(m_newsColumn, 0, 0);
        m_columnsGridLayout->addWidget(m_columnDivider, 0, 1);
        m_columnsGridLayout->addWidget(m_recentFilesColumn, 0, 2);
    }
}

bool DashboardPanel::eventFilter(QObject* watched, QEvent* event)
{
    // A click on the checkbox's text changes it, as on the text of a QCheckBox
    if (watched == m_autoLoadLabel && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton &&
            m_autoLoadLabel->rect().contains(mouse->position().toPoint())) {
            m_autoLoadCheckbox->click();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DashboardPanel::updateColumnVisibility()
{
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    // Read visibility settings from SettingsManager
    bool showNews = settings.get<bool>("dashboard.showKalahariNews");
    bool showRecentFiles = settings.get<bool>("dashboard.showRecentFiles");

    // Check if visibility changed
    bool visibilityChanged = (showNews != m_showNews) || (showRecentFiles != m_showRecentFiles);

    if (!visibilityChanged) {
        logger.debug("DashboardPanel::updateColumnVisibility - no visibility change");
        return;
    }

    // Update stored state
    m_showNews = showNews;
    m_showRecentFiles = showRecentFiles;

    logger.info("DashboardPanel::updateColumnVisibility - showNews={}, showRecentFiles={}",
                m_showNews, m_showRecentFiles);

    // Update column visibility
    if (m_newsColumn) {
        m_newsColumn->setVisible(m_showNews);
    }
    if (m_recentFilesColumn) {
        m_recentFilesColumn->setVisible(m_showRecentFiles);
    }

    // Update divider visibility:
    // - Show divider only when BOTH columns are visible AND we're in dual column mode
    // - Hide divider when only one column visible, or in single column mode
    if (m_columnDivider) {
        bool showDivider = m_showNews && m_showRecentFiles && !m_singleColumnMode;
        m_columnDivider->setVisible(showDivider);
        logger.debug("DashboardPanel: Divider visibility set to {}", showDivider);
    }

    // Adjust grid layout column stretch based on visibility
    if (m_columnsGridLayout) {
        // When both visible: equal stretch (1:0:1)
        // When only news visible: news gets all stretch (1:0:0)
        // When only recent files visible: files gets all stretch (0:0:1)
        // When neither visible: no stretch needed (0:0:0)
        int newsStretch = m_showNews ? 1 : 0;
        int filesStretch = m_showRecentFiles ? 1 : 0;

        m_columnsGridLayout->setColumnStretch(0, newsStretch);
        m_columnsGridLayout->setColumnStretch(1, 0);  // Divider always fixed
        m_columnsGridLayout->setColumnStretch(2, filesStretch);
    }
}


QPixmap DashboardPanel::loadThemedIcon(const QString& actionId) const
{
    // Load icon using getThemedIcon API with default theme colors
    QIcon icon = core::ArtProvider::getInstance().getThemedIcon(actionId);
    int iconSize = getIconSize();
    return icon.pixmap(iconSize, iconSize);
}

int DashboardPanel::getMaxItems() const
{
    // Get max items from settings, with bounds checking (3-9, default 5)
    int maxItems = core::SettingsManager::getInstance().get<int>("dashboard.maxItems");
    return std::clamp(maxItems, 3, 9);
}

int DashboardPanel::getIconSize() const
{
    // Get icon size from settings, with bounds checking (24-64, default 48)
    int iconSize = core::SettingsManager::getInstance().get<int>("dashboard.iconSize");
    return std::clamp(iconSize, 24, 64);
}

} // namespace gui
} // namespace kalahari
