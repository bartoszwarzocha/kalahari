// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file annotation_frame.h
/// @brief The frame an annotation's text is written in, at its place in the text

#ifndef KALAHARI_GUI_PANELS_ANNOTATION_FRAME_H
#define KALAHARI_GUI_PANELS_ANNOTATION_FRAME_H

#include <kalahari/editor/annotation.h>
#include <kalahari/gui/panels/writing_frame.h>

class QLabel;
class QPushButton;

namespace kalahari::gui {

/// @brief The frame an annotation's text is written in
///
/// Looks like a card of the Annotations panel and lies over the editor, under the place
/// of the annotation (above it when there is no room below), within the text column. Enter
/// starts a new line; Ctrl+Enter or Save keeps the text, Esc or its X in the corner drops it -
/// the frame says so. With the cards under the paragraphs it is the text of the annotation's
/// card, written in place, in the card's font: the card keeps its header, Save and the X are
/// not shown, and a click elsewhere keeps the text (WritingFrame::Placement::cardText). How it
/// keeps the keys and closes: WritingFrame.
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
    [[nodiscard]] const QString& author() const { return m_author; }

    /// @brief The text to start with (an annotation edited again)
    void setText(const QString& text);

    /// @brief The text as written
    [[nodiscard]] QString text() const;

    /// @brief Whether the text can be kept (it is not empty)
    [[nodiscard]] bool canSave() const override;

protected:
    void cardTextChanged() override;
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

#endif  // KALAHARI_GUI_PANELS_ANNOTATION_FRAME_H
