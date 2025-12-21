/// @file test_view_modes.cpp
/// @brief Unit tests for EditorAppearance and ViewModes (OpenSpec #00042 Phase 5)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/view_modes.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <QApplication>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// EditorColors Tests
// =============================================================================

TEST_CASE("EditorColors lightTheme returns valid colors", "[appearance][colors]") {
    EditorColors colors = EditorColors::lightTheme();

    SECTION("Background colors are valid") {
        REQUIRE(colors.editorBackground.isValid());
        REQUIRE(colors.pageBackground.isValid());
        REQUIRE(colors.pageShadow.isValid());
        REQUIRE(colors.marginArea.isValid());
    }

    SECTION("Text colors are valid") {
        REQUIRE(colors.text.isValid());
        REQUIRE(colors.textSecondary.isValid());
        REQUIRE(colors.textDimmed.isValid());
    }

    SECTION("Selection and cursor colors are valid") {
        REQUIRE(colors.selection.isValid());
        REQUIRE(colors.selectionBorder.isValid());
        REQUIRE(colors.cursor.isValid());
        REQUIRE(colors.cursorLine.isValid());
    }

    SECTION("UI element colors are valid") {
        REQUIRE(colors.ruler.isValid());
        REQUIRE(colors.rulerMarker.isValid());
        REQUIRE(colors.scrollbar.isValid());
        REQUIRE(colors.scrollbarHover.isValid());
    }

    SECTION("Accent colors are valid") {
        REQUIRE(colors.accent.isValid());
        REQUIRE(colors.accentSecondary.isValid());
        REQUIRE(colors.warning.isValid());
        REQUIRE(colors.error.isValid());
    }

    SECTION("Focus mode colors are valid") {
        REQUIRE(colors.focusHighlight.isValid());
        REQUIRE(colors.focusDimOverlay.isValid());
    }
}

TEST_CASE("EditorColors darkTheme returns different colors than light", "[appearance][colors]") {
    EditorColors light = EditorColors::lightTheme();
    EditorColors dark = EditorColors::darkTheme();

    SECTION("Editor background is different") {
        REQUIRE(dark.editorBackground != light.editorBackground);
    }

    SECTION("Page background is different") {
        REQUIRE(dark.pageBackground != light.pageBackground);
    }

    SECTION("Text color is different") {
        REQUIRE(dark.text != light.text);
    }

    SECTION("Cursor color is different") {
        REQUIRE(dark.cursor != light.cursor);
    }

    SECTION("Dark theme has darker editor background") {
        // Dark theme should have lower lightness
        REQUIRE(dark.editorBackground.lightness() < light.editorBackground.lightness());
    }

    SECTION("Dark theme has lighter text") {
        // Dark theme should have lighter text for contrast
        REQUIRE(dark.text.lightness() > light.text.lightness());
    }
}

TEST_CASE("EditorColors sepiaTheme returns warm colors", "[appearance][colors]") {
    EditorColors sepia = EditorColors::sepiaTheme();
    EditorColors light = EditorColors::lightTheme();

    SECTION("Sepia colors are valid") {
        REQUIRE(sepia.editorBackground.isValid());
        REQUIRE(sepia.pageBackground.isValid());
        REQUIRE(sepia.text.isValid());
    }

    SECTION("Page background has warm tint (more red/yellow than blue)") {
        // Sepia should have warmer tones
        REQUIRE(sepia.pageBackground.red() >= sepia.pageBackground.blue());
    }

    SECTION("Text color is warm brown") {
        // Brown text should have red > blue
        REQUIRE(sepia.text.red() >= sepia.text.blue());
    }

    SECTION("Sepia is different from light theme") {
        REQUIRE(sepia.pageBackground != light.pageBackground);
    }
}

TEST_CASE("EditorColors toJson/fromJson roundtrip preserves colors", "[appearance][colors][json]") {
    EditorColors original = EditorColors::darkTheme();

    QJsonObject json = original.toJson();
    EditorColors restored = EditorColors::fromJson(json);

    SECTION("Background colors preserved") {
        REQUIRE(restored.editorBackground == original.editorBackground);
        REQUIRE(restored.pageBackground == original.pageBackground);
        REQUIRE(restored.pageShadow == original.pageShadow);
        REQUIRE(restored.marginArea == original.marginArea);
    }

    SECTION("Text colors preserved") {
        REQUIRE(restored.text == original.text);
        REQUIRE(restored.textSecondary == original.textSecondary);
        REQUIRE(restored.textDimmed == original.textDimmed);
    }

    SECTION("Selection and cursor colors preserved") {
        REQUIRE(restored.selection == original.selection);
        REQUIRE(restored.selectionBorder == original.selectionBorder);
        REQUIRE(restored.cursor == original.cursor);
        REQUIRE(restored.cursorLine == original.cursorLine);
    }

    SECTION("UI element colors preserved") {
        REQUIRE(restored.ruler == original.ruler);
        REQUIRE(restored.rulerMarker == original.rulerMarker);
        REQUIRE(restored.scrollbar == original.scrollbar);
        REQUIRE(restored.scrollbarHover == original.scrollbarHover);
    }

    SECTION("Accent colors preserved") {
        REQUIRE(restored.accent == original.accent);
        REQUIRE(restored.accentSecondary == original.accentSecondary);
        REQUIRE(restored.warning == original.warning);
        REQUIRE(restored.error == original.error);
    }

    SECTION("Focus mode colors preserved") {
        REQUIRE(restored.focusHighlight == original.focusHighlight);
        REQUIRE(restored.focusDimOverlay == original.focusDimOverlay);
    }
}

// =============================================================================
// VisualElements Tests
// =============================================================================

TEST_CASE("VisualElements default values are sensible", "[appearance][elements]") {
    VisualElements elem;

    SECTION("Rulers are disabled by default") {
        REQUIRE(elem.showHorizontalRuler == false);
        REQUIRE(elem.showVerticalRuler == false);
    }

    SECTION("Ruler dimensions are positive") {
        REQUIRE(elem.rulerHeight > 0);
        REQUIRE(elem.rulerWidth > 0);
    }

    SECTION("Line numbers are disabled by default") {
        REQUIRE(elem.showLineNumbers == false);
        REQUIRE(elem.relativeLineNumbers == false);
    }

    SECTION("Margin guide is disabled by default") {
        REQUIRE(elem.showMarginGuide == false);
        REQUIRE(elem.marginGuideColumn > 0);
    }

    SECTION("Current line highlighting is enabled by default") {
        REQUIRE(elem.highlightCurrentLine == true);
        REQUIRE(elem.highlightCurrentParagraph == false);
    }

    SECTION("Page elements have sensible defaults") {
        REQUIRE(elem.showPageShadows == true);
        REQUIRE(elem.showPageBorders == false);
        REQUIRE(elem.showPageNumbers == true);
    }

    SECTION("Scrollbar is enabled by default") {
        REQUIRE(elem.showScrollbar == true);
        REQUIRE(elem.autoHideScrollbar == true);
        REQUIRE(elem.scrollbarWidth > 0);
    }

    SECTION("Minimap is disabled by default") {
        REQUIRE(elem.showMinimap == false);
        REQUIRE(elem.minimapWidth > 0);
    }
}

TEST_CASE("VisualElements toJson/fromJson roundtrip", "[appearance][elements][json]") {
    VisualElements original;
    original.showHorizontalRuler = true;
    original.showVerticalRuler = true;
    original.rulerHeight = 30;
    original.rulerWidth = 60;
    original.showLineNumbers = true;
    original.relativeLineNumbers = true;
    original.showMarginGuide = true;
    original.marginGuideColumn = 100;
    original.showIndentGuides = true;
    original.highlightCurrentLine = false;
    original.highlightCurrentParagraph = true;
    original.showPageShadows = false;
    original.showPageBorders = true;
    original.showPageNumbers = false;
    original.showScrollbar = false;
    original.autoHideScrollbar = false;
    original.scrollbarWidth = 20;
    original.showMinimap = true;
    original.minimapWidth = 150;

    QJsonObject json = original.toJson();
    VisualElements restored = VisualElements::fromJson(json);

    SECTION("All boolean values preserved") {
        REQUIRE(restored.showHorizontalRuler == original.showHorizontalRuler);
        REQUIRE(restored.showVerticalRuler == original.showVerticalRuler);
        REQUIRE(restored.showLineNumbers == original.showLineNumbers);
        REQUIRE(restored.relativeLineNumbers == original.relativeLineNumbers);
        REQUIRE(restored.showMarginGuide == original.showMarginGuide);
        REQUIRE(restored.showIndentGuides == original.showIndentGuides);
        REQUIRE(restored.highlightCurrentLine == original.highlightCurrentLine);
        REQUIRE(restored.highlightCurrentParagraph == original.highlightCurrentParagraph);
        REQUIRE(restored.showPageShadows == original.showPageShadows);
        REQUIRE(restored.showPageBorders == original.showPageBorders);
        REQUIRE(restored.showPageNumbers == original.showPageNumbers);
        REQUIRE(restored.showScrollbar == original.showScrollbar);
        REQUIRE(restored.autoHideScrollbar == original.autoHideScrollbar);
        REQUIRE(restored.showMinimap == original.showMinimap);
    }

    SECTION("All integer values preserved") {
        REQUIRE(restored.rulerHeight == original.rulerHeight);
        REQUIRE(restored.rulerWidth == original.rulerWidth);
        REQUIRE(restored.marginGuideColumn == original.marginGuideColumn);
        REQUIRE(restored.scrollbarWidth == original.scrollbarWidth);
        REQUIRE(restored.minimapWidth == original.minimapWidth);
    }
}

// =============================================================================
// PageLayout Tests
// =============================================================================

TEST_CASE("PageLayout pageSizePixels returns correct size for A4", "[appearance][layout]") {
    PageLayout layout;
    layout.pageSize = PageLayout::PageSize::A4;
    layout.zoomLevel = 1.0;

    QSizeF size = layout.pageSizePixels(96.0);  // Standard screen DPI

    SECTION("A4 dimensions are correct at 96 DPI") {
        // A4 is 210mm x 297mm
        // At 96 DPI: 210mm * (96/25.4) = ~793.7 pixels
        //           297mm * (96/25.4) = ~1122.5 pixels
        REQUIRE_THAT(size.width(), Catch::Matchers::WithinRel(793.7, 0.01));
        REQUIRE_THAT(size.height(), Catch::Matchers::WithinRel(1122.5, 0.01));
    }

    SECTION("Height is greater than width (portrait)") {
        REQUIRE(size.height() > size.width());
    }
}

TEST_CASE("PageLayout pageSizePixels returns correct size for Letter", "[appearance][layout]") {
    PageLayout layout;
    layout.pageSize = PageLayout::PageSize::Letter;
    layout.zoomLevel = 1.0;

    QSizeF size = layout.pageSizePixels(96.0);

    SECTION("Letter dimensions are correct at 96 DPI") {
        // Letter is 215.9mm x 279.4mm (8.5" x 11")
        // At 96 DPI: 215.9mm * (96/25.4) = ~816 pixels
        //           279.4mm * (96/25.4) = ~1056 pixels
        REQUIRE_THAT(size.width(), Catch::Matchers::WithinRel(816.0, 0.01));
        REQUIRE_THAT(size.height(), Catch::Matchers::WithinRel(1056.0, 0.01));
    }

    SECTION("Letter is wider than A4") {
        PageLayout a4Layout;
        a4Layout.pageSize = PageLayout::PageSize::A4;
        QSizeF a4Size = a4Layout.pageSizePixels(96.0);

        REQUIRE(size.width() > a4Size.width());
    }
}

TEST_CASE("PageLayout textAreaPixels accounts for margins", "[appearance][layout]") {
    PageLayout layout;
    layout.pageSize = PageLayout::PageSize::A4;
    layout.margins = QMarginsF(25.4, 25.4, 25.4, 25.4);  // 1 inch margins
    layout.zoomLevel = 1.0;

    QSizeF pageSize = layout.pageSizePixels(96.0);
    QSizeF textArea = layout.textAreaPixels(96.0);

    SECTION("Text area is smaller than page size") {
        REQUIRE(textArea.width() < pageSize.width());
        REQUIRE(textArea.height() < pageSize.height());
    }

    SECTION("Text area accounts for margins") {
        // With 1 inch (25.4mm) margins on each side
        // Text area should be page size minus 2 inches on each axis
        qreal expectedWidthReduction = 2 * 25.4 * (96.0 / 25.4);  // 2 inches in pixels
        qreal expectedHeightReduction = 2 * 25.4 * (96.0 / 25.4);  // 2 inches in pixels

        REQUIRE_THAT(pageSize.width() - textArea.width(),
                     Catch::Matchers::WithinRel(expectedWidthReduction, 0.01));
        REQUIRE_THAT(pageSize.height() - textArea.height(),
                     Catch::Matchers::WithinRel(expectedHeightReduction, 0.01));
    }
}

TEST_CASE("PageLayout toJson/fromJson roundtrip", "[appearance][layout][json]") {
    PageLayout original;
    original.pageSize = PageLayout::PageSize::Letter;
    original.customWidth = 200.0;
    original.customHeight = 300.0;
    original.margins = QMarginsF(20.0, 30.0, 25.0, 35.0);
    original.zoomLevel = 1.5;
    original.pageGap = 30.0;
    original.centerPages = false;

    QJsonObject json = original.toJson();
    PageLayout restored = PageLayout::fromJson(json);

    SECTION("Page size preserved") {
        REQUIRE(restored.pageSize == original.pageSize);
    }

    SECTION("Custom dimensions preserved") {
        REQUIRE_THAT(restored.customWidth, Catch::Matchers::WithinRel(original.customWidth, 0.001));
        REQUIRE_THAT(restored.customHeight, Catch::Matchers::WithinRel(original.customHeight, 0.001));
    }

    SECTION("Margins preserved") {
        REQUIRE_THAT(restored.margins.left(), Catch::Matchers::WithinRel(original.margins.left(), 0.001));
        REQUIRE_THAT(restored.margins.top(), Catch::Matchers::WithinRel(original.margins.top(), 0.001));
        REQUIRE_THAT(restored.margins.right(), Catch::Matchers::WithinRel(original.margins.right(), 0.001));
        REQUIRE_THAT(restored.margins.bottom(), Catch::Matchers::WithinRel(original.margins.bottom(), 0.001));
    }

    SECTION("Display options preserved") {
        REQUIRE_THAT(restored.zoomLevel, Catch::Matchers::WithinRel(original.zoomLevel, 0.001));
        REQUIRE_THAT(restored.pageGap, Catch::Matchers::WithinRel(original.pageGap, 0.001));
        REQUIRE(restored.centerPages == original.centerPages);
    }
}

// =============================================================================
// EditorAppearance Tests
// =============================================================================

TEST_CASE("EditorAppearance defaultAppearance creates valid appearance", "[appearance]") {
    EditorAppearance appearance = EditorAppearance::defaultAppearance();

    SECTION("Colors are valid") {
        REQUIRE(appearance.colors.editorBackground.isValid());
        REQUIRE(appearance.colors.text.isValid());
        REQUIRE(appearance.colors.cursor.isValid());
    }

    SECTION("Elements have default values") {
        REQUIRE(appearance.elements.highlightCurrentLine == true);
        REQUIRE(appearance.elements.showScrollbar == true);
    }

    SECTION("Typography has valid font") {
        REQUIRE(!appearance.typography.textFont.family().isEmpty());
        REQUIRE(appearance.typography.textFont.pointSize() > 0);
    }

    SECTION("Page layout is valid") {
        REQUIRE(appearance.pageLayout.pageSize == PageLayout::PageSize::A4);
        REQUIRE(appearance.pageLayout.zoomLevel > 0);
    }
}

TEST_CASE("EditorAppearance darkAppearance uses dark colors", "[appearance]") {
    EditorAppearance dark = EditorAppearance::darkAppearance();
    EditorAppearance light = EditorAppearance::defaultAppearance();

    SECTION("Uses dark theme colors") {
        REQUIRE(dark.colors.editorBackground.lightness() < light.colors.editorBackground.lightness());
    }

    SECTION("Text is light on dark background") {
        REQUIRE(dark.colors.text.lightness() > dark.colors.editorBackground.lightness());
    }

    SECTION("Cursor is visible on dark background") {
        REQUIRE(dark.colors.cursor.isValid());
        REQUIRE(dark.colors.cursor != dark.colors.editorBackground);
    }
}

TEST_CASE("EditorAppearance sepiaAppearance uses sepia colors", "[appearance]") {
    EditorAppearance sepia = EditorAppearance::sepiaAppearance();

    SECTION("Uses sepia theme colors") {
        // Sepia page background should have warm tint
        REQUIRE(sepia.colors.pageBackground.red() >= sepia.colors.pageBackground.blue());
    }

    SECTION("Text color is warm") {
        REQUIRE(sepia.colors.text.red() >= sepia.colors.text.blue());
    }
}

TEST_CASE("EditorAppearance saveToFile/loadFromFile roundtrip", "[appearance][file]") {
    EditorAppearance original = EditorAppearance::darkAppearance();
    original.elements.showLineNumbers = true;
    original.elements.showMarginGuide = true;
    original.typography.lineHeight = 2.0;
    original.typewriter.enabled = true;
    original.focusMode.enabled = true;
    original.distractionFree.enabled = true;

    QTemporaryFile tempFile;
    REQUIRE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    SECTION("Save and load preserves appearance") {
        bool saveResult = original.saveToFile(path);
        REQUIRE(saveResult);

        EditorAppearance loaded = EditorAppearance::loadFromFile(path);

        REQUIRE(loaded.colors.editorBackground == original.colors.editorBackground);
        REQUIRE(loaded.elements.showLineNumbers == original.elements.showLineNumbers);
        REQUIRE(loaded.elements.showMarginGuide == original.elements.showMarginGuide);
        REQUIRE_THAT(loaded.typography.lineHeight,
                     Catch::Matchers::WithinRel(original.typography.lineHeight, 0.001));
        REQUIRE(loaded.typewriter.enabled == original.typewriter.enabled);
        REQUIRE(loaded.focusMode.enabled == original.focusMode.enabled);
        REQUIRE(loaded.distractionFree.enabled == original.distractionFree.enabled);
    }

    SECTION("Load from non-existent file returns default") {
        EditorAppearance loaded = EditorAppearance::loadFromFile("/non/existent/path.json");
        EditorAppearance defaultAppearance = EditorAppearance::defaultAppearance();

        // Should return default appearance
        REQUIRE(loaded.colors.editorBackground == defaultAppearance.colors.editorBackground);
    }
}

TEST_CASE("EditorAppearance toJson/fromJson roundtrip", "[appearance][json]") {
    EditorAppearance original = EditorAppearance::typewriterAppearance();

    QJsonObject json = original.toJson();
    EditorAppearance restored = EditorAppearance::fromJson(json);

    SECTION("Colors preserved") {
        REQUIRE(restored.colors.editorBackground == original.colors.editorBackground);
        REQUIRE(restored.colors.text == original.colors.text);
        REQUIRE(restored.colors.pageBackground == original.colors.pageBackground);
    }

    SECTION("Elements preserved") {
        REQUIRE(restored.elements.highlightCurrentLine == original.elements.highlightCurrentLine);
        REQUIRE(restored.elements.showScrollbar == original.elements.showScrollbar);
    }

    SECTION("Typography preserved") {
        REQUIRE(restored.typography.textFont.family() == original.typography.textFont.family());
        REQUIRE_THAT(restored.typography.lineHeight,
                     Catch::Matchers::WithinRel(original.typography.lineHeight, 0.001));
    }

    SECTION("Typewriter settings preserved") {
        REQUIRE(restored.typewriter.enabled == original.typewriter.enabled);
        REQUIRE_THAT(restored.typewriter.focusPosition,
                     Catch::Matchers::WithinRel(original.typewriter.focusPosition, 0.001));
        REQUIRE(restored.typewriter.smoothScroll == original.typewriter.smoothScroll);
    }

    SECTION("Focus mode settings preserved") {
        REQUIRE(restored.focusMode.enabled == original.focusMode.enabled);
        REQUIRE_THAT(restored.focusMode.dimOpacity,
                     Catch::Matchers::WithinRel(original.focusMode.dimOpacity, 0.001));
    }

    SECTION("Distraction-free settings preserved") {
        REQUIRE(restored.distractionFree.enabled == original.distractionFree.enabled);
        REQUIRE(restored.distractionFree.fullscreen == original.distractionFree.fullscreen);
        REQUIRE(restored.distractionFree.showWordCount == original.distractionFree.showWordCount);
    }
}

// =============================================================================
// ViewModeRegistry Tests
// =============================================================================

TEST_CASE("ViewModeRegistry allModes returns all modes", "[viewmodes][registry]") {
    const std::vector<ViewModeInfo>& modes = ViewModeRegistry::allModes();

    SECTION("Returns non-empty list") {
        REQUIRE(!modes.empty());
    }

    SECTION("Contains all expected modes") {
        // Should have 7 modes: Continuous, Page, Typewriter, Focus, DistractionFree, Outline, Split
        REQUIRE(modes.size() == static_cast<size_t>(ViewMode::_Count));
    }

    SECTION("Each mode has valid metadata") {
        for (const auto& mode : modes) {
            REQUIRE(!mode.id.isEmpty());
            REQUIRE(!mode.name.isEmpty());
            REQUIRE(!mode.description.isEmpty());
            REQUIRE(!mode.iconName.isEmpty());
        }
    }

    SECTION("Continuous mode is first") {
        REQUIRE(modes[0].mode == ViewMode::Continuous);
        REQUIRE(modes[0].id == "continuous");
    }
}

TEST_CASE("ViewModeRegistry availableModes returns only available", "[viewmodes][registry]") {
    std::vector<ViewModeInfo> available = ViewModeRegistry::availableModes();
    const std::vector<ViewModeInfo>& all = ViewModeRegistry::allModes();

    SECTION("Available is subset of all") {
        REQUIRE(available.size() <= all.size());
    }

    SECTION("All returned modes are marked available") {
        for (const auto& mode : available) {
            REQUIRE(mode.available == true);
        }
    }

    SECTION("Unavailable modes are not included") {
        for (const auto& mode : all) {
            if (!mode.available) {
                bool found = false;
                for (const auto& avail : available) {
                    if (avail.mode == mode.mode) {
                        found = true;
                        break;
                    }
                }
                REQUIRE(!found);
            }
        }
    }

    SECTION("Continuous mode is available") {
        bool foundContinuous = false;
        for (const auto& mode : available) {
            if (mode.mode == ViewMode::Continuous) {
                foundContinuous = true;
                break;
            }
        }
        REQUIRE(foundContinuous);
    }
}

TEST_CASE("ViewModeRegistry info returns correct info for Continuous", "[viewmodes][registry]") {
    const ViewModeInfo& info = ViewModeRegistry::info(ViewMode::Continuous);

    SECTION("Returns correct mode") {
        REQUIRE(info.mode == ViewMode::Continuous);
    }

    SECTION("Has correct id") {
        REQUIRE(info.id == "continuous");
    }

    SECTION("Has non-empty name") {
        REQUIRE(!info.name.isEmpty());
    }

    SECTION("Has non-empty description") {
        REQUIRE(!info.description.isEmpty());
    }

    SECTION("Is available") {
        REQUIRE(info.available == true);
    }

    SECTION("Has keyboard shortcut") {
        REQUIRE(!info.shortcut.isEmpty());
    }
}

TEST_CASE("ViewModeRegistry modeFromId returns correct mode", "[viewmodes][registry]") {
    SECTION("focus returns Focus mode") {
        ViewMode mode = ViewModeRegistry::modeFromId("focus");
        REQUIRE(mode == ViewMode::Focus);
    }

    SECTION("continuous returns Continuous mode") {
        ViewMode mode = ViewModeRegistry::modeFromId("continuous");
        REQUIRE(mode == ViewMode::Continuous);
    }

    SECTION("page returns Page mode") {
        ViewMode mode = ViewModeRegistry::modeFromId("page");
        REQUIRE(mode == ViewMode::Page);
    }

    SECTION("typewriter returns Typewriter mode") {
        ViewMode mode = ViewModeRegistry::modeFromId("typewriter");
        REQUIRE(mode == ViewMode::Typewriter);
    }

    SECTION("distraction-free returns DistractionFree mode") {
        ViewMode mode = ViewModeRegistry::modeFromId("distraction-free");
        REQUIRE(mode == ViewMode::DistractionFree);
    }

    SECTION("Unknown id returns Continuous (default)") {
        ViewMode mode = ViewModeRegistry::modeFromId("unknown-mode");
        REQUIRE(mode == ViewMode::Continuous);
    }

    SECTION("Empty id returns Continuous (default)") {
        ViewMode mode = ViewModeRegistry::modeFromId("");
        REQUIRE(mode == ViewMode::Continuous);
    }
}

TEST_CASE("ViewModeRegistry nextMode cycles correctly", "[viewmodes][registry]") {
    std::vector<ViewModeInfo> available = ViewModeRegistry::availableModes();

    SECTION("Next from first available returns second available") {
        if (available.size() >= 2) {
            ViewMode next = ViewModeRegistry::nextMode(available[0].mode);
            REQUIRE(next == available[1].mode);
        }
    }

    SECTION("Next from last available wraps to first") {
        if (!available.empty()) {
            ViewMode last = available.back().mode;
            ViewMode next = ViewModeRegistry::nextMode(last);
            REQUIRE(next == available.front().mode);
        }
    }

    SECTION("Cycling through all modes returns to start") {
        if (!available.empty()) {
            ViewMode start = available[0].mode;
            ViewMode current = start;

            // Cycle through all available modes
            for (size_t i = 0; i < available.size(); ++i) {
                current = ViewModeRegistry::nextMode(current);
            }

            REQUIRE(current == start);
        }
    }
}

TEST_CASE("ViewModeRegistry previousMode cycles correctly", "[viewmodes][registry]") {
    std::vector<ViewModeInfo> available = ViewModeRegistry::availableModes();

    SECTION("Previous from second available returns first available") {
        if (available.size() >= 2) {
            ViewMode prev = ViewModeRegistry::previousMode(available[1].mode);
            REQUIRE(prev == available[0].mode);
        }
    }

    SECTION("Previous from first available wraps to last") {
        if (!available.empty()) {
            ViewMode first = available.front().mode;
            ViewMode prev = ViewModeRegistry::previousMode(first);
            REQUIRE(prev == available.back().mode);
        }
    }

    SECTION("Cycling backwards through all modes returns to start") {
        if (!available.empty()) {
            ViewMode start = available[0].mode;
            ViewMode current = start;

            // Cycle backwards through all available modes
            for (size_t i = 0; i < available.size(); ++i) {
                current = ViewModeRegistry::previousMode(current);
            }

            REQUIRE(current == start);
        }
    }
}

TEST_CASE("ViewModeRegistry idFromMode roundtrip", "[viewmodes][registry]") {
    SECTION("idFromMode and modeFromId are inverses") {
        const std::vector<ViewModeInfo>& allModes = ViewModeRegistry::allModes();

        for (const auto& modeInfo : allModes) {
            QString id = ViewModeRegistry::idFromMode(modeInfo.mode);
            ViewMode mode = ViewModeRegistry::modeFromId(id);
            REQUIRE(mode == modeInfo.mode);
        }
    }
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

    SECTION("Can set to Focus mode") {
        editor.setViewMode(ViewMode::Focus);
        REQUIRE(editor.viewMode() == ViewMode::Focus);
    }

    SECTION("Can set to Typewriter mode") {
        editor.setViewMode(ViewMode::Typewriter);
        REQUIRE(editor.viewMode() == ViewMode::Typewriter);
    }

    SECTION("Can set to DistractionFree mode") {
        editor.setViewMode(ViewMode::DistractionFree);
        REQUIRE(editor.viewMode() == ViewMode::DistractionFree);
    }

    SECTION("Can switch between modes") {
        editor.setViewMode(ViewMode::Page);
        REQUIRE(editor.viewMode() == ViewMode::Page);

        editor.setViewMode(ViewMode::Focus);
        REQUIRE(editor.viewMode() == ViewMode::Focus);

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
        editor.setViewMode(ViewMode::Focus);
        REQUIRE(signalCount == 1);
        REQUIRE(lastEmittedMode == ViewMode::Focus);
    }

    SECTION("Signal not emitted if mode unchanged") {
        editor.setViewMode(ViewMode::Continuous);  // Already Continuous
        REQUIRE(signalCount == 0);
    }

    SECTION("Multiple changes emit multiple signals") {
        editor.setViewMode(ViewMode::Page);
        editor.setViewMode(ViewMode::Focus);
        editor.setViewMode(ViewMode::Typewriter);
        REQUIRE(signalCount == 3);
        REQUIRE(lastEmittedMode == ViewMode::Typewriter);
    }
}

TEST_CASE("BookEditor default appearance has valid colors", "[editor][appearance]") {
    BookEditor editor;

    const EditorAppearance& appearance = editor.appearance();

    SECTION("Colors are valid") {
        REQUIRE(appearance.colors.editorBackground.isValid());
        REQUIRE(appearance.colors.text.isValid());
        REQUIRE(appearance.colors.cursor.isValid());
        REQUIRE(appearance.colors.selection.isValid());
    }

    SECTION("Elements are initialized") {
        // Default values from VisualElements
        REQUIRE(appearance.elements.highlightCurrentLine == true);
        REQUIRE(appearance.elements.showScrollbar == true);
    }

    SECTION("Typography is valid") {
        REQUIRE(!appearance.typography.textFont.family().isEmpty());
        REQUIRE(appearance.typography.textFont.pointSize() > 0);
        REQUIRE(appearance.typography.lineHeight > 0);
    }
}

TEST_CASE("BookEditor setAppearance changes appearance", "[editor][appearance]") {
    BookEditor editor;

    EditorAppearance dark = EditorAppearance::darkAppearance();
    editor.setAppearance(dark);

    SECTION("Appearance is updated") {
        REQUIRE(editor.appearance().colors.editorBackground == dark.colors.editorBackground);
        REQUIRE(editor.appearance().colors.text == dark.colors.text);
    }

    SECTION("Can switch to sepia appearance") {
        EditorAppearance sepia = EditorAppearance::sepiaAppearance();
        editor.setAppearance(sepia);

        REQUIRE(editor.appearance().colors.pageBackground == sepia.colors.pageBackground);
    }

    SECTION("Can modify individual settings") {
        EditorAppearance custom = editor.appearance();
        custom.elements.showLineNumbers = true;
        custom.typography.lineHeight = 2.0;

        editor.setAppearance(custom);

        REQUIRE(editor.appearance().elements.showLineNumbers == true);
        REQUIRE_THAT(editor.appearance().typography.lineHeight,
                     Catch::Matchers::WithinRel(2.0, 0.001));
    }
}

TEST_CASE("BookEditor setAppearance emits signal", "[editor][appearance][signals]") {
    BookEditor editor;

    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::appearanceChanged,
                     [&]() {
                         ++signalCount;
                     });

    SECTION("Signal emitted on appearance change") {
        EditorAppearance dark = EditorAppearance::darkAppearance();
        editor.setAppearance(dark);
        REQUIRE(signalCount == 1);
    }

    SECTION("Multiple changes emit multiple signals") {
        editor.setAppearance(EditorAppearance::darkAppearance());
        editor.setAppearance(EditorAppearance::sepiaAppearance());
        editor.setAppearance(EditorAppearance::defaultAppearance());
        REQUIRE(signalCount == 3);
    }
}

// =============================================================================
// ViewModeInfo Tests
// =============================================================================

TEST_CASE("ViewModeInfo icon returns valid icon", "[viewmodes][icon]") {
    const ViewModeInfo& info = ViewModeRegistry::info(ViewMode::Continuous);

    SECTION("Icon method does not crash") {
        REQUIRE_NOTHROW(info.icon());
        REQUIRE_NOTHROW(info.icon(32));
        REQUIRE_NOTHROW(info.icon(64));
    }

    SECTION("Large icon method does not crash") {
        REQUIRE_NOTHROW(info.largeIcon());
    }
}

TEST_CASE("ViewModeRegistry modeFromShortcut works", "[viewmodes][registry]") {
    SECTION("Returns mode for valid shortcut") {
        std::optional<ViewMode> mode = ViewModeRegistry::modeFromShortcut(QKeySequence("Ctrl+1"));
        REQUIRE(mode.has_value());
        REQUIRE(mode.value() == ViewMode::Continuous);
    }

    SECTION("Returns nullopt for unknown shortcut") {
        std::optional<ViewMode> mode = ViewModeRegistry::modeFromShortcut(QKeySequence("Ctrl+Shift+Alt+Z"));
        REQUIRE(!mode.has_value());
    }
}
