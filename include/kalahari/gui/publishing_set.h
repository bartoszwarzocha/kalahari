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
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The set for a publisher of the book open in a window (core::PublishingSet)
class PublishingSetContents {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::PublishingSetContents)

public:
    /// @brief The parts of the set of the book open in @p book, in its order, as its page
    /// shows them (PublishingSetView): each with its versions, its text and its length
    static QList<PublishingSetItem> itemsOf(const core::ProjectManager& book);

    /// @brief The parts in the set that have a text, each under its name: what Copy All and
    /// the export give
    static QList<core::PublishingSetText> textsOf(const core::ProjectManager& book);

    /// @brief The title of the book open in @p book
    static QString titleOf(const core::ProjectManager& book);

    /// @brief What the set says under the title: the author of the book; empty: none
    static QString aboutOf(const core::ProjectManager& book);

    /// @brief The whole set as plain text: the title, the author, then each part under its
    /// name (Copy All)
    static QString plainTextOf(const core::ProjectManager& book);

    /// @brief The name of the set with the parts in it: "Set for the Publisher · 4 of 5"
    static QString headingOf(const core::ProjectManager& book);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_SET_H
