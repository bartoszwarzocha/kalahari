/// @file annotation_cards.h
/// @brief The cards of the annotations under their paragraphs (the continuous views)

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/editor_appearance.h>

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>

#include <optional>
#include <vector>

class QPainter;
class QTextBlock;

namespace kalahari::editor {

/// @brief A part of a card that a click finds
enum class AnnotationCardPart {
    Card,     ///< The card itself: its header and its text
    Menu,     ///< Its "..." button: the menu of the annotation
    Done,     ///< The box of a to-do: it is done
    ShowAll,  ///< "Show all" under a text cut short
    Line,     ///< The line of a collapsed annotation: it expands
};

/// @brief A card under a paragraph, where it is
struct AnnotationCardBox {
    AnnotationMark mark;     ///< Whose card it is (the mark of its annotation)
    QRectF rect;             ///< The card, in the coordinates of its paragraph
    bool collapsed = false;  ///< Collapsed: a line with the start of its text
    bool writing = false;    ///< Its text is written in the frame over it: the room only
    int shownLines = 0;      ///< The lines of its text shown; 0: all
};

/// @brief What a point of a paragraph's cards is on
struct AnnotationCardHit {
    QString id;                                        ///< The annotation
    AnnotationCardPart part = AnnotationCardPart::Card;  ///< The part of its card
    QRectF rect;                                       ///< The card (paragraph coordinates)
};

/// @brief The cards of the annotations under their paragraphs
///
/// Each annotation not done has a card under the paragraph its mark is in, in the order of
/// the marks: the kind, the author and the date over the text (or the text alone, with the
/// author and the date at its end), in the kind's color on the paper. A collapsed one is its
/// mark alone, or a line with the start of its text. The cards are part of the view, not of
/// the text: the layout gives them room under their paragraph (spaceBelow()), and they are
/// drawn and found in the paragraph's coordinates, zoomed with its text.
///
/// One annotation at a time can be written in the frame over its card: the card keeps the
/// frame's room (setWriting()), a new one's room among the cards in the order of its place.
class AnnotationCards {
    Q_DECLARE_TR_FUNCTIONS(AnnotationCards)

public:
    /// @brief The colors the cards are drawn in
    struct Colors {
        QColor paper;      ///< What they lie on
        QColor text;       ///< The text on the paper
        QColor comment;    ///< The kinds' colors on the paper (the marks'); not valid: the text's
        QColor todo;
        QColor note;
        QColor selection;  ///< The outline of the card of the annotation gone to
    };

    // =========================================================================
    // What is shown
    // =========================================================================

    /// @brief How they look and which there are
    void setSettings(const AnnotationCardSettings& settings);
    const AnnotationCardSettings& settings() const { return m_settings; }

    /// @brief Whether the view shows them now (a continuous view, Distraction-Free as the
    ///        settings say)
    void setShown(bool shown) { m_shown = shown; }
    bool isShown() const { return m_shown; }

    /// @brief Whether an annotation of a kind has a card now
    bool hasCard(AnnotationKind kind) const;

    /// @brief A new text: its annotations are collapsed or expanded as the settings say,
    ///        none is written in its frame and every long one is cut again
    void startText();

    /// @brief Whether an annotation is collapsed
    bool isCollapsed(const QString& id) const;

    /// @brief Collapse or expand an annotation
    void setCollapsed(const QString& id, bool collapsed);

    /// @brief Collapse or expand every annotation, also those made later
    void setAllCollapsed(bool collapsed);

    /// @brief Show an annotation cut short whole
    void showWhole(const QString& id);

    // =========================================================================
    // Looks
    // =========================================================================

    void setColors(const Colors& colors) { m_colors = colors; }
    const Colors& colors() const { return m_colors; }

    /// @brief The font of the text: the cards' text is sized after it
    void setTextFont(const QFont& font) { m_textFont = font; }

    /// @brief The font of the cards' text
    QFont cardFont() const;

    /// @brief The color of a kind on the paper
    QColor kindColor(AnnotationKind kind) const;

    // =========================================================================
    // Writing in the frame over the card
    // =========================================================================

    /// @brief The annotation written in the frame, and the room the frame takes
    struct Writing {
        QString id;          ///< The annotation; empty: a new one
        AnnotationKind kind = AnnotationKind::Comment;  ///< Its kind
        int block = -1;      ///< A new one: the paragraph of its mark...
        int offset = 0;      ///< ...and the mark's place in it
        qreal height = 0;    ///< The frame's height (document units)
    };

    /// @brief An annotation is written in the frame over its card (a new one: at its place)
    void setWriting(const Writing& writing) { m_writing = writing; }

    /// @brief No annotation is written in the frame
    void clearWriting() { m_writing.reset(); }

    const std::optional<Writing>& writing() const { return m_writing; }

    // =========================================================================
    // The mouse
    // =========================================================================

    /// @brief The card the mouse is on, and its part (empty id: none)
    void setHovered(const QString& id, std::optional<AnnotationCardPart> part);
    const QString& hovered() const { return m_hovered; }
    const std::optional<AnnotationCardPart>& hoveredPart() const { return m_hoveredPart; }

    /// @brief The card of the annotation gone to (empty: none): it is outlined
    void setSelected(const QString& id) { m_selected = id; }
    const QString& selected() const { return m_selected; }

    // =========================================================================
    // Geometry and drawing (in the coordinates of a paragraph)
    // =========================================================================

    /// @brief The cards of a paragraph, where they are
    /// @param block The paragraph
    /// @param top Where its cards start: under its text and its spacing
    /// @param width The width of its text
    std::vector<AnnotationCardBox> cardsOf(const QTextBlock& block, qreal top, qreal width) const;

    /// @brief The room a paragraph's cards take under it (0: none)
    qreal spaceBelow(const QTextBlock& block, qreal width) const;

    /// @brief The card (and its part) at a point of a paragraph
    std::optional<AnnotationCardHit> hitTest(const QTextBlock& block, qreal top, qreal width,
                                             const QPointF& point) const;

    /// @brief Draw a paragraph's cards
    void paint(QPainter* painter, const QTextBlock& block, qreal top, qreal width) const;

    /// @brief The name of a kind on its card
    static QString kindTitle(AnnotationKind kind);

private:
    /// @brief The marks of a paragraph that have cards, with the room of a new annotation
    ///        written in the frame (a mark without an id)
    std::vector<AnnotationMark> cardMarks(const QTextBlock& block) const;

    /// @brief Whether a long text is cut short (not shown whole)
    bool cutsShort(const QString& id) const;

    AnnotationCardSettings m_settings;
    bool m_shown = false;
    bool m_collapsedByDefault = false;  ///< All collapsed, except m_toggled
    QSet<QString> m_toggled;            ///< Annotations not as m_collapsedByDefault says
    QSet<QString> m_whole;              ///< Long ones shown whole
    Colors m_colors;
    QFont m_textFont;
    std::optional<Writing> m_writing;
    QString m_hovered;
    std::optional<AnnotationCardPart> m_hoveredPart;
    QString m_selected;
};

}  // namespace kalahari::editor
