// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_names.cpp
/// @brief The sources in the program's words

#include "kalahari/gui/source_names.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/kind_names.h"

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

QString SourceNames::fieldLabel(const QString& field, const QString& kind) {
    const auto is = [&kind](const char* each) { return kind == QLatin1String(each); };
    if (field == QLatin1String(SourceDescription::TITLE)) {
        if (is(SourceDescription::CHAPTER)) {
            //: In Polish: Tytuł rozdziału:
            return tr("Chapter title:");
        }
        if (is(SourceDescription::ARTICLE)) {
            //: In Polish: Tytuł artykułu:
            return tr("Article title:");
        }
        if (is(SourceDescription::WEB_PAGE)) {
            //: In Polish: Tytuł strony:
            return tr("Page title:");
        }
        if (is(SourceDescription::ARCHIVE)) {
            //: In Polish: Tytuł lub opis:
            return tr("Title or description:");
        }
        //: The title of a source. In Polish: Tytuł:
        return tr("Title:");
    }
    if (field == QLatin1String(SourceDescription::SUBTITLE)) {
        //: In Polish: Podtytuł:
        return tr("Subtitle:");
    }
    if (field == QLatin1String(SourceDescription::CONTAINER)) {
        if (is(SourceDescription::CHAPTER)) {
            //: The book of a chapter. In Polish: Tytuł książki:
            return tr("Book title:");
        }
        if (is(SourceDescription::ARTICLE)) {
            //: In Polish: Czasopismo:
            return tr("Journal:");
        }
        if (is(SourceDescription::WEB_PAGE)) {
            //: The web site of a page. In Polish: Witryna:
            return tr("Site:");
        }
        //: The archive of a document. In Polish: Archiwum:
        return tr("Archive:");
    }
    if (field == QLatin1String(SourceDescription::EDITION)) {
        //: In Polish: Wydanie:
        return tr("Edition:");
    }
    if (field == QLatin1String(SourceDescription::PLACE)) {
        if (is(SourceDescription::INTERVIEW) || is(SourceDescription::FILM)) {
            //: Where a talk took place, where a film was made. In Polish: Miejsce:
            return tr("Place:");
        }
        //: In Polish: Miejsce wydania:
        return tr("Place of publication:");
    }
    if (field == QLatin1String(SourceDescription::PUBLISHER)) {
        if (is(SourceDescription::THESIS)) {
            //: In Polish: Uczelnia:
            return tr("University:");
        }
        if (is(SourceDescription::FILM)) {
            //: Who made a film or a recording. In Polish: Wytwórnia:
            return tr("Producer:");
        }
        //: In Polish: Wydawnictwo:
        return tr("Publisher:");
    }
    if (field == QLatin1String(SourceDescription::YEAR)) {
        if (is(SourceDescription::ARCHIVE) || is(SourceDescription::INTERVIEW)) {
            //: In Polish: Data:
            return tr("Date:");
        }
        //: The year of a source. In Polish: Rok:
        return tr("Year:");
    }
    if (field == QLatin1String(SourceDescription::VOLUME)) {
        //: The volume of a journal. In Polish: Tom:
        return tr("Volume:");
    }
    if (field == QLatin1String(SourceDescription::ISSUE)) {
        //: The issue of a journal. In Polish: Numer:
        return tr("Issue:");
    }
    if (field == QLatin1String(SourceDescription::PAGES)) {
        if (is(SourceDescription::CHAPTER) || is(SourceDescription::ARTICLE)) {
            //: The pages of a chapter or an article: 41–58. In Polish: Strony:
            return tr("Pages:");
        }
        //: How many pages a book has. In Polish: Liczba stron:
        return tr("Number of pages:");
    }
    if (field == QLatin1String(SourceDescription::SERIES)) {
        //: In Polish: Seria:
        return tr("Series:");
    }
    if (field == QLatin1String(SourceDescription::ISBN)) {
        return QStringLiteral("ISBN:");
    }
    if (field == QLatin1String(SourceDescription::ISSN)) {
        return QStringLiteral("ISSN:");
    }
    if (field == QLatin1String(SourceDescription::DOI)) {
        return QStringLiteral("DOI:");
    }
    if (field == QLatin1String(SourceDescription::URL)) {
        //: The web address of a source. In Polish: Adres:
        return tr("Address:");
    }
    if (field == QLatin1String(SourceDescription::PUBLISHED)) {
        //: When a web page was published. In Polish: Opublikowano:
        return tr("Published:");
    }
    if (field == QLatin1String(SourceDescription::ACCESSED)) {
        //: When the writer read a web page. In Polish: Dostęp:
        return tr("Accessed:");
    }
    if (field == QLatin1String(SourceDescription::FONDS)) {
        //: The fonds of an archive. In Polish: Zespół:
        return tr("Fonds:");
    }
    if (field == QLatin1String(SourceDescription::SIGNATURE)) {
        //: The call number of a document in an archive. In Polish: Sygnatura:
        return tr("Call number:");
    }
    if (field == QLatin1String(SourceDescription::FORM)) {
        if (is(SourceDescription::THESIS)) {
            //: Bachelor's, master's, doctoral. In Polish: Rodzaj pracy:
            return tr("Kind of thesis:");
        }
        //: What a source is: a manuscript, a sound recording of 48 minutes. In Polish: Postać:
        return tr("Form:");
    }
    if (field == QLatin1String(SourceDescription::LANGUAGE)) {
        //: In Polish: Język:
        return tr("Language:");
    }
    return field + QLatin1Char(':');
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
    example.people << core::SourcePerson{QStringLiteral("Anna Wiśniewska"), {}, {}};
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

}  // namespace kalahari::gui
