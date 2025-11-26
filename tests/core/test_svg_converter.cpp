/// @file test_svg_converter.cpp
/// @brief Unit tests for SvgConverter (Task #00020)
///
/// Tests cover:
/// - SVG color placeholder conversion (opacity-based logic)
/// - Validation of SVG syntax
/// - Edge cases (empty SVG, malformed XML)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/utils/svg_converter.h>

using namespace kalahari::core;

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("SvgConverter converts Material Design SVG to template", "[svg][converter]") {
    SvgConverter converter;

    SECTION("Converts simple path with high opacity to {COLOR_PRIMARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z" opacity="0.87"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
        REQUIRE_FALSE(result.svg.contains("opacity="));
    }

    SECTION("Converts path with low opacity to {COLOR_SECONDARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z" opacity="0.3"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_SECONDARY}"));
        REQUIRE_FALSE(result.svg.contains("opacity="));
    }

    SECTION("Converts multiple paths with mixed opacities") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="0.87"/>
  <path d="M5v-8h3L12 3" opacity="0.3"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
        REQUIRE(result.svg.contains("{COLOR_SECONDARY}"));
    }

    SECTION("Adds fill attribute to elements without opacity") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("fill=\"{COLOR_PRIMARY}\""));
    }

    SECTION("Handles circle elements") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <circle cx="12" cy="12" r="10" opacity="0.87"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
    }

    SECTION("Handles rect elements") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <rect x="0" y="0" width="24" height="24" opacity="0.3"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_SECONDARY}"));
    }
}

TEST_CASE("SvgConverter validates SVG syntax", "[svg][validation]") {
    SvgConverter converter;

    SECTION("Rejects empty SVG") {
        QString emptySvg = "";

        auto result = converter.validate(emptySvg);

        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errorMessage.isEmpty());
    }

    SECTION("Rejects malformed XML") {
        QString malformedSvg = R"(<svg xmlns="http://www.w3.org/2000/svg">
  <path d="M10 20v-6h4v6" <!-- Missing closing tag -->
</svg>)";

        auto result = converter.validate(malformedSvg);

        REQUIRE_FALSE(result.success);
    }

    SECTION("Accepts valid SVG") {
        QString validSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>
</svg>)";

        auto result = converter.validate(validSvg);

        REQUIRE(result.success);
        REQUIRE(result.errorMessage.isEmpty());
    }
}

TEST_CASE("SvgConverter handles edge cases", "[svg][edge-cases]") {
    SvgConverter converter;

    SECTION("Handles SVG with no paths") {
        QString emptyContentSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
</svg>)";

        auto result = converter.convertToTemplate(emptyContentSvg);

        REQUIRE(result.success);  // Valid SVG, just no content to convert
    }

    SECTION("Preserves existing fill attributes") {
        QString svgWithFill = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6" fill="red" opacity="0.87"/>
</svg>)";

        auto result = converter.convertToTemplate(svgWithFill);

        REQUIRE(result.success);
        // Should replace fill with color placeholder
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
    }

    SECTION("Handles SVG with xmlns attributes") {
        QString svgWithNamespace = R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="0.87"/>
</svg>)";

        auto result = converter.convertToTemplate(svgWithNamespace);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
    }
}

TEST_CASE("SvgConverter opacity threshold logic", "[svg][opacity]") {
    SvgConverter converter;

    SECTION("Opacity 0.5 exactly maps to {COLOR_SECONDARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="0.5"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_SECONDARY}"));
    }

    SECTION("Opacity 0.51 maps to {COLOR_PRIMARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="0.51"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
    }

    SECTION("Opacity 0.0 maps to {COLOR_SECONDARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="0.0"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_SECONDARY}"));
    }

    SECTION("Opacity 1.0 maps to {COLOR_PRIMARY}") {
        QString inputSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
  <path d="M10 20v-6h4v6" opacity="1.0"/>
</svg>)";

        auto result = converter.convertToTemplate(inputSvg);

        REQUIRE(result.success);
        REQUIRE(result.svg.contains("{COLOR_PRIMARY}"));
    }
}
