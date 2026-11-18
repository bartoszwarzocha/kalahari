// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set.cpp
/// @brief The set for a publisher of the open book in the program's words

#include "kalahari/gui/publishing_set.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/publishing_names.h"

#include <QLocale>

#include <algorithm>

namespace kalahari::gui {

namespace {

using core::CoverMaterial;
using core::PublishingTexts;

/// Whether @p part is a description or a tagline, which the cover has
bool ofCover(const QString& part) {
    return part == QLatin1String(PublishingTexts::TAGLINE) ||
           part == QLatin1String(PublishingTexts::BLURB);
}

/// The title of version @p element in the list of the versions of its part, with what marks
/// it: chosen for the set, or for the cover
QString versionTitleOf(const core::ProjectElement& element) {
    if (core::ProjectManager::isPublishingText(element)) {
        if (element.publishing.state == QLatin1String(PublishingTexts::CHOSEN)) {
            //: A version of a part of the set for a publisher chosen for the set: %1 its
            //: title. In Polish: %1 ★
            return PublishingSetContents::tr("%1 ★").arg(element.title);
        }
        return element.title;
    }
    if (element.cover.state == QLatin1String(CoverMaterial::CHOSEN)) {
        //: A description or a tagline chosen for the cover, among the versions of its part of
        //: the set for a publisher: %1 its title. In Polish: %1 ★ na okładce
        return PublishingSetContents::tr("%1 ★ on the cover").arg(element.title);
    }
    return element.title;
}

/// The versions of @p part of the set of @p book: those it can have, else the one it has
QList<PublishingSetVersion> versionsOf(const core::ProjectManager& book, const QString& part,
                                       const QString& elementId) {
    QList<PublishingSetVersion> versions;
    if (const core::BookProject* project = book.project()) {
        for (const core::ProjectElement* version : PublishingTexts::versionsOf(*project, part)) {
            versions << PublishingSetVersion{version->id, versionTitleOf(*version)};
        }
    }
    const bool listed = std::any_of(versions.cbegin(), versions.cend(),
                                    [&elementId](const PublishingSetVersion& version) {
                                        return version.id == elementId;
                                    });
    if (!listed) {
        if (const core::ProjectElement* element = book.findElement(elementId)) {
            versions << PublishingSetVersion{element->id, element->title};
        }
    }
    return versions;
}

/// The target length of element @p element of the set in the unit @p words; 0: none
int targetOf(const core::ProjectManager& book, const core::ProjectElement& element, bool words) {
    if (core::ProjectManager::isPublishingText(element)) {
        return book.publishingTargetOf(element.id, words);
    }
    return ofCover(element.cover.part) ? CoverMaterial::targetOf(element.cover, words) : 0;
}

/// Part @p part of the set of @p book as its page shows it
PublishingSetItem itemOf(const core::ProjectManager& book, const core::PublishingSetPart& part) {
    PublishingSetItem item;
    item.part = part.part;
    item.name = PublishingNames::partName(part.part);
    item.elementId = part.elementId;
    item.on = part.on;
    item.fromCover = ofCover(part.part);
    item.italic = part.part == QLatin1String(PublishingTexts::PITCH) ||
                  part.part == QLatin1String(PublishingTexts::TAGLINE);
    item.versions = versionsOf(book, part.part, part.elementId);
    const core::ProjectElement* element = book.findElement(part.elementId);
    if (element == nullptr) {
        return item;
    }
    item.text = book.publishingTextOf(element->id).trimmed();
    const bool words = CoverMaterial::countsWords();
    item.length = PublishingNames::lengthText(PublishingTexts::lengthOf(item.text, words),
                                              targetOf(book, *element, words), words);
    return item;
}

}  // namespace

QList<PublishingSetItem> PublishingSetContents::itemsOf(const core::ProjectManager& book) {
    QList<PublishingSetItem> items;
    for (const core::PublishingSetPart& part : book.publishingSetParts()) {
        items << itemOf(book, part);
    }
    return items;
}

QList<core::PublishingSetText> PublishingSetContents::textsOf(const core::ProjectManager& book) {
    QList<core::PublishingSetText> texts;
    for (const core::PublishingSetPart& part : book.publishingSetParts()) {
        if (!part.on || part.elementId.isEmpty()) {
            continue;
        }
        const QString text = book.publishingTextOf(part.elementId).trimmed();
        if (!text.isEmpty()) {
            texts << core::PublishingSetText{PublishingNames::partName(part.part), text};
        }
    }
    return texts;
}

QString PublishingSetContents::titleOf(const core::ProjectManager& book) {
    const core::ProjectBook* shown = book.book();
    return shown != nullptr ? shown->title : QString();
}

QString PublishingSetContents::aboutOf(const core::ProjectManager& book) {
    const core::ProjectBook* shown = book.book();
    return shown != nullptr ? shown->author : QString();
}

QString PublishingSetContents::plainTextOf(const core::ProjectManager& book) {
    return PublishingTexts::setToPlainText(titleOf(book), aboutOf(book), textsOf(book));
}

QString PublishingSetContents::headingOf(const core::ProjectManager& book) {
    const QList<core::PublishingSetPart> parts = book.publishingSetParts();
    const auto in = std::count_if(parts.cbegin(), parts.cend(),
                                  [](const core::PublishingSetPart& part) { return part.on; });
    //: The name of the set for a publisher with how many of its parts it has: %1 those in it,
    //: %2 all. In Polish: Zestaw dla wydawnictwa · %1 z %2
    return tr("Set for the Publisher · %1 of %2")
        .arg(QLocale().toString(static_cast<int>(in)),
             QLocale().toString(static_cast<int>(parts.size())));
}

}  // namespace kalahari::gui
