/// @file svg_converter.h
/// @brief SVG converter for Material Design → Kalahari template format
///
/// SvgConverter transforms Material Design SVG icons to Kalahari template format
/// by replacing color attributes with placeholders for runtime theme support.
///
/// Conversion rules:
/// - `fill="currentColor"` + `opacity="0.3"` → `fill="{COLOR_SECONDARY}"`
/// - `fill="currentColor"` (no opacity) → `fill="{COLOR_PRIMARY}"`
/// - Minification: Remove comments, metadata, unnecessary attributes
/// - Validation: Ensure valid XML, viewBox, drawable elements
///
/// Example usage:
/// @code
/// SvgConverter converter;
/// QString materialSvg = downloadedSvg;
/// auto result = converter.convertToTemplate(materialSvg);
/// if (result.success) {
///     qDebug() << "Converted SVG:" << result.svg;
/// } else {
///     qDebug() << "Conversion failed:" << result.errorMessage;
/// }
/// @endcode

#pragma once

#include <QString>

namespace kalahari {
namespace core {

/// @brief Result of SVG conversion
struct SvgConversionResult {
    bool success = false;           ///< true if conversion succeeded
    QString svg;                    ///< Converted SVG (empty if failed)
    QString errorMessage;           ///< Error message (empty if succeeded)

    /// @brief Check if conversion succeeded
    operator bool() const { return success; }
};

/// @brief SVG converter for Material Design → Kalahari template format
///
/// Converts Material Design SVG icons to Kalahari template format with
/// color placeholders ({COLOR_PRIMARY}, {COLOR_SECONDARY}).
class SvgConverter {
public:
    /// @brief Default constructor
    SvgConverter() = default;

    /// @brief Destructor
    ~SvgConverter() = default;

    // Disable copy and move (stateless class)
    SvgConverter(const SvgConverter&) = delete;
    SvgConverter& operator=(const SvgConverter&) = delete;
    SvgConverter(SvgConverter&&) = delete;
    SvgConverter& operator=(SvgConverter&&) = delete;

    /// @brief Convert Material Design SVG to Kalahari template format
    ///
    /// Applies conversion rules:
    /// 1. Parse SVG XML structure
    /// 2. Find all drawable elements (path, circle, rect, polygon, etc.)
    /// 3. Replace `fill="currentColor"` with placeholders based on opacity:
    ///    - opacity="0.3" (or 0.0-0.5) → {COLOR_SECONDARY}
    ///    - no opacity or opacity > 0.5 → {COLOR_PRIMARY}
    /// 4. Minify SVG (remove comments, metadata, unnecessary attributes)
    /// 5. Validate result
    ///
    /// @param svgData Raw Material Design SVG string
    /// @return Conversion result with success status, converted SVG, or error message
    SvgConversionResult convertToTemplate(const QString& svgData);

    /// @brief Validate SVG structure
    ///
    /// Checks:
    /// - Valid XML syntax (parsable by QDomDocument)
    /// - Contains at least one drawable element (path, circle, rect, etc.)
    /// - Has required `viewBox` attribute on root <svg> element
    ///
    /// @param svgData SVG string to validate
    /// @return Validation result with success status and error message
    SvgConversionResult validate(const QString& svgData);

private:
    /// @brief Replace color attributes with placeholders
    ///
    /// Processes all drawable elements and replaces `fill="currentColor"` with:
    /// - {COLOR_SECONDARY} if element has opacity ≤ 0.5
    /// - {COLOR_PRIMARY} otherwise
    ///
    /// @param svgXml Parsed SVG DOM document
    /// @return Modified SVG as QString
    QString replaceColorPlaceholders(const QString& svgXml);

    /// @brief Minify SVG (remove comments, metadata, unnecessary attributes)
    ///
    /// Removes:
    /// - XML comments (<!-- ... -->)
    /// - <metadata>, <title>, <desc> tags
    /// - Unnecessary namespaces (preserve xmlns, xmlns:xlink)
    /// - Empty attributes
    ///
    /// Preserves:
    /// - Essential attributes (viewBox, width, height, fill, d, cx, cy, r, etc.)
    /// - All drawable elements (path, circle, rect, polygon, etc.)
    ///
    /// @param svgXml SVG string to minify
    /// @return Minified SVG
    QString minifySvg(const QString& svgXml);

    /// @brief Get effective opacity for element
    ///
    /// Reads `opacity` attribute from element or parent elements.
    /// Returns 1.0 if no opacity specified.
    ///
    /// @param elementAttributes Attributes string (e.g., 'fill="currentColor" opacity="0.3"')
    /// @return Opacity value (0.0 - 1.0)
    double getOpacityFromAttributes(const QString& elementAttributes) const;
};

} // namespace core
} // namespace kalahari
