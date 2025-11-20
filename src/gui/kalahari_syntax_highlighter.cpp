/// @file kalahari_syntax_highlighter.cpp
/// @brief Syntax highlighter implementation (stub - Task #00007)

#include "kalahari/gui/kalahari_syntax_highlighter.h"

namespace kalahari {
namespace gui {

KalahariSyntaxHighlighter::KalahariSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // STUB: No initialization needed yet
    // Phase 1 will add:
    // - QTextCharFormat for different styles (bold, italic, headers)
    // - Regular expressions for Markdown syntax
    // - Color schemes from themes
}

void KalahariSyntaxHighlighter::highlightBlock(const QString& text) {
    // STUB: No highlighting implemented
    // Phase 1 will add:
    // - Markdown syntax highlighting
    // - Bold/italic/heading detection
    // - Link highlighting
    // - Code block detection
    Q_UNUSED(text);
}

} // namespace gui
} // namespace kalahari
