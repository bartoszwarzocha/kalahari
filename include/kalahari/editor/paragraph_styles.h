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
class QTextCursor;
class QTextDocument;

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

/// @brief Width of the short line of a scene break (SceneBreakLook::Line), as a share of the
///        line
inline constexpr qreal SCENE_BREAK_LINE_SHARE = 1.0 / 6.0;

/// @brief KML element of an object: "hr", "scene-break", "page-break"; empty for None
QString kmlElementOf(TextObject object);

/// @brief Object of a KML element; None for any other element
TextObject textObjectOfElement(const QString& element);

// =============================================================================
// Editing around objects
// =============================================================================

/// @brief Block format of a new paragraph: Normal, without an alignment of its own (zero
///        margins, as the editor gives every block)
QTextBlockFormat paragraphBlockFormat();

/// @brief Block format of an object's block: no style or alignment of its own
QTextBlockFormat objectBlockFormat();

/// @brief Whether the character at a position of the document is an object
bool isObjectAt(const QTextDocument& document, int position);

/// @brief Whether a cursor without a selection is in an object's block (before or after the
///        object)
bool isAtObject(const QTextCursor& cursor);

/// @brief Give every object in the blocks from @p from to @p to a block of its own
///
/// An edit can join an object with text: a selection from a paragraph into an object
/// deleted, an object pasted into a paragraph. The text before it and after it become
/// paragraphs of their own, in the format of the block they came from (@p tailFormat for
/// text after an object that starts its block, when given). Call it within the edit block
/// of the change: one undo step with it.
/// @param cursor The editor's cursor: at the place of a new paragraph break it stays at the
///        end of the text before the break, or after an object goes to the start of the text
///        after it
void separateObjects(QTextCursor& cursor, int from, int to,
                     const QTextBlockFormat* tailFormat = nullptr,
                     const QTextCharFormat* tailCharFormat = nullptr);

/// @brief Remove the cursor's selection; objects it leaves joined with text get blocks of
///        their own again (separateObjects())
///
/// The text after the selection keeps the format of its paragraph when the selection
/// starts at an object: an object never gives its block to text. A selection of whole
/// objects (from the start of one to the end of another) takes their blocks too, as Delete
/// takes an object (removeBlock()); with @p keepParagraph, for text typed in their place, an
/// empty paragraph stays there.
void removeSelectionKeepingObjects(QTextCursor& cursor, bool keepParagraph = false);

/// @brief Move a cursor at an object into a new empty paragraph, for text to go in
///
/// Before the object (at the start of its block) the paragraph goes before it, after the
/// object after it; anywhere else nothing changes. Text, a line break or a paste never go
/// into an object's block. Call it within the edit block of the change.
/// @return Whether the cursor went into a new paragraph
bool leaveObject(QTextCursor& cursor);

/// @brief Insert an object at a cursor without a selection, as one edit block
///
/// The object gets a block of its own: the text before the cursor stays in its paragraph,
/// the text after it goes to a paragraph after the object, in the paragraph's format. At
/// the end of a paragraph a new one follows the object, in @p nextFormat; an empty
/// paragraph stays after the object. Before an object the new one goes before it, after an
/// object after it. The cursor ends after the new object: at the start of the paragraph
/// after it, or after it when it was put next to another object.
/// @param nextFormat Block format of the paragraph after an object put at the end of one
///        (the style Enter would start there)
void insertObject(QTextCursor& cursor, TextObject object, const QTextBlockFormat& nextFormat);

/// @brief Remove a whole block with its paragraph break, as one edit block: an object, or
///        an empty paragraph next to one
///
/// The block after it takes its place with its own formats; the last block goes with the
/// break before it, the only one becomes an empty paragraph. A break at a table stays
/// (isTableBreakAt()): the block goes with the other break, and the only paragraph of a
/// cell or between a table and the start or end of the text, or another table, becomes an
/// empty paragraph. The annotations on its start stay on the place. The cursor ends where
/// the block was.
void removeBlock(QTextCursor& cursor, const QTextBlock& block);

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
