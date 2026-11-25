// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_description.cpp
/// @brief The bibliographic description of a material, its bookmarks and the source of a quote

#include "kalahari/core/source_description.h"

#include "kalahari/core/material_kinds.h"

#include <initializer_list>

namespace kalahari::core {

bool SourceDescription::isEmpty() const {
    return type.isEmpty() && people.isEmpty() && values.isEmpty() && bibliography.isEmpty() &&
           extra.isEmpty();
}

QString SourceDescription::value(const char* field) const {
    return values.value(QString::fromLatin1(field));
}

void SourceDescription::setValue(const QString& field, const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        values.remove(field);
    } else {
        values.insert(field, trimmed);
    }
}

QString SourceDescription::kind() const {
    return type.isEmpty() ? QString::fromLatin1(BOOK) : type;
}

bool SourceDescription::inBibliography(bool linked) const {
    if (bibliography == QLatin1String(ALWAYS)) {
        return true;
    }
    if (bibliography == QLatin1String(NEVER)) {
        return false;
    }
    return linked;
}

QList<SourcePerson> SourceDescription::peopleOf(const char* role) const {
    const bool author = qstrcmp(role, AUTHOR) == 0;
    QList<SourcePerson> found;
    for (const SourcePerson& person : people) {
        if (person.name.trimmed().isEmpty()) {
            continue;
        }
        if (person.role == QLatin1String(role) || (author && person.role.isEmpty())) {
            found << person;
        }
    }
    return found;
}

const QStringList& SourceDescription::kinds() {
    static const QStringList all = {
        QString::fromLatin1(BOOK),      QString::fromLatin1(CHAPTER),
        QString::fromLatin1(ARTICLE),   QString::fromLatin1(WEB_PAGE),
        QString::fromLatin1(ARCHIVE),   QString::fromLatin1(INTERVIEW),
        QString::fromLatin1(FILM),      QString::fromLatin1(THESIS),
        QString::fromLatin1(OTHER)};
    return all;
}

const QStringList& SourceDescription::roles() {
    static const QStringList all = {
        QString::fromLatin1(AUTHOR),      QString::fromLatin1(EDITOR),
        QString::fromLatin1(TRANSLATOR),  QString::fromLatin1(INTERVIEWEE),
        QString::fromLatin1(INTERVIEWER), QString::fromLatin1(DIRECTOR),
        QString::fromLatin1(SUPERVISOR),  QString::fromLatin1(INSTITUTION)};
    return all;
}

QStringList SourceDescription::fieldsOf(const QString& kind) {
    const auto list = [](std::initializer_list<const char*> fields) {
        QStringList names;
        for (const char* field : fields) {
            names << QString::fromLatin1(field);
        }
        return names;
    };
    if (kind == QLatin1String(CHAPTER)) {
        return list({TITLE, PAGES, CONTAINER, EDITION, PLACE, YEAR, PUBLISHER, ISBN});
    }
    if (kind == QLatin1String(ARTICLE)) {
        return list({TITLE, CONTAINER, YEAR, VOLUME, ISSUE, PAGES, DOI, ISSN, URL});
    }
    if (kind == QLatin1String(WEB_PAGE)) {
        return list({TITLE, CONTAINER, URL, PUBLISHED, ACCESSED});
    }
    if (kind == QLatin1String(ARCHIVE)) {
        return list({TITLE, YEAR, CONTAINER, FONDS, SIGNATURE, FORM});
    }
    if (kind == QLatin1String(INTERVIEW)) {
        return list({TITLE, PLACE, YEAR, FORM, URL});
    }
    if (kind == QLatin1String(FILM)) {
        return list({TITLE, YEAR, PLACE, PUBLISHER, FORM, URL});
    }
    if (kind == QLatin1String(THESIS)) {
        return list({TITLE, SUBTITLE, FORM, PUBLISHER, PLACE, YEAR, URL});
    }
    if (kind == QLatin1String(OTHER)) {
        return list({TITLE, YEAR, PLACE, PUBLISHER, FORM, URL});
    }
    return list({TITLE, SUBTITLE, EDITION, PLACE, YEAR, PUBLISHER, PAGES, SERIES, ISBN,
                 LANGUAGE});
}

const QStringList& SourceDescription::allFields() {
    static const QStringList all = {
        QString::fromLatin1(TITLE),     QString::fromLatin1(SUBTITLE),
        QString::fromLatin1(CONTAINER), QString::fromLatin1(EDITION),
        QString::fromLatin1(PLACE),     QString::fromLatin1(PUBLISHER),
        QString::fromLatin1(YEAR),      QString::fromLatin1(VOLUME),
        QString::fromLatin1(ISSUE),     QString::fromLatin1(PAGES),
        QString::fromLatin1(SERIES),    QString::fromLatin1(ISBN),
        QString::fromLatin1(ISSN),      QString::fromLatin1(DOI),
        QString::fromLatin1(URL),       QString::fromLatin1(PUBLISHED),
        QString::fromLatin1(ACCESSED),  QString::fromLatin1(FONDS),
        QString::fromLatin1(SIGNATURE), QString::fromLatin1(FORM),
        QString::fromLatin1(LANGUAGE)};
    return all;
}

QString SourceDescription::kindFor(const QString& materialKind) {
    if (materialKind == QLatin1String(MaterialKinds::PUBLICATION)) {
        return QString::fromLatin1(BOOK);
    }
    if (materialKind == QLatin1String(MaterialKinds::WEB_PAGE)) {
        return QString::fromLatin1(WEB_PAGE);
    }
    if (materialKind == QLatin1String(MaterialKinds::RECORDING)) {
        return QString::fromLatin1(FILM);
    }
    return QString::fromLatin1(OTHER);
}

bool SourceDescription::canDescribe(const QString& materialKind) {
    return materialKind == QLatin1String(MaterialKinds::PUBLICATION) ||
           materialKind == QLatin1String(MaterialKinds::DOCUMENT) ||
           materialKind == QLatin1String(MaterialKinds::WEB_PAGE) ||
           materialKind == QLatin1String(MaterialKinds::RECORDING);
}

bool QuoteSource::isEmpty() const {
    return source.isEmpty() && page.isEmpty() && author.isEmpty() && work.isEmpty() &&
           extra.isEmpty();
}

PersonName PersonName::of(const QString& name, bool institution) {
    const QString simplified = name.simplified();
    if (institution) {
        return {.family = simplified, .given = {}};
    }
    const qsizetype comma = simplified.indexOf(QLatin1Char(','));
    if (comma >= 0) {
        return {.family = simplified.left(comma).trimmed(),
                .given = simplified.mid(comma + 1).trimmed()};
    }
    const qsizetype space = simplified.lastIndexOf(QLatin1Char(' '));
    if (space < 0) {
        return {.family = simplified, .given = {}};
    }
    return {.family = simplified.mid(space + 1), .given = simplified.left(space)};
}

QString PersonName::initials() const {
    QStringList names;
    for (const QString& name : given.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        QStringList parts;
        for (const QString& part : name.split(QLatin1Char('-'), Qt::SkipEmptyParts)) {
            parts << part.left(1) + QLatin1Char('.');
        }
        names << parts.join(QLatin1Char('-'));
    }
    return names.join(QLatin1Char(' '));
}

}  // namespace kalahari::core
