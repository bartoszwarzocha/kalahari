/// @file test_view_modes.cpp
/// @brief Unit tests for EditorAppearance and the view modes of BookEditor

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/view_modes.h>
#include <kalahari/editor/book_editor.h>
#include <QApplication>
#include <QList>

using namespace kalahari::editor;

// =============================================================================
// EditorAppearance Tests
// =============================================================================

TEST_CASE("EditorColors give the colors of the light and of the dark paper", "[appearance][colors]") {
    EditorColors colors;
    colors.continuous.backgroundLight = QColor(250, 250, 245);
    colors.continuous.textLight = QColor(20, 20, 20);
    colors.continuous.backgroundDark = QColor(30, 30, 35);
    colors.continuous.textDark = QColor(230, 230, 230);
    colors.focus.inactiveLight = QColor(160, 160, 160);
    colors.focus.inactiveDark = QColor(110, 110, 115);

    CHECK(colors.background(EditorColorMode::Light) == QColor(250, 250, 245));
    CHECK(colors.textColor(EditorColorMode::Light) == QColor(20, 20, 20));
    CHECK(colors.focusInactiveColor(EditorColorMode::Light) == QColor(160, 160, 160));
    CHECK(colors.background(EditorColorMode::Dark) == QColor(30, 30, 35));
    CHECK(colors.textColor(EditorColorMode::Dark) == QColor(230, 230, 230));
    CHECK(colors.focusInactiveColor(EditorColorMode::Dark) == QColor(110, 110, 115));
}

TEST_CASE("PageLayout page sizes in millimetres and their ids", "[appearance][layout]") {
    PageLayout layout;
    CHECK(layout.pageSizeMm() == QSizeF(210.0, 297.0));  // A4

    layout.pageSize = PageLayout::PageSize::Letter;
    CHECK(layout.pageSizeMm() == QSizeF(215.9, 279.4));

    layout.pageSize = PageLayout::PageSize::Custom;
    layout.customWidth = 120.0;
    layout.customHeight = 190.0;
    CHECK(layout.pageSizeMm() == QSizeF(120.0, 190.0));

    // The ids saved in the settings come back as the same sizes; an unknown one is A4
    for (PageLayout::PageSize size :
         {PageLayout::PageSize::A4, PageLayout::PageSize::A5, PageLayout::PageSize::B5,
          PageLayout::PageSize::Trade6x9, PageLayout::PageSize::Letter,
          PageLayout::PageSize::Legal, PageLayout::PageSize::Custom}) {
        CHECK(PageLayout::pageSizeFromId(PageLayout::pageSizeId(size)) == size);
    }
    CHECK(PageLayout::pageSizeId(PageLayout::PageSize::Trade6x9) == QStringLiteral("6x9"));
    CHECK(PageLayout::pageSizeFromId(QStringLiteral("A3")) == PageLayout::PageSize::A4);
}

// =============================================================================
// BookEditor Integration Tests
// =============================================================================

TEST_CASE("BookEditor default viewMode is Continuous", "[editor][viewmodes]") {
    BookEditor editor;

    REQUIRE(editor.viewMode() == ViewMode::Continuous);
}

TEST_CASE("BookEditor setViewMode changes mode", "[editor][viewmodes]") {
    BookEditor editor;

    SECTION("Can set to Page mode") {
        editor.setViewMode(ViewMode::Page);
        REQUIRE(editor.viewMode() == ViewMode::Page);
    }

    SECTION("Typewriter scrolling is independent of the view mode") {
        editor.setViewMode(ViewMode::Page);
        editor.setTypewriterEnabled(true);
        REQUIRE(editor.viewMode() == ViewMode::Page);
        REQUIRE(editor.isTypewriterEnabled());
        editor.setViewMode(ViewMode::Continuous);
        REQUIRE(editor.isTypewriterEnabled());
    }

    SECTION("Focus is independent of the view mode") {
        editor.setViewMode(ViewMode::Page);
        editor.setFocusModeEnabled(true);
        REQUIRE(editor.viewMode() == ViewMode::Page);
        REQUIRE(editor.isFocusModeEnabled());
        editor.setViewMode(ViewMode::Continuous);
        REQUIRE(editor.isFocusModeEnabled());
        editor.setFocusModeEnabled(false);
        REQUIRE_FALSE(editor.isFocusModeEnabled());
        REQUIRE(editor.viewMode() == ViewMode::Continuous);
    }

    SECTION("Distraction-Free is independent of the view mode") {
        editor.setViewMode(ViewMode::Page);
        editor.setDistractionFree(true);
        REQUIRE(editor.viewMode() == ViewMode::Page);
        REQUIRE(editor.isDistractionFree());
        editor.setViewMode(ViewMode::Continuous);
        REQUIRE(editor.isDistractionFree());
        editor.setDistractionFree(false);
        REQUIRE_FALSE(editor.isDistractionFree());
        REQUIRE(editor.viewMode() == ViewMode::Continuous);
    }

    SECTION("Can switch between modes") {
        editor.setViewMode(ViewMode::Page);
        REQUIRE(editor.viewMode() == ViewMode::Page);

        editor.setViewMode(ViewMode::Continuous);
        REQUIRE(editor.viewMode() == ViewMode::Continuous);
    }
}

TEST_CASE("BookEditor setViewMode emits signal", "[editor][viewmodes][signals]") {
    BookEditor editor;

    ViewMode lastEmittedMode = ViewMode::Continuous;
    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::viewModeChanged,
                     [&](ViewMode mode) {
                         lastEmittedMode = mode;
                         ++signalCount;
                     });

    SECTION("Signal emitted on mode change") {
        editor.setViewMode(ViewMode::Page);
        REQUIRE(signalCount == 1);
        REQUIRE(lastEmittedMode == ViewMode::Page);
    }

    SECTION("Signal not emitted if mode unchanged") {
        editor.setViewMode(ViewMode::Continuous);  // Already Continuous
        REQUIRE(signalCount == 0);
    }

    SECTION("Multiple changes emit multiple signals") {
        editor.setViewMode(ViewMode::Page);
        editor.setViewMode(ViewMode::Continuous);
        editor.setViewMode(ViewMode::Page);
        REQUIRE(signalCount == 3);
        REQUIRE(lastEmittedMode == ViewMode::Page);
    }

    SECTION("Distraction-Free leaves the view mode") {
        editor.setDistractionFree(true);
        editor.setDistractionFree(false);
        REQUIRE(signalCount == 0);
    }
}

TEST_CASE("BookEditor setDistractionFree emits a signal once per change", "[editor][viewmodes][signals]") {
    BookEditor editor;

    QList<bool> emitted;
    QObject::connect(&editor, &BookEditor::distractionFreeModeChanged,
                     [&emitted](bool enabled) { emitted.append(enabled); });

    editor.setDistractionFree(true);
    editor.setDistractionFree(true, QStringLiteral("Press Esc"));  // only the hint changes
    editor.setDistractionFree(false);
    editor.setDistractionFree(false);
    REQUIRE(emitted == QList<bool>{true, false});
}

TEST_CASE("BookEditor default appearance has valid colors", "[editor][appearance]") {
    BookEditor editor;

    const EditorAppearance& appearance = editor.appearance();

    SECTION("Colors are valid") {
        REQUIRE(appearance.colors.background(appearance.colorMode).isValid());
        REQUIRE(appearance.colors.textColor(appearance.colorMode).isValid());
        REQUIRE(appearance.colors.selection.isValid());
    }

    SECTION("Typography is valid") {
        REQUIRE(!appearance.typography.textFont.family().isEmpty());
        REQUIRE(appearance.typography.textFont.pointSize() > 0);
        REQUIRE(appearance.typography.lineHeight > 0);
    }
}

TEST_CASE("BookEditor setAppearance changes appearance", "[editor][appearance]") {
    BookEditor editor;

    EditorAppearance custom = editor.appearance();
    custom.colorMode = EditorColorMode::Light;
    custom.colors.continuous.backgroundLight = QColor(250, 245, 230);
    custom.typography.lineHeight = 2.0;
    custom.textFrameBorder.show = true;
    editor.setAppearance(custom);

    CHECK(editor.appearance().colorMode == EditorColorMode::Light);
    CHECK(editor.appearance().colors.continuous.backgroundLight == QColor(250, 245, 230));
    CHECK_THAT(editor.appearance().typography.lineHeight, Catch::Matchers::WithinRel(2.0, 0.001));
    CHECK(editor.appearance().textFrameBorder.show);
}

TEST_CASE("BookEditor setAppearance emits signal", "[editor][appearance][signals]") {
    BookEditor editor;

    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::appearanceChanged,
                     [&]() {
                         ++signalCount;
                     });

    EditorAppearance appearance = editor.appearance();
    editor.setAppearance(appearance);
    REQUIRE(signalCount == 1);

    appearance.typography.lineHeight = 2.0;
    editor.setAppearance(appearance);
    appearance.colorMode = EditorColorMode::Light;
    editor.setAppearance(appearance);
    REQUIRE(signalCount == 3);
}
