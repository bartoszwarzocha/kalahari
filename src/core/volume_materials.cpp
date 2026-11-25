// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file volume_materials.cpp
/// @brief The materials of the cover and the texts for publishers of the volumes of a series

#include <kalahari/core/volume_materials.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/material_kinds.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/publishing_texts.h>

#include <QList>

#include <utility>

namespace kalahari::core {

namespace {

/// Each element of @p elements and of its groups, in order (a group before its elements);
/// @p List is const for the elements not to change
template <typename Element, typename List>
QList<Element*> allIn(List& elements) {
    QList<Element*> all;
    QList<std::pair<List*, qsizetype>> lists{{&elements, 0}};
    while (!lists.isEmpty()) {
        auto& [list, next] = lists.last();
        if (next >= list->size()) {
            lists.removeLast();
            continue;
        }
        Element& element = (*list)[next];
        ++next;
        all << &element;
        if (!element.elements.isEmpty()) {
            lists.append({&element.elements, 0});
        }
    }
    return all;
}

/// Each element of the Workshop of @p project and of its groups, in order, to change
QList<ProjectElement*> workshopOf(BookProject& project) {
    return allIn<ProjectElement>(project.workshop.elements);
}

/// Whether @p material is chosen: for the set, or for the cover
bool isChosen(const ProjectElement& material) {
    return material.publishing.state == QLatin1String(PublishingTexts::CHOSEN) ||
           material.cover.state == QLatin1String(CoverMaterial::CHOSEN);
}

/// Whether @p other, chosen, takes the place of @p chosen: a text for publishers of its part,
/// a material of the cover of its part and place, where one is chosen
bool samePlace(const ProjectElement& chosen, const ProjectElement& other) {
    if (!chosen.publishing.part.isEmpty()) {
        return other.publishing.part == chosen.publishing.part &&
               other.publishing.state == QLatin1String(PublishingTexts::CHOSEN);
    }
    return CoverMaterial::choosesOne(chosen.cover.part) &&
           other.cover.part == chosen.cover.part && other.cover.place == chosen.cover.place &&
           other.cover.state == QLatin1String(CoverMaterial::CHOSEN);
}

}  // namespace

bool VolumeMaterials::isVolumeMaterial(const ProjectElement& element) {
    if (!element.cover.part.isEmpty() || !element.publishing.part.isEmpty()) {
        return true;
    }
    const KindReference& kind = element.kind;
    return kind.packageId == QLatin1String(ProjectManager::BASE_PACKAGE) &&
           (kind.kindId == QLatin1String(MaterialKinds::COVER) ||
            kind.kindId == QLatin1String(PublishingTexts::KIND));
}

QString VolumeMaterials::volumeOf(const BookProject& project, const ProjectElement& material) {
    if (project.books.size() < 2 || material.bookId.isEmpty() ||
        project.bookIndex(material.bookId) < 0) {
        return {};
    }
    return material.bookId;
}

bool VolumeMaterials::isFor(const BookProject& project, const ProjectElement& material,
                            const ProjectBook& book) {
    const QString volume = volumeOf(project, material);
    return volume.isEmpty() || volume == book.id;
}

bool VolumeMaterials::sameVolume(const BookProject& project, const ProjectElement& first,
                                 const ProjectElement& second) {
    return volumeOf(project, first) == volumeOf(project, second);
}

QList<const ProjectElement*> VolumeMaterials::chosenForCover(const BookProject& project,
                                                             const ProjectBook& book) {
    QList<const ProjectElement*> chosen =
        allIn<const ProjectElement>(project.workshop.elements);
    chosen.removeIf([&project, &book](const ProjectElement* material) {
        return material->cover.state != QLatin1String(CoverMaterial::CHOSEN) ||
               !isFor(project, *material, book);
    });
    return chosen;
}

QStringList VolumeMaterials::unchooseOthers(BookProject& project, const ProjectElement& chosen) {
    if (!isChosen(chosen)) {
        return {};
    }
    // A copy: the element may be one of those that change
    const ProjectElement kept = chosen;
    QStringList drafts;
    for (ProjectElement* other : workshopOf(project)) {
        if (other->id == kept.id || !samePlace(kept, *other) ||
            !sameVolume(project, kept, *other)) {
            continue;
        }
        if (!kept.publishing.part.isEmpty()) {
            other->publishing.state.clear();
        } else {
            other->cover.state.clear();
        }
        drafts << other->id;
    }
    return drafts;
}

void VolumeMaterials::toFirstVolume(BookProject& project) {
    if (project.books.size() != 1) {
        return;
    }
    const QString bookId = project.books.first().id;
    for (ProjectElement* material : workshopOf(project)) {
        if (isVolumeMaterial(*material)) {
            material->bookId = bookId;
        }
    }
}

void VolumeMaterials::release(BookProject& project, const QString& bookId) {
    for (ProjectElement* material : workshopOf(project)) {
        if (material->bookId != bookId) {
            continue;
        }
        material->bookId.clear();
        if (isChosen(*material)) {
            material->publishing.state.clear();
            material->cover.state.clear();
        }
    }
}

}  // namespace kalahari::core
