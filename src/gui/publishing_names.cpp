// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_names.cpp
/// @brief The texts for publishers in the program's words

#include "kalahari/gui/publishing_names.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/localized_text.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/kind_names.h"

#include <QList>
#include <QLocale>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

using core::PublishingTexts;

bool PublishingNames::isPublishingKind(const core::KindRef& kind) {
    return kind && kind.package != nullptr &&
           kind.package->id == QLatin1String(core::ProjectManager::BASE_PACKAGE) &&
           kind.kind->id == QLatin1String(PublishingTexts::KIND);
}

QString PublishingNames::partName(const QString& part) {
    if (part == QLatin1String(PublishingTexts::PITCH)) {
        //: A text for publishers: one sentence about who, what happens and what is at stake.
        //: In Polish: Zdanie o książce
        return tr("Sentence about the book");
    }
    if (part == QLatin1String(PublishingTexts::SYNOPSIS)) {
        //: A text for publishers: the whole story with its end. In Polish: Streszczenie
        return tr("Synopsis");
    }
    if (part == QLatin1String(PublishingTexts::ABOUT_AUTHOR)) {
        //: A part of the set for a publisher: the note about the author of the back part of
        //: the book. In Polish: Nota o autorze
        return tr("About the author");
    }
    // The description and the tagline come from the cover, with its names
    if (part == QLatin1String(PublishingTexts::BLURB) ||
        part == QLatin1String(PublishingTexts::TAGLINE)) {
        return CoverNames::partName(part);
    }
    return {};
}

QString PublishingNames::stateName(const QString& state) {
    if (state == QLatin1String(PublishingTexts::CHOSEN)) {
        //: The state of a text for publishers. In Polish: wybrany do zestawu
        return tr("chosen for the set");
    }
    if (state == QLatin1String(PublishingTexts::REJECTED)) {
        //: The state of a text for publishers. In Polish: odrzucony
        return tr("rejected");
    }
    if (!state.isEmpty()) {
        return state;
    }
    //: The state of a text for publishers, at first. In Polish: roboczy
    return tr("draft");
}

QString PublishingNames::shortStateName(const QString& state) {
    if (state == QLatin1String(PublishingTexts::CHOSEN)) {
        //: The state of a text for publishers in a list. In Polish: ★ w zestawie
        return tr("★ in the set");
    }
    return stateName(state);
}

QString PublishingNames::groupName(const QString& part) {
    if (part == QLatin1String(PublishingTexts::PITCH)) {
        //: A group of the list of the texts for publishers. In Polish: Zdania o książce
        return tr("Sentences about the book");
    }
    if (part == QLatin1String(PublishingTexts::SYNOPSIS)) {
        //: A group of the list of the texts for publishers. In Polish: Streszczenia
        return tr("Synopses");
    }
    if (part == QLatin1String(PublishingTexts::BLURB) ||
        part == QLatin1String(PublishingTexts::TAGLINE)) {
        //: A group of the list of the texts for publishers: the descriptions or the taglines of
        //: the Cover; %1 the group of the cover. In Polish: %1 · z Okładki
        return tr("%1 · from the Cover").arg(CoverNames::groupName(part));
    }
    return {};
}

QString PublishingNames::partIconId(const QString& part) {
    if (part == QLatin1String(PublishingTexts::PITCH)) {
        return QStringLiteral("material.publishingPitch");
    }
    if (part == QLatin1String(PublishingTexts::SYNOPSIS)) {
        return QStringLiteral("material.publishingSynopsis");
    }
    if (part == QLatin1String(PublishingTexts::BLURB) ||
        part == QLatin1String(PublishingTexts::TAGLINE)) {
        return CoverNames::partIconId(part);
    }
    return QStringLiteral("material.publishing");
}

QString PublishingNames::newTitle(const core::ProjectManager& book, const QString& part) {
    QStringList titles;
    if (const core::BookProject* project = book.project()) {
        for (const core::ProjectElement* text : PublishingTexts::textsOf(*project, part)) {
            titles << text->title;
        }
    }
    // The next number, past a title the writer kept
    int number = static_cast<int>(titles.size()) + 1;
    QString title;
    do {
        //: The title of a new text for publishers: %1 its part, %2 its number. In Polish: %1 –
        //: wersja %2
        title = tr("%1 – version %2").arg(partName(part)).arg(number);
        ++number;
    } while (titles.contains(title));
    return title;
}

QString PublishingNames::noTemplateName() {
    //: A text for publishers without a template. In Polish: Bez szablonu
    return tr("No template");
}

QString PublishingNames::templateName(const core::ProjectManager& book,
                                      const QString& reference) {
    if (reference.isEmpty()) {
        return noTemplateName();
    }
    const std::optional<core::PublishingTemplate> found = book.publishingTemplate(reference);
    return found ? found->name.text(programLanguage()) : reference;
}

QString PublishingNames::lengthText(int length, int target, bool words) {
    if (target <= 0) {
        return CoverNames::lengthText(length, words);
    }
    if (words) {
        //: The length of a text for publishers in a list with its target: %1 its words, %2 the
        //: target. In Polish: %1 / %2 sł.
        return tr("%1 / %2 w.").arg(QLocale().toString(length), QLocale().toString(target));
    }
    //: The length of a text for publishers in a list with its target: %1 its characters with
    //: spaces, %2 the target. In Polish: %1 / %2 zn.
    return tr("%1 / %2 ch.").arg(QLocale().toString(length), QLocale().toString(target));
}

QString PublishingNames::stateMessage(const QString& title, const QString& state,
                                      const QStringList& drafts) {
    QString text;
    if (state == QLatin1String(PublishingTexts::CHOSEN)) {
        //: The status bar: a text is chosen for the set for publishers; %1 its title. In
        //: Polish: Wybrano do zestawu: „%1”
        text = tr("Chosen for the set: “%1”").arg(title);
    } else {
        // Rejected or a draft again: as a material of the cover
        text = CoverNames::stateMessage(title, state, {});
    }
    for (const QString& draft : drafts) {
        //: The status bar, after a text is chosen for the set: the one chosen before for its
        //: part is a draft again; %1 its title. In Polish: „%1” wraca do roboczych
        text += QStringLiteral(" · ") + tr("“%1” is a draft again").arg(draft);
    }
    return text;
}

QString PublishingNames::pagesText(int characters) {
    const double pages = PublishingTexts::pagesOf(characters);
    const QString number = QLocale().toString(pages, 'f', 1);
    if (pages < 1.0) {
        //: The length of a synopsis in standard pages (1800 characters with spaces), below one:
        //: %1 the pages, "0.6". In Polish: około %1 strony znormalizowanej
        return tr("about %1 of a standard page").arg(number);
    }
    //: The length of a synopsis in standard pages, more than one: %1 the pages, "2.5", always
    //: with a fraction. In Polish: około %1 strony znormalizowanej
    return tr("about %1 standard pages").arg(number);
}

QString PublishingNames::targetSourceText(const core::PublishingData& data,
                                          const core::PublishingTemplate* templ, int target,
                                          bool inWords) {
    if ((inWords ? data.targetWords : data.targetCharacters) > 0) {
        //: The target length of a text for publishers is its own. In Polish: Docelowa długość
        //: tego tekstu.
        return tr("The target length of this text.");
    }
    const QString length = CoverNames::lengthText(target, inWords);
    if (templ != nullptr && (inWords ? templ->targetWords : templ->targetCharacters) > 0) {
        //: The target length of a text for publishers from its template: %1 the template, %2
        //: the length. In Polish: Docelowa długość z szablonu „%1”: %2
        return tr("The target length of the template \"%1\": %2")
            .arg(templ->name.text(programLanguage()), length);
    }
    //: The target length of a text for publishers from the Settings: %1 the length. In
    //: Polish: Docelowa długość z Ustawień: %1
    return tr("The target length from the Settings: %1").arg(length);
}

}  // namespace kalahari::gui
