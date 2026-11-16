// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_lookup.cpp
/// @brief The lookup of the description of a source in a catalog on the web, by its ISBN or
/// DOI

#include "kalahari/gui/source_lookup.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/source_catalogs.h"
#include "kalahari/gui/material_downloader.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QVariant>

#include <memory>
#include <optional>

namespace kalahari::gui {

namespace {

using core::SourceCatalogs;

/// @brief The HTTP status of an answer that has nothing at its address
constexpr int NOT_FOUND = 404;

/// @brief The number @p text has: its DOI, else its ISBN; empty for neither
QString numberOf(const QString& text) {
    const QString doi = SourceCatalogs::doi(text);
    return doi.isEmpty() ? SourceCatalogs::isbn(text) : doi;
}

/// @brief The answer of a lookup as it comes
struct Answer {
    QByteArray data;        ///< What came so far
    bool tooLarge = false;  ///< It went over MAXIMUM_ANSWER and was ended
};

/// @brief The request of a lookup in @p catalog at @p address, as the program sends one; a RIS
/// record is asked of doi.org
QNetworkRequest requestOf(const QUrl& address, const QString& catalog) {
    QNetworkRequest request = MaterialDownloader::requestOf(address);
    if (catalog == QLatin1String(SourceCatalogs::DOI_ORG)) {
        request.setRawHeader("Accept", "application/x-research-info-systems");
    }
    return request;
}

}  // namespace

SourceLookup::SourceLookup(QObject* parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {}

QString SourceLookup::catalogOf(const QString& text) {
    if (!SourceCatalogs::doi(text).isEmpty()) {
        return QString::fromLatin1(SourceCatalogs::DOI_ORG);
    }
    if (!SourceCatalogs::isbn(text).isEmpty()) {
        return SourceCatalogs::settingsIsbnCatalog();
    }
    return {};
}

void SourceLookup::setAddressPattern(const QString& catalog, const QString& pattern) {
    m_patterns.insert(catalog, pattern);
}

QUrl SourceLookup::addressOf(const QString& text) const {
    const QString catalog = catalogOf(text);
    if (catalog.isEmpty()) {
        return {};
    }
    const QString pattern = m_patterns.value(catalog, SourceCatalogs::addressPattern(catalog));
    const QString number = QString::fromLatin1(QUrl::toPercentEncoding(numberOf(text), "/:"));
    return {pattern.arg(number)};
}

bool SourceLookup::start(const QString& text) {
    cancel();
    const QString catalog = catalogOf(text);
    if (catalog.isEmpty()) {
        return false;
    }
    const QString number = numberOf(text);
    const QNetworkRequest request = requestOf(addressOf(text), catalog);
    core::Logger::getInstance().info("SourceLookup: asking {} for {}",
                                     SourceCatalogs::catalogName(catalog).toStdString(),
                                     number.toStdString());

    QNetworkReply* reply = m_network->get(request);
    m_reply = reply;
    auto answer = std::make_shared<Answer>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, answer]() {
        // A part that comes after cancel() or abort() is not read
        if (!reply->isOpen()) {
            return;
        }
        answer->data.append(reply->readAll());
        if (answer->data.size() > MAXIMUM_ANSWER) {
            answer->tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, answer, number, catalog]() {
        reply->deleteLater();
        // Ended by cancel() or by a new lookup
        if (m_reply != reply) {
            return;
        }
        m_reply = nullptr;
        if (!answer->tooLarge && reply->isOpen()) {
            answer->data.append(reply->readAll());
        }
        Result result;
        if (answer->tooLarge) {
            result = {.number = number, .catalog = catalog, .source = std::nullopt,
                      //: In Polish: Odpowiedź katalogu jest za duża.
                      .error = tr("The answer of the catalog is too large.")};
        } else if (reply->error() != QNetworkReply::NoError) {
            result = {.number = number, .catalog = catalog, .source = std::nullopt,
                      .error = failureOf(reply, catalog)};
        } else {
            result = resultOf(number, catalog, answer->data);
        }
        emit finished(result);
    });
    return true;
}

void SourceLookup::cancel() {
    if (m_reply == nullptr) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->abort();
}

bool SourceLookup::isRunning() const {
    return m_reply != nullptr;
}

SourceLookup::Result SourceLookup::resultOf(const QString& number, const QString& catalog,
                                            const QByteArray& data) {
    Result result{.number = number, .catalog = catalog, .source = std::nullopt, .error = {}};
    if (catalog == QLatin1String(SourceCatalogs::DOI_ORG)) {
        result.source = SourceCatalogs::fromDoiRecord(data, number);
        if (!result.source) {
            //: In Polish: Serwis doi.org nie podał opisu tego DOI.
            result.error = tr("doi.org gave no description of this DOI.");
        }
        return result;
    }
    result.source = catalog == QLatin1String(SourceCatalogs::OPEN_LIBRARY)
                        ? SourceCatalogs::fromOpenLibrary(data, number)
                        : SourceCatalogs::fromGoogleBooks(data, number);
    if (!result.source) {
        //: %1: the name of the catalog, Google Books or Open Library. In Polish: %1 nie ma
        //: książki o tym numerze ISBN.
        result.error = tr("%1 has no book with this ISBN.")
                           .arg(SourceCatalogs::catalogName(catalog));
    }
    return result;
}

QString SourceLookup::failureOf(const QNetworkReply* reply, const QString& catalog) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (catalog == QLatin1String(SourceCatalogs::DOI_ORG) && status == NOT_FOUND) {
        //: In Polish: Serwis doi.org nie zna tego DOI.
        return tr("doi.org does not know this DOI.");
    }
    return MaterialDownloader::failureOf(reply);
}

}  // namespace kalahari::gui
