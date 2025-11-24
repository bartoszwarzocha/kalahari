/// @file icon_downloader.h
/// @brief HTTP downloader for Material Design icons with automatic conversion
///
/// IconDownloader fetches SVG icons from Material Design repository (GitHub Raw)
/// and converts them to Kalahari template format with color placeholders.
///
/// Features:
/// - Async HTTP download using QNetworkAccessManager
/// - Material Design URL construction (category mapping)
/// - Custom source URL support
/// - Progress reporting via signals
/// - Error handling (network, HTTP, timeout)
///
/// Example usage:
/// @code
/// IconDownloader downloader;
/// connect(&downloader, &IconDownloader::downloadComplete,
///         [](QString theme, QString svgData) {
///     qDebug() << "Downloaded" << theme << ":" << svgData.length() << "bytes";
/// });
/// downloader.downloadIcon("save", {"twotone", "rounded", "outlined"});
/// @endcode

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>
#include <QMap>

namespace kalahari {
namespace core {

/// @brief HTTP downloader for Material Design icons
///
/// Downloads SVG icons from Material Design repository and converts them
/// to Kalahari template format with {COLOR_PRIMARY} and {COLOR_SECONDARY} placeholders.
class IconDownloader : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param sourceUrl Material Design icon source URL (default: GitHub Raw)
    /// @param parent Parent QObject for memory management
    explicit IconDownloader(const QString& sourceUrl = getDefaultSourceUrl(),
                           QObject* parent = nullptr);

    /// @brief Destructor
    ~IconDownloader() override = default;

    // Disable copy and move
    IconDownloader(const IconDownloader&) = delete;
    IconDownloader& operator=(const IconDownloader&) = delete;
    IconDownloader(IconDownloader&&) = delete;
    IconDownloader& operator=(IconDownloader&&) = delete;

    /// @brief Get default Material Design source URL (GitHub Raw)
    /// @return Default source URL
    static QString getDefaultSourceUrl();

    /// @brief Set custom source URL
    /// @param url Custom Material Design icon source URL
    /// @note URL should end with trailing slash
    void setSourceUrl(const QString& url);

    /// @brief Get current source URL
    /// @return Current source URL
    QString getSourceUrl() const { return m_sourceUrl; }

    /// @brief Download icon in specified themes
    ///
    /// Initiates async HTTP download for each theme variant.
    /// Emits downloadComplete() for each successful download.
    /// Emits downloadError() on failure.
    ///
    /// @param iconName Icon name (e.g., "save", "open", "folder_open")
    /// @param themes List of theme names ("twotone", "rounded", "outlined")
    /// @note Download is asynchronous - connect to signals for results
    void downloadIcon(const QString& iconName, const QStringList& themes);

    /// @brief Download multiple icons in batch
    ///
    /// Downloads multiple icons sequentially. Emits progress() signal
    /// for each completed icon.
    ///
    /// @param iconNames List of icon names
    /// @param themes List of theme names for each icon
    void downloadIcons(const QStringList& iconNames, const QStringList& themes);

    /// @brief Get Material Design category for icon name
    ///
    /// Maps icon name to Material Design category (e.g., "save" → "content").
    /// Returns empty string if category is unknown (requires custom URL).
    ///
    /// @param iconName Icon name
    /// @return Category name or empty string
    static QString getCategoryForIcon(const QString& iconName);

signals:
    /// @brief Emitted when icon download completes successfully
    /// @param theme Theme name ("twotone", "rounded", "outlined")
    /// @param svgData Raw SVG data (not yet converted)
    void downloadComplete(const QString& theme, const QString& svgData);

    /// @brief Emitted when download fails
    /// @param iconName Icon name that failed
    /// @param theme Theme name that failed
    /// @param errorMessage Human-readable error message
    void downloadError(const QString& iconName, const QString& theme, const QString& errorMessage);

    /// @brief Emitted for batch download progress
    /// @param current Current icon index (1-based)
    /// @param total Total number of icons
    /// @param iconName Current icon name
    void progress(int current, int total, const QString& iconName);

private slots:
    /// @brief Handle QNetworkReply finished signal
    void onReplyFinished();

private:
    /// @brief Construct Material Design URL for icon
    ///
    /// Builds URL like:
    /// {base}/{category}/{icon_name}/{variant}/24px.svg
    ///
    /// Example:
    /// https://raw.githubusercontent.com/.../content/save/materialiconstwotone/24px.svg
    ///
    /// @param iconName Icon name (e.g., "save")
    /// @param theme Theme name (e.g., "twotone")
    /// @return Full URL or empty string if category unknown
    QString constructUrl(const QString& iconName, const QString& theme) const;

    /// @brief Get Material Design variant name for theme
    ///
    /// Maps Kalahari theme name to Material Design variant:
    /// - "twotone" → "materialiconstwotone"
    /// - "rounded" → "materialiconsround"
    /// - "outlined" → "materialiconsoutlined"
    ///
    /// @param theme Kalahari theme name
    /// @return Material Design variant name
    static QString getVariantForTheme(const QString& theme);

    /// @brief Initialize category mapping
    ///
    /// Populates m_categoryMap with common icon → category mappings.
    /// Called once in constructor.
    void initializeCategoryMap();

    QString m_sourceUrl;                    ///< Material Design source URL
    QNetworkAccessManager* m_networkManager; ///< HTTP client
    QMap<QString, QString> m_categoryMap;   ///< Icon name → category mapping

    // Batch download state
    QStringList m_batchIconNames;           ///< Remaining icons in batch
    QStringList m_batchThemes;              ///< Themes for batch download
    int m_batchTotal = 0;                   ///< Total icons in batch
    int m_batchCurrent = 0;                 ///< Current icon index (0-based)

    // Track pending requests
    QMap<QNetworkReply*, QPair<QString, QString>> m_pendingRequests; ///< reply → (iconName, theme)
};

} // namespace core
} // namespace kalahari
