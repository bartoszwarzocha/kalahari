// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file shared_history.h
/// @brief A step that changed more texts than one at once, undone and redone in all of them

#pragma once

namespace kalahari::editor {

class BookEditor;

/// @brief Steps that change more texts than one at once (Replace All in Book): while such a
///        step is the last of a text, the writer's Undo in the text (Redo, once it was undone)
///        does it in every text it changed, not in this one alone
///        (BookEditor::undoCommand())
class SharedHistory {
public:
    virtual ~SharedHistory() = default;

    /// @brief Whether the writer's Undo in @p text undoes a step of all the texts now
    [[nodiscard]] virtual bool undoesFor(const BookEditor& text) const = 0;

    /// @brief Whether the writer's Redo in @p text redoes a step of all the texts now
    [[nodiscard]] virtual bool redoesFor(const BookEditor& text) const = 0;

    /// @brief Undo the step in every text it changed (undoesFor() said so)
    virtual void undoFor(BookEditor& text) = 0;

    /// @brief Redo the step in every text it changed (redoesFor() said so)
    virtual void redoFor(BookEditor& text) = 0;

protected:
    SharedHistory() = default;
    SharedHistory(const SharedHistory&) = default;
    SharedHistory& operator=(const SharedHistory&) = default;
};

}  // namespace kalahari::editor
