// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_lookup.h
/// @brief The lookup of the description of a source in a catalog on the web, by its ISBN or
/// DOI

#ifndef KALAHARI_GUI_SOURCE_LOOKUP_H
#define KALAHARI_GUI_SOURCE_LOOKUP_H

#include "kalahari/core/source_description.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace kalahari::gui {

/// @brief Asks a catalog on the web for the description of a source: an ISBN goes to the
/// catalog of the Settings, a DOI to doi.org (core::SourceCatalogs)
///
/// Only the number goes out, and only when the writer asks. One lookup at a time: a new one
/// ends the one on its way.
class SourceLookup : public QObject {
    Q_OBJECT

public:
    /// @brief Most of an answer read
    static constexpr qint64 MAXIMUM_ANSWER = qint64{1024} * 1024;

    /// @brief What a lookup gave
    struct Result {
        QString number;                                 ///< The ISBN or DOI looked up
        QString catalog;                                ///< core::SourceCatalogs::GOOGLE_BOOKS...
        std::optional<core::SourceDescription> source;  ///< The description found
        QString error;  ///< Why none came, in a sentence; empty when one came
    };

    /// @param parent Parent object
    explicit SourceLookup(QObject* parent = nullptr);

    /// @brief The catalog number @p text goes to: DOI_ORG for a DOI, the ISBN catalog of the
    /// Settings for an ISBN; empty for neither
    [[nodiscard]] static QString catalogOf(const QString& text);

    /// @brief Ask catalog @p catalog at @p pattern (%1: the number) rather than at its address
    /// on the web (for the tests)
    void setAddressPattern(const QString& catalog, const QString& pattern);

    /// @brief The address number @p text is asked at; empty for neither an ISBN nor a DOI
    [[nodiscard]] QUrl addressOf(const QString& text) const;

    /// @brief Look up the description of number @p text (an ISBN or a DOI); finished() says
    /// what came
    /// @return false for neither an ISBN nor a DOI: nothing is asked
    bool start(const QString& text);

    /// @brief End the lookup on its way: finished() does not come for it
    void cancel();

    /// @brief Whether a lookup is on its way
    [[nodiscard]] bool isRunning() const;

signals:
    /// @brief A lookup ended: @p result has the description found, or why none came
    void finished(const kalahari::gui::SourceLookup::Result& result);

private:
    /// @brief What the answer @p data of catalog @p catalog to @p number says
    [[nodiscard]] static Result resultOf(const QString& number, const QString& catalog,
                                         const QByteArray& data);

    /// @brief Why @p reply of catalog @p catalog brought no description
    [[nodiscard]] static QString failureOf(const QNetworkReply* reply, const QString& catalog);

    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_reply;     ///< The lookup on its way; none: no lookup
    QHash<QString, QString> m_patterns;  ///< Catalog -> the address it is asked at, by the tests
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_SOURCE_LOOKUP_H
