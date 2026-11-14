/// @file note_frame.h
/// @brief The frame a note's text is written in, at the place of the note

#pragma once

#include <kalahari/editor/text_footnotes.h>
#include <kalahari/gui/panels/writing_frame.h>

#include <QColor>
#include <QFont>

namespace kalahari::gui {

/// @brief The frame a footnote's or an endnote's text is written in
///
/// It takes the place of the note's text: in the continuous views under its paragraph, as
/// wide as the text, where the note makes room for it; in the page view over the notes at the
/// bottom of the page, within the editor. Its header says which note it is ("Footnote 3"), its
/// footer what the keys do. The text keeps what a note keeps: bold, italic, underline,
/// strikethrough, raised and lowered text and links (Ctrl+B, Ctrl+I, Ctrl+U; pasted text
/// loses the rest), paragraphs (Enter) and line breaks (Shift+Enter). Ctrl+Enter keeps it,
/// Esc or the X drops it; how it keeps the keys and closes: WritingFrame.
class NoteFrame : public WritingFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param commands The commands of its window, whose keys it knows
    /// @param editor The editor it lies over (its parent)
    NoteFrame(CommandRegistry& commands, QWidget* editor);

    /// @brief The note it is for: its kind and its number ("Footnote 3")
    void setNote(editor::NoteKind kind, const QString& number);

    /// @brief The text to start with: KML of the note's paragraphs (Footnote::content)
    void setContent(const QString& content);

    /// @brief The text as written: KML of its paragraphs
    QString content() const;

    /// @brief The font of the text: the book's, as tall as the editor shows it
    void setTextFont(const QFont& font);

    /// @brief The color the links of the text are shown in
    void setLinkColor(const QColor& color);

protected:
    /// @brief Ctrl+K: the window of the link at the cursor, or of a new one on the selection
    void editLink();

    bool textKeyPressed(QKeyEvent* event) override;
    QString styleSheetFor(const AnnotationCardColors& colors) const override;
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace kalahari::gui
