/// @file window_parts.cpp
/// @brief The book and the commands of the one book window of the tests

#include "window_parts.h"

#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/command_registry.h>

namespace kalahari {
namespace test {

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

}  // namespace test
}  // namespace kalahari
