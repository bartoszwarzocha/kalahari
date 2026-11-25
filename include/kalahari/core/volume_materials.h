// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file volume_materials.h
/// @brief The materials of the cover and the texts for publishers of the volumes of a series:
///        each is for one volume, or for the whole series

#ifndef KALAHARI_CORE_VOLUME_MATERIALS_H
#define KALAHARI_CORE_VOLUME_MATERIALS_H

#include <QList>
#include <QString>
#include <QStringList>

namespace kalahari::core {

struct BookProject;
struct ProjectBook;
struct ProjectElement;

/// @brief Which volume of a series a material of the cover or a text for publishers is for
///
/// Such a material of the Workshop is for the volume its ProjectElement::bookId names, or for
/// the whole series when it names none (or a book the project no longer has). Each volume has
/// its own versions chosen: a material chosen for a volume takes the place only of those of
/// its volume, and a volume without a version chosen of its own has the one of the whole
/// series. A book that is not a series has all of them.
struct VolumeMaterials {
    /// @brief Whether @p element is a material of the cover or a text for publishers, which is
    /// for a volume of a series
    static bool isVolumeMaterial(const ProjectElement& element);

    /// @brief The id of the volume of @p project that @p material is for; empty: the whole
    /// series, also when @p project is no series
    static QString volumeOf(const BookProject& project, const ProjectElement& material);

    /// @brief Whether @p material is for @p book of @p project: for that volume or for the
    /// whole series; any material is for the book of a project that is no series
    static bool isFor(const BookProject& project, const ProjectElement& material,
                      const ProjectBook& book);

    /// @brief Whether @p first and @p second are for the same volume of @p project, or both
    /// for the whole series
    static bool sameVolume(const BookProject& project, const ProjectElement& first,
                           const ProjectElement& second);

    /// @brief The materials of the cover chosen for @p book of @p project: of its volume and
    /// of the whole series (isFor()), in the order of the Workshop
    static QList<const ProjectElement*> chosenForCover(const BookProject& project,
                                                       const ProjectBook& book);

    /// @brief The materials chosen in the place of @p chosen, other than it, become drafts:
    /// the texts for publishers of its part, the materials of the cover of its part and place
    /// when one is chosen for it (CoverMaterial::choosesOne()); of its volume only. Nothing
    /// changes when @p chosen is not chosen.
    /// @return The ids of the drafts
    static QStringList unchooseOthers(BookProject& project, const ProjectElement& chosen);

    /// @brief Before @p project of one book becomes a series: all its materials of the cover
    /// and texts for publishers, which are its book's, become those of the first volume;
    /// nothing changes in a series
    static void toFirstVolume(BookProject& project);

    /// @brief The materials of book @p bookId, which leaves the series, become those of the
    /// whole series, and drafts, so as not to take the place of those chosen
    static void release(BookProject& project, const QString& bookId);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_VOLUME_MATERIALS_H
