/// @file annotation_frame.h
/// @brief The frame an annotation's text is written in, at its place in the text

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/gui/panels/annotation_colors.h>

#include <QFrame>
#include <QRectF>

#include <functional>

class QLabel;
class QPushButton;
class QTextEdit;
class QToolButton;

namespace kalahari::gui {

/// @brief The frame an annotation's text is written in
///
/// Looks like a card of the Annotations panel and lies over the editor, under the place
/// of the annotation (above it when there is no room below), within the text column. Enter
/// starts a new line; Ctrl+Enter or Save keeps the text, Esc drops it - the frame says so.
/// Its X in the corner and a click in the editor beside it close it, the text kept. While
/// it has the keys, the window's shortcuts do nothing (bold, the annotation commands...
/// would change the text under it), except saving, closing and quitting; no key and no
/// click goes through it to the editor.
class AnnotationFrame : public QFrame {
    Q_OBJECT

public:
    /// @brief Where the frame goes: its place in the text and the room it has, in the
    /// coordinates of the editor it lies over
    struct Placement {
        QRectF place;   ///< The annotation's place: a caret's rectangle
        QRectF column;  ///< The text column (empty: the whole editor)
    };

    /// @brief Gives the placement as it is now (the view scrolls and changes)
    using PlacementProvider = std::function<Placement()>;

    /// @brief Constructor
    /// @param editor The editor it lies over (its parent)
    explicit AnnotationFrame(QWidget* editor);

    /// @brief The keys that keep the text, as the frame shows them ("Ctrl+Enter")
    static QString saveKeysText();

    /// @brief The annotation's kind (the frame's header)
    void setKind(editor::AnnotationKind kind);

    /// @brief Who the annotation is by (on the right of the header; empty: no one shown)
    void setAuthor(const QString& author);

    /// @brief Who the annotation is by, as set
    const QString& author() const { return m_author; }

    /// @brief The text to start with (an annotation edited again)
    void setText(const QString& text);

    /// @brief The text as written
    QString text() const;

    /// @brief Whether the text can be kept (it is not empty)
    bool canSave() const;

    /// @brief The frame's colors
    void setColors(const AnnotationCardColors& colors);

    /// @brief The colors it is shown in
    const AnnotationCardColors& colors() const { return m_colors; }

    /// @brief Follow a place of the text: the frame goes to it again whenever the editor is
    /// painted (scrolled, zoomed, edited) or resized
    void setPlacement(PlacementProvider provider);

    /// @brief Put the frame where its placement says now
    void place();

    /// @brief Give the keys to the text, with the cursor at its end
    void startTyping();

signals:
    /// @brief Ctrl+Enter or Save: keep the text
    void saveRequested();

    /// @brief Esc: drop the text
    void cancelRequested();

    /// @brief Its X, or a click in the editor beside it: close it, the text kept
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    /// @brief Ctrl+Enter: keep the text, when there is any
    void requestSave();

    /// @brief The text box as high as its text, between a few lines and a limit
    void updateTextHeight();

    /// @brief The author, shortened to the room the header has
    void updateAuthorLabel();

    /// @brief The colors of the texts and the button
    void updateStyle();

    /// @brief Ask for a place() once the editor's event is over
    void schedulePlacement();

    AnnotationCardColors m_colors;
    PlacementProvider m_placement;
    bool m_placementPending = false;
    QString m_author;
    QLabel* m_kindLabel{nullptr};
    QLabel* m_authorLabel{nullptr};
    QTextEdit* m_textEdit{nullptr};
    QLabel* m_hintLabel{nullptr};
    QPushButton* m_saveButton{nullptr};
    QToolButton* m_closeButton{nullptr};
};

}  // namespace kalahari::gui
