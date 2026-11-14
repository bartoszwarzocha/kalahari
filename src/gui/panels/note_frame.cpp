/// @file note_frame.cpp
/// @brief The frame a note's text is written in, at the place of the note

#include "kalahari/gui/panels/note_frame.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_link.h"
#include "kalahari/gui/dialogs/hyperlink_dialog.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui {

namespace {

/// @brief How much of the less important text the lines between the parts of the frame have
constexpr double SEPARATOR_SHARE = 0.3;

/// @brief The text of a note: what is pasted or dropped keeps only what a note keeps, and the
/// links are shown as links (only shown: the text keeps no color or underline of its own)
class NoteTextEdit : public QTextEdit {
public:
    NoteTextEdit() {
        setAcceptRichText(true);
        // After the change: the extra selections are cursors of the document, which a cursor
        // made or dropped while the document is cleared (QTextDocument::clear()) breaks
        connect(this, &QTextEdit::textChanged, this, &NoteTextEdit::showLinks,
                Qt::QueuedConnection);
        connect(this, &QTextEdit::cursorPositionChanged, this, &NoteTextEdit::leaveLinkEnd);
    }

    /// @brief The color the links are shown in
    void setLinkColor(const QColor& color) {
        m_linkColor = color;
        showLinks();
    }

protected:
    void insertFromMimeData(const QMimeData* source) override {
        if (source == nullptr) {
            return;
        }
        QTextDocument pasted;
        if (source->hasHtml()) {
            pasted.setHtml(source->html());
        } else if (source->hasText()) {
            pasted.setPlainText(source->text());
        } else {
            return;
        }
        QTextDocument kept;
        editor::setNoteContent(kept, editor::noteContentOf(pasted));
        textCursor().insertFragment(QTextDocumentFragment(&kept));
        ensureCursorVisible();
    }

private:
    /// @brief The links in their color, underlined, as extra selections
    void showLinks() {
        QList<ExtraSelection> links;
        for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
            for (const editor::TextLink& link : editor::linksIn(block)) {
                ExtraSelection selection;
                selection.cursor = QTextCursor(document());
                selection.cursor.setPosition(link.from);
                selection.cursor.setPosition(link.to, QTextCursor::KeepAnchor);
                if (m_linkColor.isValid()) {
                    selection.format.setForeground(m_linkColor);
                }
                selection.format.setFontUnderline(true);
                links.append(selection);
            }
        }
        setExtraSelections(links);
    }

    /// @brief Text typed just after a link is not a part of it (as in the text)
    void leaveLinkEnd() {
        const QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            return;
        }
        const QString link = editor::linkAddressOf(cursor.charFormat());
        if (link.isEmpty()) {
            return;
        }
        QTextCursor next(cursor);
        const bool atEnd = !next.movePosition(QTextCursor::NextCharacter) ||
                           next.block() != cursor.block() ||
                           editor::linkAddressOf(next.charFormat()) != link;
        if (atEnd) {
            QTextCharFormat format = cursor.charFormat();
            editor::setLinkAddress(format, QString());
            setCurrentCharFormat(format);
        }
    }

    QColor m_linkColor;
};

/// @brief The link the cursor is in, all of it; else the cursor as it is
QTextCursor wholeLink(QTextCursor cursor) {
    const QString address = editor::linkAddressOf(cursor.charFormat());
    if (cursor.hasSelection() || address.isEmpty()) {
        return cursor;
    }
    const int position = cursor.position();
    for (const editor::TextLink& link : editor::linksIn(cursor.block())) {
        if (position >= link.from && position <= link.to) {
            cursor.setPosition(link.from);
            cursor.setPosition(link.to, QTextCursor::KeepAnchor);
            break;
        }
    }
    return cursor;
}

/// @brief The address of a link on a range of the text (empty: no link)
void setLinkOn(const QTextCursor& range, const QString& address) {
    QTextDocument* document = range.document();
    const int from = range.selectionStart();
    const int to = range.selectionEnd();
    for (QTextBlock block = document->findBlock(from); block.isValid() && block.position() < to;
         block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int start = std::max(from, fragment.position());
            const int end = std::min(to, fragment.position() + fragment.length());
            if (!fragment.isValid() || start >= end) {
                continue;
            }
            QTextCursor piece(document);
            piece.setPosition(start);
            piece.setPosition(end, QTextCursor::KeepAnchor);
            QTextCharFormat format = fragment.charFormat();
            editor::setLinkAddress(format, address);
            piece.setCharFormat(format);
        }
    }
}

}  // namespace

NoteFrame::NoteFrame(CommandRegistry& commands, QWidget* editor)
    : WritingFrame(commands, editor, new NoteTextEdit(), QStringLiteral("noteFrame"))
{
    // As in the mockup: the title, then the X in the corner; the text without a box of its own
    if (auto* frameLayout = qobject_cast<QVBoxLayout*>(layout())) {
        frameLayout->setContentsMargins(10, 4, 6, 6);
    }
    headerLayout()->insertStretch(1, 1);
    textEdit()->setObjectName(QStringLiteral("noteFrameText"));
    //: The frame a footnote's or an endnote's text is written in. In Polish: Wpisz treść
    //: przypisu...
    textEdit()->setPlaceholderText(tr("Type the text of the note..."));
    //: %1 is the keys that keep the text: "Ctrl+Enter" (on macOS their symbols). In Polish:
    //: %1 – zachowaj · Esc – anuluj
    hintLabel()->setText(tr("%1 – keep · Esc – cancel").arg(saveKeysText()));
}

void NoteFrame::setNote(editor::NoteKind kind, const QString& number) {
    setTitle(editor::BookEditor::noteTitle(kind, number));
}

void NoteFrame::setContent(const QString& content) {
    editor::setNoteContent(*textEdit()->document(), content);
    textEdit()->document()->clearUndoRedoStacks();
}

QString NoteFrame::content() const {
    return editor::noteContentOf(*textEdit()->document());
}

void NoteFrame::setTextFont(const QFont& font) {
    textEdit()->setFont(font);
    textEdit()->document()->setDefaultFont(font);
    updateTextHeight();
}

bool NoteFrame::textKeyPressed(QKeyEvent* event) {
    QTextEdit* text = textEdit();

    // Bold, italic and underline, on the selection or the text typed next, and a link: with
    // the keys the commands have in the text (Ctrl+B, Ctrl+I, Ctrl+U and Ctrl+K by default)
    QTextCharFormat format;
    const QTextCharFormat current = text->currentCharFormat();
    if (hasCommandKeys(event, "format.bold")) {
        format.setFontWeight(current.fontWeight() > QFont::Normal ? QFont::Normal : QFont::Bold);
    } else if (hasCommandKeys(event, "format.italic")) {
        format.setFontItalic(!current.fontItalic());
    } else if (hasCommandKeys(event, "format.underline")) {
        format.setFontUnderline(!current.fontUnderline());
    } else if (hasCommandKeys(event, "insert.link")) {
        editLink();  // as in the text: the window of a link's text and address
        return true;
    } else {
        return false;
    }
    text->mergeCurrentCharFormat(format);
    return true;
}

void NoteFrame::setLinkColor(const QColor& color) {
    static_cast<NoteTextEdit*>(textEdit())->setLinkColor(color);
}

void NoteFrame::editLink() {
    QTextEdit* text = textEdit();
    QTextCursor cursor = wholeLink(text->textCursor());
    // The address of the link the selection starts in (the format after its first character)
    QTextCursor first(text->document());
    first.setPosition(cursor.hasSelection() ? cursor.selectionStart() + 1 : cursor.position());
    const QString existing = editor::linkAddressOf(first.charFormat());
    QTextDocument* document = text->document();
    const bool oneParagraph =
        document->findBlock(cursor.selectionStart()) == document->findBlock(cursor.selectionEnd());

    showOwnWindow([this, text, cursor, existing, oneParagraph]() mutable {
        dialogs::HyperlinkDialog dialog(window());
        dialog.setLink(cursor.selectedText(), existing, !existing.isEmpty(), oneParagraph);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        cursor.beginEditBlock();
        if (dialog.removeAsked()) {
            setLinkOn(cursor, QString());
        } else {
            const QString target = editor::normalizedLinkAddress(dialog.address());
            const QString shown = dialog.text().isEmpty() ? dialog.address() : dialog.text();
            if (oneParagraph && shown != cursor.selectedText()) {
                // The text it shows, written anew: in the format of its start
                QTextCharFormat format = text->currentCharFormat();
                editor::setLinkAddress(format, target);
                cursor.insertText(shown, format);
            } else {
                setLinkOn(cursor, target);
            }
        }
        cursor.endEditBlock();
        cursor.clearSelection();
        text->setTextCursor(cursor);
    });
}

QString NoteFrame::styleSheetFor(const AnnotationCardColors& colors) const {
    // The text as on the paper, without a box of its own
    return WritingFrame::styleSheetFor(colors) +
           QStringLiteral("QTextEdit#noteFrameText { color: %1; background: transparent;"
                          " border: none; }")
               .arg(colors.text.name());
}

void NoteFrame::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    const AnnotationCardColors& shown = colors();
    const QColor background =
        shown.background.isValid() ? shown.background : palette().color(QPalette::Base);
    const QColor border = shown.kind.isValid() ? shown.kind : palette().color(QPalette::Mid);
    const QColor secondary =
        shown.secondary.isValid() ? shown.secondary : palette().color(QPalette::PlaceholderText);

    // A box in the frame's color, a line under the header and over the footer
    painter.fillRect(rect(), background);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    const QColor separator = mixedColor(background, secondary, SEPARATOR_SHARE);
    painter.setPen(QPen(separator, 1.0));
    const QRect text = textEdit()->geometry();
    const qreal headerLine = std::floor(text.top() - 2.0) + 0.5;
    const qreal footerLine = std::floor(text.bottom() + 3.0) + 0.5;
    painter.drawLine(QPointF(1.0, headerLine), QPointF(width() - 1.0, headerLine));
    painter.drawLine(QPointF(1.0, footerLine), QPointF(width() - 1.0, footerLine));
}

}  // namespace kalahari::gui
