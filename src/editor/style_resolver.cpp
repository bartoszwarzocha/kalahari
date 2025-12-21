/// @file style_resolver.cpp
/// @brief Style resolver implementation (OpenSpec #00042 Phase 6)

#include <kalahari/editor/style_resolver.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/database_types.h>
#include <kalahari/core/logger.h>

#include <QFontDatabase>

namespace kalahari::editor {

// =============================================================================
// ResolvedParagraphStyle Conversion Methods
// =============================================================================

QFont ResolvedParagraphStyle::toFont() const
{
    QFont font(fontFamily, fontSize);
    font.setBold(bold);
    font.setItalic(italic);
    font.setUnderline(underline);
    return font;
}

QTextCharFormat ResolvedParagraphStyle::toCharFormat() const
{
    QTextCharFormat format;
    format.setFont(toFont());
    format.setForeground(textColor);
    return format;
}

QTextBlockFormat ResolvedParagraphStyle::toBlockFormat() const
{
    QTextBlockFormat format;
    format.setAlignment(alignment);
    format.setTextIndent(firstLineIndent);
    format.setLeftMargin(leftMargin);
    format.setRightMargin(rightMargin);
    format.setTopMargin(spaceBefore);
    format.setBottomMargin(spaceAfter);

    // Line height as percentage (1.0 = 100%, 1.5 = 150%)
    if (lineHeight > 0.0) {
        format.setLineHeight(lineHeight * 100.0, QTextBlockFormat::ProportionalHeight);
    }

    return format;
}

// =============================================================================
// ResolvedCharacterStyle Conversion Methods
// =============================================================================

QFont ResolvedCharacterStyle::toFont() const
{
    QFont font(fontFamily, fontSize);
    font.setBold(bold);
    font.setItalic(italic);
    font.setUnderline(underline);
    font.setStrikeOut(strikethrough);
    return font;
}

QTextCharFormat ResolvedCharacterStyle::toCharFormat() const
{
    QTextCharFormat format;
    format.setFont(toFont());
    format.setForeground(textColor);

    if (backgroundColor.alpha() > 0) {
        format.setBackground(backgroundColor);
    }

    if (strikethrough) {
        format.setFontStrikeOut(true);
    }

    return format;
}

// =============================================================================
// StyleResolver Constructor/Destructor
// =============================================================================

StyleResolver::StyleResolver(QObject* parent)
    : QObject(parent)
{
    core::Logger::getInstance().debug("StyleResolver created");
}

StyleResolver::~StyleResolver()
{
    core::Logger::getInstance().debug("StyleResolver destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void StyleResolver::setDatabase(core::ProjectDatabase* database)
{
    if (m_database == database) {
        return;
    }

    m_database = database;
    invalidateCache();

    if (m_database) {
        core::Logger::getInstance().debug("StyleResolver connected to database");
    }
}

// =============================================================================
// Style Resolution
// =============================================================================

ResolvedParagraphStyle StyleResolver::resolveParagraphStyle(const QString& styleId) const
{
    if (styleId.isEmpty()) {
        return defaultParagraphStyle();
    }

    // Check cache first
    if (m_cacheValid && m_paragraphCache.contains(styleId)) {
        return m_paragraphCache.value(styleId);
    }

    // Resolve with inheritance
    QSet<QString> visited;
    ResolvedParagraphStyle resolved = resolveWithInheritance(styleId, visited);

    // Cache the result
    m_paragraphCache.insert(styleId, resolved);
    m_cacheValid = true;

    return resolved;
}

ResolvedCharacterStyle StyleResolver::resolveCharacterStyle(const QString& styleId) const
{
    if (styleId.isEmpty()) {
        return defaultCharacterStyle();
    }

    // Check cache first
    if (m_cacheValid && m_characterCache.contains(styleId)) {
        return m_characterCache.value(styleId);
    }

    // Resolve with inheritance
    QSet<QString> visited;
    ResolvedCharacterStyle resolved = resolveCharWithInheritance(styleId, visited);

    // Cache the result
    m_characterCache.insert(styleId, resolved);
    m_cacheValid = true;

    return resolved;
}

// =============================================================================
// Default Styles
// =============================================================================

ResolvedParagraphStyle StyleResolver::defaultParagraphStyle() const
{
    ResolvedParagraphStyle style;
    style.id = "default";
    style.name = tr("Default");

    // Font defaults
    style.fontFamily = QString::fromLatin1(DEFAULT_FONT_FAMILY);
    style.fontSize = DEFAULT_FONT_SIZE;
    style.bold = false;
    style.italic = false;
    style.underline = false;
    style.textColor = Qt::black;

    // Paragraph defaults
    style.alignment = Qt::AlignLeft;
    style.firstLineIndent = 0.0;
    style.leftMargin = 0.0;
    style.rightMargin = 0.0;
    style.spaceBefore = 0.0;
    style.spaceAfter = 0.0;
    style.lineHeight = 1.0;

    return style;
}

ResolvedCharacterStyle StyleResolver::defaultCharacterStyle() const
{
    ResolvedCharacterStyle style;
    style.id = "default";
    style.name = tr("Default");

    // Font defaults
    style.fontFamily = QString::fromLatin1(DEFAULT_FONT_FAMILY);
    style.fontSize = DEFAULT_FONT_SIZE;
    style.bold = false;
    style.italic = false;
    style.underline = false;
    style.strikethrough = false;
    style.textColor = Qt::black;
    style.backgroundColor = Qt::transparent;

    return style;
}

// =============================================================================
// Cache Management
// =============================================================================

void StyleResolver::invalidateCache()
{
    m_paragraphCache.clear();
    m_characterCache.clear();
    m_cacheValid = false;
    m_stylesLoaded = false;

    core::Logger::getInstance().debug("StyleResolver cache invalidated");
}

void StyleResolver::reloadFromDatabase()
{
    invalidateCache();

    if (m_database && m_database->isOpen()) {
        ensureStylesLoaded();
        core::Logger::getInstance().info("StyleResolver reloaded {} paragraph styles, {} character styles",
                                         m_paragraphStyles.size(), m_characterStyles.size());
    }

    emit stylesChanged();
}

// =============================================================================
// Internal Resolution Methods
// =============================================================================

ResolvedParagraphStyle StyleResolver::resolveWithInheritance(
    const QString& styleId,
    QSet<QString>& visited) const
{
    // Circular inheritance detection
    if (visited.contains(styleId)) {
        core::Logger::getInstance().warn(
            "Circular style inheritance detected for: {}", styleId.toStdString());
        return defaultParagraphStyle();
    }
    visited.insert(styleId);

    // Ensure styles are loaded
    ensureStylesLoaded();

    // Find the style definition
    const core::ParagraphStyle* style = findParagraphStyle(styleId);
    if (!style) {
        core::Logger::getInstance().debug(
            "Paragraph style not found: {}, using default", styleId.toStdString());
        return defaultParagraphStyle();
    }

    // Start with base style (if any) or default
    ResolvedParagraphStyle resolved;
    if (!style->baseStyle.isEmpty()) {
        // Recursively resolve base style
        resolved = resolveWithInheritance(style->baseStyle, visited);
    } else {
        // Start with default
        resolved = defaultParagraphStyle();
    }

    // Apply this style's properties on top
    mergeStyles(resolved, *style);

    return resolved;
}

ResolvedCharacterStyle StyleResolver::resolveCharWithInheritance(
    const QString& styleId,
    QSet<QString>& visited) const
{
    // Circular inheritance detection
    if (visited.contains(styleId)) {
        core::Logger::getInstance().warn(
            "Circular character style inheritance detected for: {}", styleId.toStdString());
        return defaultCharacterStyle();
    }
    visited.insert(styleId);

    // Ensure styles are loaded
    ensureStylesLoaded();

    // Find the style definition
    const core::CharacterStyle* style = findCharacterStyle(styleId);
    if (!style) {
        core::Logger::getInstance().debug(
            "Character style not found: {}, using default", styleId.toStdString());
        return defaultCharacterStyle();
    }

    // Character styles don't have inheritance in current schema
    // Start with default and apply properties
    ResolvedCharacterStyle resolved = defaultCharacterStyle();
    mergeCharStyles(resolved, *style);

    return resolved;
}

void StyleResolver::mergeStyles(ResolvedParagraphStyle& base,
                                const core::ParagraphStyle& child) const
{
    // Update ID and name
    base.id = child.id;
    base.name = child.name;

    // Extract and apply properties from child's properties map
    extractFontProperties(child.properties, base);
    extractParagraphProperties(child.properties, base);
}

void StyleResolver::mergeCharStyles(ResolvedCharacterStyle& base,
                                    const core::CharacterStyle& child) const
{
    // Update ID and name
    base.id = child.id;
    base.name = child.name;

    // Extract and apply properties from child's properties map
    extractCharProperties(child.properties, base);
}

void StyleResolver::extractFontProperties(const QVariantMap& properties,
                                          ResolvedParagraphStyle& style) const
{
    if (properties.contains("fontFamily")) {
        style.fontFamily = properties.value("fontFamily").toString();
    }

    if (properties.contains("fontSize")) {
        style.fontSize = properties.value("fontSize").toInt();
    }

    if (properties.contains("bold")) {
        style.bold = properties.value("bold").toBool();
    }

    if (properties.contains("italic")) {
        style.italic = properties.value("italic").toBool();
    }

    if (properties.contains("underline")) {
        style.underline = properties.value("underline").toBool();
    }

    if (properties.contains("textColor")) {
        QString colorStr = properties.value("textColor").toString();
        if (!colorStr.isEmpty()) {
            style.textColor = QColor(colorStr);
        }
    }
}

void StyleResolver::extractParagraphProperties(const QVariantMap& properties,
                                               ResolvedParagraphStyle& style) const
{
    if (properties.contains("alignment")) {
        QString alignStr = properties.value("alignment").toString().toLower();
        if (alignStr == "left") {
            style.alignment = Qt::AlignLeft;
        } else if (alignStr == "center") {
            style.alignment = Qt::AlignHCenter;
        } else if (alignStr == "right") {
            style.alignment = Qt::AlignRight;
        } else if (alignStr == "justify") {
            style.alignment = Qt::AlignJustify;
        }
    }

    if (properties.contains("firstLineIndent")) {
        style.firstLineIndent = properties.value("firstLineIndent").toDouble();
    }

    if (properties.contains("leftMargin")) {
        style.leftMargin = properties.value("leftMargin").toDouble();
    }

    if (properties.contains("rightMargin")) {
        style.rightMargin = properties.value("rightMargin").toDouble();
    }

    if (properties.contains("spaceBefore")) {
        style.spaceBefore = properties.value("spaceBefore").toDouble();
    }

    if (properties.contains("spaceAfter")) {
        style.spaceAfter = properties.value("spaceAfter").toDouble();
    }

    if (properties.contains("lineHeight")) {
        style.lineHeight = properties.value("lineHeight").toDouble();
    }
}

void StyleResolver::extractCharProperties(const QVariantMap& properties,
                                          ResolvedCharacterStyle& style) const
{
    if (properties.contains("fontFamily")) {
        style.fontFamily = properties.value("fontFamily").toString();
    }

    if (properties.contains("fontSize")) {
        style.fontSize = properties.value("fontSize").toInt();
    }

    if (properties.contains("bold")) {
        style.bold = properties.value("bold").toBool();
    }

    if (properties.contains("italic")) {
        style.italic = properties.value("italic").toBool();
    }

    if (properties.contains("underline")) {
        style.underline = properties.value("underline").toBool();
    }

    if (properties.contains("strikethrough")) {
        style.strikethrough = properties.value("strikethrough").toBool();
    }

    if (properties.contains("textColor")) {
        QString colorStr = properties.value("textColor").toString();
        if (!colorStr.isEmpty()) {
            style.textColor = QColor(colorStr);
        }
    }

    if (properties.contains("backgroundColor")) {
        QString colorStr = properties.value("backgroundColor").toString();
        if (!colorStr.isEmpty()) {
            style.backgroundColor = QColor(colorStr);
        }
    }
}

const core::ParagraphStyle* StyleResolver::findParagraphStyle(const QString& styleId) const
{
    for (const auto& style : m_paragraphStyles) {
        if (style.id == styleId) {
            return &style;
        }
    }
    return nullptr;
}

const core::CharacterStyle* StyleResolver::findCharacterStyle(const QString& styleId) const
{
    for (const auto& style : m_characterStyles) {
        if (style.id == styleId) {
            return &style;
        }
    }
    return nullptr;
}

void StyleResolver::ensureStylesLoaded() const
{
    if (m_stylesLoaded) {
        return;
    }

    if (!m_database || !m_database->isOpen()) {
        m_paragraphStyles.clear();
        m_characterStyles.clear();
        m_stylesLoaded = true;
        return;
    }

    // Load all styles from database
    m_paragraphStyles = m_database->getParagraphStyles();
    m_characterStyles = m_database->getCharacterStyles();
    m_stylesLoaded = true;

    core::Logger::getInstance().debug(
        "Loaded {} paragraph styles, {} character styles from database",
        m_paragraphStyles.size(), m_characterStyles.size());
}

}  // namespace kalahari::editor
