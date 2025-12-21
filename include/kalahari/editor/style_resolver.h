/// @file style_resolver.h
/// @brief Style resolver with inheritance support (OpenSpec #00042 Phase 6)
///
/// StyleResolver provides:
/// - Style ID to style definition resolution
/// - Style inheritance chain resolution
/// - Caching for performance
/// - Conversion to Qt formats (QFont, QTextCharFormat, QTextBlockFormat)

#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QFont>
#include <QColor>
#include <QTextCharFormat>
#include <QTextBlockFormat>

// Forward declarations
namespace kalahari::core {
class ProjectDatabase;
struct ParagraphStyle;
struct CharacterStyle;
}

namespace kalahari::editor {

// =============================================================================
// Resolved Style Structures
// =============================================================================

/// @brief Resolved paragraph style with all inherited properties applied
///
/// This struct contains the fully-resolved style properties after
/// inheritance chain resolution. All optional properties from the
/// inheritance chain are flattened into concrete values.
struct ResolvedParagraphStyle {
    QString id;              ///< Style ID
    QString name;            ///< Display name

    // Font properties (from embedded or linked CharacterStyle)
    QString fontFamily;      ///< Font family name
    int fontSize{12};        ///< Font size in points
    bool bold{false};        ///< Bold weight
    bool italic{false};      ///< Italic style
    bool underline{false};   ///< Underline decoration
    QColor textColor{Qt::black};  ///< Text foreground color

    // Paragraph properties
    Qt::Alignment alignment{Qt::AlignLeft};  ///< Text alignment
    qreal firstLineIndent{0.0};  ///< First line indent in points
    qreal leftMargin{0.0};       ///< Left margin in points
    qreal rightMargin{0.0};      ///< Right margin in points
    qreal spaceBefore{0.0};      ///< Space before paragraph in points
    qreal spaceAfter{0.0};       ///< Space after paragraph in points
    qreal lineHeight{1.0};       ///< Line height multiplier (1.0 = single spacing)

    /// @brief Convert to QFont
    /// @return QFont with font properties applied
    QFont toFont() const;

    /// @brief Convert to QTextCharFormat
    /// @return Character format for text rendering
    QTextCharFormat toCharFormat() const;

    /// @brief Convert to QTextBlockFormat
    /// @return Block format for paragraph rendering
    QTextBlockFormat toBlockFormat() const;
};

/// @brief Resolved character (inline) style with all inherited properties applied
///
/// Character styles are applied to text runs within paragraphs.
/// They can override paragraph-level font settings.
struct ResolvedCharacterStyle {
    QString id;              ///< Style ID
    QString name;            ///< Display name

    // Font properties
    QString fontFamily;      ///< Font family name (empty = inherit from paragraph)
    int fontSize{12};        ///< Font size in points
    bool bold{false};        ///< Bold weight
    bool italic{false};      ///< Italic style
    bool underline{false};   ///< Underline decoration
    bool strikethrough{false};  ///< Strikethrough decoration
    QColor textColor{Qt::black};       ///< Text foreground color
    QColor backgroundColor{Qt::transparent};  ///< Text background color

    /// @brief Convert to QFont
    /// @return QFont with font properties applied
    QFont toFont() const;

    /// @brief Convert to QTextCharFormat
    /// @return Character format for text rendering
    QTextCharFormat toCharFormat() const;
};

// =============================================================================
// StyleResolver Class
// =============================================================================

/// @brief Resolves style IDs to complete style definitions with inheritance
///
/// The StyleResolver is responsible for:
/// 1. Looking up styles by ID from the database
/// 2. Resolving inheritance chains (child -> parent -> grandparent)
/// 3. Merging inherited properties
/// 4. Caching resolved styles for performance
/// 5. Converting resolved styles to Qt formats
///
/// Usage:
/// @code
/// auto resolver = new StyleResolver(this);
/// resolver->setDatabase(projectDatabase);
///
/// // Resolve a paragraph style
/// auto style = resolver->resolveParagraphStyle("heading1");
/// textEdit->setCurrentCharFormat(style.toCharFormat());
/// textEdit->setBlockFormat(style.toBlockFormat());
/// @endcode
class StyleResolver : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a style resolver
    /// @param parent Parent QObject for ownership
    explicit StyleResolver(QObject* parent = nullptr);

    /// @brief Destructor
    ~StyleResolver() override;

    // Non-copyable
    StyleResolver(const StyleResolver&) = delete;
    StyleResolver& operator=(const StyleResolver&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the database for style lookup
    /// @param database ProjectDatabase containing style definitions
    /// @note Automatically invalidates cache when database changes
    void setDatabase(core::ProjectDatabase* database);

    /// @brief Get the current database
    /// @return Current database or nullptr
    core::ProjectDatabase* database() const { return m_database; }

    // =========================================================================
    // Style Resolution
    // =========================================================================

    /// @brief Resolve a paragraph style by ID
    /// @param styleId Style ID to resolve
    /// @return Fully resolved style with inheritance applied
    /// @note Returns default style if ID not found
    ResolvedParagraphStyle resolveParagraphStyle(const QString& styleId) const;

    /// @brief Resolve a character style by ID
    /// @param styleId Style ID to resolve
    /// @return Fully resolved style
    /// @note Returns default style if ID not found
    ResolvedCharacterStyle resolveCharacterStyle(const QString& styleId) const;

    // =========================================================================
    // Default Styles
    // =========================================================================

    /// @brief Get the default paragraph style
    /// @return Default style with sensible defaults
    ResolvedParagraphStyle defaultParagraphStyle() const;

    /// @brief Get the default character style
    /// @return Default style with sensible defaults
    ResolvedCharacterStyle defaultCharacterStyle() const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /// @brief Invalidate the style cache
    /// @note Should be called when styles are modified externally
    void invalidateCache();

    /// @brief Reload all styles from database
    /// @note Also invalidates cache
    void reloadFromDatabase();

signals:
    /// @brief Emitted when styles have been reloaded or cache invalidated
    void stylesChanged();

private:
    // =========================================================================
    // Internal Resolution Methods
    // =========================================================================

    /// @brief Resolve paragraph style with inheritance chain
    /// @param styleId Style ID to resolve
    /// @param visited Set of already-visited styles (for circular detection)
    /// @return Resolved style
    ResolvedParagraphStyle resolveWithInheritance(
        const QString& styleId,
        QSet<QString>& visited) const;

    /// @brief Resolve character style with inheritance chain
    /// @param styleId Style ID to resolve
    /// @param visited Set of already-visited styles (for circular detection)
    /// @return Resolved style
    ResolvedCharacterStyle resolveCharWithInheritance(
        const QString& styleId,
        QSet<QString>& visited) const;

    /// @brief Merge child style properties onto base style
    /// @param base Base style to modify (resolved parent)
    /// @param child Child style with overriding properties
    void mergeStyles(ResolvedParagraphStyle& base,
                     const core::ParagraphStyle& child) const;

    /// @brief Merge child character style properties onto base style
    /// @param base Base style to modify (resolved parent)
    /// @param child Child style with overriding properties
    void mergeCharStyles(ResolvedCharacterStyle& base,
                         const core::CharacterStyle& child) const;

    /// @brief Extract font properties from style's properties map
    /// @param properties QVariantMap of style properties
    /// @param style Style to populate with font properties
    void extractFontProperties(const QVariantMap& properties,
                               ResolvedParagraphStyle& style) const;

    /// @brief Extract paragraph properties from style's properties map
    /// @param properties QVariantMap of style properties
    /// @param style Style to populate with paragraph properties
    void extractParagraphProperties(const QVariantMap& properties,
                                    ResolvedParagraphStyle& style) const;

    /// @brief Extract character style properties from properties map
    /// @param properties QVariantMap of style properties
    /// @param style Style to populate with character properties
    void extractCharProperties(const QVariantMap& properties,
                               ResolvedCharacterStyle& style) const;

    /// @brief Find a paragraph style by ID in the loaded styles
    /// @param styleId Style ID to find
    /// @return Pointer to style or nullptr if not found
    const core::ParagraphStyle* findParagraphStyle(const QString& styleId) const;

    /// @brief Find a character style by ID in the loaded styles
    /// @param styleId Style ID to find
    /// @return Pointer to style or nullptr if not found
    const core::CharacterStyle* findCharacterStyle(const QString& styleId) const;

    /// @brief Ensure styles are loaded from database
    void ensureStylesLoaded() const;

    // =========================================================================
    // Members
    // =========================================================================

    core::ProjectDatabase* m_database{nullptr};

    // Cached style data from database
    mutable QList<core::ParagraphStyle> m_paragraphStyles;
    mutable QList<core::CharacterStyle> m_characterStyles;
    mutable bool m_stylesLoaded{false};

    // Resolved style caches
    mutable QHash<QString, ResolvedParagraphStyle> m_paragraphCache;
    mutable QHash<QString, ResolvedCharacterStyle> m_characterCache;
    mutable bool m_cacheValid{false};

    // Default font settings
    static constexpr const char* DEFAULT_FONT_FAMILY = "Segoe UI";
    static constexpr int DEFAULT_FONT_SIZE = 12;
};

}  // namespace kalahari::editor
