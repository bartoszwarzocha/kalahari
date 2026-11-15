// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file bibliography.cpp
/// @brief The entries of the sources in the styles of a bibliography, the references in the
/// text and the reading of BibTeX and RIS

#include "kalahari/core/bibliography.h"

#include "kalahari/core/settings_manager.h"

#include <QRegularExpression>

#include <functional>
#include <string>

namespace kalahari::core {

namespace {

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
    while (!end.isEmpty() && (end.back() == QChar(0x201D) || end.back() == QLatin1Char('"') ||
                              end.back() == QLatin1Char('\''))) {
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
        : m_source(source), m_title(title.trimmed()), m_words(wordsFor(language)), m_rich(rich) {}

    [[nodiscard]] const Words& words() const { return m_words; }
    [[nodiscard]] const SourceDescription& source() const { return m_source; }

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
        return word(m_words.openQuote) + text(plain) + word(m_words.closeQuote);
    }

    /// The value of @p field as the writer typed it
    [[nodiscard]] QString raw(const char* field) const { return m_source.value(field); }

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
        return m_source.peopleOf(role);
    }

    /// The authors: the people who wrote it, the ones asked in a talk, the institutions
    [[nodiscard]] QList<SourcePerson> authors() const {
        QList<SourcePerson> found = people(SourceDescription::AUTHOR);
        if (m_source.kind() == QLatin1String(SourceDescription::INTERVIEW)) {
            found << people(SourceDescription::INTERVIEWEE);
        }
        if (found.isEmpty() && m_source.kind() != QLatin1String(SourceDescription::WEB_PAGE)) {
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

    const SourceDescription& m_source;
    QString m_title;
    const Words& m_words;
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
// PN-ISO 690
// ---------------------------------------------------------------------------------------------

QString pnNames(const Writer& w, const QList<SourcePerson>& people) {
    QStringList names;
    for (const SourcePerson& person : people) {
        names << w.inverted(person, false, true);
    }
    if (names.size() > 3) {
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
    if (names.isEmpty()) {
        return {};
    }
    return word(before) + QLatin1Char(' ') + names.join(QStringLiteral(", "));
}

QString pnEntry(const Writer& w) {
    const Words& words = w.words();
    const QString authors = pnNames(w, w.authors());
    const QString year = w.value(SourceDescription::YEAR);
    const QString edition = w.text(editionText(w.raw(SourceDescription::EDITION), words));
    const QString translators = pnPeopleAfter(w, SourceDescription::TRANSLATOR, words.translated);
    const QString pages = w.value(SourceDescription::PAGES);
    const QString form = w.value(SourceDescription::FORM);
    // The title in italics with its form in brackets: [rękopis]
    const QString titled = form.isEmpty()
                               ? w.italic(w.title())
                               : w.italic(w.title()) + QStringLiteral(" [") + form +
                                     QLatin1Char(']');

    if (isKind(w, SourceDescription::CHAPTER)) {
        const QString in = joined({word(words.in), pnEditors(w)}, QStringLiteral(" "));
        const QString published =
            joined({placeAndPublisher(w), year,
                    pages.isEmpty() ? QString() : word(words.pages) + QLatin1Char(' ') + pages},
                   QStringLiteral(", "));
        return sentences({authors, w.text(w.title()), in,
                          w.italic(w.raw(SourceDescription::CONTAINER)), edition, published,
                          w.value(SourceDescription::ISBN).isEmpty()
                              ? QString()
                              : QStringLiteral("ISBN ") + w.value(SourceDescription::ISBN)});
    }
    if (isKind(w, SourceDescription::ARTICLE)) {
        const QString volume = w.value(SourceDescription::VOLUME);
        const QString issue = w.value(SourceDescription::ISSUE);
        const QString where = joined(
            {year, volume.isEmpty() ? QString() : word(words.volume) + QLatin1Char(' ') + volume,
             issue.isEmpty() ? QString() : word(words.issue) + QLatin1Char(' ') + issue,
             pages.isEmpty() ? QString() : word(words.pages) + QLatin1Char(' ') + pages},
            QStringLiteral(", "));
        const QString issn = w.value(SourceDescription::ISSN);
        return sentences({authors, w.text(w.title()), w.italic(w.raw(SourceDescription::CONTAINER)),
                          where, issn.isEmpty() ? QString() : QStringLiteral("ISSN ") + issn,
                          pnLocation(w)});
    }
    if (isKind(w, SourceDescription::WEB_PAGE)) {
        QStringList owners;
        for (const SourcePerson& person : w.people(SourceDescription::INSTITUTION)) {
            owners << w.text(person.name.simplified());
        }
        const QString site = owners.isEmpty() ? w.value(SourceDescription::CONTAINER)
                                              : owners.join(QStringLiteral(", "));
        const QString accessed = w.value(SourceDescription::ACCESSED);
        const QString when =
            accessed.isEmpty()
                ? QString()
                : QLatin1Char('[') + word(words.accessed) + QLatin1Char(' ') + accessed +
                      QLatin1Char(']');
        const QString publishedIn =
            joined({joined({site, w.value(SourceDescription::PUBLISHED)}, QStringLiteral(", ")),
                    when},
                   QStringLiteral(" "));
        return sentences({authors, w.italic(w.title()) + QLatin1Char(' ') + word(words.online),
                          publishedIn, pnLocation(w)});
    }
    if (isKind(w, SourceDescription::ARCHIVE)) {
        const QString fonds = w.value(SourceDescription::FONDS);
        const QString signature = w.value(SourceDescription::SIGNATURE);
        const QString where = joined(
            {w.value(SourceDescription::CONTAINER),
             fonds.isEmpty() ? QString() : word(words.fonds) + QLatin1Char(' ') + fonds,
             signature.isEmpty() ? QString()
                                 : word(words.signature) + QLatin1Char(' ') + signature},
            QStringLiteral(", "));
        return sentences({authors, titled, year, where, pnLocation(w)});
    }
    if (isKind(w, SourceDescription::INTERVIEW) || isKind(w, SourceDescription::FILM) ||
        isKind(w, SourceDescription::OTHER)) {
        const QString asked =
            pnPeopleAfter(w, SourceDescription::INTERVIEWER, words.interviewed);
        const QString directed = pnPeopleAfter(w, SourceDescription::DIRECTOR, words.directed);
        return sentences({authors, titled, asked, directed,
                          joined({placeAndPublisher(w), year}, QStringLiteral(", ")),
                          pnLocation(w)});
    }
    if (isKind(w, SourceDescription::THESIS)) {
        return sentences({authors, w.italic(w.fullTitle()), form,
                          joined({placeAndPublisher(w), year}, QStringLiteral(", ")),
                          pnPeopleAfter(w, SourceDescription::SUPERVISOR, words.supervised),
                          pnLocation(w)});
    }
    // A book; the editors stand for the authors of a book without them
    const QString series = w.value(SourceDescription::SERIES);
    const QString isbn = w.value(SourceDescription::ISBN);
    return sentences({authors.isEmpty() ? pnEditors(w) : authors, w.italic(w.fullTitle()),
                      translators, edition,
                      joined({placeAndPublisher(w), year}, QStringLiteral(", ")),
                      series.isEmpty() ? QString() : QLatin1Char('(') + series + QLatin1Char(')'),
                      isbn.isEmpty() ? QString() : QStringLiteral("ISBN ") + isbn,
                      pnLocation(w)});
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

QString apaEntry(const Writer& w) {
    const Words& words = w.words();
    QString authors = apaNames(w, w.authors());
    const QList<SourcePerson> editors = w.people(SourceDescription::EDITOR);
    const bool chapter = isKind(w, SourceDescription::CHAPTER);
    if (authors.isEmpty() && !editors.isEmpty() && !chapter) {
        authors = apaNames(w, editors) + QStringLiteral(" (") + word(words.editorWord) +
                  QStringLiteral(".)");
    }
    const QString year = w.value(SourceDescription::YEAR);
    const QString date = QLatin1Char('(') + (year.isEmpty() ? word(words.noDate) : year) +
                         QLatin1Char(')');
    const QString head = joined({authors.isEmpty() ? QString() : dotted(authors), dotted(date)},
                                QStringLiteral(" "));
    const QString edition = w.raw(SourceDescription::EDITION);
    const QString editionPart =
        edition.isEmpty()
            ? QString()
            : QStringLiteral(" (") + w.text(editionText(edition, words, true)) + QLatin1Char(')');
    const QString pages = w.value(SourceDescription::PAGES);
    const QString form = w.value(SourceDescription::FORM);
    const QString formPart = form.isEmpty() ? QString()
                                            : QStringLiteral(" [") + form + QLatin1Char(']');

    if (chapter) {
        QStringList names;
        for (const SourcePerson& person : editors) {
            names << w.direct(person, true);
        }
        const QString by = names.isEmpty()
                               ? QString()
                               : Writer::list(names, word(words.andWord)) + QStringLiteral(" (") +
                                     word(words.editorWord) + QStringLiteral(".), ");
        const QString inBook = word(words.in) + QLatin1Char(' ') + by +
                               w.italic(w.raw(SourceDescription::CONTAINER)) +
                               (pages.isEmpty() ? QString()
                                                : QStringLiteral(" (") + word(words.pages) +
                                                      QLatin1Char(' ') + pages + QLatin1Char(')'));
        return joined({head, sentences({w.text(w.title()), inBook,
                                        w.value(SourceDescription::PUBLISHER)}),
                       apaDoiOrUrl(w)},
                      QStringLiteral(" "));
    }
    if (isKind(w, SourceDescription::ARTICLE)) {
        const QString volume = w.raw(SourceDescription::VOLUME);
        const QString issue = w.value(SourceDescription::ISSUE);
        const QString journal = joined(
            {w.italic(w.raw(SourceDescription::CONTAINER)),
             w.italic(volume) +
                 (issue.isEmpty() ? QString() : QLatin1Char('(') + issue + QLatin1Char(')')),
             pages},
            QStringLiteral(", "));
        return joined({head, sentences({w.text(w.title()), journal}), apaDoiOrUrl(w)},
                      QStringLiteral(" "));
    }
    if (isKind(w, SourceDescription::WEB_PAGE)) {
        QStringList owners;
        for (const SourcePerson& person : w.people(SourceDescription::INSTITUTION)) {
            owners << w.text(person.name.simplified());
        }
        const QString site = owners.isEmpty() ? w.value(SourceDescription::CONTAINER)
                                              : owners.join(QStringLiteral(", "));
        const QString published = w.value(SourceDescription::PUBLISHED);
        const QString when = QLatin1Char('(') +
                             (published.isEmpty() ? (year.isEmpty() ? word(words.noDate) : year)
                                                  : published) +
                             QLatin1Char(')');
        const QString webHead = joined(
            {authors.isEmpty() ? QString() : dotted(authors), dotted(when)}, QStringLiteral(" "));
        return joined({webHead, sentences({w.italic(w.title()), site}),
                       w.value(SourceDescription::URL)},
                      QStringLiteral(" "));
    }
    if (isKind(w, SourceDescription::ARCHIVE)) {
        const QString where = joined({w.value(SourceDescription::FONDS),
                                      w.value(SourceDescription::SIGNATURE)},
                                     QStringLiteral(", "));
        return joined(
            {head,
             sentences({w.italic(w.title()) + formPart,
                        joined({w.value(SourceDescription::CONTAINER),
                                where.isEmpty() ? QString()
                                                : QLatin1Char('(') + where + QLatin1Char(')')},
                               QStringLiteral(" "))}),
             apaDoiOrUrl(w)},
            QStringLiteral(" "));
    }
    return joined({head, sentences({w.italic(w.fullTitle(true)) + editionPart + formPart,
                                    w.value(SourceDescription::PUBLISHER)}),
                   apaDoiOrUrl(w)},
                  QStringLiteral(" "));
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
    if (names.size() > 3) {
        return names.first() + QLatin1Char(' ') + word(w.words().others);
    }
    return Writer::list(names, word(w.words().andWord));
}

QString harvardEntry(const Writer& w) {
    const Words& words = w.words();
    const QString year = w.value(SourceDescription::YEAR);
    const QString authors = harvardNames(w, w.authors());
    const QString head = joined({authors, QLatin1Char('(') +
                                              (year.isEmpty() ? word(words.noDate) : year) +
                                              QLatin1Char(')')},
                                QStringLiteral(" "));
    const QString pages = w.value(SourceDescription::PAGES);
    const QString pagesText =
        pages.isEmpty() ? QString() : word(words.pages) + QLatin1Char(' ') + pages;
    if (isKind(w, SourceDescription::CHAPTER)) {
        const QString editors = harvardNames(w, w.people(SourceDescription::EDITOR));
        const QString inBook = joined(
            {word(words.in), editors.isEmpty() ? QString()
                                               : editors + QStringLiteral(" (") +
                                                     word(words.editorShort) + QLatin1Char(')'),
             w.italic(w.raw(SourceDescription::CONTAINER))},
            QStringLiteral(" "));
        return sentences({head + QLatin1Char(' ') + w.quoted(w.title()) + QLatin1Char(',') +
                              QLatin1Char(' ') + inBook,
                          joined({placeAndPublisher(w), pagesText}, QStringLiteral(", "))});
    }
    if (isKind(w, SourceDescription::ARTICLE)) {
        const QString volume = w.value(SourceDescription::VOLUME);
        const QString issue = w.value(SourceDescription::ISSUE);
        const QString journal = joined(
            {w.italic(w.raw(SourceDescription::CONTAINER)),
             volume + (issue.isEmpty() ? QString() : QLatin1Char('(') + issue + QLatin1Char(')')),
             pagesText},
            QStringLiteral(", "));
        return sentences({head + QLatin1Char(' ') + w.quoted(w.title()) + QLatin1Char(','),
                          journal, apaDoiOrUrl(w)});
    }
    if (isKind(w, SourceDescription::WEB_PAGE)) {
        const QString url = w.value(SourceDescription::URL);
        const QString accessed = w.value(SourceDescription::ACCESSED);
        return sentences(
            {head + QLatin1Char(' ') + w.italic(w.title()), w.value(SourceDescription::CONTAINER),
             url.isEmpty() ? QString()
                           : word(words.available) + QLatin1Char(' ') + url +
                                 (accessed.isEmpty()
                                      ? QString()
                                      : QStringLiteral(" (") + word(words.accessed) +
                                            QStringLiteral(": ") + accessed + QLatin1Char(')'))});
    }
    QString title = w.title();
    const QString subtitle = w.raw(SourceDescription::SUBTITLE);
    if (!subtitle.isEmpty()) {
        title += QStringLiteral(": ") + subtitle.at(0).toLower() + subtitle.mid(1);
    }
    return sentences({head + QLatin1Char(' ') + w.italic(title),
                      w.value(SourceDescription::FORM),
                      w.text(editionText(w.raw(SourceDescription::EDITION), words)),
                      placeAndPublisher(w), apaDoiOrUrl(w)});
}

// ---------------------------------------------------------------------------------------------
// BibTeX and RIS
// ---------------------------------------------------------------------------------------------

/// The people of a BibTeX field: "Wiśniewska, Anna and Nowak, Jan"
QList<SourcePerson> bibtexPeople(const QString& names, const char* role) {
    QList<SourcePerson> people;
    static const QRegularExpression separator(QStringLiteral("\\s+and\\s+"),
                                              QRegularExpression::CaseInsensitiveOption);
    for (const QString& name : names.split(separator, Qt::SkipEmptyParts)) {
        SourcePerson person;
        person.name = name.simplified();
        if (qstrcmp(role, SourceDescription::AUTHOR) != 0) {
            person.role = QString::fromLatin1(role);
        }
        people << person;
    }
    return people;
}

/// @p text with the accents of LaTeX written as letters: "Zieli{\\'n}ska" is "Zielińska"
QString withoutLatexAccents(QString text) {
    static const QList<std::pair<QRegularExpression, QString>> letters = {
        {QRegularExpression(QStringLiteral("\\\\l(?![A-Za-z])\\s?")), QStringLiteral("\u0142")},
        {QRegularExpression(QStringLiteral("\\\\L(?![A-Za-z])\\s?")), QStringLiteral("\u0141")},
        {QRegularExpression(QStringLiteral("\\\\ss(?![A-Za-z])\\s?")), QStringLiteral("\u00DF")},
        {QRegularExpression(QStringLiteral("\\\\o(?![A-Za-z])\\s?")), QStringLiteral("\u00F8")},
        {QRegularExpression(QStringLiteral("\\\\O(?![A-Za-z])\\s?")), QStringLiteral("\u00D8")}};
    for (const auto& [pattern, letter] : letters) {
        text.replace(pattern, letter);
    }
    // An accent over a letter: \'n, \'{n}, {\'n}, \k{a}, \c c
    static const QRegularExpression accent(
        QStringLiteral("\\\\(['`^\"~.=]|[kcuvH](?=[\\s{]))\\s*\\{?([A-Za-z])\\}?"));
    static const QMap<QString, QChar> marks = {
        {QStringLiteral("'"), QChar(0x0301)}, {QStringLiteral("`"), QChar(0x0300)},
        {QStringLiteral("^"), QChar(0x0302)}, {QStringLiteral("\""), QChar(0x0308)},
        {QStringLiteral("~"), QChar(0x0303)}, {QStringLiteral("."), QChar(0x0307)},
        {QStringLiteral("="), QChar(0x0304)}, {QStringLiteral("k"), QChar(0x0328)},
        {QStringLiteral("c"), QChar(0x0327)}, {QStringLiteral("u"), QChar(0x0306)},
        {QStringLiteral("v"), QChar(0x030C)}, {QStringLiteral("H"), QChar(0x030B)}};
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

/// The value of a BibTeX field without its braces and quotes
QString bibtexValue(QString value) {
    value = value.trimmed();
    if (value.endsWith(QLatin1Char(','))) {
        value.chop(1);
        value = value.trimmed();
    }
    if ((value.startsWith(QLatin1Char('{')) && value.endsWith(QLatin1Char('}'))) ||
        (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))) {
        value = value.mid(1, value.size() - 2);
    }
    value = withoutLatexAccents(value);
    value.remove(QLatin1Char('{'));
    value.remove(QLatin1Char('}'));
    value.replace(QStringLiteral("--"), QStringLiteral("–"));
    return value.simplified();
}

/// The fields of a BibTeX record: name -> value, the names in small letters
QMap<QString, QString> bibtexFields(const QString& body) {
    QMap<QString, QString> fields;
    qsizetype i = 0;
    const qsizetype size = body.size();
    while (i < size) {
        // The name, up to "="
        const qsizetype equals = body.indexOf(QLatin1Char('='), i);
        if (equals < 0) {
            break;
        }
        const QString name = body.mid(i, equals - i).trimmed().toLower();
        qsizetype j = equals + 1;
        while (j < size && body.at(j).isSpace()) {
            ++j;
        }
        // The value: in braces (nested), in quotes, or a word up to the comma
        const qsizetype end = [&body, j, size]() -> qsizetype {
            if (j < size && body.at(j) == QLatin1Char('{')) {
                int depth = 0;
                for (qsizetype at = j; at < size; ++at) {
                    if (body.at(at) == QLatin1Char('{')) {
                        ++depth;
                    } else if (body.at(at) == QLatin1Char('}') && --depth == 0) {
                        return at + 1;
                    }
                }
                return size;
            }
            if (j < size && body.at(j) == QLatin1Char('"')) {
                const qsizetype quote = body.indexOf(QLatin1Char('"'), j + 1);
                return quote < 0 ? size : quote + 1;
            }
            const qsizetype comma = body.indexOf(QLatin1Char(','), j);
            return comma < 0 ? size : comma;
        }();
        fields.insert(name.section(QLatin1Char(','), -1).trimmed(),
                      bibtexValue(body.mid(j, end - j)));
        const qsizetype comma = body.indexOf(QLatin1Char(','), end);
        if (comma < 0) {
            break;
        }
        i = comma + 1;
    }
    return fields;
}

std::optional<SourceDescription> parseBibtex(const QString& text) {
    static const QRegularExpression head(QStringLiteral("@\\s*([A-Za-z]+)\\s*[{(]\\s*[^,]*,"));
    const QRegularExpressionMatch match = head.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    const QString type = match.captured(1).toLower();
    qsizetype end = text.lastIndexOf(QLatin1Char('}'));
    if (end <= match.capturedEnd()) {
        end = text.size();
    }
    const QMap<QString, QString> fields =
        bibtexFields(text.mid(match.capturedEnd(), end - match.capturedEnd()));

    SourceDescription source;
    if (type == QLatin1String("article")) {
        source.type = QString::fromLatin1(SourceDescription::ARTICLE);
    } else if (type == QLatin1String("incollection") || type == QLatin1String("inbook") ||
               type == QLatin1String("inproceedings")) {
        source.type = QString::fromLatin1(SourceDescription::CHAPTER);
    } else if (type.endsWith(QLatin1String("thesis"))) {
        source.type = QString::fromLatin1(SourceDescription::THESIS);
    } else if (type == QLatin1String("online") || type == QLatin1String("electronic") ||
               (type == QLatin1String("misc") && fields.contains(QStringLiteral("url")))) {
        source.type = QString::fromLatin1(SourceDescription::WEB_PAGE);
    } else if (type == QLatin1String("book")) {
        source.type = QString::fromLatin1(SourceDescription::BOOK);
    } else {
        source.type = QString::fromLatin1(SourceDescription::OTHER);
    }
    source.people << bibtexPeople(fields.value(QStringLiteral("author")), SourceDescription::AUTHOR)
                  << bibtexPeople(fields.value(QStringLiteral("editor")), SourceDescription::EDITOR)
                  << bibtexPeople(fields.value(QStringLiteral("translator")),
                                  SourceDescription::TRANSLATOR);
    const auto take = [&fields, &source](const char* from, const char* to) {
        const QString value = fields.value(QString::fromLatin1(from));
        if (!value.isEmpty() && source.value(to).isEmpty()) {
            source.setValue(QString::fromLatin1(to), value);
        }
    };
    take("title", SourceDescription::TITLE);
    take("subtitle", SourceDescription::SUBTITLE);
    take("booktitle", SourceDescription::CONTAINER);
    take("journal", SourceDescription::CONTAINER);
    take("journaltitle", SourceDescription::CONTAINER);
    take("edition", SourceDescription::EDITION);
    take("address", SourceDescription::PLACE);
    take("location", SourceDescription::PLACE);
    take("publisher", SourceDescription::PUBLISHER);
    take("school", SourceDescription::PUBLISHER);
    take("institution", SourceDescription::PUBLISHER);
    take("year", SourceDescription::YEAR);
    take("date", SourceDescription::YEAR);
    take("volume", SourceDescription::VOLUME);
    take("number", SourceDescription::ISSUE);
    take("pages", SourceDescription::PAGES);
    take("series", SourceDescription::SERIES);
    take("isbn", SourceDescription::ISBN);
    take("issn", SourceDescription::ISSN);
    take("doi", SourceDescription::DOI);
    take("url", SourceDescription::URL);
    take("urldate", SourceDescription::ACCESSED);
    take("howpublished", SourceDescription::FORM);
    take("language", SourceDescription::LANGUAGE);
    if (source.type == QLatin1String(SourceDescription::THESIS) &&
        source.value(SourceDescription::FORM).isEmpty()) {
        take("type", SourceDescription::FORM);
    }
    if (source.people.isEmpty() && source.values.isEmpty()) {
        return std::nullopt;
    }
    return source;
}

std::optional<SourceDescription> parseRis(const QString& text) {
    static const QRegularExpression line(QStringLiteral("^([A-Z][A-Z0-9])  -(?: (.*))?$"));
    SourceDescription source;
    QString type;
    QString startPage;
    QString endPage;
    QString number;
    bool started = false;
    for (const QString& each : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = line.match(each.trimmed());
        if (!match.hasMatch()) {
            continue;
        }
        const QString tag = match.captured(1);
        const QString value = match.captured(2).trimmed();
        if (tag == QLatin1String("TY")) {
            if (started) {
                break;
            }
            started = true;
            type = value.toUpper();
            continue;
        }
        if (!started) {
            continue;
        }
        if (tag == QLatin1String("ER")) {
            break;
        }
        if (value.isEmpty()) {
            continue;
        }
        SourcePerson person;
        person.name = value.simplified();
        if (tag == QLatin1String("AU") || tag == QLatin1String("A1")) {
            source.people << person;
        } else if (tag == QLatin1String("ED") || tag == QLatin1String("A2") ||
                   tag == QLatin1String("A3")) {
            person.role = QString::fromLatin1(SourceDescription::EDITOR);
            source.people << person;
        } else if (tag == QLatin1String("A4")) {
            person.role = QString::fromLatin1(SourceDescription::TRANSLATOR);
            source.people << person;
        } else if (tag == QLatin1String("SP")) {
            startPage = value;
        } else if (tag == QLatin1String("EP")) {
            endPage = value;
        } else if (tag == QLatin1String("SN")) {
            number = value;
        } else {
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
            if (field != fields.cend() && source.value(field.value()).isEmpty()) {
                QString shown = value;
                // "2011/05/01/" is the year 2011
                if (tag == QLatin1String("PY") || tag == QLatin1String("Y1")) {
                    shown = value.section(QLatin1Char('/'), 0, 0);
                }
                source.setValue(QString::fromLatin1(field.value()), shown);
            }
        }
    }
    if (!started) {
        return std::nullopt;
    }
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
    source.type = QString::fromLatin1(kinds.value(type, SourceDescription::OTHER));
    if (!startPage.isEmpty()) {
        source.setValue(QString::fromLatin1(SourceDescription::PAGES),
                        endPage.isEmpty() ? startPage : startPage + QStringLiteral("–") + endPage);
    }
    if (!number.isEmpty()) {
        const bool serial = source.type == QLatin1String(SourceDescription::ARTICLE);
        source.setValue(QString::fromLatin1(serial ? SourceDescription::ISSN
                                                   : SourceDescription::ISBN),
                        number);
    }
    if (source.people.isEmpty() && source.values.isEmpty()) {
        return std::nullopt;
    }
    return source;
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
    const QString pageText = page.trimmed().isEmpty()
                                 ? QString()
                                 : word(words.page) + QLatin1Char(' ') + page.trimmed();
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
    const Words& words = wordsFor(language);
    const QString page = quote.page.trimmed();
    return joined({quote.author.simplified(), quote.work.simplified(),
                   page.isEmpty() ? QString() : word(words.page) + QLatin1Char(' ') + page},
                  QStringLiteral(", "));
}

std::optional<SourceDescription> Bibliography::parse(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1Char('@')) || trimmed.contains(QRegularExpression(
                                                     QStringLiteral("^\\s*@\\s*[A-Za-z]+\\s*[{(]"),
                                                     QRegularExpression::MultilineOption))) {
        return parseBibtex(trimmed);
    }
    return parseRis(text);
}

}  // namespace kalahari::core
