/// @file material_downloader.cpp
/// @brief What the program takes from the web for a material with an address

#include "kalahari/gui/material_downloader.h"

#include "kalahari/gui/material_source.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextDocumentFragment>

#include <memory>

namespace kalahari {
namespace gui {

namespace {

/// The values of MaterialDownloader::SETTING
constexpr const char* NEVER = "never";
constexpr const char* ALWAYS = "always";

/// Whether @p data has the end of the title of a page
bool hasTitleEnd(const QByteArray& data) {
    return data.toLower().contains("</title");
}

/// Why download @p reply failed, in a sentence for the writer
QString failureOf(const QNetworkReply* reply) {
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
        //: In Polish: Nie znaleziono serwera tego adresu. Może nie ma połączenia z siecią.
        return MaterialDownloader::tr("The server of the address was not found. The computer "
                                      "may not be connected to the network.");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        //: In Polish: Serwer nie odpowiedział na czas.
        return MaterialDownloader::tr("The server did not answer in time.");
    case QNetworkReply::ContentNotFoundError:
        //: In Polish: Pod tym adresem nic nie ma (błąd 404).
        return MaterialDownloader::tr("There is nothing at this address (error 404).");
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
        //: In Polish: Serwer nie pozwala pobrać tego adresu.
        return MaterialDownloader::tr("The server does not allow downloading this address.");
    default:
        //: %1: what went wrong, in English, from Qt. In Polish: Pobieranie się nie udało (%1).
        return MaterialDownloader::tr("The download failed (%1).").arg(reply->errorString());
    }
}

} // namespace

MaterialDownloader::Policy MaterialDownloader::policy() {
    const std::string value =
        core::SettingsManager::getInstance().get<std::string>(SETTING, "ask");
    if (value == NEVER) {
        return Policy::Never;
    }
    return value == ALWAYS ? Policy::Always : Policy::Ask;
}

MaterialDownloader::MaterialDownloader(QObject* parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {}

void MaterialDownloader::fetchTitle(const QString& elementId, const QUrl& url) {
    fetch(elementId, url, false);
}

void MaterialDownloader::fetchPicture(const QString& elementId, const QUrl& url) {
    fetch(elementId, url, true);
}

void MaterialDownloader::fetch(const QString& elementId, const QUrl& url, bool picture) {
    QNetworkRequest request(url);
    request.setTransferTimeout(TIMEOUT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString program = QCoreApplication::applicationName().isEmpty()
                                ? QStringLiteral("Kalahari")
                                : QCoreApplication::applicationName();
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(program, QCoreApplication::applicationVersion()));
    core::Logger::getInstance().info("MaterialDownloader: {} {}",
                                     picture ? "Downloading the picture" : "Reading the title of",
                                     url.toString().toStdString());

    m_fetching.insert(elementId);
    QNetworkReply* reply = m_network->get(request);
    auto data = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    Result result;
    result.elementId = elementId;
    result.url = url;
    result.picture = picture;

    const auto end = [this, reply, done](const Result& ended) {
        *done = true;
        m_fetching.remove(ended.elementId);
        if (!ended.error.isEmpty()) {
            core::Logger::getInstance().warn("MaterialDownloader: {}: {}",
                                             ended.url.toString().toStdString(),
                                             ended.error.toStdString());
        }
        emit finished(ended);
        reply->abort();
    };
    // A page is read up to its title; a picture whole, up to its limit
    const auto take = [reply, data, done, end, result]() {
        if (*done) {
            return;
        }
        data->append(reply->readAll());
        Result taken = result;
        if (!result.picture && (hasTitleEnd(*data) || data->size() >= MAXIMUM_PAGE)) {
            taken.title = pageTitle(*data, reply->header(QNetworkRequest::ContentTypeHeader)
                                               .toString()
                                               .toLatin1());
            end(taken);
        } else if (result.picture && data->size() > MAXIMUM_PICTURE) {
            //: %1: the most, e.g. 50 MB. In Polish: Obraz jest większy niż %1.
            taken.error = tr("The picture is larger than %1.")
                              .arg(QLocale().formattedDataSize(MAXIMUM_PICTURE, 0));
            end(taken);
        }
    };
    connect(reply, &QNetworkReply::readyRead, this, take);
    connect(reply, &QNetworkReply::finished, this, [reply, data, done, end, take, result]() {
        reply->deleteLater();
        if (*done) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            Result failed = result;
            failed.error = failureOf(reply);
            end(failed);
            return;
        }
        take();
        if (*done) {
            return;
        }
        Result taken = result;
        if (!result.picture) {
            taken.title = pageTitle(
                *data, reply->header(QNetworkRequest::ContentTypeHeader).toString().toLatin1());
        } else if (const QString suffix = pictureSuffix(*data); !suffix.isEmpty()) {
            taken.data = *data;
            taken.fileName = fileNameOfAddress(result.url, suffix);
        } else {
            //: In Polish: Pod tym adresem nie ma obrazu, który program zna.
            taken.error = tr("There is no picture the program knows at this address.");
        }
        end(taken);
    });
}

QString MaterialDownloader::pageTitle(const QByteArray& html, const QByteArray& contentType) {
    static const QRegularExpression charsetPattern(
        QStringLiteral("charset\\s*=\\s*[\"']?([\\w.:-]+)"),
        QRegularExpression::CaseInsensitiveOption);
    // The charset of the header, else of the <meta> of the page, at its beginning
    QRegularExpressionMatch charset =
        charsetPattern.match(QString::fromLatin1(contentType));
    if (!charset.hasMatch()) {
        charset = charsetPattern.match(QString::fromLatin1(html.left(4096)));
    }
    QStringDecoder decoder(charset.hasMatch() ? charset.captured(1).toLatin1().constData()
                                              : "UTF-8");
    if (!decoder.isValid()) {
        decoder = QStringDecoder(QStringDecoder::Utf8);
    }
    const QString text = decoder.decode(html);

    static const QRegularExpression titlePattern(
        QStringLiteral("<title[^>]*>(.*?)</title\\s*>"),
        QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch title = titlePattern.match(text);
    if (!title.hasMatch()) {
        return {};
    }
    // "&amp;" and "&#8211;" are the characters they name
    const QString found =
        QTextDocumentFragment::fromHtml(title.captured(1)).toPlainText().simplified();
    // Letters of a charset the program cannot read: Qt without ICU (the Windows build) reads
    // only UTF-8, UTF-16, UTF-32 and Latin-1
    return found.contains(QChar::ReplacementCharacter) ? QString() : found;
}

QString MaterialDownloader::pictureSuffix(const QByteArray& data) {
    QByteArray bytes = data;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    if (!reader.canRead()) {
        return {};
    }
    const QString format = QString::fromLatin1(reader.format()).toLower();
    return format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format;
}

} // namespace gui
} // namespace kalahari
