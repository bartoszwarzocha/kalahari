// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file bibliography.cpp
/// @brief The entries of the sources in the styles of a bibliography and the references in the
/// text (the reading of BibTeX and RIS is in bibliography_records.cpp)

#include "kalahari/core/bibliography.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"

#include <QChar>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <string>
#include <utility>

namespace kalahari::core {

namespace {

/// The closing double quotation mark: "”"
constexpr char16_t CLOSING_QUOTE = u'\u201D';

/// The most names an entry shows before "i in." in PN-ISO 690 and Harvard
constexpr qsizetype MOST_NAMED = 3;

/// The words of a bibliography in one language
struct Words {
    const char* edition;      ///< "Wyd." before the edition
    const char* editionLow;   ///< "wyd." in parentheses
    bool editionAfter;        ///< The word after the number: "2nd ed."
    const char* page;         ///< "s." before a page
    const char* pages;        ///< "s." before pages
    const char* in;           ///< "W:" before the book of a chapter
    const char* editorShort;  ///< "red." after an editor
    const char* editorWord;   ///< "Red." in APA
    const char* editedBy;     ///< "red." before editors in Chicago and MLA
    const char* translated;   ///< "Tłum." before translators
    const char* directed;     ///< "Reż." before directors
    const char* supervised;   ///< "Promotor:" before supervisors
    const char* interviewed;  ///< "Rozmowę prowadzi:" before the one who asks
    const char* online;       ///< "[online]"
    const char* accessed;     ///< "dostęp"
    const char* available;    ///< "Dostępne w:"
    const char* volume;       ///< "t."
    const char* issue;        ///< "nr"
    const char* others;       ///< "i in."
    const char* andWord;      ///< "i"
    const char* apaAnd;       ///< "i" in APA, "&" in English
    const char* noDate;       ///< "b.r."
    const char* fonds;        ///< "zespół"
    const char* signature;    ///< "sygn."
    const char* openQuote;    ///< "„"
    const char* closeQuote;   ///< "”"
};

constexpr Words POLISH{.edition = "Wyd.",
                       .editionLow = "wyd.",
                       .editionAfter = false,
                       .page = "s.",
                       .pages = "s.",
                       .in = "W:",
                       .editorShort = "red.",
                       .editorWord = "Red.",
                       .editedBy = "red.",
                       .translated = "Tłum.",
                       .directed = "Reż.",
                       .supervised = "Promotor:",
                       .interviewed = "Rozmowę prowadzi:",
                       .online = "[online]",
                       .accessed = "dostęp",
                       .available = "Dostępne w:",
                       .volume = "t.",
                       .issue = "nr",
                       .others = "i in.",
                       .andWord = "i",
                       .apaAnd = "i",
                       .noDate = "b.r.",
                       .fonds = "zespół",
                       .signature = "sygn.",
                       .openQuote = "\u201E",
                       .closeQuote = "\u201D"};

constexpr Words ENGLISH{.edition = "ed.",
                        .editionLow = "ed.",
                        .editionAfter = true,
                        .page = "p.",
                        .pages = "pp.",
                        .in = "In:",
                        .editorShort = "ed.",
                        .editorWord = "Ed.",
                        .editedBy = "edited by",
                        .translated = "Translated by",
                        .directed = "Directed by",
                        .supervised = "Supervisor:",
                        .interviewed = "Interviewed by",
                        .online = "[online]",
                        .accessed = "accessed",
                        .available = "Available at:",
                        .volume = "vol.",
                        .issue = "no.",
                        .others = "et al.",
                        .andWord = "and",
                        .apaAnd = "&",
                        .noDate = "n.d.",
                        .fonds = "fonds",
                        .signature = "ref.",
                        .openQuote = "\u201C",
                        .closeQuote = "\u201D"};

const Words& wordsFor(const QString& language) {
    return language.startsWith(QLatin1String("pl")) ? POLISH : ENGLISH;
}

QString word(const char* text) {
    return QString::fromUtf8(text);
}

/// Whether @p text, a rich one too, ends with a full stop, a question or an exclamation mark
bool endsWithStop(const QString& text) {
    QString end = text;
    static const QRegularExpression tags(QStringLiteral("(<[^>]*>)+$"));
    end.remove(tags);
    end = end.trimmed();
    while (!end.isEmpty() && (end.back() == QChar(CLOSING_QUOTE) ||
                              end.back() == QLatin1Char('"') || end.back() == QLatin1Char('\''))) {
        end.chop(1);
    }
    return end.endsWith(QLatin1Char('.')) || end.endsWith(QLatin1Char('?')) ||
           end.endsWith(QLatin1Char('!'));
}

/// @p text with a full stop at its end, unless it has one
QString dotted(const QString& text) {
    if (text.isEmpty() || endsWithStop(text)) {
        return text;
    }
    return text + QLatin1Char('.');
}

/// The parts that are not empty, each a sentence ending with a full stop, joined by spaces
QString sentences(const QStringList& parts) {
    QStringList shown;
    for (const QString& part : parts) {
        if (!part.trimmed().isEmpty()) {
            shown << dotted(part.trimmed());
        }
    }
    return shown.join(QLatin1Char(' '));
}

/// The parts that are not empty, joined by @p separator
QString joined(const QStringList& parts, const QString& separator) {
    QStringList shown;
    for (const QString& part : parts) {
        if (!part.trimmed().isEmpty()) {
            shown << part.trimmed();
        }
    }
    return shown.join(separator);
}

/// "t. 12": @p label (a word of the language) and @p value; nothing without a value
QString labelled(const char* label, const QString& value) {
    return value.isEmpty() ? QString() : word(label) + QLatin1Char(' ') + value;
}

/// "(Seria)": @p value between @p before and @p after; nothing without a value
QString enclosed(const QString& value, const QString& before, const QString& after) {
    return value.isEmpty() ? QString() : before + value + after;
}

/// "Wyd. 2 popr.", "2nd ed.": an edition that starts with its number gets the word of the
/// language before or after it; a text of its own ("Second edition") stays as it is
QString editionText(const QString& edition, const Words& words, bool lower = false) {
    if (edition.isEmpty()) {
        return {};
    }
    if (!edition.front().isDigit()) {
        return edition;
    }
    const QString name = word(lower ? words.editionLow : words.edition);
    return words.editionAfter ? edition + QLatin1Char(' ') + name
                              : name + QLatin1Char(' ') + edition;
}

/// The writer of the entries of one source in one language
class Writer {
public:
    Writer(const SourceDescription& source, const QString& title, const QString& language,
           bool rich)
        : m_source(&source), m_title(title.trimmed()), m_words(&wordsFor(language)), m_rich(rich) {}

    [[nodiscard]] const Words& words() const { return *m_words; }
    [[nodiscard]] const SourceDescription& source() const { return *m_source; }

    /// The text as the entry has it: escaped in a rich entry
    [[nodiscard]] QString text(const QString& plain) const {
        return m_rich ? plain.toHtmlEscaped() : plain;
    }

    /// The text in italics in a rich entry
    [[nodiscard]] QString italic(const QString& plain) const {
        if (plain.isEmpty()) {
            return {};
        }
        return m_rich ? QStringLiteral("<i>%1</i>").arg(plain.toHtmlEscaped()) : plain;
    }

    /// The text in the quotation marks of the language
    [[nodiscard]] QString quoted(const QString& plain) const {
        if (plain.isEmpty()) {
            return {};
        }
        return word(m_words->openQuote) + text(plain) + word(m_words->closeQuote);
    }

    /// The value of @p field as the writer typed it
    [[nodiscard]] QString raw(const char* field) const { return m_source->value(field); }

    /// The value of @p field as the entry has it
    [[nodiscard]] QString value(const char* field) const { return text(raw(field)); }

    /// The title of the work, the title of the material for a description without one
    [[nodiscard]] QString title() const {
        const QString own = raw(SourceDescription::TITLE);
        return own.isEmpty() ? m_title : own;
    }

    /// The title and the subtitle after a colon; @p capital: the subtitle starts with a capital
    [[nodiscard]] QString fullTitle(bool capital = false) const {
        QString subtitle = raw(SourceDescription::SUBTITLE);
        if (subtitle.isEmpty()) {
            return title();
        }
        if (capital) {
            subtitle[0] = subtitle.at(0).toUpper();
        }
        return title() + QStringLiteral(": ") + subtitle;
    }

    /// The people of @p role
    [[nodiscard]] QList<SourcePerson> people(const char* role) const {
        return m_source->peopleOf(role);
    }

    /// The authors: the people who wrote it, the ones asked in a talk, the institutions
    [[nodiscard]] QList<SourcePerson> authors() const {
        QList<SourcePerson> found = people(SourceDescription::AUTHOR);
        if (m_source->kind() == QLatin1String(SourceDescription::INTERVIEW)) {
            found << people(SourceDescription::INTERVIEWEE);
        }
        if (found.isEmpty() && m_source->kind() != QLatin1String(SourceDescription::WEB_PAGE)) {
            found = people(SourceDescription::INSTITUTION);
        }
        return found;
    }

    /// The name of @p person, family first: "Wiśniewska, Anna"; @p initials: "Wiśniewska, A.";
    /// @p upper: "WIŚNIEWSKA, Anna"
    [[nodiscard]] QString inverted(const SourcePerson& person, bool initials,
                                   bool upper = false) const {
        const PersonName name = nameOf(person);
        const QString family = upper ? name.family.toUpper() : name.family;
        const QString given = initials ? name.initials() : name.given;
        return text(given.isEmpty() ? family : family + QStringLiteral(", ") + given);
    }

    /// The name of @p person, given names first: "Anna Wiśniewska"; @p initials: "A. Wiśniewska"
    [[nodiscard]] QString direct(const SourcePerson& person, bool initials) const {
        const PersonName name = nameOf(person);
        const QString given = initials ? name.initials() : name.given;
        return text(given.isEmpty() ? name.family : given + QLatin1Char(' ') + name.family);
    }

    /// Names joined by commas, the last one by "and" (@p andWord) and a comma before it when
    /// @p serialComma
    [[nodiscard]] static QString list(const QStringList& names, const QString& andWord,
                                      bool serialComma = false) {
        if (names.size() < 2) {
            return names.value(0);
        }
        const QString& last = names.last();
        const QString head = names.mid(0, names.size() - 1).join(QStringLiteral(", "));
        const QString comma = serialComma && names.size() > 2 ? QStringLiteral(",") : QString();
        return head + comma + QLatin1Char(' ') + andWord + QLatin1Char(' ') + last;
    }

private:
    [[nodiscard]] static PersonName nameOf(const SourcePerson& person) {
        return PersonName::of(person.name,
                              person.role == QLatin1String(SourceDescription::INSTITUTION));
    }

    const SourceDescription* m_source;  ///< Not null: the description outlives the writer
    QString m_title;
    const Words* m_words;  ///< Not null: one of the words of the languages
    bool m_rich;
};

/// The kinds of the styles' entries
bool isKind(const Writer& writer, const char* kind) {
    return writer.source().kind() == QLatin1String(kind);
}

/// "Kraków: Oficyna Oaza", "Kraków", "Oficyna Oaza"
QString placeAndPublisher(const Writer& w) {
    return joined({w.value(SourceDescription::PLACE), w.value(SourceDescription::PUBLISHER)},
                  QStringLiteral(": "));
}

/// "DOI 10.1/x" or "Dostępne w: https://..."
QString pnLocation(const Writer& w) {
    const QString doi = w.value(SourceDescription::DOI);
    if (!doi.isEmpty()) {
        return QStringLiteral("DOI ") + doi;
    }
    const QString url = w.value(SourceDescription::URL);
    return url.isEmpty() ? QString() : word(w.words().available) + QLatin1Char(' ') + url;
}


// ---------------------------------------------------------------------------------------------
// The parts every style has
// ---------------------------------------------------------------------------------------------

/// The writer of the entry of one kind of source
using KindEntry = QString (*)(const Writer&);

/// The entry of the kind of the source: the first of @p kinds that is it, else @p otherwise
QString entryOfKind(const Writer& w, std::initializer_list<std::pair<const char*, KindEntry>> kinds,
                    KindEntry otherwise) {
    for (const auto& [kind, write] : kinds) {
        if (isKind(w, kind)) {
            return write(w);
        }
    }
    return otherwise(w);
}

/// "Kraków: Oficyna Oaza, 2011": the place, the publisher and the year
QString placedAndDated(const Writer& w) {
    return joined({placeAndPublisher(w), w.value(SourceDescription::YEAR)}, QStringLiteral(", "));
}

/// The year, "b.r." without one
QString yearOrNoDate(const Writer& w) {
    const QString year = w.value(SourceDescription::YEAR);
    return year.isEmpty() ? word(w.words().noDate) : year;
}

/// The edition as the entry has it: "Wyd. 2", "2nd ed."
QString editionOf(const Writer& w, bool lower = false) {
    return w.text(editionText(w.raw(SourceDescription::EDITION), w.words(), lower));
}

/// The owners of a web page (its institutions), else the name of the site
QString siteOf(const Writer& w) {
    QStringList owners;
    for (const SourcePerson& person : w.people(SourceDescription::INSTITUTION)) {
        owners << w.text(person.name.simplified());
    }
    return owners.isEmpty() ? w.value(SourceDescription::CONTAINER)
                            : owners.join(QStringLiteral(", "));
}

// ---------------------------------------------------------------------------------------------
// PN-ISO 690
// ---------------------------------------------------------------------------------------------

QString pnNames(const Writer& w, const QList<SourcePerson>& people) {
    QStringList names;
    for (const SourcePerson& person : people) {
        names << w.inverted(person, false, true);
    }
    if (names.size() > MOST_NAMED) {
        return names.first() + QLatin1Char(' ') + word(w.words().others);
    }
    return names.join(QStringLiteral(", "));
}

/// "LIS, Ewa (red.)": the editors of a book or of the book of a chapter
QString pnEditors(const Writer& w) {
    const QList<SourcePerson> editors = w.people(SourceDescription::EDITOR);
    if (editors.isEmpty()) {
        return {};
    }
    return pnNames(w, editors) + QStringLiteral(" (") + word(w.words().editorShort) +
           QLatin1Char(')');
}

QString pnPeopleAfter(const Writer& w, const char* role, const char* before) {
    QStringList names;
    for (const SourcePerson& person : w.people(role)) {
        names << w.direct(person, false);
    }
    return labelled(before, names.join(QStringLiteral(", ")));
}

QString pnAuthors(const Writer& w) {
    return pnNames(w, w.authors());
}

/// The title in italics with its form in brackets: "Atlas [rękopis]"
QString pnTitled(const Writer& w) {
    return w.italic(w.title()) +
           enclosed(w.value(SourceDescription::FORM), QStringLiteral(" ["), QStringLiteral("]"));
}

QString pnChapter(const Writer& w) {
    const Words& words = w.words();
    const QString in = joined({word(words.in), pnEditors(w)}, QStringLiteral(" "));
    const QString published =
        joined({placeAndPublisher(w), w.value(SourceDescription::YEAR),
                labelled(words.pages, w.value(SourceDescription::PAGES))},
               QStringLiteral(", "));
    return sentences({pnAuthors(w), w.text(w.title()), in,
                      w.italic(w.raw(SourceDescription::CONTAINER)), editionOf(w), published,
                      enclosed(w.value(SourceDescription::ISBN), QStringLiteral("ISBN "), {})});
}

QString pnArticle(const Writer& w) {
    const Words& words = w.words();
    const QString where = joined({w.value(SourceDescription::YEAR),
                                  labelled(words.volume, w.value(SourceDescription::VOLUME)),
                                  labelled(words.issue, w.value(SourceDescription::ISSUE)),
                                  labelled(words.pages, w.value(SourceDescription::PAGES))},
                                 QStringLiteral(", "));
    return sentences({pnAuthors(w), w.text(w.title()), w.italic(w.raw(SourceDescription::CONTAINER)),
                      where,
                      enclosed(w.value(SourceDescription::ISSN), QStringLiteral("ISSN "), {}),
                      pnLocation(w)});
}

QString pnWebPage(const Writer& w) {
    const Words& words = w.words();
    const QString when = enclosed(labelled(words.accessed, w.value(SourceDescription::ACCESSED)),
                                  QStringLiteral("["), QStringLiteral("]"));
    const QString publishedIn =
        joined({joined({siteOf(w), w.value(SourceDescription::PUBLISHED)}, QStringLiteral(", ")),
                when},
               QStringLiteral(" "));
    return sentences({pnAuthors(w), w.italic(w.title()) + QLatin1Char(' ') + word(words.online),
                      publishedIn, pnLocation(w)});
}

QString pnArchive(const Writer& w) {
    const Words& words = w.words();
    const QString where =
        joined({w.value(SourceDescription::CONTAINER),
                labelled(words.fonds, w.value(SourceDescription::FONDS)),
                labelled(words.signature, w.value(SourceDescription::SIGNATURE))},
               QStringLiteral(", "));
    return sentences(
        {pnAuthors(w), pnTitled(w), w.value(SourceDescription::YEAR), where, pnLocation(w)});
}

/// A talk, a film or a recording, and a source of another kind
QString pnRecording(const Writer& w) {
    const Words& words = w.words();
    return sentences({pnAuthors(w), pnTitled(w),
                      pnPeopleAfter(w, SourceDescription::INTERVIEWER, words.interviewed),
                      pnPeopleAfter(w, SourceDescription::DIRECTOR, words.directed),
                      placedAndDated(w), pnLocation(w)});
}

QString pnThesis(const Writer& w) {
    return sentences({pnAuthors(w), w.italic(w.fullTitle()), w.value(SourceDescription::FORM),
                      placedAndDated(w),
                      pnPeopleAfter(w, SourceDescription::SUPERVISOR, w.words().supervised),
                      pnLocation(w)});
}

/// A book; the editors stand for the authors of a book without them
QString pnBook(const Writer& w) {
    const QString authors = pnAuthors(w);
    return sentences(
        {authors.isEmpty() ? pnEditors(w) : authors, w.italic(w.fullTitle()),
         pnPeopleAfter(w, SourceDescription::TRANSLATOR, w.words().translated), editionOf(w),
         placedAndDated(w),
         enclosed(w.value(SourceDescription::SERIES), QStringLiteral("("), QStringLiteral(")")),
         enclosed(w.value(SourceDescription::ISBN), QStringLiteral("ISBN "), {}), pnLocation(w)});
}

QString pnEntry(const Writer& w) {
    return entryOfKind(w,
                       {{SourceDescription::CHAPTER, pnChapter},
                        {SourceDescription::ARTICLE, pnArticle},
                        {SourceDescription::WEB_PAGE, pnWebPage},
                        {SourceDescription::ARCHIVE, pnArchive},
                        {SourceDescription::INTERVIEW, pnRecording},
                        {SourceDescription::FILM, pnRecording},
                        {SourceDescription::OTHER, pnRecording},
                        {SourceDescription::THESIS, pnThesis}},
                       pnBook);
}

// ---------------------------------------------------------------------------------------------
// APA
// ---------------------------------------------------------------------------------------------

QString apaNames(const Writer& w, const QList<SourcePerson>& people) {
    QStringList names;
    for (const SourcePerson& person : people) {
        names << w.inverted(person, true);
    }
    return Writer::list(names, word(w.words().apaAnd), true);
}

QString apaDoiOrUrl(const Writer& w) {
    const QString doi = w.value(SourceDescription::DOI);
    if (!doi.isEmpty()) {
        return doi.startsWith(QLatin1String("http")) ? doi
                                                     : QStringLiteral("https://doi.org/") + doi;
    }
    return w.value(SourceDescription::URL);
}

/// The authors; the editors stand for the authors of a book without them, not of a chapter
QString apaAuthors(const Writer& w) {
    QString authors = apaNames(w, w.authors());
    const QList<SourcePerson> editors = w.people(SourceDescription::EDITOR);
    if (!authors.isEmpty() || editors.isEmpty() || isKind(w, SourceDescription::CHAPTER)) {
        return authors;
    }
    return apaNames(w, editors) + QStringLiteral(" (") + word(w.words().editorWord) +
           QStringLiteral(".)");
}

/// "Wiśniewska, A. (2011)." with @p date in parentheses
QString apaHead(const Writer& w, const QString& date) {
    return joined({dotted(apaAuthors(w)), dotted(QLatin1Char('(') + date + QLatin1Char(')'))},
                  QStringLiteral(" "));
}

/// The form in brackets: " [rękopis]"
QString apaForm(const Writer& w) {
    return enclosed(w.value(SourceDescription::FORM), QStringLiteral(" ["), QStringLiteral("]"));
}

QString apaChapter(const Writer& w) {
    const Words& words = w.words();
    QStringList names;
    for (const SourcePerson& person : w.people(SourceDescription::EDITOR)) {
        names << w.direct(person, true);
    }
    const QString by =
        enclosed(Writer::list(names, word(words.andWord)), QString(),
                 QStringLiteral(" (") + word(words.editorWord) + QStringLiteral(".), "));
    const QString inBook =
        word(words.in) + QLatin1Char(' ') + by + w.italic(w.raw(SourceDescription::CONTAINER)) +
        enclosed(labelled(words.pages, w.value(SourceDescription::PAGES)), QStringLiteral(" ("),
                 QStringLiteral(")"));
    return joined({apaHead(w, yearOrNoDate(w)),
                   sentences({w.text(w.title()), inBook, w.value(SourceDescription::PUBLISHER)}),
                   apaDoiOrUrl(w)},
                  QStringLiteral(" "));
}

QString apaArticle(const Writer& w) {
    const QString journal =
        joined({w.italic(w.raw(SourceDescription::CONTAINER)),
                w.italic(w.raw(SourceDescription::VOLUME)) +
                    enclosed(w.value(SourceDescription::ISSUE), QStringLiteral("("),
                             QStringLiteral(")")),
                w.value(SourceDescription::PAGES)},
               QStringLiteral(", "));
    return joined({apaHead(w, yearOrNoDate(w)), sentences({w.text(w.title()), journal}),
                   apaDoiOrUrl(w)},
                  QStringLiteral(" "));
}

QString apaWebPage(const Writer& w) {
    const QString published = w.value(SourceDescription::PUBLISHED);
    return joined({apaHead(w, published.isEmpty() ? yearOrNoDate(w) : published),
                   sentences({w.italic(w.title()), siteOf(w)}), w.value(SourceDescription::URL)},
                  QStringLiteral(" "));
}

QString apaArchive(const Writer& w) {
    const QString where =
        joined({w.value(SourceDescription::FONDS), w.value(SourceDescription::SIGNATURE)},
               QStringLiteral(", "));
    const QString held =
        joined({w.value(SourceDescription::CONTAINER),
                enclosed(where, QStringLiteral("("), QStringLiteral(")"))},
               QStringLiteral(" "));
    return joined({apaHead(w, yearOrNoDate(w)), sentences({w.italic(w.title()) + apaForm(w), held}),
                   apaDoiOrUrl(w)},
                  QStringLiteral(" "));
}

QString apaBook(const Writer& w) {
    const QString edition = enclosed(editionOf(w, true), QStringLiteral(" ("), QStringLiteral(")"));
    return joined({apaHead(w, yearOrNoDate(w)),
                   sentences({w.italic(w.fullTitle(true)) + edition + apaForm(w),
                              w.value(SourceDescription::PUBLISHER)}),
                   apaDoiOrUrl(w)},
                  QStringLiteral(" "));
}

QString apaEntry(const Writer& w) {
    return entryOfKind(w,
                       {{SourceDescription::CHAPTER, apaChapter},
                        {SourceDescription::ARTICLE, apaArticle},
                        {SourceDescription::WEB_PAGE, apaWebPage},
                        {SourceDescription::ARCHIVE, apaArchive}},
                       apaBook);
}

// ---------------------------------------------------------------------------------------------
// Chicago (notes and bibliography), MLA and Harvard
// ---------------------------------------------------------------------------------------------

/// "Wiśniewska, Anna, and Jan Nowak": the first one family first, the others given names first
QString firstInvertedNames(const Writer& w, const QList<SourcePerson>& people, bool serialComma,
                           int most) {
    QStringList names;
    for (qsizetype i = 0; i < people.size(); ++i) {
        names << (i == 0 ? w.inverted(people.at(i), false) : w.direct(people.at(i), false));
    }
    if (most > 0 && names.size() > most) {
        return names.first() + QStringLiteral(", ") + word(w.words().others);
    }
    return Writer::list(names, word(w.words().andWord), serialComma);
}

QString editorsDirect(const Writer& w) {
    QStringList names;
    for (const SourcePerson& person : w.people(SourceDescription::EDITOR)) {
        names << w.direct(person, false);
    }
    return Writer::list(names, word(w.words().andWord));
}

QString chicagoEntry(const Writer& w) {
    const Words& words = w.words();
    const QString authors = firstInvertedNames(w, w.authors(), true, 10);
    const QString year = w.value(SourceDescription::YEAR);
    const QString pages = w.value(SourceDescription::PAGES);
    const QString edition = w.text(editionText(w.raw(SourceDescription::EDITION), words));
    const QString placed = joined({placeAndPublisher(w), year}, QStringLiteral(", "));
    if (isKind(w, SourceDescription::CHAPTER)) {
        const QString editors = editorsDirect(w);
        const QString inBook = joined(
            {word(words.in) + QLatin1Char(' ') + w.italic(w.raw(SourceDescription::CONTAINER)),
             editors.isEmpty() ? QString() : word(words.editedBy) + QLatin1Char(' ') + editors,
             pages},
            QStringLiteral(", "));
        return sentences({authors, w.quoted(dotted(w.title())), inBook, placed});
    }
    if (isKind(w, SourceDescription::ARTICLE)) {
        const QString volume = w.value(SourceDescription::VOLUME);
        const QString issue = w.value(SourceDescription::ISSUE);
        QString journal = joined({w.italic(w.raw(SourceDescription::CONTAINER)), volume},
                                 QStringLiteral(" "));
        if (!issue.isEmpty()) {
            journal += QStringLiteral(", ") + word(words.issue) + QLatin1Char(' ') + issue;
        }
        if (!year.isEmpty()) {
            journal += QStringLiteral(" (") + year + QLatin1Char(')');
        }
        if (!pages.isEmpty()) {
            journal += QStringLiteral(": ") + pages;
        }
        return sentences({authors, w.quoted(dotted(w.title())), journal, apaDoiOrUrl(w)});
    }
    if (isKind(w, SourceDescription::WEB_PAGE)) {
        return sentences(
            {authors, w.quoted(dotted(w.title())), w.value(SourceDescription::CONTAINER),
             w.value(SourceDescription::PUBLISHED),
             w.value(SourceDescription::ACCESSED).isEmpty()
                 ? QString()
                 : word(words.accessed) + QLatin1Char(' ') + w.value(SourceDescription::ACCESSED),
             w.value(SourceDescription::URL)});
    }
    const QString form = w.value(SourceDescription::FORM);
    return sentences({authors.isEmpty() ? QString() : authors, w.italic(w.fullTitle(true)), form,
                      edition, placed, apaDoiOrUrl(w)});
}

QString mlaEntry(const Writer& w) {
    const Words& words = w.words();
    const QString authors = firstInvertedNames(w, w.authors(), true, 2);
    const QString year = w.value(SourceDescription::YEAR);
    const QString pages = w.value(SourceDescription::PAGES);
    const QString pagesText =
        pages.isEmpty() ? QString() : word(words.pages) + QLatin1Char(' ') + pages;
    if (isKind(w, SourceDescription::CHAPTER)) {
        const QString editors = editorsDirect(w);
        const QString container = joined(
            {w.italic(w.raw(SourceDescription::CONTAINER)),
             editors.isEmpty() ? QString() : word(words.editedBy) + QLatin1Char(' ') + editors,
             w.value(SourceDescription::PUBLISHER), year, pagesText},
            QStringLiteral(", "));
        return sentences({authors, w.quoted(dotted(w.title())), container});
    }
    if (isKind(w, SourceDescription::ARTICLE)) {
        const QString volume = w.value(SourceDescription::VOLUME);
        const QString issue = w.value(SourceDescription::ISSUE);
        const QString container = joined(
            {w.italic(w.raw(SourceDescription::CONTAINER)),
             volume.isEmpty() ? QString() : word(words.volume) + QLatin1Char(' ') + volume,
             issue.isEmpty() ? QString() : word(words.issue) + QLatin1Char(' ') + issue, year,
             pagesText},
            QStringLiteral(", "));
        return sentences({authors, w.quoted(dotted(w.title())), container, apaDoiOrUrl(w)});
    }
    if (isKind(w, SourceDescription::WEB_PAGE)) {
        const QString accessed = w.value(SourceDescription::ACCESSED);
        return sentences({authors, w.quoted(dotted(w.title())),
                          joined({w.italic(w.raw(SourceDescription::CONTAINER)),
                                  w.value(SourceDescription::PUBLISHED),
                                  w.value(SourceDescription::URL)},
                                 QStringLiteral(", ")),
                          accessed.isEmpty() ? QString()
                                             : word(words.accessed) + QLatin1Char(' ') + accessed});
    }
    const QString edition =
        w.text(editionText(w.raw(SourceDescription::EDITION), words));
    return sentences({authors, w.italic(w.fullTitle(true)),
                      joined({edition, w.value(SourceDescription::PUBLISHER), year},
                             QStringLiteral(", ")),
                      w.value(SourceDescription::URL)});
}

QString harvardNames(const Writer& w, const QList<SourcePerson>& people) {
    QStringList names;
    for (const SourcePerson& person : people) {
        names << w.inverted(person, true);
    }
    if (names.size() > MOST_NAMED) {
        return names.first() + QLatin1Char(' ') + word(w.words().others);
    }
    return Writer::list(names, word(w.words().andWord));
}

/// "Wiśniewska, A. (2011)"
QString harvardHead(const Writer& w) {
    return joined({harvardNames(w, w.authors()),
                   QLatin1Char('(') + yearOrNoDate(w) + QLatin1Char(')')},
                  QStringLiteral(" "));
}

QString harvardPages(const Writer& w) {
    return labelled(w.words().pages, w.value(SourceDescription::PAGES));
}

QString harvardChapter(const Writer& w) {
    const Words& words = w.words();
    const QString editors =
        enclosed(harvardNames(w, w.people(SourceDescription::EDITOR)), QString(),
                 QStringLiteral(" (") + word(words.editorShort) + QLatin1Char(')'));
    const QString inBook =
        joined({word(words.in), editors, w.italic(w.raw(SourceDescription::CONTAINER))},
               QStringLiteral(" "));
    return sentences({harvardHead(w) + QLatin1Char(' ') + w.quoted(w.title()) +
                          QStringLiteral(", ") + inBook,
                      joined({placeAndPublisher(w), harvardPages(w)}, QStringLiteral(", "))});
}

QString harvardArticle(const Writer& w) {
    const QString journal =
        joined({w.italic(w.raw(SourceDescription::CONTAINER)),
                w.value(SourceDescription::VOLUME) +
                    enclosed(w.value(SourceDescription::ISSUE), QStringLiteral("("),
                             QStringLiteral(")")),
                harvardPages(w)},
               QStringLiteral(", "));
    return sentences({harvardHead(w) + QLatin1Char(' ') + w.quoted(w.title()) + QLatin1Char(','),
                      journal, apaDoiOrUrl(w)});
}

QString harvardWebPage(const Writer& w) {
    const Words& words = w.words();
    const QString accessed =
        enclosed(w.value(SourceDescription::ACCESSED),
                 QStringLiteral(" (") + word(words.accessed) + QStringLiteral(": "),
                 QStringLiteral(")"));
    const QString available = enclosed(w.value(SourceDescription::URL),
                                       word(words.available) + QLatin1Char(' '), accessed);
    return sentences({harvardHead(w) + QLatin1Char(' ') + w.italic(w.title()),
                      w.value(SourceDescription::CONTAINER), available});
}

/// A book: the subtitle after a colon, in small letters
QString harvardBook(const Writer& w) {
    QString title = w.title();
    const QString subtitle = w.raw(SourceDescription::SUBTITLE);
    if (!subtitle.isEmpty()) {
        title += QStringLiteral(": ") + subtitle.at(0).toLower() + subtitle.mid(1);
    }
    return sentences({harvardHead(w) + QLatin1Char(' ') + w.italic(title),
                      w.value(SourceDescription::FORM), editionOf(w), placeAndPublisher(w),
                      apaDoiOrUrl(w)});
}

QString harvardEntry(const Writer& w) {
    return entryOfKind(w,
                       {{SourceDescription::CHAPTER, harvardChapter},
                        {SourceDescription::ARTICLE, harvardArticle},
                        {SourceDescription::WEB_PAGE, harvardWebPage}},
                       harvardBook);
}

}  // namespace

const QStringList& Bibliography::styles() {
    static const QStringList all = {QString::fromLatin1(PN_ISO_690), QString::fromLatin1(APA),
                                    QString::fromLatin1(CHICAGO), QString::fromLatin1(MLA),
                                    QString::fromLatin1(HARVARD)};
    return all;
}

QString Bibliography::styleName(const QString& style) {
    if (style == QLatin1String(PN_ISO_690)) {
        return QStringLiteral("PN-ISO 690");
    }
    if (style == QLatin1String(APA)) {
        return QStringLiteral("APA");
    }
    if (style == QLatin1String(CHICAGO)) {
        return QStringLiteral("Chicago");
    }
    if (style == QLatin1String(MLA)) {
        return QStringLiteral("MLA");
    }
    if (style == QLatin1String(HARVARD)) {
        return QStringLiteral("Harvard");
    }
    return {};
}

QString Bibliography::settingsStyle() {
    const QString style = QString::fromStdString(
        SettingsManager::getInstance().get<std::string>(STYLE_SETTING, PN_ISO_690));
    return styles().contains(style) ? style : QString::fromLatin1(PN_ISO_690);
}

QString Bibliography::knownStyle(const QString& style) {
    return styles().contains(style) ? style : settingsStyle();
}

const QStringList& Bibliography::referenceForms() {
    static const QStringList all = {QString::fromLatin1(AUTHOR_YEAR),
                                    QString::fromLatin1(AUTHOR_TITLE), QString::fromLatin1(NUMBER)};
    return all;
}

QString Bibliography::settingsReferenceForm() {
    const QString form = QString::fromStdString(
        SettingsManager::getInstance().get<std::string>(REFERENCE_SETTING, AUTHOR_YEAR));
    return referenceForms().contains(form) ? form : QString::fromLatin1(AUTHOR_YEAR);
}

QString Bibliography::entry(const SourceDescription& source, const QString& title,
                            const QString& style, const QString& language, bool rich) {
    const Writer writer(source, title, language, rich);
    const QString known = knownStyle(style);
    if (known == QLatin1String(APA)) {
        return apaEntry(writer);
    }
    if (known == QLatin1String(CHICAGO)) {
        return chicagoEntry(writer);
    }
    if (known == QLatin1String(MLA)) {
        return mlaEntry(writer);
    }
    if (known == QLatin1String(HARVARD)) {
        return harvardEntry(writer);
    }
    return pnEntry(writer);
}

QString Bibliography::reference(const SourceDescription& source, const QString& title,
                                const QString& page, const QString& form, int number,
                                const QString& language) {
    const Words& words = wordsFor(language);
    const QString pageText = labelled(words.page, page.trimmed());
    if (form == QLatin1String(NUMBER)) {
        return QLatin1Char('[') +
               joined({QString::number(number), pageText}, QStringLiteral(", ")) +
               QLatin1Char(']');
    }
    const Writer writer(source, title, language, false);
    QStringList families;
    for (const SourcePerson& person : writer.authors()) {
        families << PersonName::of(person.name,
                                   person.role == QLatin1String(SourceDescription::INSTITUTION))
                        .family;
    }
    QString who;
    if (families.size() > 2) {
        who = families.first() + QLatin1Char(' ') + word(words.others);
    } else {
        who = Writer::list(families, word(words.andWord));
    }
    if (form == QLatin1String(AUTHOR_TITLE)) {
        return joined({who, writer.title(), pageText}, QStringLiteral(", "));
    }
    const QString year = source.value(SourceDescription::YEAR);
    const QString named = joined({who.isEmpty() ? writer.title() : who,
                                  year.isEmpty() ? word(words.noDate) : year},
                                 QStringLiteral(" "));
    return QLatin1Char('(') + joined({named, pageText}, QStringLiteral(", ")) + QLatin1Char(')');
}

QString Bibliography::reference(const QuoteSource& quote, const QString& language) {
    return joined({quote.author.simplified(), quote.work.simplified(),
                   labelled(wordsFor(language).page, quote.page.trimmed())},
                  QStringLiteral(", "));
}

}  // namespace kalahari::core
