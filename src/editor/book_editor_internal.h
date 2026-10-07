/// @file book_editor_internal.h
/// @brief Helpers shared by the BookEditor source files (not part of the editor's API)

#pragma once

#include <kalahari/editor/kml_document_model.h>
#include <functional>
#include <utility>
#include <QAbstractUndoItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace kalahari::editor {

/// @brief Get paragraph length (excluding block separator)
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Character count in paragraph (length - 1 to exclude block separator)
inline int paragraphLength(QTextDocument* doc, int index) {
    if (!doc) return 0;
    QTextBlock block = doc->findBlockByNumber(index);
    return block.isValid() ? (block.length() - 1) : 0;
}

/// @brief Undo item that calls back when its step is undone or redone
class CallbackUndoItem final : public QAbstractUndoItem {
public:
    explicit CallbackUndoItem(std::function<void()> callback)
        : m_callback(std::move(callback)) {}

    void undo() override { m_callback(); }
    void redo() override { m_callback(); }

private:
    std::function<void()> m_callback;
};

/// @brief Fill a document from a parsed KML model, starting at the cursor's (empty) block
///
/// Each paragraph gets zero margins and its own alignment, if it has one (without one it
/// is shown with the default), and its text on a clean base format with the run formats
/// on top. Shared by loading a chapter and pasting Kalahari content,
/// so both read KML the same way.
inline void appendParagraphs(QTextCursor& cursor, const KmlDocumentModel& model) {
    QTextBlockFormat zeroMarginFormat;
    zeroMarginFormat.setTopMargin(0);
    zeroMarginFormat.setBottomMargin(0);

    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        QTextBlockFormat blockFormat = zeroMarginFormat;
        if (const Qt::Alignment alignment = model.paragraphAlignment(i); alignment) {
            blockFormat.setAlignment(alignment);
        }
        if (i > 0) {
            cursor.insertBlock(blockFormat);
        } else {
            cursor.setBlockFormat(blockFormat);
        }

        // An EXPLICIT default char format, so the text does not take the format the cursor
        // still carries from the previous paragraph's last run (formatting bled into every
        // following paragraph on reload); the runs then format only their own ranges.
        const int blockStart = cursor.position();
        cursor.insertText(model.paragraphText(i), QTextCharFormat());
        for (const auto& run : model.paragraphFormats(i)) {
            cursor.setPosition(blockStart + static_cast<int>(run.start));
            cursor.setPosition(blockStart + static_cast<int>(run.end), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(run.format);
        }
        cursor.movePosition(QTextCursor::EndOfBlock);
    }
}

}  // namespace kalahari::editor
