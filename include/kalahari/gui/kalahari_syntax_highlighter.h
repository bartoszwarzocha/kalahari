/// @file kalahari_syntax_highlighter.h
/// @brief Syntax highlighter stub for Kalahari editor (Task #00007)
///
/// Placeholder for Phase 1 implementation. Currently does no highlighting.
/// Phase 1 will add Markdown syntax support, bold/italic detection, etc.

#pragma once

#include <QSyntaxHighlighter>
#include <QTextDocument>

namespace kalahari {
namespace gui {

/// @brief Syntax highlighter for Kalahari text editor
///
/// STUB: No highlighting implemented yet. Will be enhanced in Phase 1
/// to support Markdown, plain text, and custom formatting.
///
/// Phase 1 features (planned):
/// - Markdown syntax highlighting (headers, bold, italic, links)
/// - Code block detection
/// - Quote highlighting
/// - List markers
class KalahariSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QTextDocument
    explicit KalahariSyntaxHighlighter(QTextDocument* parent = nullptr);

    /// @brief Destructor
    ~KalahariSyntaxHighlighter() override = default;

protected:
    /// @brief Highlight single block of text
    /// @param text Text to highlight
    ///
    /// STUB: Currently does nothing. Will implement in Phase 1:
    /// - Markdown headers (# ## ###)
    /// - Bold (**text** or __text__)
    /// - Italic (*text* or _text_)
    /// - Links ([text](url))
    /// - Code blocks (```)
    void highlightBlock(const QString& text) override;
};

} // namespace gui
} // namespace kalahari
