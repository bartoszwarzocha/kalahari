// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_names.cpp
/// @brief The sources in the program's words

#include "kalahari/gui/source_names.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/editor/auto_correct.h"
#include "kalahari/gui/kind_names.h"

#include <QHash>
#include <QLatin1String>

#include <algorithm>
#include <iterator>

namespace kalahari::gui {

using core::SourceDescription;

QString SourceNames::baseKindOf(const core::ProjectManager& book,
                                const core::ProjectElement& element) {
    const core::KindRef kind = book.kindOf(element);
    return kind && kind.package != nullptr &&
                   kind.package->id == QLatin1String(core::ProjectManager::BASE_PACKAGE)
               ? kind.kind->id
               : QString();
}

bool SourceNames::canDescribe(const core::ProjectManager& book,
                              const core::ProjectElement& element) {
    return SourceDescription::canDescribe(baseKindOf(book, element));
}

bool SourceNames::isQuote(const core::ProjectManager& book, const core::ProjectElement& element) {
    return baseKindOf(book, element) == QLatin1String(core::MaterialKinds::QUOTE);
}

QString SourceNames::kindName(const QString& kind) {
    if (kind == QLatin1String(SourceDescription::BOOK)) {
        //: A kind of source. In Polish: Książka
        return tr("Book");
    }
    if (kind == QLatin1String(SourceDescription::CHAPTER)) {
        //: A kind of source. In Polish: Rozdział w książce
        return tr("Chapter in a book");
    }
    if (kind == QLatin1String(SourceDescription::ARTICLE)) {
        //: A kind of source. In Polish: Artykuł w czasopiśmie
        return tr("Article in a journal");
    }
    if (kind == QLatin1String(SourceDescription::WEB_PAGE)) {
        //: A kind of source. In Polish: Strona internetowa
        return tr("Web page");
    }
    if (kind == QLatin1String(SourceDescription::ARCHIVE)) {
        //: A kind of source: a letter, a diary, a record in an archive. In Polish: Dokument
        //: archiwalny
        return tr("Archival document");
    }
    if (kind == QLatin1String(SourceDescription::INTERVIEW)) {
        //: A kind of source. In Polish: Rozmowa lub wywiad
        return tr("Talk or interview");
    }
    if (kind == QLatin1String(SourceDescription::FILM)) {
        //: A kind of source. In Polish: Film lub nagranie
        return tr("Film or recording");
    }
    if (kind == QLatin1String(SourceDescription::THESIS)) {
        //: A kind of source. In Polish: Praca dyplomowa
        return tr("Thesis");
    }
    if (kind == QLatin1String(SourceDescription::OTHER)) {
        //: A kind of source. In Polish: Inne
        return tr("Other");
    }
    return kind;
}

QString SourceNames::roleName(const QString& role) {
    if (role.isEmpty() || role == QLatin1String(SourceDescription::AUTHOR)) {
        //: What a person of a source did. In Polish: autor
        return tr("author");
    }
    if (role == QLatin1String(SourceDescription::EDITOR)) {
        //: What a person of a source did. In Polish: redakcja
        return tr("editor");
    }
    if (role == QLatin1String(SourceDescription::TRANSLATOR)) {
        //: What a person of a source did. In Polish: tłumaczenie
        return tr("translator");
    }
    if (role == QLatin1String(SourceDescription::INTERVIEWEE)) {
        //: The one asked in a talk or an interview. In Polish: rozmówca
        return tr("interviewee");
    }
    if (role == QLatin1String(SourceDescription::INTERVIEWER)) {
        //: The one who asks in a talk or an interview. In Polish: prowadzący
        return tr("interviewer");
    }
    if (role == QLatin1String(SourceDescription::DIRECTOR)) {
        //: What a person of a film did. In Polish: reżyseria
        return tr("director");
    }
    if (role == QLatin1String(SourceDescription::SUPERVISOR)) {
        //: The supervisor of a thesis. In Polish: promotor
        return tr("supervisor");
    }
    if (role == QLatin1String(SourceDescription::INSTITUTION)) {
        //: The author of a source is an institution. In Polish: instytucja
        return tr("institution");
    }
    return role;
}

namespace {

bool isKindOf(const QString& kind, const char* each) {
    return kind == QLatin1String(each);
}

QString titleLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::CHAPTER)) {
        //: In Polish: Tytuł rozdziału:
        return SourceNames::tr("Chapter title:");
    }
    if (isKindOf(kind, SourceDescription::ARTICLE)) {
        //: In Polish: Tytuł artykułu:
        return SourceNames::tr("Article title:");
    }
    if (isKindOf(kind, SourceDescription::WEB_PAGE)) {
        //: In Polish: Tytuł strony:
        return SourceNames::tr("Page title:");
    }
    if (isKindOf(kind, SourceDescription::ARCHIVE)) {
        //: In Polish: Tytuł lub opis:
        return SourceNames::tr("Title or description:");
    }
    //: The title of a source. In Polish: Tytuł:
    return SourceNames::tr("Title:");
}

QString containerLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::CHAPTER)) {
        //: The book of a chapter. In Polish: Tytuł książki:
        return SourceNames::tr("Book title:");
    }
    if (isKindOf(kind, SourceDescription::ARTICLE)) {
        //: In Polish: Czasopismo:
        return SourceNames::tr("Journal:");
    }
    if (isKindOf(kind, SourceDescription::WEB_PAGE)) {
        //: The web site of a page. In Polish: Witryna:
        return SourceNames::tr("Site:");
    }
    //: The archive of a document. In Polish: Archiwum:
    return SourceNames::tr("Archive:");
}

QString placeLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::INTERVIEW) || isKindOf(kind, SourceDescription::FILM)) {
        //: Where a talk took place, where a film was made. In Polish: Miejsce:
        return SourceNames::tr("Place:");
    }
    //: In Polish: Miejsce wydania:
    return SourceNames::tr("Place of publication:");
}

QString publisherLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::THESIS)) {
        //: In Polish: Uczelnia:
        return SourceNames::tr("University:");
    }
    if (isKindOf(kind, SourceDescription::FILM)) {
        //: Who made a film or a recording. In Polish: Wytwórnia:
        return SourceNames::tr("Producer:");
    }
    //: In Polish: Wydawnictwo:
    return SourceNames::tr("Publisher:");
}

QString yearLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::ARCHIVE) || isKindOf(kind, SourceDescription::INTERVIEW)) {
        //: In Polish: Data:
        return SourceNames::tr("Date:");
    }
    //: The year of a source. In Polish: Rok:
    return SourceNames::tr("Year:");
}

QString pagesLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::CHAPTER) || isKindOf(kind, SourceDescription::ARTICLE)) {
        //: The pages of a chapter or an article: 41–58. In Polish: Strony:
        return SourceNames::tr("Pages:");
    }
    //: How many pages a book has. In Polish: Liczba stron:
    return SourceNames::tr("Number of pages:");
}

QString formLabel(const QString& kind) {
    if (isKindOf(kind, SourceDescription::THESIS)) {
        //: Bachelor's, master's, doctoral. In Polish: Rodzaj pracy:
        return SourceNames::tr("Kind of thesis:");
    }
    //: What a source is: a manuscript, a sound recording of 48 minutes. In Polish: Postać:
    return SourceNames::tr("Form:");
}

/// The labels of the fields whose label is the same for every kind of source
QHash<QString, QString> sameLabels() {
    QHash<QString, QString> labels;
    //: In Polish: Podtytuł:
    labels.insert(QLatin1String(SourceDescription::SUBTITLE), SourceNames::tr("Subtitle:"));
    //: In Polish: Wydanie:
    labels.insert(QLatin1String(SourceDescription::EDITION), SourceNames::tr("Edition:"));
    //: The volume of a journal. In Polish: Tom:
    labels.insert(QLatin1String(SourceDescription::VOLUME), SourceNames::tr("Volume:"));
    //: The issue of a journal. In Polish: Numer:
    labels.insert(QLatin1String(SourceDescription::ISSUE), SourceNames::tr("Issue:"));
    //: In Polish: Seria:
    labels.insert(QLatin1String(SourceDescription::SERIES), SourceNames::tr("Series:"));
    labels.insert(QLatin1String(SourceDescription::ISBN), QStringLiteral("ISBN:"));
    labels.insert(QLatin1String(SourceDescription::ISSN), QStringLiteral("ISSN:"));
    labels.insert(QLatin1String(SourceDescription::DOI), QStringLiteral("DOI:"));
    //: The web address of a source. In Polish: Adres:
    labels.insert(QLatin1String(SourceDescription::URL), SourceNames::tr("Address:"));
    //: When a web page was published. In Polish: Opublikowano:
    labels.insert(QLatin1String(SourceDescription::PUBLISHED), SourceNames::tr("Published:"));
    //: When the writer read a web page. In Polish: Dostęp:
    labels.insert(QLatin1String(SourceDescription::ACCESSED), SourceNames::tr("Accessed:"));
    //: The fonds of an archive. In Polish: Zespół:
    labels.insert(QLatin1String(SourceDescription::FONDS), SourceNames::tr("Fonds:"));
    //: The call number of a document in an archive. In Polish: Sygnatura:
    labels.insert(QLatin1String(SourceDescription::SIGNATURE), SourceNames::tr("Call number:"));
    //: In Polish: Język:
    labels.insert(QLatin1String(SourceDescription::LANGUAGE), SourceNames::tr("Language:"));
    return labels;
}

}  // namespace

QString SourceNames::fieldLabel(const QString& field, const QString& kind) {
    using KindLabel = QString (*)(const QString&);
    static const QHash<QString, KindLabel> byKind = {
        {QLatin1String(SourceDescription::TITLE), titleLabel},
        {QLatin1String(SourceDescription::CONTAINER), containerLabel},
        {QLatin1String(SourceDescription::PLACE), placeLabel},
        {QLatin1String(SourceDescription::PUBLISHER), publisherLabel},
        {QLatin1String(SourceDescription::YEAR), yearLabel},
        {QLatin1String(SourceDescription::PAGES), pagesLabel},
        {QLatin1String(SourceDescription::FORM), formLabel}};
    if (const auto label = byKind.constFind(field); label != byKind.cend()) {
        return label.value()(kind);
    }
    return sameLabels().value(field, field + QLatin1Char(':'));
}

QString SourceNames::inclusionName(const QString& inclusion) {
    if (inclusion == QLatin1String(SourceDescription::ALWAYS)) {
        //: A source is in the bibliography of the book always. In Polish: Zawsze
        return tr("Always");
    }
    if (inclusion == QLatin1String(SourceDescription::NEVER)) {
        //: A source is in the bibliography of the book never. In Polish: Nigdy
        return tr("Never");
    }
    //: A source is in the bibliography of the book when it or a quote of it is linked with the
    //: text. In Polish: Gdy jest powiązane z tekstem
    return tr("When linked with the text");
}

QString SourceNames::referenceExample(const QString& form) {
    SourceDescription example;
    example.people << core::SourcePerson{
        .name = QStringLiteral("Anna Wiśniewska"), .role = {}, .extra = {}};
    example.setValue(QString::fromLatin1(SourceDescription::TITLE),
                     QStringLiteral("Atlas pustyń świata"));
    example.setValue(QString::fromLatin1(SourceDescription::YEAR), QStringLiteral("2011"));
    return core::Bibliography::reference(example, QString(), QStringLiteral("45"), form, 1,
                                         programLanguage());
}

QString SourceNames::shortName(const SourceDescription& source, const QString& title) {
    const QString own = source.value(SourceDescription::TITLE);
    const QString shown = own.isEmpty() ? title : own;
    QStringList details;
    const QList<core::SourcePerson> authors = source.peopleOf(SourceDescription::AUTHOR);
    if (!authors.isEmpty()) {
        details << core::PersonName::of(authors.first().name).family;
    }
    if (const QString year = source.value(SourceDescription::YEAR); !year.isEmpty()) {
        details << year;
    }
    //: A source in a list to choose from: %1 its title, %2 its author and year. In Polish:
    //: %1 – %2
    return details.isEmpty() ? shown : tr("%1 – %2").arg(shown, details.join(QStringLiteral(", ")));
}

QString SourceNames::authorsOf(const SourceDescription& source) {
    QStringList names;
    for (const core::SourcePerson& person : source.people) {
        if (person.name.trimmed().isEmpty() ||
            !(person.role.isEmpty() || person.role == QLatin1String(SourceDescription::AUTHOR) ||
              person.role == QLatin1String(SourceDescription::INTERVIEWEE) ||
              person.role == QLatin1String(SourceDescription::INSTITUTION))) {
            continue;
        }
        const core::PersonName name = core::PersonName::of(
            person.name, person.role == QLatin1String(SourceDescription::INSTITUTION));
        names << (name.given.isEmpty() ? name.family : name.family + QLatin1Char(' ') + name.given);
    }
    return names.join(QStringLiteral("; "));
}

QString SourceNames::referenceTo(const core::ProjectManager& book, const QString& sourceId,
                                 const QString& page) {
    const core::ProjectElement* source = book.findElement(sourceId);
    if (source == nullptr) {
        return {};
    }
    // The number of the source in the bibliography, for the form "[1, s. 45]"
    const QList<const core::ProjectElement*> sources = book.sources();
    const auto at = std::ranges::find(sources, source);
    const int number =
        at != sources.cend() ? static_cast<int>(std::distance(sources.cbegin(), at)) + 1 : 1;
    return core::Bibliography::reference(source->source, source->title, page,
                                         core::Bibliography::settingsReferenceForm(), number,
                                         book.bookLanguage());
}

QString SourceNames::quotedWithReference(const core::ProjectManager& book,
                                         const QString& sourceId, const QString& text,
                                         const QString& page) {
    const editor::QuoteMarks marks = editor::QuoteMarks::forLanguage(book.bookLanguage());
    const QString quoted = marks.doubleOpening + text.simplified() + marks.doubleClosing;
    const QString reference = referenceTo(book, sourceId, page);
    return reference.isEmpty() ? quoted : quoted + QLatin1Char(' ') + reference;
}

}  // namespace kalahari::gui
