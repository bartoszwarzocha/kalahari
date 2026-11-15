// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file bibliography_records.cpp
/// @brief The descriptions pasted from a library catalog or a bibliography manager: records of
/// BibTeX and RIS

#include "kalahari/core/bibliography.h"
#include "kalahari/core/source_description.h"

#include <QChar>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace kalahari::core {

namespace {

// ---------------------------------------------------------------------------------------------
// BibTeX
// ---------------------------------------------------------------------------------------------

/// The people of a BibTeX field: "Wiśniewska, Anna and Nowak, Jan"
QList<SourcePerson> bibtexPeople(const QString& names, const char* role) {
    QList<SourcePerson> people;
    static const QRegularExpression separator(QStringLiteral("\\s+and\\s+"),
                                              QRegularExpression::CaseInsensitiveOption);
    const bool author = QLatin1String(role) == QLatin1String(SourceDescription::AUTHOR);
    for (const QString& name : names.split(separator, Qt::SkipEmptyParts)) {
        SourcePerson person;
        person.name = name.simplified();
        if (!author) {
            person.role = QString::fromLatin1(role);
        }
        people << person;
    }
    return people;
}

/// @p text with the accents of LaTeX written as letters: "Zieli{\\'n}ska" is "Zielińska"
QString withoutLatexAccents(QString text) {
    static const QList<std::pair<QRegularExpression, QString>> letters = {
        {QRegularExpression(QStringLiteral("\\\\l(?![A-Za-z])\\s?")), QStringLiteral("ł")},
        {QRegularExpression(QStringLiteral("\\\\L(?![A-Za-z])\\s?")), QStringLiteral("Ł")},
        {QRegularExpression(QStringLiteral("\\\\ss(?![A-Za-z])\\s?")), QStringLiteral("ß")},
        {QRegularExpression(QStringLiteral("\\\\o(?![A-Za-z])\\s?")), QStringLiteral("ø")},
        {QRegularExpression(QStringLiteral("\\\\O(?![A-Za-z])\\s?")), QStringLiteral("Ø")}};
    for (const auto& [pattern, letter] : letters) {
        text.replace(pattern, letter);
    }
    // An accent over a letter: \'n, \'{n}, {\'n}, \k{a}, \c c
    static const QRegularExpression accent(
        QStringLiteral("\\\\(['`^\"~.=]|[kcuvH](?=[\\s{]))\\s*\\{?([A-Za-z])\\}?"));
    static const QMap<QString, QChar> marks = {
        {QStringLiteral("'"), QChar(u'́')}, {QStringLiteral("`"), QChar(u'̀')},
        {QStringLiteral("^"), QChar(u'̂')}, {QStringLiteral("\""), QChar(u'̈')},
        {QStringLiteral("~"), QChar(u'̃')}, {QStringLiteral("."), QChar(u'̇')},
        {QStringLiteral("="), QChar(u'̄')}, {QStringLiteral("k"), QChar(u'̨')},
        {QStringLiteral("c"), QChar(u'̧')}, {QStringLiteral("u"), QChar(u'̆')},
        {QStringLiteral("v"), QChar(u'̌')}, {QStringLiteral("H"), QChar(u'̋')}};
    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator matches = accent.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += text.mid(last, match.capturedStart() - last);
        result += match.captured(2) + marks.value(match.captured(1));
        last = match.capturedEnd();
    }
    result += text.mid(last);
    return result.normalized(QString::NormalizationForm_C);
}

/// Whether @p value starts with @p open and ends with @p close
bool enclosedIn(const QString& value, QChar open, QChar close) {
    return value.startsWith(open) && value.endsWith(close);
}

/// The value of a BibTeX field without its braces and quotes
QString bibtexValue(QString value) {
    value = value.trimmed();
    if (value.endsWith(QLatin1Char(','))) {
        value.chop(1);
        value = value.trimmed();
    }
    if (enclosedIn(value, QLatin1Char('{'), QLatin1Char('}')) ||
        enclosedIn(value, QLatin1Char('"'), QLatin1Char('"'))) {
        value = value.mid(1, value.size() - 2);
    }
    value = withoutLatexAccents(value);
    value.remove(QLatin1Char('{'));
    value.remove(QLatin1Char('}'));
    value.replace(QStringLiteral("--"), QStringLiteral("–"));
    return value.simplified();
}

/// The end of a value in braces that starts at @p start: after the brace that closes it
qsizetype closingBrace(const QString& body, qsizetype start) {
    int depth = 0;
    for (qsizetype at = start; at < body.size(); ++at) {
        if (body.at(at) == QLatin1Char('{')) {
            ++depth;
        } else if (body.at(at) == QLatin1Char('}') && --depth == 0) {
            return at + 1;
        }
    }
    return body.size();
}

/// The end of the value that starts at @p start: in braces (nested), in quotes, or a word up to
/// the comma
qsizetype valueEnd(const QString& body, qsizetype start) {
    const QChar first = start < body.size() ? body.at(start) : QChar();
    if (first == QLatin1Char('{')) {
        return closingBrace(body, start);
    }
    const bool quoted = first == QLatin1Char('"');
    const qsizetype end = body.indexOf(quoted ? QLatin1Char('"') : QLatin1Char(','),
                                       quoted ? start + 1 : start);
    if (end < 0) {
        return body.size();
    }
    return quoted ? end + 1 : end;
}

/// The fields of a BibTeX record: name -> value, the names in small letters
QMap<QString, QString> bibtexFields(const QString& body) {
    QMap<QString, QString> fields;
    qsizetype at = 0;
    while (at < body.size()) {
        // The name, up to "="
        const qsizetype equals = body.indexOf(QLatin1Char('='), at);
        if (equals < 0) {
            break;
        }
        const QString name = body.mid(at, equals - at).trimmed().toLower();
        qsizetype start = equals + 1;
        while (start < body.size() && body.at(start).isSpace()) {
            ++start;
        }
        const qsizetype end = valueEnd(body, start);
        fields.insert(name.section(QLatin1Char(','), -1).trimmed(),
                      bibtexValue(body.mid(start, end - start)));
        const qsizetype comma = body.indexOf(QLatin1Char(','), end);
        if (comma < 0) {
            break;
        }
        at = comma + 1;
    }
    return fields;
}

/// The kind of source of a BibTeX type: "article" is an article; "misc" with an address is a web
/// page
const char* bibtexKind(const QString& type, bool withUrl) {
    static const QMap<QString, const char*> kinds = {
        {QStringLiteral("article"), SourceDescription::ARTICLE},
        {QStringLiteral("incollection"), SourceDescription::CHAPTER},
        {QStringLiteral("inbook"), SourceDescription::CHAPTER},
        {QStringLiteral("inproceedings"), SourceDescription::CHAPTER},
        {QStringLiteral("online"), SourceDescription::WEB_PAGE},
        {QStringLiteral("electronic"), SourceDescription::WEB_PAGE},
        {QStringLiteral("book"), SourceDescription::BOOK}};
    if (type.endsWith(QLatin1String("thesis"))) {
        return SourceDescription::THESIS;
    }
    if (type == QLatin1String("misc") && withUrl) {
        return SourceDescription::WEB_PAGE;
    }
    return kinds.value(type, SourceDescription::OTHER);
}

/// Takes the fields of a BibTeX record into @p source: the first of the fields that go into
/// one field of the description ("journal", "journaltitle")
void takeBibtexFields(SourceDescription& source, const QMap<QString, QString>& fields) {
    static const QList<std::pair<const char*, const char*>> taken = {
        {"title", SourceDescription::TITLE},
        {"subtitle", SourceDescription::SUBTITLE},
        {"booktitle", SourceDescription::CONTAINER},
        {"journal", SourceDescription::CONTAINER},
        {"journaltitle", SourceDescription::CONTAINER},
        {"edition", SourceDescription::EDITION},
        {"address", SourceDescription::PLACE},
        {"location", SourceDescription::PLACE},
        {"publisher", SourceDescription::PUBLISHER},
        {"school", SourceDescription::PUBLISHER},
        {"institution", SourceDescription::PUBLISHER},
        {"year", SourceDescription::YEAR},
        {"date", SourceDescription::YEAR},
        {"volume", SourceDescription::VOLUME},
        {"number", SourceDescription::ISSUE},
        {"pages", SourceDescription::PAGES},
        {"series", SourceDescription::SERIES},
        {"isbn", SourceDescription::ISBN},
        {"issn", SourceDescription::ISSN},
        {"doi", SourceDescription::DOI},
        {"url", SourceDescription::URL},
        {"urldate", SourceDescription::ACCESSED},
        {"howpublished", SourceDescription::FORM},
        {"language", SourceDescription::LANGUAGE},
        // The type of a thesis is its form, when "howpublished" did not give one
        {"type", SourceDescription::FORM}};
    const bool thesis = source.type == QLatin1String(SourceDescription::THESIS);
    for (const auto& [from, to] : taken) {
        const QString value = fields.value(QString::fromLatin1(from));
        const bool form = thesis || QLatin1String(from) != QLatin1String("type");
        if (!value.isEmpty() && form && source.value(to).isEmpty()) {
            source.setValue(QString::fromLatin1(to), value);
        }
    }
}

std::optional<SourceDescription> parseBibtex(const QString& text) {
    static const QRegularExpression head(QStringLiteral("@\\s*([A-Za-z]+)\\s*[{(]\\s*[^,]*,"));
    const QRegularExpressionMatch match = head.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    qsizetype end = text.lastIndexOf(QLatin1Char('}'));
    if (end <= match.capturedEnd()) {
        end = text.size();
    }
    const QMap<QString, QString> fields =
        bibtexFields(text.mid(match.capturedEnd(), end - match.capturedEnd()));

    SourceDescription source;
    source.type = QString::fromLatin1(
        bibtexKind(match.captured(1).toLower(), fields.contains(QStringLiteral("url"))));
    source.people << bibtexPeople(fields.value(QStringLiteral("author")), SourceDescription::AUTHOR)
                  << bibtexPeople(fields.value(QStringLiteral("editor")), SourceDescription::EDITOR)
                  << bibtexPeople(fields.value(QStringLiteral("translator")),
                                  SourceDescription::TRANSLATOR);
    takeBibtexFields(source, fields);
    if (source.people.isEmpty() && source.values.isEmpty()) {
        return std::nullopt;
    }
    return source;
}

// ---------------------------------------------------------------------------------------------
// RIS
// ---------------------------------------------------------------------------------------------

/// A RIS record as it is read: the description and the tags that make one field of it together
struct RisRecord {
    SourceDescription source;  ///< The description read so far
    QString type;              ///< "TY": BOOK, JOUR...
    QString startPage;         ///< "SP"
    QString endPage;           ///< "EP"
    QString number;            ///< "SN": the ISBN of a book or the ISSN of a journal
};

/// Takes a person of tag @p tag ("AU", "ED"...) into @p source; false for a tag of no person
bool takeRisPerson(SourceDescription& source, const QString& tag, const QString& value) {
    static const QMap<QString, const char*> roles = {
        {QStringLiteral("AU"), SourceDescription::AUTHOR},
        {QStringLiteral("A1"), SourceDescription::AUTHOR},
        {QStringLiteral("ED"), SourceDescription::EDITOR},
        {QStringLiteral("A2"), SourceDescription::EDITOR},
        {QStringLiteral("A3"), SourceDescription::EDITOR},
        {QStringLiteral("A4"), SourceDescription::TRANSLATOR}};
    const auto role = roles.constFind(tag);
    if (role == roles.cend()) {
        return false;
    }
    SourcePerson person;
    person.name = value.simplified();
    if (QLatin1String(role.value()) != QLatin1String(SourceDescription::AUTHOR)) {
        person.role = QString::fromLatin1(role.value());
    }
    source.people << person;
    return true;
}

/// Takes the field of tag @p tag ("TI", "PY"...) into @p source, unless it has one
void takeRisField(SourceDescription& source, const QString& tag, const QString& value) {
    static const QMap<QString, const char*> fields = {
        {QStringLiteral("TI"), SourceDescription::TITLE},
        {QStringLiteral("T1"), SourceDescription::TITLE},
        {QStringLiteral("BT"), SourceDescription::CONTAINER},
        {QStringLiteral("T2"), SourceDescription::CONTAINER},
        {QStringLiteral("JO"), SourceDescription::CONTAINER},
        {QStringLiteral("JF"), SourceDescription::CONTAINER},
        {QStringLiteral("T3"), SourceDescription::SERIES},
        {QStringLiteral("ET"), SourceDescription::EDITION},
        {QStringLiteral("CY"), SourceDescription::PLACE},
        {QStringLiteral("PB"), SourceDescription::PUBLISHER},
        {QStringLiteral("PY"), SourceDescription::YEAR},
        {QStringLiteral("Y1"), SourceDescription::YEAR},
        {QStringLiteral("DA"), SourceDescription::YEAR},
        {QStringLiteral("VL"), SourceDescription::VOLUME},
        {QStringLiteral("IS"), SourceDescription::ISSUE},
        {QStringLiteral("DO"), SourceDescription::DOI},
        {QStringLiteral("UR"), SourceDescription::URL},
        {QStringLiteral("Y2"), SourceDescription::ACCESSED},
        {QStringLiteral("M3"), SourceDescription::FORM},
        {QStringLiteral("LA"), SourceDescription::LANGUAGE}};
    const auto field = fields.constFind(tag);
    if (field == fields.cend() || !source.value(field.value()).isEmpty()) {
        return;
    }
    // "2011/05/01/" is the year 2011
    const bool dated = tag == QLatin1String("PY") || tag == QLatin1String("Y1");
    source.setValue(QString::fromLatin1(field.value()),
                    dated ? value.section(QLatin1Char('/'), 0, 0) : value);
}

/// Takes tag @p tag of a RIS record into @p record
void takeRisTag(RisRecord& record, const QString& tag, const QString& value) {
    if (takeRisPerson(record.source, tag, value)) {
        return;
    }
    if (tag == QLatin1String("SP")) {
        record.startPage = value;
    } else if (tag == QLatin1String("EP")) {
        record.endPage = value;
    } else if (tag == QLatin1String("SN")) {
        record.number = value;
    } else {
        takeRisField(record.source, tag, value);
    }
}

/// The description of a RIS record that was read to its end
std::optional<SourceDescription> risDescription(RisRecord record) {
    static const QMap<QString, const char*> kinds = {
        {QStringLiteral("BOOK"), SourceDescription::BOOK},
        {QStringLiteral("EBOOK"), SourceDescription::BOOK},
        {QStringLiteral("EDBOOK"), SourceDescription::BOOK},
        {QStringLiteral("CHAP"), SourceDescription::CHAPTER},
        {QStringLiteral("ECHAP"), SourceDescription::CHAPTER},
        {QStringLiteral("JOUR"), SourceDescription::ARTICLE},
        {QStringLiteral("EJOUR"), SourceDescription::ARTICLE},
        {QStringLiteral("MGZN"), SourceDescription::ARTICLE},
        {QStringLiteral("NEWS"), SourceDescription::ARTICLE},
        {QStringLiteral("ELEC"), SourceDescription::WEB_PAGE},
        {QStringLiteral("WEB"), SourceDescription::WEB_PAGE},
        {QStringLiteral("BLOG"), SourceDescription::WEB_PAGE},
        {QStringLiteral("MANSCPT"), SourceDescription::ARCHIVE},
        {QStringLiteral("PCOMM"), SourceDescription::INTERVIEW},
        {QStringLiteral("MPCT"), SourceDescription::FILM},
        {QStringLiteral("VIDEO"), SourceDescription::FILM},
        {QStringLiteral("SOUND"), SourceDescription::FILM},
        {QStringLiteral("THES"), SourceDescription::THESIS}};
    SourceDescription& source = record.source;
    source.type = QString::fromLatin1(kinds.value(record.type, SourceDescription::OTHER));
    if (!record.startPage.isEmpty()) {
        const QString pages = record.endPage.isEmpty()
                                  ? record.startPage
                                  : record.startPage + QStringLiteral("–") + record.endPage;
        source.setValue(QString::fromLatin1(SourceDescription::PAGES), pages);
    }
    if (!record.number.isEmpty()) {
        const bool serial = source.type == QLatin1String(SourceDescription::ARTICLE);
        source.setValue(
            QString::fromLatin1(serial ? SourceDescription::ISSN : SourceDescription::ISBN),
            record.number);
    }
    if (source.people.isEmpty() && source.values.isEmpty()) {
        return std::nullopt;
    }
    return source;
}

std::optional<SourceDescription> parseRis(const QString& text) {
    static const QRegularExpression line(QStringLiteral("^([A-Z][A-Z0-9])  -(?: (.*))?$"));
    RisRecord record;
    bool started = false;
    for (const QString& each : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = line.match(each.trimmed());
        if (!match.hasMatch()) {
            continue;
        }
        const QString tag = match.captured(1);
        const QString value = match.captured(2).trimmed();
        if (tag == QLatin1String("TY")) {
            // The first record of several
            if (started) {
                break;
            }
            started = true;
            record.type = value.toUpper();
        } else if (started && tag == QLatin1String("ER")) {
            break;
        } else if (started && !value.isEmpty()) {
            takeRisTag(record, tag, value);
        }
    }
    if (!started) {
        return std::nullopt;
    }
    return risDescription(record);
}

}  // namespace

std::optional<SourceDescription> Bibliography::parse(const QString& text) {
    const QString trimmed = text.trimmed();
    static const QRegularExpression bibtex(QStringLiteral("^\\s*@\\s*[A-Za-z]+\\s*[{(]"),
                                           QRegularExpression::MultilineOption);
    if (trimmed.startsWith(QLatin1Char('@')) || trimmed.contains(bibtex)) {
        return parseBibtex(trimmed);
    }
    return parseRis(text);
}

}  // namespace kalahari::core
