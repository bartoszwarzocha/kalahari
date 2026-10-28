/// @file material_downloader.h
/// @brief What the program takes from the web for a material with an address: the title of
/// its page, or the copy of its picture

#pragma once

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace kalahari {
namespace gui {

/// @brief Downloads for the materials with a web address: the title of a page, or a picture
/// whose copy the material gets
///
/// The program connects to the address of a material only on the writer's wish, as the
/// setting SETTING says: in the Add Element window, whose box is empty each time ("ask"), or
/// at every new material ("always"), or never. Save Copy on the card of a picture on the web
/// downloads it too, unless the setting says never. Each download asks only the one address
/// of the material, and a page is read only up to its title.
class MaterialDownloader : public QObject {
    Q_OBJECT

public:
    /// @brief The setting of the downloads: "ask" (a box in the Add Element window, empty each
    /// time; the usual one), "never" or "always"
    static constexpr const char* SETTING = "network.materialDownloads";

    /// @brief What SETTING says
    enum class Policy {
        Ask,     ///< The Add Element window asks, with a box empty each time
        Never,   ///< The program does not connect to the addresses of materials
        Always,  ///< Every new material with an address gets its title or its copy
    };

    /// @brief What the setting says now
    static Policy policy();

    /// @brief Most of a page read for its title
    static constexpr qint64 MAXIMUM_PAGE = 1024 * 1024;

    /// @brief Most of a picture downloaded
    static constexpr qint64 MAXIMUM_PICTURE = 50 * 1024 * 1024;

    /// @brief Milliseconds a download waits for the next part of its data
    static constexpr int TIMEOUT = 20000;

    /// @brief What a download gave
    struct Result {
        QString elementId;     ///< The material it is for
        QUrl url;              ///< Its address
        bool picture = false;  ///< A picture was downloaded, else a page for its title
        QString title;         ///< The title of the page; empty when it has none
        QByteArray data;       ///< The picture
        QString fileName;      ///< A name for its copy, with the suffix of its format
        QString error;         ///< Why nothing came, in a sentence; empty when it came
    };

    explicit MaterialDownloader(QObject* parent = nullptr);

    /// @brief Read the page at @p url for the title of material @p elementId
    void fetchTitle(const QString& elementId, const QUrl& url);

    /// @brief Download the picture at @p url for the copy of material @p elementId
    void fetchPicture(const QString& elementId, const QUrl& url);

    /// @brief Whether a download for material @p elementId is on its way
    bool isFetching(const QString& elementId) const { return m_fetching.contains(elementId); }

    /// @brief The title of page @p html: its <title>, with the characters its entities name,
    /// in the charset @p contentType or the page's <meta> gives, else in UTF-8; empty when it
    /// has none, or when its letters are of a charset the program cannot read
    static QString pageTitle(const QByteArray& html, const QByteArray& contentType = {});

    /// @brief The suffix of a file of picture @p data ("jpg", "png"); empty when it is no
    /// picture the program knows
    static QString pictureSuffix(const QByteArray& data);

signals:
    /// @brief A download ended, with @p result
    void finished(const kalahari::gui::MaterialDownloader::Result& result);

private:
    /// @brief Download @p url for material @p elementId: a picture, or a page for its title
    void fetch(const QString& elementId, const QUrl& url, bool picture);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_fetching;  ///< The materials whose downloads are on their way
};

} // namespace gui
} // namespace kalahari
