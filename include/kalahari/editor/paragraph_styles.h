/// @file paragraph_styles.h
/// @brief The paragraph styles of a book and the objects of its text (a horizontal line, the
///        scene and page breaks)
///
/// A paragraph's style is a property of its block format: the id of a style of the book's
/// type package (book_type_package.h). A paragraph without one is in the style Normal. The
/// editor shows Normal as the Settings say (font, size, spacing, indent) and the other styles
/// in the proportions their package gives them (see KalahariTextDocumentLayout).
///
/// An object is a block of its own holding one object replacement character (U+FFFC) whose
/// character format names the kind of object; the layout sizes and draws it.

#pragma once

#include <QList>
#include <QString>
#include <QTextFormat>
#include <QVariantMap>

class QTextBlock;
class QTextBlockFormat;
class QTextCharFormat;

namespace kalahari::core {
struct PackageParagraphStyle;
}

namespace kalahari::editor {

// =============================================================================
// The style of a paragraph
// =============================================================================

/// @brief Block format property: the id of the paragraph's style (a QString)
///
/// Without it the paragraph is in the style Normal.
inline constexpr int PARAGRAPH_STYLE_PROPERTY = QTextFormat::UserProperty + 110;

/// @brief Id of the style Normal: the text of the book
inline constexpr char NORMAL_STYLE_ID[] = "normal";

/// @brief Number of heading levels with elements of their own in KML (<h1>-<h3>)
inline constexpr int HEADING_LEVELS = 3;

/// @brief Id of the style of a heading level: "heading_1", "heading_2", "heading_3"
/// @param level 1 to HEADING_LEVELS
QString headingStyleId(int level);

/// @brief Heading level of a style (1 to HEADING_LEVELS), 0 for any other style
int headingLevelOf(const QString& styleId);

/// @brief Heading level of a KML element: 1 to HEADING_LEVELS for <h1>-<h3>, else 0
int headingLevelOfElement(QStringView element);

/// @brief The style of a paragraph's block format: its id, NORMAL_STYLE_ID without one
QString paragraphStyleOf(const QTextBlockFormat& format);

/// @brief Put a paragraph in a style; Normal (or no id) leaves the format without one
void setParagraphStyle(QTextBlockFormat& format, const QString& styleId);

// =============================================================================
// Objects of the text
// =============================================================================

/// @brief The objects a block of the text can be instead of a paragraph
enum class TextObject {
    None,            ///< A paragraph of text
    HorizontalRule,  ///< A thin line as wide as the text (<hr/>)
    SceneBreak,      ///< A break between two scenes, shown as the Settings say (<scene-break/>)
    PageBreak        ///< The text after it starts on a new page (<page-break/>)
};

/// @brief QTextFormat::objectType() of each object
inline constexpr int HORIZONTAL_RULE_OBJECT = QTextFormat::UserObject + 1;
inline constexpr int SCENE_BREAK_OBJECT = QTextFormat::UserObject + 2;
inline constexpr int PAGE_BREAK_OBJECT = QTextFormat::UserObject + 3;

/// @brief The object a format's object type names; None for any other format
TextObject textObjectOf(const QTextFormat& format);

/// @brief The character format of an object's character
QTextCharFormat objectCharFormat(TextObject object);

/// @brief The object a block is; None for a paragraph of text
///
/// A block is an object when it holds nothing but the object replacement character in the
/// format of an object.
TextObject blockObject(const QTextBlock& block);

/// @brief How the scene breaks look (Settings > Editor > General, editor.sceneBreak.style)
enum class SceneBreakLook {
    Asterisks,  ///< "* * *"
    Asterism,   ///< "⁂"
    Fleuron,    ///< "❦"
    Line,       ///< A short line
    Blank       ///< An empty line
};

/// @brief Name of a look in the settings: "asterisks", "asterism", "fleuron", "line", "blank"
QString sceneBreakLookName(SceneBreakLook look);

/// @brief The look a name of the settings stands for; Asterisks for an unknown name
SceneBreakLook sceneBreakLookFromName(const QString& name);

/// @brief The text of a scene break that is drawn as text; empty for a line or a blank
QString sceneBreakText(SceneBreakLook look);

/// @brief KML element of an object: "hr", "scene-break", "page-break"; empty for None
QString kmlElementOf(TextObject object);

/// @brief Object of a KML element; None for any other element
TextObject textObjectOfElement(const QString& element);

// =============================================================================
// The styles of a book
// =============================================================================

/// @brief A paragraph style of a book, as its package defines it
struct ParagraphStyle {
    QString id;              ///< Unique among the book's styles
    QString name;            ///< Name in the program's language
    QString nextStyle;       ///< Style Enter starts after a paragraph of it; empty: the same
    QVariantMap properties;  ///< Its own and inherited properties (sizes in points), as the
                             ///< package names them: fontSize, lineHeight, alignment...

    bool operator==(const ParagraphStyle&) const = default;
};

/// @brief The paragraph styles of a book
///
/// Built from the styles of the book's type package and the packages it uses
/// (BookTypeRegistry::paragraphStyles()), with the inheritance resolved. There is always a
/// style Normal: without one in the package, the set has one without properties.
class ParagraphStyleSet {
public:
    /// @brief A set with only the style Normal, without properties
    ParagraphStyleSet();

    /// @brief The styles of a package
    /// @param styles Styles as BookTypeRegistry::paragraphStyles() gives them
    /// @param language Language of the names, e.g. "pl"
    static ParagraphStyleSet fromPackage(const QList<core::PackageParagraphStyle>& styles,
                                         const QString& language);

    /// @brief The styles in the order of their package
    const QList<ParagraphStyle>& styles() const { return m_styles; }

    /// @brief The style with this id, or nullptr
    const ParagraphStyle* find(const QString& id) const;

    /// @brief Whether the set has a style with this id
    bool contains(const QString& id) const { return find(id) != nullptr; }

    /// @brief The style Normal
    const ParagraphStyle& normal() const;

    /// @brief The style a paragraph is shown in: its own, or Normal when the set has no style
    ///        of this id (a paragraph pasted from another book keeps its id)
    const ParagraphStyle& shownStyle(const QString& id) const;

    /// @brief Style Enter starts after a paragraph of @p id at its end
    QString nextStyleOf(const QString& id) const;

    /// @brief A property of a style as it is shown (see shownStyle())
    QVariant property(const QString& id, const QString& name) const;

    /// @brief Whether the spelling and grammar of paragraphs in the style are checked
    ///        (the property spellCheck; true without it)
    bool isChecked(const QString& id) const;

    bool operator==(const ParagraphStyleSet&) const = default;

private:
    QList<ParagraphStyle> m_styles;
};

}  // namespace kalahari::editor
