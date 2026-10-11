/// @file icon_downloader.cpp
/// @brief Implementation of IconDownloader

#include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/logger.h"
#include <QCoreApplication>
#include <QNetworkRequest>
#include <QUrl>

using namespace kalahari::core;

namespace {
    constexpr int DOWNLOAD_TIMEOUT_MS = 10000; // 10 seconds

    /// @brief A message for the writer, translated (lupdate finds it in gui/utils/core_texts.cpp)
    QString message(const char* source) {
        return QCoreApplication::translate("kalahari::core::IconDownloader", source);
    }
}

// ============================================================================
// IconDownloader Implementation
// ============================================================================

IconDownloader::IconDownloader(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    // Set transfer timeout on the network manager (Qt6 API)
    m_networkManager->setTransferTimeout(DOWNLOAD_TIMEOUT_MS);
    Logger::getInstance().debug("IconDownloader: Initialized");
}

void IconDownloader::downloadFromUrl(const QString& url, const QString& theme) {
    Logger::getInstance().info("IconDownloader: Downloading from URL: {}", url.toStdString());

    // Validate URL
    QUrl qurl(url);
    if (!qurl.isValid()) {
        QString error = message("Invalid URL: %1").arg(url);
        emit downloadError(url, error);
        Logger::getInstance().error("IconDownloader: {}", error.toStdString());
        return;
    }

    // Create HTTP GET request
    QNetworkRequest request(qurl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, "Kalahari/1.0");

    // Send request
    QNetworkReply* reply = m_networkManager->get(request);

    // Track request
    m_pendingRequests.insert(reply, qMakePair(url, theme));

    // Connect finished signal
    connect(reply, &QNetworkReply::finished, this, &IconDownloader::onReplyFinished);
}

void IconDownloader::downloadFromUrls(const QStringList& urls, const QStringList& themes) {
    if (urls.isEmpty()) {
        Logger::getInstance().warn("IconDownloader: Batch download called with empty URL list");
        return;
    }

    if (urls.size() != themes.size()) {
        Logger::getInstance().error("IconDownloader: URLs and themes lists must have same size");
        return;
    }

    Logger::getInstance().info("IconDownloader: Starting batch download of {} icons", urls.size());

    // Initialize batch state
    m_batchUrls = urls;
    m_batchThemes = themes;
    m_batchTotal = urls.size();
    m_batchCurrent = 0;

    // Download first icon (rest will be triggered by progress signal)
    downloadFromUrl(m_batchUrls.first(), m_batchThemes.first());
}

void IconDownloader::onReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Logger::getInstance().error("IconDownloader: onReplyFinished() called with invalid sender");
        return;
    }

    // Get request metadata
    if (!m_pendingRequests.contains(reply)) {
        Logger::getInstance().warn("IconDownloader: Received reply for unknown request");
        reply->deleteLater();
        return;
    }

    auto metadata = m_pendingRequests.take(reply);
    QString url = metadata.first;
    QString theme = metadata.second;

    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;

        switch (reply->error()) {
            case QNetworkReply::ContentNotFoundError:
                errorMsg = message("Not found (404): %1").arg(url);
                break;

            case QNetworkReply::ContentAccessDenied:
                errorMsg = message("Access denied (403): %1").arg(url);
                break;

            case QNetworkReply::TimeoutError:
            case QNetworkReply::OperationCanceledError:
                errorMsg = message("The server did not answer within %1 seconds")
                               .arg(DOWNLOAD_TIMEOUT_MS / 1000);
                break;

            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::RemoteHostClosedError:
                errorMsg = message("Cannot connect to the server");
                break;

            case QNetworkReply::ProxyConnectionRefusedError:
            case QNetworkReply::ProxyConnectionClosedError:
            case QNetworkReply::ProxyNotFoundError:
            case QNetworkReply::ProxyTimeoutError:
            case QNetworkReply::ProxyAuthenticationRequiredError:
                errorMsg = message("The proxy server did not let the connection through");
                break;

            case QNetworkReply::SslHandshakeFailedError:
                errorMsg = message("A secure connection to the server failed");
                break;

            default:
                // Qt's own description of the problem stays as the detail
                errorMsg = message("Download failed: %1").arg(reply->errorString());
                break;
        }

        Logger::getInstance().error("IconDownloader: Download failed (network error {}): {}",
                                    static_cast<int>(reply->error()), errorMsg.toStdString());
        emit downloadError(url, errorMsg);
        reply->deleteLater();
        return;
    }

    // Read SVG data
    QByteArray svgData = reply->readAll();
    QString svgString = QString::fromUtf8(svgData);

    if (svgString.isEmpty()) {
        QString error = message("The downloaded file is empty");
        Logger::getInstance().error("IconDownloader: {}", error.toStdString());
        emit downloadError(url, error);
        reply->deleteLater();
        return;
    }

    Logger::getInstance().info("IconDownloader: ✓ Downloaded ({}) - {} bytes",
                               theme.toStdString(), svgData.size());

    // Emit success signal
    emit downloadComplete(theme, svgString);

    // Handle batch download progress
    if (m_batchTotal > 0) {
        m_batchCurrent++;
        emit progress(m_batchCurrent, m_batchTotal, url);

        // Download next icon in batch
        if (m_batchCurrent < m_batchTotal) {
            downloadFromUrl(m_batchUrls[m_batchCurrent], m_batchThemes[m_batchCurrent]);
        } else {
            // Batch complete
            Logger::getInstance().info("IconDownloader: Batch download complete ({} icons)",
                                       m_batchTotal);
            m_batchTotal = 0;
            m_batchCurrent = 0;
            m_batchUrls.clear();
            m_batchThemes.clear();
        }
    }

    reply->deleteLater();
}
