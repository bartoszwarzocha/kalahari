/// @file test_editor_panel.cpp
/// @brief The editor panel between the settings and its editor

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/panels/editor_panel.h"

using namespace kalahari;

TEST_CASE("Editor panel: the paper chosen in the editor is kept in the settings", "[gui][editor]") {
    // Regression: Switch to Light Mode / Switch to Dark Mode in the editor's context menu
    // changed only the editor on the screen, so the next start, or Apply in the settings,
    // brought the saved paper back
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>("editor.darkMode", true);
    gui::EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    REQUIRE(bookEditor->editorColorMode() == editor::EditorColorMode::Dark);

    bookEditor->toggleEditorColorMode();
    CHECK(bookEditor->editorColorMode() == editor::EditorColorMode::Light);
    CHECK_FALSE(settings.get<bool>("editor.darkMode", true));

    // The settings applied again keep the paper
    panel.applySettings();
    CHECK(bookEditor->editorColorMode() == editor::EditorColorMode::Light);

    bookEditor->toggleEditorColorMode();
    CHECK(bookEditor->editorColorMode() == editor::EditorColorMode::Dark);
    CHECK(settings.get<bool>("editor.darkMode", false));
}

TEST_CASE("Editor panel: Focus comes from the settings", "[gui][editor]") {
    auto& settings = core::SettingsManager::getInstance();
    gui::EditorPanel panel;
    CHECK_FALSE(panel.getBookEditor()->isFocusModeEnabled());

    // View > Focus keeps its state in the settings of all editors
    settings.set<bool>("editor.focus.enabled", true);
    panel.applySettings();
    CHECK(panel.getBookEditor()->isFocusModeEnabled());

    settings.set<bool>("editor.focus.enabled", false);
    panel.applySettings();
    CHECK_FALSE(panel.getBookEditor()->isFocusModeEnabled());
}
