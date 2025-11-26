/// @file icon_downloader.h
/// @brief HTTP downloader for Material Design icons with automatic conversion
///
/// IconDownloader fetches SVG icons from any URL and converts them to
/// Kalahari template format with color placeholders.
///
/// Features:
/// - Async HTTP download using QNetworkAccessManager
/// - Full URL support (no hardcoded mappings)
/// - Progress reporting via signals
/// - Error handling (network, HTTP, timeout)
///
/// Example usage:
/// @code
/// IconDownloader downloader;
/// connect(&downloader, &IconDownloader::downloadComplete,
///         [](QString theme, QString svgData) {
///     qDebug() << "Downloaded:" << svgData.length() << "bytes";
/// });
/// downloader.downloadFromUrl("https://example.com/icon.svg", "twotone");
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

/// @brief HTTP downloader for SVG icons
///
/// Downloads SVG icons from any URL and converts them to Kalahari template
/// format with {COLOR_PRIMARY} and {COLOR_SECONDARY} placeholders.
class IconDownloader : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QObject for memory management
    explicit IconDownloader(QObject* parent = nullptr);

    /// @brief Destructor
    ~IconDownloader() override = default;

    // Disable copy and move
    IconDownloader(const IconDownloader&) = delete;
    IconDownloader& operator=(const IconDownloader&) = delete;
    IconDownloader(IconDownloader&&) = delete;
    IconDownloader& operator=(IconDownloader&&) = delete;

    /// @brief Download icon from full URL
    ///
    /// Initiates async HTTP download from the specified URL.
    /// Emits downloadComplete() on success, downloadError() on failure.
    ///
    /// @param url Full URL to SVG file
    /// @param theme Theme name for output (e.g., "twotone", "rounded")
    void downloadFromUrl(const QString& url, const QString& theme);

    /// @brief Download multiple icons from URLs
    ///
    /// Downloads multiple icons sequentially. Emits progress() signal
    /// for each completed icon.
    ///
    /// @param urls List of full URLs to download
    /// @param themes Corresponding theme names for each URL
    void downloadFromUrls(const QStringList& urls, const QStringList& themes);

signals:
    /// @brief Emitted when icon download completes successfully
    /// @param theme Theme name
    /// @param svgData Raw SVG data (not yet converted)
    void downloadComplete(const QString& theme, const QString& svgData);

    /// @brief Emitted when download fails
    /// @param url URL that failed
    /// @param errorMessage Human-readable error message
    void downloadError(const QString& url, const QString& errorMessage);

    /// @brief Emitted for batch download progress
    /// @param current Current icon index (1-based)
    /// @param total Total number of icons
    /// @param url Current URL
    void progress(int current, int total, const QString& url);

private slots:
    /// @brief Handle QNetworkReply finished signal
    void onReplyFinished();

private:
    QNetworkAccessManager* m_networkManager; ///< HTTP client

    // Batch download state
    QStringList m_batchUrls;                 ///< Remaining URLs in batch
    QStringList m_batchThemes;               ///< Themes for batch download
    int m_batchTotal = 0;                    ///< Total icons in batch
    int m_batchCurrent = 0;                  ///< Current icon index (0-based)

    // Track pending requests
    QMap<QNetworkReply*, QPair<QString, QString>> m_pendingRequests; ///< reply → (url, theme)
};

} // namespace core
} // namespace kalahari
