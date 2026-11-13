/// @file annotation_frame.h
/// @brief The frame an annotation's text is written in, at its place in the text

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/gui/panels/writing_frame.h>

class QLabel;
class QPushButton;

namespace kalahari::gui {

/// @brief The frame an annotation's text is written in
///
/// Looks like a card of the Annotations panel and lies over the editor, under the place
/// of the annotation (above it when there is no room below), within the text column; with
/// the cards under the paragraphs, over the annotation's card, as wide as it (the card makes
/// room for the frame as high as it is: cardHeightNeeded()). Enter starts a new line;
/// Ctrl+Enter or Save keeps the text, Esc or its X in the corner drops it - the frame says
/// so. How it keeps the keys and closes: WritingFrame.
class AnnotationFrame : public WritingFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param commands The commands of its window, whose keys it knows
    /// @param editor The editor it lies over (its parent)
    AnnotationFrame(CommandRegistry& commands, QWidget* editor);

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
    bool canSave() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /// @brief The author, shortened to the room the header has
    void updateAuthorLabel();

    QString m_author;
    QLabel* m_authorLabel{nullptr};
    QPushButton* m_saveButton{nullptr};
};

}  // namespace kalahari::gui
