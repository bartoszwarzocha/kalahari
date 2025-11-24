/// @file icon_downloader.cpp
/// @brief Implementation of IconDownloader

#include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/logger.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>

using namespace kalahari::core;

// ============================================================================
// Constants
// ============================================================================

namespace {
    constexpr const char* DEFAULT_SOURCE_URL =
        "https://raw.githubusercontent.com/google/material-design-icons/master/src/";

    constexpr int DOWNLOAD_TIMEOUT_MS = 10000; // 10 seconds
}

// ============================================================================
// IconDownloader Implementation
// ============================================================================

IconDownloader::IconDownloader(const QString& sourceUrl, QObject* parent)
    : QObject(parent)
    , m_sourceUrl(sourceUrl)
    , m_networkManager(new QNetworkAccessManager(this))
{
    initializeCategoryMap();

    Logger::getInstance().debug("IconDownloader: Initialized with source URL: {}",
                                m_sourceUrl.toStdString());
}

QString IconDownloader::getDefaultSourceUrl() {
    return QString(DEFAULT_SOURCE_URL);
}

void IconDownloader::setSourceUrl(const QString& url) {
    m_sourceUrl = url;

    // Ensure trailing slash
    if (!m_sourceUrl.endsWith('/')) {
        m_sourceUrl += '/';
    }

    Logger::getInstance().info("IconDownloader: Source URL changed to: {}",
                               m_sourceUrl.toStdString());
}

void IconDownloader::downloadIcon(const QString& iconName, const QStringList& themes) {
    Logger::getInstance().info("IconDownloader: Downloading icon '{}' in {} themes",
                               iconName.toStdString(), themes.size());

    for (const QString& theme : themes) {
        QString url = constructUrl(iconName, theme);

        if (url.isEmpty()) {
            QString error = QString("Cannot construct URL for icon '%1' (unknown category). "
                                  "Please provide custom source URL with full path.")
                          .arg(iconName);
            emit downloadError(iconName, theme, error);
            Logger::getInstance().warn("IconDownloader: {}", error.toStdString());
            continue;
        }

        Logger::getInstance().debug("IconDownloader: Requesting URL: {}", url.toStdString());

        // Create HTTP GET request
        QUrl qurl(url);
        QNetworkRequest request(qurl);
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, "Kalahari/1.0");

        // Set timeout
        request.setTransferTimeout(DOWNLOAD_TIMEOUT_MS);

        // Send request
        QNetworkReply* reply = m_networkManager->get(request);

        // Track request
        m_pendingRequests.insert(reply, qMakePair(iconName, theme));

        // Connect finished signal
        connect(reply, &QNetworkReply::finished, this, &IconDownloader::onReplyFinished);
    }
}

void IconDownloader::downloadIcons(const QStringList& iconNames, const QStringList& themes) {
    if (iconNames.isEmpty()) {
        Logger::getInstance().warn("IconDownloader: Batch download called with empty icon list");
        return;
    }

    Logger::getInstance().info("IconDownloader: Starting batch download of {} icons",
                               iconNames.size());

    // Initialize batch state
    m_batchIconNames = iconNames;
    m_batchThemes = themes;
    m_batchTotal = iconNames.size();
    m_batchCurrent = 0;

    // Download first icon (rest will be triggered by progress signal)
    downloadIcon(m_batchIconNames.first(), m_batchThemes);
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
    QString iconName = metadata.first;
    QString theme = metadata.second;

    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;

        switch (reply->error()) {
            case QNetworkReply::ContentNotFoundError:
                errorMsg = QString("Icon '%1' not found (404). Check icon name or category mapping.")
                         .arg(iconName);
                break;

            case QNetworkReply::TimeoutError:
                errorMsg = QString("Download timeout (%1ms). Check internet connection.")
                         .arg(DOWNLOAD_TIMEOUT_MS);
                break;

            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::ConnectionRefusedError:
                errorMsg = QString("Network error: Cannot connect to icon source. Check internet connection.");
                break;

            default:
                errorMsg = QString("HTTP error: %1").arg(reply->errorString());
                break;
        }

        Logger::getInstance().error("IconDownloader: Download failed for '{}' ({}): {}",
                                    iconName.toStdString(), theme.toStdString(),
                                    errorMsg.toStdString());

        emit downloadError(iconName, theme, errorMsg);
        reply->deleteLater();
        return;
    }

    // Read SVG data
    QByteArray svgData = reply->readAll();
    QString svgString = QString::fromUtf8(svgData);

    if (svgString.isEmpty()) {
        QString error = QString("Downloaded SVG is empty");
        Logger::getInstance().error("IconDownloader: {}", error.toStdString());
        emit downloadError(iconName, theme, error);
        reply->deleteLater();
        return;
    }

    Logger::getInstance().info("IconDownloader: ✓ Downloaded '{}' ({}) - {} bytes",
                               iconName.toStdString(), theme.toStdString(),
                               svgData.size());

    // Emit success signal (conversion happens externally)
    emit downloadComplete(theme, svgString);

    // Handle batch download progress
    if (m_batchTotal > 0) {
        m_batchCurrent++;
        emit progress(m_batchCurrent, m_batchTotal, iconName);

        // Download next icon in batch
        if (m_batchCurrent < m_batchTotal) {
            downloadIcon(m_batchIconNames[m_batchCurrent], m_batchThemes);
        } else {
            // Batch complete
            Logger::getInstance().info("IconDownloader: Batch download complete ({} icons)",
                                       m_batchTotal);
            m_batchTotal = 0;
            m_batchCurrent = 0;
            m_batchIconNames.clear();
            m_batchThemes.clear();
        }
    }

    reply->deleteLater();
}

QString IconDownloader::constructUrl(const QString& iconName, const QString& theme) const {
    // Get category for icon
    QString category = getCategoryForIcon(iconName);
    if (category.isEmpty()) {
        Logger::getInstance().debug("IconDownloader: No category mapping for icon '{}'",
                                    iconName.toStdString());
        return QString(); // Unknown category
    }

    // Get Material Design variant name
    QString variant = getVariantForTheme(theme);
    if (variant.isEmpty()) {
        Logger::getInstance().error("IconDownloader: Unknown theme '{}'",
                                    theme.toStdString());
        return QString();
    }

    // Construct URL: {base}/{category}/{icon_name}/{variant}/24px.svg
    QString url = QString("%1%2/%3/%4/24px.svg")
                 .arg(m_sourceUrl)
                 .arg(category)
                 .arg(iconName)
                 .arg(variant);

    return url;
}

QString IconDownloader::getVariantForTheme(const QString& theme) {
    if (theme == "twotone") {
        return "materialiconstwotone";
    } else if (theme == "rounded") {
        return "materialiconsround";
    } else if (theme == "outlined") {
        return "materialiconsoutlined";
    } else if (theme == "filled") {
        return "materialicons"; // Filled variant (optional)
    } else {
        return QString(); // Unknown theme
    }
}

QString IconDownloader::getCategoryForIcon(const QString& iconName) {
    // Static category map (initialized once)
    static IconDownloader tempInstance;
    return tempInstance.m_categoryMap.value(iconName, QString());
}

void IconDownloader::initializeCategoryMap() {
    // File operations (category: "file")
    m_categoryMap["folder_open"] = "file";
    m_categoryMap["folder"] = "file";
    m_categoryMap["description"] = "action"; // "file" icon
    m_categoryMap["insert_drive_file"] = "file";

    // Content operations (category: "content")
    m_categoryMap["save"] = "content";
    m_categoryMap["save_as"] = "content";
    m_categoryMap["content_copy"] = "content";
    m_categoryMap["content_cut"] = "content";
    m_categoryMap["content_paste"] = "content";
    m_categoryMap["add"] = "content";
    m_categoryMap["remove"] = "content";
    m_categoryMap["undo"] = "content";
    m_categoryMap["redo"] = "content";

    // Action icons (category: "action")
    m_categoryMap["delete"] = "action";
    m_categoryMap["close"] = "navigation"; // "close" is in navigation
    m_categoryMap["search"] = "action";
    m_categoryMap["settings"] = "action";
    m_categoryMap["info"] = "action";
    m_categoryMap["help"] = "action";
    m_categoryMap["check_circle"] = "action";
    m_categoryMap["error"] = "alert"; // Error icon
    m_categoryMap["warning"] = "alert"; // Warning icon

    // Editor operations (category: "editor")
    m_categoryMap["format_bold"] = "editor";
    m_categoryMap["format_italic"] = "editor";
    m_categoryMap["format_underlined"] = "editor";
    m_categoryMap["insert_photo"] = "editor";

    // Navigation (category: "navigation")
    m_categoryMap["menu"] = "navigation";
    m_categoryMap["arrow_back"] = "navigation";
    m_categoryMap["arrow_forward"] = "navigation";

    // Image operations (category: "image")
    m_categoryMap["image"] = "image";
    m_categoryMap["photo"] = "image";

    // Communication (category: "communication")
    m_categoryMap["email"] = "communication";
    m_categoryMap["message"] = "communication";

    // Device (category: "device")
    m_categoryMap["devices"] = "device";

    // Toggle (category: "toggle")
    m_categoryMap["check_box"] = "toggle";
    m_categoryMap["radio_button_checked"] = "toggle";

    Logger::getInstance().debug("IconDownloader: Initialized category map with {} entries",
                                m_categoryMap.size());
}
