// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file volume_names.h
/// @brief The volumes of a series that the materials of the cover and the texts for
/// publishers are for, in the program's words

#ifndef KALAHARI_GUI_VOLUME_NAMES_H
#define KALAHARI_GUI_VOLUME_NAMES_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>

class QComboBox;

namespace kalahari {
namespace core {
class ProjectManager;
struct ProjectElement;
}  // namespace core

namespace gui {

/// @brief The volume of a material of the cover or a text for publishers of a series
/// (core::VolumeMaterials) in the program's language
class VolumeNames {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::VolumeNames)

public:
    /// @brief The name of the whole series as the volume of a material: "Whole series"
    static QString wholeSeriesName();

    /// @brief The name of the volume of @p material of the series open in @p book: "Volume 2:
    /// Dunes" (NavigatorPanel::volumeName()), or wholeSeriesName(); empty when the book is no
    /// series
    static QString nameOf(const core::ProjectManager& book, const core::ProjectElement& material);

    /// @brief The place of the volume of @p material in the series open in @p book: 0 for the
    /// whole series (and a book that is no series), 1 for the first volume...
    static int orderOf(const core::ProjectManager& book, const core::ProjectElement& material);

    /// @brief Give @p box the whole series and the volumes of the series open in @p book, each
    /// with its id (empty for the whole series), @p bookId chosen
    static void fillBox(QComboBox& box, const core::ProjectManager& book, const QString& bookId);

    /// @brief What the status bar says when material @p title is for volume @p volume (its
    /// name) now: also those of @p drafts, chosen for its place there before, are drafts again
    static QString changeMessage(const QString& title, const QString& volume,
                                 const QStringList& drafts);
};

}  // namespace gui
}  // namespace kalahari

#endif  // KALAHARI_GUI_VOLUME_NAMES_H
