// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_clippings.cpp
/// @brief The list of the clippings: its columns and what a clipping shows in them

#include "kalahari/gui/material_list.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/version_texts.h"

#include <QDateTime>

#include <string>

namespace kalahari::gui {

QList<MaterialColumn> MaterialList::clippingColumns() {
    using core::Clippings;
    const bool keeps =
        core::SettingsManager::getInstance().get<std::string>(
            Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_KEEP) ==
        Clippings::AFTER_INSERT_KEEP;
    const auto column = [](const char* id, const QString& title, bool shown) {
        return MaterialColumn{
            .id = QString::fromLatin1(id), .title = title, .shown = shown, .sortable = true};
    };
    //: A column of the list of materials. In Polish: Tytuł
    return {column(TITLE, tr("Title"), true),
            //: The text a clipping was cut from, in the list of the clippings. In Polish: Skąd
            column(CLIPPING_FROM, tr("From"), true),
            //: When a clipping was cut, in the list of the clippings. In Polish: Wycięto
            column(CLIPPING_CUT, tr("Cut"), true),
            //: The words of a clipping. In Polish: Słowa
            column(WORDS, tr("Words"), true),
            //: The note of a clipping, in the list of the clippings. In Polish: Notatka
            column(CLIPPING_NOTE, tr("Note"), true),
            //: When a clipping was inserted into a text, in the list of the clippings. In
            //: Polish: Wstawiony
            column(CLIPPING_INSERTED, tr("Inserted"), keeps),
            //: The time the file was changed. In Polish: Data zmiany
            column(MODIFIED, tr("Modified"), false),
            //: In Polish: Powiązania
            column(LINKS, tr("Links"), false)};
}

void MaterialList::setClippingTexts(MaterialRow& row, const core::ProjectManager& book,
                                    const core::ProjectElement& element) {
    if (!core::ProjectManager::isClipping(element)) {
        return;
    }
    const core::ClippingData& data = element.clipping;
    const ClippingFrom from = ClippingTexts::fromOf(book, data);
    row.clipping = true;
    row.originLive = from.live;
    const auto set = [&row](const char* id, const QString& text, const QDateTime& order) {
        if (!text.isEmpty()) {
            row.texts.insert(QString::fromLatin1(id), text);
        }
        if (order.isValid()) {
            row.order.insert(QString::fromLatin1(id), order);
        }
    };
    set(CLIPPING_FROM, from.title, {});
    set(CLIPPING_CUT, data.origin.cut.isValid() ? versionTime(data.origin.cut) : QString(),
        data.origin.cut);
    set(CLIPPING_NOTE, book.notes(element.id).simplified(), {});
    set(CLIPPING_INSERTED, ClippingTexts::insertedText(data),
        data.inserted.isEmpty() ? QDateTime() : data.inserted.last().at);
}

}  // namespace kalahari::gui
