// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file clipping_texts.cpp
/// @brief What the program says of a clipping

#include "kalahari/gui/clipping_texts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/version_texts.h"

#include <QChar>
#include <QStringList>

namespace kalahari::gui {

namespace {

/// @brief What stands for the words left out before the last ones: "…"
constexpr char16_t ELLIPSIS = u'\u2026';

}  // namespace

ClippingFrom ClippingTexts::fromOf(const core::ProjectManager& book,
                                   const core::ClippingData& data) {
    ClippingFrom from;
    if (core::isEmpty(data.origin)) {
        return from;
    }
    if (const core::ProjectElement* text = book.findElement(data.origin.element)) {
        from.title = text->title;
        from.live = true;
        return from;
    }
    //: Where a clipping comes from: a text deleted since. %1: its title then. In Polish:
    //: %1 (usunięty)
    from.title = tr("%1 (deleted)").arg(data.origin.title);
    return from;
}

QString ClippingTexts::placeText(const core::ClippingOrigin& origin) {
    if (core::isEmpty(origin) || origin.paragraph <= 0) {
        return {};
    }
    const QString before = lastWords(origin.before, PLACE_TEXT_WORDS);
    if (before.isEmpty()) {
        //: Where a clipping was cut from: the start of a paragraph. %1: its number. In
        //: Polish: początek akapitu %1
        return tr("the start of paragraph %1").arg(origin.paragraph);
    }
    //: Where a clipping was cut from. %1: the number of the paragraph, %2: the last words
    //: before the place. In Polish: akapit %1, po słowach „%2”
    return tr("paragraph %1, after \"%2\"").arg(QString::number(origin.paragraph), before);
}

QString ClippingTexts::insertedText(const core::ClippingData& data) {
    return data.inserted.isEmpty() ? QString() : versionTime(data.inserted.last().at);
}

QString ClippingTexts::restoreToolTip(const ClippingFrom& from) {
    if (from.title.isEmpty()) {
        //: In Polish: Ten wycinek dodano ręcznie: nie ma pierwotnego miejsca
        return tr("This clipping was added by hand: it has no place it came from");
    }
    if (!from.live) {
        //: In Polish: Tekstu, z którego go wycięto, nie ma już w książce
        return tr("The text it was cut from is no longer in the book");
    }
    //: %1: the title of the text. In Polish: Wstaw go tam, skąd go wycięto, w „%1”
    return tr("Insert it where it was cut from, in \"%1\"").arg(from.title);
}

QString ClippingTexts::lastWords(const QString& text, int count) {
    const QStringList words = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() <= count) {
        return words.join(QLatin1Char(' '));
    }
    return QChar(ELLIPSIS) + words.mid(words.size() - count).join(QLatin1Char(' '));
}

}  // namespace kalahari::gui
