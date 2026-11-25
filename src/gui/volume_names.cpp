// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file volume_names.cpp
/// @brief The volumes of the materials of the cover and the texts for publishers in words

#include "kalahari/gui/volume_names.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/volume_materials.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QComboBox>
#include <QList>
#include <QSignalBlocker>

#include <algorithm>

namespace kalahari::gui {

QString VolumeNames::wholeSeriesName() {
    //: The volume of a material of the cover or a text for publishers that is for all the
    //: volumes of a series. In Polish: Cała seria
    return tr("Whole series");
}

QString VolumeNames::nameOf(const core::ProjectManager& book,
                            const core::ProjectElement& material) {
    const core::BookProject* project = book.project();
    if (project == nullptr || !book.isSeries()) {
        return {};
    }
    const qsizetype index =
        project->bookIndex(core::VolumeMaterials::volumeOf(*project, material));
    return index >= 0 ? NavigatorPanel::volumeName(project->books.at(index), index)
                      : wholeSeriesName();
}

int VolumeNames::orderOf(const core::ProjectManager& book, const core::ProjectElement& material) {
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return 0;
    }
    const qsizetype index =
        project->bookIndex(core::VolumeMaterials::volumeOf(*project, material));
    return static_cast<int>(index + 1);
}

void VolumeNames::fillBox(QComboBox& box, const core::ProjectManager& book,
                          const QString& bookId) {
    const QSignalBlocker blocker(&box);
    box.clear();
    box.addItem(wholeSeriesName(), QString());
    if (const core::BookProject* project = book.project()) {
        const QList<core::ProjectBook>& volumes = project->books;
        for (qsizetype i = 0; i < volumes.size(); ++i) {
            box.addItem(NavigatorPanel::volumeName(volumes.at(i), i), volumes.at(i).id);
        }
    }
    box.setCurrentIndex(std::max(0, box.findData(bookId)));
}

QString VolumeNames::changeMessage(const QString& title, const QString& volume,
                                   const QStringList& drafts) {
    //: The status bar: a material of the cover or a text for publishers is for another volume
    //: of the series, or for the whole series; %1 its title, %2 the volume. In Polish: „%1”
    //: dotyczy teraz: %2
    QString text = tr("“%1” is now for: %2").arg(title, volume);
    for (const QString& draft : drafts) {
        //: The status bar, after a material is given to another volume: the one chosen there
        //: before for its place is a draft again; %1 its title. In Polish: „%1” wraca do
        //: roboczych
        text += QStringLiteral(" · ") + tr("“%1” is a draft again").arg(draft);
    }
    return text;
}

}  // namespace kalahari::gui
