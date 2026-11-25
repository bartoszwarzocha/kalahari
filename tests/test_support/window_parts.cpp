// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file window_parts.cpp
/// @brief The book and the commands of the one book window of the tests

#include "kalahari_tests/test_support/window_parts.h"

#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/command_registry.h>

namespace kalahari::test {

core::ProjectManager& book() {
    // As the window's: the books it opens go to the recent books
    static core::ProjectManager& book = []() -> core::ProjectManager& {
        static core::ProjectManager made;
        core::RecentBooksManager::getInstance().follow(made);
        return made;
    }();
    return book;
}

gui::CommandRegistry& commands() {
    static gui::CommandRegistry commands;
    return commands;
}

} // namespace kalahari::test
