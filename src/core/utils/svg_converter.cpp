/// @file svg_converter.cpp
/// @brief Implementation of SvgConverter

#include "kalahari/core/utils/svg_converter.h"
#include "kalahari/core/logger.h"
#include <QDomDocument>
#include <QRegularExpression>
#include <QXmlStreamReader>

using namespace kalahari::core;

// ============================================================================
// SvgConverter Implementation
// ============================================================================

SvgConversionResult SvgConverter::convertToTemplate(const QString& svgData) {
    Logger::getInstance().debug("SvgConverter: Converting SVG ({} bytes)", svgData.length());

    // Step 1: Validate input SVG
    SvgConversionResult validationResult = validate(svgData);
    if (!validationResult.success) {
        Logger::getInstance().error("SvgConverter: Validation failed: {}",
                                    validationResult.errorMessage.toStdString());
        return validationResult;
    }

    // Step 2: Replace color attributes with placeholders
    QString convertedSvg = replaceColorPlaceholders(svgData);

    // Step 3: Minify SVG
    convertedSvg = minifySvg(convertedSvg);

    // Step 4: Final validation
    SvgConversionResult finalValidation = validate(convertedSvg);
    if (!finalValidation.success) {
        Logger::getInstance().error("SvgConverter: Converted SVG is invalid: {}",
                                    finalValidation.errorMessage.toStdString());
        return {false, QString(), "Conversion produced invalid SVG: " + finalValidation.errorMessage};
    }

    Logger::getInstance().info("SvgConverter: ✓ Conversion successful ({} → {} bytes)",
                               svgData.length(), convertedSvg.length());

    return {true, convertedSvg, QString()};
}

SvgConversionResult SvgConverter::validate(const QString& svgData) {
    if (svgData.isEmpty()) {
        return {false, QString(), "SVG data is empty"};
    }

    // Check 1: Valid XML syntax
    QDomDocument doc;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt 6.5+: Use ParseResult struct
    auto parseResult = doc.setContent(svgData);
    if (!parseResult) {
        QString error = QString("Invalid XML syntax at line %1, column %2: %3")
                       .arg(parseResult.errorLine)
                       .arg(parseResult.errorColumn)
                       .arg(parseResult.errorMessage);
        return {false, QString(), error};
    }
#else
    // Qt 6.4 and earlier: Use old setContent signature with output parameters
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    if (!doc.setContent(svgData, &errorMsg, &errorLine, &errorColumn)) {
        QString error = QString("Invalid XML syntax at line %1, column %2: %3")
                       .arg(errorLine)
                       .arg(errorColumn)
                       .arg(errorMsg);
        return {false, QString(), error};
    }
#endif

    // Check 2: Root element is <svg>
    QDomElement root = doc.documentElement();
    if (root.tagName() != "svg") {
        return {false, QString(), QString("Root element is not <svg> (found: %1)").arg(root.tagName())};
    }

    // Check 3: Has viewBox attribute
    if (!root.hasAttribute("viewBox") && !root.hasAttribute("viewbox")) {
        return {false, QString(), "Missing required 'viewBox' attribute on <svg> element"};
    }

    // Note: SVG with no drawable elements is valid - just nothing to convert
    // All checks passed
    return {true, QString(), QString()};
}

QString SvgConverter::replaceColorPlaceholders(const QString& svgXml) {
    // Use QDomDocument for safe XML manipulation
    QDomDocument doc;
    auto parseResult = doc.setContent(svgXml);
    if (!parseResult) {
        Logger::getInstance().warn("SvgConverter: Failed to parse SVG for color replacement");
        return svgXml; // Return original on parse failure
    }

    // Process all drawable elements (path, circle, rect, polygon, etc.)
    QStringList drawableElements = {"path", "circle", "rect", "polygon", "polyline", "ellipse", "line"};

    for (const QString& tagName : drawableElements) {
        QDomNodeList elements = doc.elementsByTagName(tagName);

        for (int i = 0; i < elements.count(); ++i) {
            QDomElement element = elements.at(i).toElement();
            if (element.isNull()) {
                continue;
            }

            // Get opacity (default 1.0)
            double opacity = 1.0;
            bool hadOpacity = element.hasAttribute("opacity");
            if (hadOpacity) {
                bool ok;
                opacity = element.attribute("opacity").toDouble(&ok);
                if (!ok) {
                    opacity = 1.0;
                }
                // Remove opacity attribute after reading (it's encoded in color choice)
                element.removeAttribute("opacity");
            }

            // Determine color based on opacity
            QString colorPlaceholder = (opacity <= 0.5) ? "{COLOR_SECONDARY}" : "{COLOR_PRIMARY}";

            // Case 1: Replace existing fill attribute
            if (element.hasAttribute("fill") && element.attribute("fill") != "none") {
                element.setAttribute("fill", colorPlaceholder);
            }
            // Case 2: Add fill if missing (and not fill="none")
            else if (!element.hasAttribute("fill") && element.hasAttribute("d")) {
                // Only add fill to <path> with data
                element.setAttribute("fill", colorPlaceholder);
            }
            else if (!element.hasAttribute("fill") && tagName != "path") {
                // Add fill to other drawable elements without fill
                element.setAttribute("fill", colorPlaceholder);
            }
        }
    }

    return doc.toString(-1); // -1 = no indentation (compact)
}

QString SvgConverter::minifySvg(const QString& svgXml) {
    QString result = svgXml;

    // Remove XML comments <!-- ... -->
    QRegularExpression commentRegex("<!--.*?-->", QRegularExpression::DotMatchesEverythingOption);
    result.remove(commentRegex);

    // Remove <metadata>...</metadata>
    QRegularExpression metadataRegex("<metadata[^>]*>.*?</metadata>",
                                     QRegularExpression::DotMatchesEverythingOption);
    result.remove(metadataRegex);

    // Remove <title>...</title>
    QRegularExpression titleRegex("<title[^>]*>.*?</title>",
                                  QRegularExpression::DotMatchesEverythingOption);
    result.remove(titleRegex);

    // Remove <desc>...</desc>
    QRegularExpression descRegex("<desc[^>]*>.*?</desc>",
                                 QRegularExpression::DotMatchesEverythingOption);
    result.remove(descRegex);

    // Remove unnecessary xmlns:* (preserve xmlns and xmlns:xlink)
    // Example: xmlns:sketch="http://..." → remove
    QString xmlnsPattern = "\\s+xmlns:(?!xlink)[^\\s=]+=\"[^\"]*\"";
    QRegularExpression unnecessaryXmlnsRegex(xmlnsPattern);
    result.remove(unnecessaryXmlnsRegex);

    // Normalize whitespace (collapse multiple spaces/newlines)
    result = result.trimmed();
    result.replace(QRegularExpression("\\s+"), " ");

    // Remove spaces around element boundaries (compact output)
    result.replace(QRegularExpression("> <"), "><");

    return result;
}

double SvgConverter::getOpacityFromAttributes(const QString& elementAttributes) const {
    // Search for opacity="value" attribute
    QString opacityPattern = "opacity\\s*=\\s*\"([0-9.]+)\"";
    QRegularExpression opacityRegex(opacityPattern);
    QRegularExpressionMatch match = opacityRegex.match(elementAttributes);

    if (match.hasMatch()) {
        QString opacityStr = match.captured(1);
        bool ok;
        double opacity = opacityStr.toDouble(&ok);
        if (ok) {
            return opacity;
        }
    }

    // No opacity attribute found = fully opaque
    return 1.0;
}
