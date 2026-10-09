/// @file annotation.h
/// @brief Annotations on the text: comments, TODOs and notes
///
/// An annotation is anchored to the text it is about: to a fragment, whose every
/// character carries it, or to a place in the text, where the character before the place
/// carries it (at the start of a paragraph, the paragraph's block character format does).
/// The whole annotation - its kind, text, author and state - travels with its anchor in
/// the KmlPropAnnotations character property, so the document's native undo, the
/// clipboard and drag and drop handle annotations together with the text.
///
/// Editing keeps annotations where the writer put them: text typed next to a fragment
/// does not join it, text typed inside it does, and deleted text leaves its annotations on
/// its place. Only removing an annotation takes it off the text; cut and moved text takes
/// its annotations along.

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QStringView>
#include <QTextCharFormat>

#include <optional>
#include <string>
#include <vector>

class QTextBlock;
class QTextCursor;
class QTextDocument;

namespace kalahari::editor {

/// @brief What an annotation is
enum class AnnotationKind {
    Comment,  ///< A remark to discuss or revise the text; can be resolved
    Todo,     ///< Something to do; can be done
    Note,     ///< The author's own note
};

/// @brief An annotation: a comment, a TODO or a note
struct Annotation {
    QString id;                                     ///< Unique in the book
    AnnotationKind kind = AnnotationKind::Comment;  ///< What it is
    QString text;                                   ///< Its own text, maybe of several lines
    QString author;                                 ///< Who made it
    QDateTime created;                              ///< When it was made (invalid: unknown)
    bool done = false;                              ///< A TODO done, a comment resolved
    bool point = false;                             ///< On a place, not on a fragment
    QMap<QString, QString> otherAttributes;         ///< KML attributes the editor does not know

    bool operator==(const Annotation& other) const = default;
};

/// @brief The annotations one character carries
using AnnotationList = QList<Annotation>;

/// @brief Where an annotation is in the text
///
/// For a fragment: from its first character to the position after its last one. For a
/// place: both are the position of the place.
struct AnnotationPlace {
    Annotation annotation;  ///< The annotation
    int start = 0;          ///< Where the fragment starts, or the place
    int end = 0;            ///< Where the fragment ends, or the place
};

/// @brief The mark of an annotation in its paragraph: after the last character of its
/// fragment, or on its place
struct AnnotationMark {
    int offset = 0;                                 ///< Where, in the paragraph's text
    AnnotationKind kind = AnnotationKind::Comment;  ///< The annotation's kind
    QString id;                                     ///< The annotation's id
    QString text;                                   ///< The annotation's text

    bool operator==(const AnnotationMark& other) const = default;
};

// =============================================================================
// Annotations
// =============================================================================

/// @brief The KML name of a kind: "comment", "todo" or "note"
QString annotationKindName(AnnotationKind kind);

/// @brief The kind a KML name stands for (std::nullopt for an unknown name)
std::optional<AnnotationKind> annotationKindFromName(QStringView name);

/// @brief The theme's editor color of a kind (the "editor" section of a theme file)
///
/// Each kind has a color for the light and one for the dark paper, e.g.
/// "annotationCommentLightPaper"; ThemeManager::editorColor() reads it.
/// @param kind The kind
/// @param darkPaper true: the color on dark paper (and on a dark panel)
std::string annotationColorKey(AnnotationKind kind, bool darkPaper);

/// @brief A new annotation id, unique in every book
QString newAnnotationId();

/// @brief The annotations a format carries (none when it carries none)
AnnotationList annotationsOf(const QTextFormat& format);

/// @brief Make a format carry these annotations (none: it carries none)
void setAnnotations(QTextFormat& format, const AnnotationList& annotations);

/// @brief A list with an annotation added, or put in place of the one with its id
AnnotationList withAnnotation(AnnotationList list, const Annotation& annotation);

// =============================================================================
// Annotations in a document
// =============================================================================

/// @brief The annotations of a document, in text order (by where they end)
std::vector<AnnotationPlace> annotationsIn(const QTextDocument& document);

/// @brief Where an annotation is (std::nullopt when the document has none with this id)
std::optional<AnnotationPlace> findAnnotation(const QTextDocument& document, const QString& id);

/// @brief The ids of the annotations of a document
QSet<QString> annotationIdsIn(const QTextDocument& document);

/// @brief The marks a paragraph shows, in text order
///
/// One for each annotation not done that ends in the paragraph: a fragment that goes on
/// into a later paragraph has its mark there. A to-do done or a resolved comment has none.
std::vector<AnnotationMark> annotationMarksIn(const QTextBlock& block);

/// @brief Anchor an annotation to the text, as one undo step
///
/// The annotation goes on the cursor's selection, or on its position when nothing is
/// selected (or the selection holds no text, only paragraph breaks). It gets a new id
/// when it has none.
/// @return The annotation as anchored: with its id and its point flag
Annotation addAnnotation(const QTextCursor& cursor, Annotation annotation);

/// @brief Give an annotation new data (kind, text, state...) wherever it is, as one undo step
///
/// Its anchor stays: whether it is on a fragment or on a place.
/// @param document The document
/// @param annotation The annotation's new data; its id says which one it is
/// @param joinPreviousStep true: the change joins the document's last undo step instead of
///        making its own (typing an annotation's text makes one step of it all)
/// @return false when the document has no annotation with this id
bool updateAnnotation(QTextDocument& document, const Annotation& annotation,
                      bool joinPreviousStep = false);

/// @brief Take an annotation off the text, as one undo step
/// @return false when the document has no annotation with this id
bool removeAnnotation(QTextDocument& document, const QString& id);

// =============================================================================
// Editing with annotations
// =============================================================================

/// @brief The character format for text inserted at the cursor's position
///
/// The format the document gives the position, carrying only the annotations of the
/// fragments the position is inside of. Text typed after a fragment does not join it,
/// and an annotation on a place never spreads to new text.
QTextCharFormat insertionFormat(const QTextCursor& cursor);

/// @brief The fragments every character of the text from @p from to @p to belongs to
///
/// Text that replaces it joins them. None when the range holds no characters.
AnnotationList enclosingFragments(const QTextDocument& document, int from, int to);

/// @brief The annotations that would leave the text with the text from @p from to @p to
///
/// Those on places inside it (a place at @p to included: its character goes) and on
/// fragments that lie wholly inside it, in text order, as annotations on a place.
AnnotationList annotationsLeavingWith(const QTextDocument& document, int from, int to);

/// @brief Anchor annotations to a place in the text
///
/// They go on the character before the place, or at the start of a paragraph on the
/// paragraph. An annotation the place already has is replaced.
void anchorToPlace(QTextDocument& document, int position, const AnnotationList& annotations);

/// @brief Remove the cursor's selection; its annotations stay on its place
///
/// An annotation whose anchor goes with the text stays on the place of the text (see
/// annotationsLeavingWith()). Without a selection nothing changes.
void removeKeepingAnnotations(QTextCursor& cursor);

/// @brief Remove the cursor's selection to put new text in its place
///
/// The annotations of the selection stay on its place, except the fragments the whole
/// selection belongs to: the new text joins them, as it joins the fragments the place is
/// inside of. One undo step with the insertion when both are in one edit block.
/// @return The fragments the new text joins
AnnotationList removeForReplacement(QTextCursor& cursor);

/// @brief Type text at the cursor, replacing its selection
///
/// The text gets insertionFormat(); replacing a selection, it also joins the fragments
/// the whole selection belongs to, and the other annotations of the selection stay on its
/// place. A line break ("\n") starts a new paragraph. Without a selection, consecutive
/// calls make one undo step, as typing does.
void insertKeepingAnnotations(QTextCursor& cursor, const QString& text);

/// @brief Start a new paragraph at the cursor, replacing its selection
///
/// The new paragraph does not take the annotations of the place.
void insertParagraphKeepingAnnotations(QTextCursor& cursor);

/// @brief Delete the character before (Backspace) or after (Delete) the cursor
///
/// Or the paragraph break there. The annotations of the deleted character stay on its
/// place; without any, consecutive calls make one undo step, as deleting does.
void deleteCharacterKeepingAnnotations(QTextCursor& cursor, bool backward);

/// @brief A format whose annotations have new ids
/// @param format The format
/// @param newIds New id of each annotation to rename; the others keep theirs
QTextCharFormat withRenamedAnnotations(QTextCharFormat format, const QHash<QString, QString>& newIds);

/// @brief Prepare text from elsewhere (the clipboard, a drop) for the place it goes to
///
/// Its annotations whose ids @p document already has become copies with new ids, and its
/// characters join @p fragments - the fragments of the place. Paragraph starts keep only
/// their annotations on a place.
/// @param content The text to insert (changed in place; its undo is not recorded)
/// @param document The document the text goes into
/// @param fragments Fragments the inserted text joins
void prepareForInsertion(QTextDocument& content, const QTextDocument& document,
                         const AnnotationList& fragments);

}  // namespace kalahari::editor

Q_DECLARE_METATYPE(kalahari::editor::Annotation)
