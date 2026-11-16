// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_names.cpp
/// @brief The texts for publishers in the program's words

#include "kalahari/gui/publishing_names.h"

#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/cover_names.h"

#include <QLocale>

namespace kalahari::gui {

using core::PublishingTexts;

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

}  // namespace kalahari::gui
