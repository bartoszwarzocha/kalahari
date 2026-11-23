// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set.h
/// @brief The set for a publisher of the open book in the program's words: its parts as its
///        page shows them, and its text for Copy All and the export

#ifndef KALAHARI_GUI_PUBLISHING_SET_H
#define KALAHARI_GUI_PUBLISHING_SET_H

#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <QCoreApplication>
#include <QList>
#include <QString>

class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectBook;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The set for a publisher of the book open in a window (core::PublishingSet)
class PublishingSetContents {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::PublishingSetContents)

public:
    /// @brief The parts of the set of the book open in @p book, in its order, as its page
    /// shows them (PublishingSetView): each with its versions, its text and its length
    static QList<PublishingSetItem> itemsOf(const core::ProjectManager& book);

    /// @brief The parts in the set of volume @p volume of the book open in @p book (nullptr:
    /// of its current book) that have a text, each under its name: what Copy All and the
    /// export give
    static QList<core::PublishingSetText> textsOf(const core::ProjectManager& book,
                                                  const core::ProjectBook* volume = nullptr);

    /// @brief The title of volume @p volume of the book open in @p book (nullptr: of its
    /// current book)
    static QString titleOf(const core::ProjectManager& book,
                           const core::ProjectBook* volume = nullptr);

    /// @brief What the set says under the title: the author of volume @p volume of the book
    /// open in @p book (nullptr: of its current book); empty: none
    static QString aboutOf(const core::ProjectManager& book,
                           const core::ProjectBook* volume = nullptr);

    /// @brief The whole set as plain text: the title, the author, then each part under its
    /// name (Copy All)
    static QString plainTextOf(const core::ProjectManager& book);

    /// @brief Copy the whole set to the clipboard as plain text (Copy All)
    /// @return What the status bar says; empty when the set has no text to copy
    static QString copyAll(const core::ProjectManager& book);

    /// @brief Ask before the set goes out when its synopsis has points of its template with no
    ///        fragment linked to them in its text (PublishingTexts::warnsOfPoints()); going back
    ///        to the synopsis is the default
    /// @param parent Parent of the window
    /// @param exporting Export Set...; else Copy All
    /// @param back Gets the synopsis when the writer goes back to it
    /// @return Whether the set goes out
    static bool confirmPoints(const core::ProjectManager& book, QWidget* parent, bool exporting,
                              QString* back);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_SET_H
