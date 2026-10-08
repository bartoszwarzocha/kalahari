/// @file test_editor_panel.cpp
/// @brief The editor panel between the settings and its editor

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <algorithm>
#include <memory>

using namespace kalahari;
using Catch::Approx;

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

TEST_CASE("Editor panel: every document opens at 100%, or at the page's width when it is wider",
          "[gui][editor]") {
    // Regression: only the first chapter of the run fitted a small screen; a chapter
    // opened after it opened at 100%, wider than the editor
    const auto open = [](int editorWidth) {
        auto panel = std::make_unique<gui::EditorPanel>();
        test::resizeWidget(*panel, QSize(editorWidth, 600));  // the size of its tab
        editor::BookEditor* bookEditor = panel->getBookEditor();
        test::resizeWidget(*bookEditor, panel->size());  // all of the panel
        bookEditor->fromKml(test::kmlOf({QStringLiteral("A chapter")}));
        return panel;
    };

    // The width of the page at 100%, as Page Width counts it: the screen's resolution and
    // the paper's scale make it differ between the systems
    const auto probe = open(1000);
    probe->getBookEditor()->zoomToPageWidth();
    const double pageAt100 = 1000.0 / probe->getBookEditor()->zoomFactor();

    struct Opened {
        double editorPerPage;  ///< The editor's width for the page's width at 100%
        bool pageWiderThanEditor;
    };
    for (const Opened& document : {Opened{0.6, true}, Opened{0.6, true}, Opened{1.5, false}}) {
        const int editorWidth = static_cast<int>(document.editorPerPage * pageAt100);
        CAPTURE(editorWidth);
        const auto panel = open(editorWidth);
        editor::BookEditor* bookEditor = panel->getBookEditor();
        const double opened = bookEditor->zoomFactor();

        bookEditor->zoomToPageWidth();
        const double pageWidth = bookEditor->zoomFactor();
        REQUIRE((pageWidth < 1.0) == document.pageWiderThanEditor);
        CHECK(opened == Approx(std::min(1.0, pageWidth)));
    }
}
