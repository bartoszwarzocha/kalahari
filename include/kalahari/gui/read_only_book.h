// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file read_only_book.h
/// @brief What the program says of a book open read-only, as the program does not have all the
/// book types the book uses (core::ProjectManager::isReadOnly())

#pragma once

#include <QString>

namespace kalahari {
namespace core {
class ProjectManager;
} // namespace core

namespace gui {

/// @brief Why a command, a field or a panel that would change the open book is greyed out
/// while the book is open read-only: "The book is open read-only"
QString readOnlyReason();

/// @brief Why the book open in @p book is open read-only, and what can be done with it: "The book
/// is open read-only: the program does not have its type "Novel". You can read it and export
/// it."; empty while no book is open in it or the book is not read-only
///
/// The type and the packages are named after the book's cards of them, or after their ids.
QString readOnlyBookMessage(const core::ProjectManager& book);

} // namespace gui
} // namespace kalahari
