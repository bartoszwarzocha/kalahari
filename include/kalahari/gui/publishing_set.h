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

    /// @brief The name of the set with the parts in it: "Set for the Publisher · 4 of 5"
    static QString headingOf(const core::ProjectManager& book);

    /// @brief The parts in the set and the length of their texts in all, in the unit of the
    /// cover: "5 parts · 1,234 characters in all"
    static QString summaryOf(const core::ProjectManager& book);

    /// @brief Where the text of @p part comes from: "Texts for Publishers", "Cover", "The back
    /// matter of the book"
    static QString sourceOf(const QString& part);

    /// @brief Copy the whole set to the clipboard as plain text (Copy All)
    /// @return What the status bar says; empty when the set has no text to copy
    static QString copyAll(const core::ProjectManager& book);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_SET_H
