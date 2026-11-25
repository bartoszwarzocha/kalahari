// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file cover_names.cpp
/// @brief The materials of the cover in the program's words

#include "kalahari/gui/cover_names.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"

#include <QLocale>

#include <functional>

namespace kalahari::gui {

using core::CoverMaterial;

bool CoverNames::isCoverKind(const core::KindRef& kind) {
    return kind && kind.package != nullptr &&
           kind.package->id == QLatin1String(core::ProjectManager::BASE_PACKAGE) &&
           kind.kind->id == QLatin1String(core::MaterialKinds::COVER);
}

bool CoverNames::isCoverMaterial(const core::ProjectManager& book,
                                 const core::ProjectElement& element) {
    return isCoverKind(book.kindOf(element));
}

QString CoverNames::partName(const QString& part) {
    if (part == QLatin1String(CoverMaterial::BLURB)) {
        //: What a material of the cover is: the text on the back or on a flap. In Polish: Opis
        //: na okładkę
        return tr("Cover description");
    }
    if (part == QLatin1String(CoverMaterial::TAGLINE)) {
        //: A short sentence on the front of the cover. In Polish: Hasło
        return tr("Tagline");
    }
    if (part == QLatin1String(CoverMaterial::OPINION)) {
        //: Words of a critic, a writer or a magazine about the book. In Polish: Opinia lub
        //: rekomendacja
        return tr("Opinion or recommendation");
    }
    if (part == QLatin1String(CoverMaterial::BIO)) {
        //: What a material of the cover is: a short text about the author for a flap or the
        //: back, apart from the note About the Author at the end of the book. In Polish:
        //: Notka biograficzna
        return tr("Author bio");
    }
    if (part == QLatin1String(CoverMaterial::DESIGN)) {
        //: A design of the cover by a designer, or the writer's own sketch. In Polish: Projekt
        //: lub szkic okładki
        return tr("Cover design or sketch");
    }
    if (part == QLatin1String(CoverMaterial::AUTHOR_PHOTO)) {
        //: What a material of the cover is: a portrait of the author for a flap. In Polish:
        //: Zdjęcie autora
        return tr("Author photo");
    }
    if (part == QLatin1String(CoverMaterial::REFERENCE)) {
        //: A picture that shows a mood, colors or a typeface for the cover. In Polish:
        //: Ilustracja referencyjna
        return tr("Reference picture");
    }
    if (part == QLatin1String(CoverMaterial::NOTE)) {
        //: Ideas and remarks about the cover. In Polish: Notatka
        return tr("Note");
    }
    return {};
}

QString CoverNames::groupName(const QString& part) {
    if (part == QLatin1String(CoverMaterial::BLURB)) {
        //: A group of the list of the cover. In Polish: Opisy na okładkę
        return tr("Cover descriptions");
    }
    if (part == QLatin1String(CoverMaterial::TAGLINE)) {
        //: A group of the list of the cover. In Polish: Hasła
        return tr("Taglines");
    }
    if (part == QLatin1String(CoverMaterial::OPINION)) {
        //: A group of the list of the cover. In Polish: Opinie i rekomendacje
        return tr("Opinions and recommendations");
    }
    if (part == QLatin1String(CoverMaterial::BIO)) {
        //: A group of the list of the cover. In Polish: Notki biograficzne
        return tr("Author bios");
    }
    if (part == QLatin1String(CoverMaterial::DESIGN)) {
        //: A group of the list of the cover. In Polish: Projekty i szkice
        return tr("Designs and sketches");
    }
    if (part == QLatin1String(CoverMaterial::AUTHOR_PHOTO)) {
        //: A group of the list of the cover. In Polish: Zdjęcia autora
        return tr("Author photos");
    }
    if (part == QLatin1String(CoverMaterial::REFERENCE)) {
        //: A group of the list of the cover. In Polish: Ilustracje referencyjne
        return tr("Reference pictures");
    }
    if (part == QLatin1String(CoverMaterial::NOTE)) {
        //: A group of the list of the cover. In Polish: Notatki
        return tr("Notes");
    }
    //: A group of the list of the cover: materials without a kind the program knows. In
    //: Polish: Inne
    return tr("Other");
}

QString CoverNames::newTitle(const core::ProjectManager& book, const QString& part) {
    QString title;
    if (part == QLatin1String(CoverMaterial::BLURB)) {
        //: The title of a new description for the cover; %1 its number. In Polish: Opis %1
        title = tr("Description %1");
    } else if (part == QLatin1String(CoverMaterial::TAGLINE)) {
        //: The title of a new tagline; %1 its number. In Polish: Hasło %1
        title = tr("Tagline %1");
    } else if (part == QLatin1String(CoverMaterial::OPINION)) {
        //: The title of a new opinion; %1 its number. In Polish: Opinia %1
        title = tr("Opinion %1");
    } else if (part == QLatin1String(CoverMaterial::BIO)) {
        //: The title of a new author bio; %1 its number. In Polish: Notka %1
        title = tr("Bio %1");
    } else if (part == QLatin1String(CoverMaterial::DESIGN)) {
        //: The title of a new design of the cover; %1 its number. In Polish: Projekt %1
        title = tr("Design %1");
    } else if (part == QLatin1String(CoverMaterial::AUTHOR_PHOTO)) {
        //: The title of a new photo of the author; %1 its number. In Polish: Zdjęcie autora %1
        title = tr("Author photo %1");
    } else if (part == QLatin1String(CoverMaterial::REFERENCE)) {
        //: The title of a new reference picture; %1 its number. In Polish: Ilustracja %1
        title = tr("Illustration %1");
    } else if (part == QLatin1String(CoverMaterial::NOTE)) {
        //: The title of a new note about the cover; %1 its number. In Polish: Notatka %1
        title = tr("Note %1");
    } else {
        return {};
    }
    int count = 0;
    if (const core::BookProject* project = book.project()) {
        const std::function<void(const QList<core::ProjectElement>&)> countIn =
            [&](const QList<core::ProjectElement>& elements) {
                for (const core::ProjectElement& element : elements) {
                    if (element.cover.part == part && isCoverMaterial(book, element)) {
                        ++count;
                    }
                    countIn(element.elements);
                }
            };
        countIn(project->workshop.elements);
    }
    return title.arg(count + 1);
}

QString CoverNames::partIconId(const QString& part) {
    if (part == QLatin1String(CoverMaterial::BLURB)) {
        return QStringLiteral("material.coverBlurb");
    }
    if (part == QLatin1String(CoverMaterial::TAGLINE)) {
        return QStringLiteral("material.coverTagline");
    }
    if (part == QLatin1String(CoverMaterial::OPINION)) {
        return QStringLiteral("material.coverOpinion");
    }
    if (part == QLatin1String(CoverMaterial::BIO)) {
        return QStringLiteral("material.coverBio");
    }
    if (part == QLatin1String(CoverMaterial::DESIGN)) {
        return QStringLiteral("material.coverDesign");
    }
    if (part == QLatin1String(CoverMaterial::AUTHOR_PHOTO)) {
        return QStringLiteral("material.coverAuthorPhoto");
    }
    if (part == QLatin1String(CoverMaterial::REFERENCE)) {
        return QStringLiteral("common.image");
    }
    if (part == QLatin1String(CoverMaterial::NOTE)) {
        return QStringLiteral("material.coverNote");
    }
    return {};
}

QString CoverNames::placeName(const QString& place) {
    if (place == QLatin1String(CoverMaterial::FRONT)) {
        //: A place on the cover, in a list and a menu. In Polish: Okładka – przód
        return tr("Cover – front");
    }
    if (place == QLatin1String(CoverMaterial::SPINE)) {
        //: A place on the cover, in a list and a menu. In Polish: Okładka – grzbiet
        return tr("Cover – spine");
    }
    if (place == QLatin1String(CoverMaterial::BACK)) {
        //: A place on the cover, in a list and a menu. In Polish: Okładka – tył
        return tr("Cover – back");
    }
    if (place == QLatin1String(CoverMaterial::FRONT_FLAP)) {
        //: A place on the cover, in a list and a menu. In Polish: Okładka – skrzydełko przednie
        return tr("Cover – front flap");
    }
    if (place == QLatin1String(CoverMaterial::BACK_FLAP)) {
        //: A place on the cover, in a list and a menu. In Polish: Okładka – skrzydełko tylne
        return tr("Cover – back flap");
    }
    if (place == QLatin1String(CoverMaterial::WHOLE)) {
        //: The place of a design: the whole cover, in a list and a menu. In Polish: Okładka –
        //: cała okładka
        return tr("Cover – whole cover");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_FRONT)) {
        //: A place on the jacket, in a list and a menu. In Polish: Obwoluta – przód
        return tr("Jacket – front");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_SPINE)) {
        //: A place on the jacket, in a list and a menu. In Polish: Obwoluta – grzbiet
        return tr("Jacket – spine");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_BACK)) {
        //: A place on the jacket, in a list and a menu. In Polish: Obwoluta – tył
        return tr("Jacket – back");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_FRONT_FLAP)) {
        //: A place on the jacket, in a list and a menu. In Polish: Obwoluta – skrzydełko
        //: przednie
        return tr("Jacket – front flap");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_BACK_FLAP)) {
        //: A place on the jacket, in a list and a menu. In Polish: Obwoluta – skrzydełko tylne
        return tr("Jacket – back flap");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_WHOLE)) {
        //: The place of a design: the whole jacket, in a list and a menu. In Polish: Obwoluta
        //: – cała obwoluta
        return tr("Jacket – whole jacket");
    }
    // A place a later version adds is shown as it is written
    if (!place.isEmpty()) {
        return place;
    }
    //: A material without a place on the cover yet. In Polish: Nie ustalono
    return tr("Not set");
}

QString CoverNames::shortPlaceName(const QString& place) {
    if (place == QLatin1String(CoverMaterial::FRONT) ||
        place == QLatin1String(CoverMaterial::JACKET_FRONT)) {
        //: A place on the sheet of the cover or of the jacket. In Polish: Przód
        return tr("Front");
    }
    if (place == QLatin1String(CoverMaterial::SPINE) ||
        place == QLatin1String(CoverMaterial::JACKET_SPINE)) {
        //: A place on the sheet of the cover or of the jacket. In Polish: Grzbiet
        return tr("Spine");
    }
    if (place == QLatin1String(CoverMaterial::BACK) ||
        place == QLatin1String(CoverMaterial::JACKET_BACK)) {
        //: A place on the sheet of the cover or of the jacket. In Polish: Tył
        return tr("Back");
    }
    if (place == QLatin1String(CoverMaterial::FRONT_FLAP) ||
        place == QLatin1String(CoverMaterial::JACKET_FRONT_FLAP)) {
        //: A place on the sheet of the cover or of the jacket. In Polish: Skrzydełko przednie
        return tr("Front flap");
    }
    if (place == QLatin1String(CoverMaterial::BACK_FLAP) ||
        place == QLatin1String(CoverMaterial::JACKET_BACK_FLAP)) {
        //: A place on the sheet of the cover or of the jacket. In Polish: Skrzydełko tylne
        return tr("Back flap");
    }
    if (place == QLatin1String(CoverMaterial::WHOLE)) {
        //: The place of a design: the whole cover. In Polish: Cała okładka
        return tr("Whole cover");
    }
    if (place == QLatin1String(CoverMaterial::JACKET_WHOLE)) {
        //: The place of a design: the whole jacket. In Polish: Cała obwoluta
        return tr("Whole jacket");
    }
    return placeName(place);
}

QString CoverNames::placeGroupName(const QString& place) {
    if (CoverMaterial::isJacketPlace(place)) {
        //: The part of the cover a place is on: the jacket of a hardback. In Polish: Obwoluta
        return tr("Jacket");
    }
    //: The part of the cover a place is on: the cover itself. In Polish: Okładka
    return tr("Cover");
}

QString CoverNames::partOffText(const QString& place, const core::CoverParts& parts) {
    if (CoverMaterial::isOn(place, parts)) {
        return {};
    }
    if (CoverMaterial::isJacketPlace(place)) {
        //: After the place of a material on the jacket, while the book has no jacket. In
        //: Polish: obwoluta wyłączona
        return tr("jacket turned off");
    }
    //: After the place of a material on a flap of the cover, while the cover has no flaps.
    //: In Polish: skrzydełka wyłączone
    return tr("flaps turned off");
}

QString CoverNames::fillName(const QString& fill) {
    using core::Endpaper;
    if (fill == QLatin1String(Endpaper::PICTURE)) {
        //: How an endpaper is filled: one picture over both of its pages. In Polish: Obraz na
        //: obie strony
        return tr("Picture across both pages");
    }
    if (fill == QLatin1String(Endpaper::TILE)) {
        //: How an endpaper is filled: a small picture repeated like a wallpaper. In Polish:
        //: Obraz powtarzany
        return tr("Repeated picture");
    }
    if (fill == QLatin1String(Endpaper::PATTERN)) {
        //: How an endpaper is filled: a pattern of the program in two colors. In Polish:
        //: Gotowy wzór
        return tr("Ready-made pattern");
    }
    if (fill == QLatin1String(Endpaper::COLOR)) {
        //: How an endpaper is filled: one color. In Polish: Jeden kolor
        return tr("One color");
    }
    if (!fill.isEmpty()) {
        return fill;
    }
    //: An endpaper of white paper. In Polish: Bez wypełnienia
    return tr("No fill");
}

QString CoverNames::patternName(const QString& pattern) {
    using core::Endpaper;
    if (pattern == QLatin1String(Endpaper::STRIPES)) {
        //: A pattern of an endpaper. In Polish: paski
        return tr("stripes");
    }
    if (pattern == QLatin1String(Endpaper::GRID)) {
        //: A pattern of an endpaper. In Polish: kratka
        return tr("grid");
    }
    if (pattern == QLatin1String(Endpaper::DIAGONAL)) {
        //: A pattern of an endpaper. In Polish: ukośne linie
        return tr("diagonal lines");
    }
    if (pattern == QLatin1String(Endpaper::CHECKER)) {
        //: A pattern of an endpaper. In Polish: szachownica
        return tr("checkerboard");
    }
    if (pattern == QLatin1String(Endpaper::WAVES)) {
        //: A pattern of an endpaper. In Polish: fale
        return tr("waves");
    }
    //: A pattern of an endpaper. In Polish: kropki
    return tr("dots");
}

QString CoverNames::fillSummary(const core::Endpaper& endpaper, const QString& pictureTitle) {
    using core::Endpaper;
    const QString& fill = endpaper.fill;
    if (fill == QLatin1String(Endpaper::PICTURE)) {
        //: How an endpaper is filled, in short: %1 the title of its picture. In Polish: obraz:
        //: %1
        return tr("picture: %1").arg(pictureTitle);
    }
    if (fill == QLatin1String(Endpaper::TILE)) {
        //: How an endpaper is filled, in short: %1 the title of its picture, %2 its width in
        //: percent of a page. In Polish: obraz powtarzany: %1, %2%
        return tr("repeated picture: %1, %2%").arg(pictureTitle).arg(endpaper.scale);
    }
    if (fill == QLatin1String(Endpaper::PATTERN)) {
        //: How an endpaper is filled, in short: %1 the name of its pattern. In Polish: wzór: %1
        return tr("pattern: %1").arg(patternName(endpaper.shownPattern()));
    }
    if (fill == QLatin1String(Endpaper::COLOR)) {
        //: How an endpaper is filled, in short. In Polish: jeden kolor
        return tr("one color");
    }
    if (!fill.isEmpty()) {
        return fill;
    }
    //: How an endpaper is filled, in short: white paper. In Polish: bez wypełnienia
    return tr("no fill");
}

QString CoverNames::stateName(const QString& state) {
    if (state == QLatin1String(CoverMaterial::CHOSEN)) {
        //: The state of a material of the cover. In Polish: wybrany na okładkę
        return tr("chosen for the cover");
    }
    if (state == QLatin1String(CoverMaterial::REJECTED)) {
        //: The state of a material of the cover. In Polish: odrzucony
        return tr("rejected");
    }
    if (!state.isEmpty()) {
        return state;
    }
    //: The state of a material of the cover, at first. In Polish: roboczy
    return tr("draft");
}

QString CoverNames::shortStateName(const QString& state) {
    if (state == QLatin1String(CoverMaterial::CHOSEN)) {
        //: The state of a material of the cover in a list. In Polish: ★ wybrany
        return tr("★ chosen");
    }
    return stateName(state);
}

QString CoverNames::consentName(const QString& consent) {
    if (consent == QLatin1String(CoverMaterial::GRANTED)) {
        //: The consent to print an opinion on the cover. In Polish: jest zgoda
        return tr("granted");
    }
    if (consent == QLatin1String(CoverMaterial::ASKED)) {
        //: The consent to print an opinion: asked for, no answer yet. In Polish: poproszono o
        //: zgodę
        return tr("asked for");
    }
    if (consent == QLatin1String(CoverMaterial::REFUSED)) {
        //: No consent to print an opinion. In Polish: brak zgody
        return tr("none");
    }
    if (!consent.isEmpty()) {
        return consent;
    }
    //: The consent to print an opinion, not said. In Polish: nie ustalono
    return tr("not said");
}

QString CoverNames::shortConsentName(const QString& consent) {
    if (consent == QLatin1String(CoverMaterial::GRANTED)) {
        //: The consent to print an opinion in the list of the cover. In Polish: jest
        return tr("given");
    }
    if (consent == QLatin1String(CoverMaterial::ASKED)) {
        //: The consent to print an opinion in the list of the cover: asked for, no answer
        //: yet. In Polish: czeka
        return tr("waiting");
    }
    if (consent == QLatin1String(CoverMaterial::REFUSED)) {
        //: No consent to print an opinion, in the list of the cover. In Polish: brak
        return tr("missing");
    }
    return consentName(consent);
}

QString CoverNames::lengthText(int length, bool words) {
    if (words) {
        //: The length of a text of the cover in a list: %1 its words. In Polish: %1 sł.
        return tr("%1 w.").arg(QLocale().toString(length));
    }
    //: The length of a text of the cover in a list: %1 its characters with spaces. In Polish:
    //: %1 zn.
    return tr("%1 ch.").arg(QLocale().toString(length));
}

QString CoverNames::countsText(int characters, int words) {
    //: The counts of a text of the cover: %1 the characters with spaces, %2 the words. In
    //: Polish: %1 znaków ze spacjami · %2 słów (with the plural forms of the numbers)
    return tr("%n characters with spaces", nullptr, characters) + QStringLiteral(" · ") +
           tr("%n words", nullptr, words);
}

QString CoverNames::targetText(int target, bool words) {
    if (words) {
        //: The target length of a text of the cover. In Polish: cel: do %n słów
        return tr("target: up to %n words", nullptr, target);
    }
    //: The target length of a text of the cover. In Polish: cel: do %n znaków
    return tr("target: up to %n characters", nullptr, target);
}

QString CoverNames::overText(int excess, bool words) {
    if (words) {
        //: A text of the cover past its target. In Polish: o %n słów za dużo
        return tr("%n words too many", nullptr, excess);
    }
    //: A text of the cover past its target. In Polish: o %n znaków za dużo
    return tr("%n characters too many", nullptr, excess);
}

QString CoverNames::stateMessage(const QString& title, const QString& state,
                                 const QStringList& drafts) {
    QString text;
    if (state == QLatin1String(CoverMaterial::CHOSEN)) {
        //: The status bar: a material is chosen for the cover; %1 its title. In Polish:
        //: Wybrano na okładkę: „%1”
        text = tr("Chosen for the cover: “%1”").arg(title);
    } else if (state == QLatin1String(CoverMaterial::REJECTED)) {
        //: The status bar: a material of the cover is rejected; %1 its title. In Polish:
        //: Odrzucono: „%1”
        text = tr("Rejected: “%1”").arg(title);
    } else {
        //: The status bar: a material of the cover is a draft again; %1 its title. In Polish:
        //: „%1” jest znów roboczy
        text = tr("“%1” is a draft again").arg(title);
    }
    for (const QString& draft : drafts) {
        //: The status bar, after a material is chosen for the cover: the one chosen before for
        //: its place is a draft again; %1 its title. In Polish: „%1” wraca do roboczych
        text += QStringLiteral(" · ") + tr("“%1” is a draft again").arg(draft);
    }
    return text;
}

QString CoverNames::unitName(bool words) {
    //: The unit of a target length, after its number. In Polish: słów / znaków
    return words ? tr("words") : tr("characters");
}

QString CoverNames::printedOpinion(const QString& text, const core::CoverData& cover) {
    QString who = cover.author.trimmed();
    if (!who.isEmpty() && !cover.role.trimmed().isEmpty()) {
        who += QStringLiteral(", ") + cover.role.trimmed();
    }
    //: An opinion as the cover prints it: %1 its words. In Polish: „%1”
    QString printed = tr("“%1”").arg(text.simplified());
    if (!who.isEmpty()) {
        //: An opinion with its author: %1 the opinion in quotes, %2 the author and who the
        //: author is. In Polish: %1 – %2
        printed = tr("%1 – %2").arg(printed, who);
    }
    return printed;
}

} // namespace kalahari::gui
