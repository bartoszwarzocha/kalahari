// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file window_parts.h
/// @brief The book and the commands of the one book window of the tests
///
/// The program gives each book window its own book (core::ProjectManager) and its own commands
/// (gui::CommandRegistry). The tests build the parts of a window without the window: these are
/// the book and the commands those parts get, the same for the whole run. Their commands are
/// cleared before each test case (resetSingletons()); the book is closed by the tests that open it.

#pragma once

namespace kalahari {
namespace core {
class ProjectManager;
}  // namespace core
namespace gui {
class CommandRegistry;
}  // namespace gui

namespace test {

/// @brief The book of the window of the tests
core::ProjectManager& book();

/// @brief The commands of the window of the tests
gui::CommandRegistry& commands();

}  // namespace test
}  // namespace kalahari
