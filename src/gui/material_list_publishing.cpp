// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_publishing.cpp
/// @brief The list of the texts for publishers: its columns, what a text shows in them, and
/// the descriptions and the taglines of the cover shown there too

#include "kalahari/gui/material_list.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/publishing_names.h"

#include <QString>
#include <QStringList>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// The parts of the cover the list of the texts shows: those of the set
const QStringList& coverParts() {
    static const QStringList parts{QString::fromLatin1(PublishingTexts::TAGLINE),
                                   QString::fromLatin1(PublishingTexts::BLURB)};
    return parts;
}

/// The order of a state in the column State: a draft first, then the texts chosen, then those
/// rejected
int stateOrder(const QString& state) {
    if (state == QLatin1String(PublishingTexts::CHOSEN)) {
        return 1;
    }
    return state == QLatin1String(PublishingTexts::REJECTED) ? 2 : 0;
}

/// Put @p text in column @p id of @p row, sorted by @p order when it is valid
void setCell(MaterialRow& row, const char* id, const QString& text, const QVariant& order = {}) {
    if (!text.isEmpty()) {
        row.texts.insert(QString::fromLatin1(id), text);
    }
    if (order.isValid()) {
        row.order.insert(QString::fromLatin1(id), order);
    }
}

}  // namespace

QList<MaterialColumn> MaterialList::publishingColumns() {
    const auto column = [](const char* id, const QString& title, bool shown) {
        return MaterialColumn{
            .id = QString::fromLatin1(id), .title = title, .shown = shown, .sortable = true};
    };
    //: What a text for publishers is: a sentence about the book, a synopsis. In Polish: Rodzaj
    return {column(PUBLISHING_PART, tr("Kind"), true),
            //: A column of the list of the texts for publishers: a short text, else the title.
            //: In Polish: Treść albo tytuł
            column(PUBLISHING_TEXT, tr("Text or title"), true),
            //: The template of a text for publishers. In Polish: Szablon
            column(PUBLISHING_TEMPLATE, tr("Template"), true),
            //: Chosen for the set, a draft, rejected. In Polish: Stan
            column(PUBLISHING_STATE, tr("State"), true),
            //: The length of a text with its target. In Polish: Długość
            column(PUBLISHING_LENGTH, tr("Length"), true),
            //: The time the file was changed. In Polish: Data zmiany
            column(MODIFIED, tr("Modified"), true),
            //: A column of the list of materials. In Polish: Tytuł
            column(TITLE, tr("Title"), false),
            //: In Polish: Powiązania
            column(LINKS, tr("Links"), false)};
}

void MaterialList::setPublishingTexts(MaterialRow& row, const core::ProjectManager& book,
                                      const core::ProjectElement& element) {
    if (!core::ProjectManager::isPublishingText(element)) {
        return;
    }
    const core::PublishingData& data = element.publishing;
    row.publishing = true;
    row.group = data.part;
    row.state = data.state;
    const QString text = book.publishingTextOf(element.id);
    // A sentence shows its words; a synopsis, longer, its title
    if (data.part == QLatin1String(PublishingTexts::PITCH)) {
        row.body = text.simplified();
    }
    setCell(row, PUBLISHING_PART, PublishingNames::partName(data.part),
            static_cast<int>(PublishingTexts::parts().indexOf(data.part)));
    setCell(row, PUBLISHING_TEXT, row.body.isEmpty() ? element.title : row.body);
    setCell(row, PUBLISHING_TEMPLATE, PublishingNames::templateName(book, data.templateId));
    setCell(row, PUBLISHING_STATE, PublishingNames::shortStateName(data.state),
            stateOrder(data.state));
    const bool words = core::CoverMaterial::countsWords();
    const int length = PublishingTexts::lengthOf(text, words);
    setCell(row, PUBLISHING_LENGTH,
            PublishingNames::lengthText(length, book.publishingTargetOf(element.id, words), words),
            length);
}

void MaterialList::setPublishingCoverTexts(MaterialRow& row, const core::ProjectElement& element) {
    using core::CoverMaterial;
    const core::CoverData& cover = element.cover;
    row.fromCover = true;
    row.group = cover.part;
    row.state = cover.state;
    row.body = element.description.simplified();
    setCell(row, PUBLISHING_PART, CoverNames::partName(cover.part),
            static_cast<int>(PublishingTexts::parts().size() + coverParts().indexOf(cover.part)));
    setCell(row, PUBLISHING_TEXT, row.body.isEmpty() ? element.title : row.body);
    // A material of the cover has no template; its state is that of the cover
    setCell(row, PUBLISHING_TEMPLATE, QStringLiteral("–"));
    setCell(row, PUBLISHING_STATE, CoverNames::shortStateName(cover.state),
            stateOrder(cover.state));
    const bool words = CoverMaterial::countsWords();
    const int length = CoverMaterial::lengthOf(element.description, words);
    setCell(row, PUBLISHING_LENGTH,
            PublishingNames::lengthText(length, CoverMaterial::targetOf(cover, words), words),
            length);
}

QList<const core::ProjectElement*> MaterialList::publishingCoverTexts(
    const core::ProjectManager& book) {
    QList<const core::ProjectElement*> texts;
    if (const core::BookProject* project = book.project()) {
        for (const QString& part : coverParts()) {
            texts << PublishingTexts::versionsOf(*project, part);
        }
    }
    return texts;
}

}  // namespace kalahari::gui
