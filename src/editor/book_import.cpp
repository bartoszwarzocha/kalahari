// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_import.cpp
/// @brief Import of a file into a book: the document's texts and parts, the kinds of their
///        titles and the book's styles of the file's styles

#include <kalahari/editor/book_import.h>

#include <kalahari/core/text_statistics.h>

#include <QSet>

#include <algorithm>
#include <array>

namespace kalahari::editor {

namespace {

/// The longest title the start of a paragraph gives a text, in characters
constexpr qsizetype MAX_PARAGRAPH_TITLE = 60;

/// The deepest level of a heading of KML
constexpr int KML_HEADING_LEVELS = 3;

/// Whether a block is a paragraph with text (not only spaces)
bool hasText(const ImportBlock& block) {
    return block.kind == ImportBlock::Kind::Paragraph && !importPlainText(block).trimmed().isEmpty();
}

/// Whether a block has something of a text: characters, an image, a note, a table
bool hasContent(const ImportBlock& block) {
    if (block.kind == ImportBlock::Kind::Table) {
        return true;
    }
    if (block.kind != ImportBlock::Kind::Paragraph) {
        return false;
    }
    return std::any_of(block.runs.begin(), block.runs.end(), [](const ImportRun& run) {
        return run.kind == ImportRun::Kind::Image || run.kind == ImportRun::Kind::Note ||
               (run.kind == ImportRun::Kind::Text && !run.text.trimmed().isEmpty());
    });
}

/// The runs of the blocks without those of one side of the tracked changes
void resolveChanges(std::vector<ImportBlock>& blocks, ImportRevision dropped) {
    std::vector<ImportBlock> kept;
    kept.reserve(blocks.size());
    for (ImportBlock& block : blocks) {
        std::erase_if(block.runs, [dropped](const ImportRun& run) { return run.revision == dropped; });
        for (ImportRun& run : block.runs) {
            run.revision = ImportRevision::None;
        }
        if (block.table) {
            auto table = std::make_shared<ImportTable>(*block.table);
            for (ImportRow& row : table->rows) {
                for (ImportCell& cell : row.cells) {
                    resolveChanges(cell.blocks, dropped);
                }
            }
            block.table = std::move(table);
        }
        const bool gone = block.revision == dropped && block.kind == ImportBlock::Kind::Paragraph &&
                          !hasContent(block);
        block.revision = ImportRevision::None;
        if (!gone) {
            kept.push_back(std::move(block));
        }
    }
    blocks = std::move(kept);
}

/// The notes a block refers to, in its runs and its table's cells
void notesOf(const ImportBlock& block, std::vector<int>& notes) {
    for (const ImportRun& run : block.runs) {
        if (run.kind == ImportRun::Kind::Note && run.index >= 0) {
            notes.push_back(run.index);
        }
    }
    if (block.table) {
        for (const ImportRow& row : block.table->rows) {
            for (const ImportCell& cell : row.cells) {
                for (const ImportBlock& inner : cell.blocks) {
                    notesOf(inner, notes);
                }
            }
        }
    }
}

/// The text of a block for the word count: its paragraphs one per line
void appendText(const ImportBlock& block, QString& text) {
    if (block.kind == ImportBlock::Kind::Paragraph) {
        text += importPlainText(block) + QLatin1Char('\n');
    } else if (block.table) {
        for (const ImportRow& row : block.table->rows) {
            for (const ImportCell& cell : row.cells) {
                for (const ImportBlock& inner : cell.blocks) {
                    appendText(inner, text);
                }
            }
        }
    }
}

/// The words of blocks [from, to) of a document, with those of their notes
int wordsOf(const ImportDocument& document, size_t from, size_t to) {
    QString text;
    std::vector<int> notes;
    for (size_t i = from; i < to && i < document.blocks.size(); ++i) {
        appendText(document.blocks[i], text);
        notesOf(document.blocks[i], notes);
    }
    const core::WordCountRules rules;
    core::TextCounts counts = core::countText(text, rules);
    for (const int note : notes) {
        if (note < static_cast<int>(document.notes.size())) {
            QString noteText;
            for (const ImportBlock& paragraph : document.notes[static_cast<size_t>(note)].paragraphs) {
                appendText(paragraph, noteText);
            }
            core::addNoteCounts(counts, noteText, rules);
        }
    }
    return counts.words;
}

/// The title of a heading: its text in one line
QString titleOf(const ImportBlock& block) {
    return importPlainText(block).simplified();
}

/// The title the start of a paragraph gives: its first words, at most MAX_PARAGRAPH_TITLE
/// characters, with an ellipsis when it goes on
QString paragraphTitle(const ImportBlock& block) {
    QString text = titleOf(block);
    if (text.size() <= MAX_PARAGRAPH_TITLE) {
        return text;
    }
    qsizetype end = text.lastIndexOf(QLatin1Char(' '), MAX_PARAGRAPH_TITLE);
    if (end <= 0) {
        end = MAX_PARAGRAPH_TITLE;
    }
    while (end > 1 && (text.at(end - 1).isPunct() || text.at(end - 1).isSpace())) {
        --end;  // "tekstu…", not "tekstu,…"
    }
    return text.left(end) + QChar(0x2026);
}

/// The level of the heading of a top-level block that has text; 0: none
int levelOf(const ImportDocument& document, const ImportBlock& block) {
    return hasText(block) && block.list < 0 ? document.headingLevelOf(block) : 0;
}

/// A text of blocks [from, to): its title its first heading (not in it when the heading is
/// its title only), else the start of its first paragraph
ImportPiece pieceOf(const ImportDocument& document, size_t from, size_t to, bool headingAsTitle,
                    bool* fromHeading = nullptr) {
    ImportPiece piece;
    if (fromHeading) {
        *fromHeading = false;
    }
    piece.from = from;
    piece.to = to;
    for (size_t i = from; i < to; ++i) {
        const ImportBlock& block = document.blocks[i];
        if (!hasContent(block)) {
            continue;
        }
        if (levelOf(document, block) > 0) {
            piece.title = titleOf(block);
            if (headingAsTitle) {
                piece.from = i + 1;
            }
            if (fromHeading) {
                *fromHeading = true;
            }
        } else if (hasText(block)) {
            piece.title = paragraphTitle(block);
        }
        break;
    }
    return piece;
}

/// Whether blocks [from, to) of a document have something of a text
bool hasContent(const ImportDocument& document, size_t from, size_t to) {
    for (size_t i = from; i < to && i < document.blocks.size(); ++i) {
        if (hasContent(document.blocks[i])) {
            return true;
        }
    }
    return false;
}

/// The texts between the page breaks (which go), or from each block that starts a text
std::vector<ImportPiece> splitAt(const ImportDocument& document, const SplitOptions& options,
                                 bool pageBreaks) {
    std::vector<ImportPiece> pieces;
    bool firstFromHeading = true;
    const size_t count = document.blocks.size();
    size_t from = 0;
    const auto add = [&](size_t to) {
        if (hasContent(document, from, to)) {
            bool fromHeading = false;
            pieces.push_back(pieceOf(document, from, to, options.headingAsTitle, &fromHeading));
            if (pieces.size() == 1) {
                firstFromHeading = fromHeading;
            }
        }
    };
    for (size_t i = 0; i < count; ++i) {
        const ImportBlock& block = document.blocks[i];
        if (pageBreaks && block.kind == ImportBlock::Kind::PageBreak) {
            add(i);
            from = i + 1;  // the page break goes: a text starts on a new page
        } else if (!pageBreaks && block.startsText && i > from) {
            add(i);
            from = i;
        }
    }
    add(count);
    if (pieces.size() > 1 && !firstFromHeading) {
        pieces.front().beforeTitles = true;  // a title page
    }
    return pieces;
}

/// The texts and parts at the headings
std::vector<ImportPiece> splitAtHeadings(const ImportDocument& document, const SplitOptions& options) {
    std::vector<ImportPiece> pieces;
    const size_t count = document.blocks.size();
    const int partLevel = options.partLevel != options.textLevel ? options.partLevel : 0;

    // The text being read, and whether it is kept only with content (the text before the first
    // title, a part's text before its first text); a text of a heading stays without any
    std::optional<ImportPiece> text = ImportPiece();
    text->beforeTitles = true;
    bool needsContent = true;
    const auto close = [&](size_t to) {
        if (!text) {
            return;
        }
        text->to = to;
        if (!needsContent || hasContent(document, text->from, text->to)) {
            text->words = wordsOf(document, text->from, text->to);
            pieces.push_back(*text);
        }
        text.reset();
    };
    const auto start = [&](size_t heading, bool inPart) {
        ImportPiece piece;
        piece.title = titleOf(document.blocks[heading]);
        piece.inPart = inPart;
        piece.from = options.headingAsTitle ? heading + 1 : heading;
        text = piece;
        needsContent = false;
    };

    bool inPart = false;
    for (size_t i = 0; i < count; ++i) {
        const int level = levelOf(document, document.blocks[i]);
        if (level == 0) {
            continue;
        }
        if (partLevel > 0 && level == partLevel) {
            // A part when headings of texts come after it before the next part
            bool texts = false;
            for (size_t j = i + 1; j < count; ++j) {
                const int next = levelOf(document, document.blocks[j]);
                if (next == options.textLevel) {
                    texts = true;
                    break;
                }
                if (next == partLevel) {
                    break;
                }
            }
            close(i);
            if (texts) {
                ImportPiece part;
                part.part = true;
                part.title = titleOf(document.blocks[i]);
                part.from = i + 1;
                part.to = i + 1;
                pieces.push_back(part);
                inPart = true;
                // The part's text before its first text: a text of the part with its title
                ImportPiece opening;
                opening.title = part.title;
                opening.inPart = true;
                opening.from = i + 1;
                text = opening;
                needsContent = true;
            } else {
                inPart = false;
                start(i, false);
            }
        } else if (level == options.textLevel) {
            close(i);
            start(i, inPart);
        }
    }
    close(count);
    if (pieces.size() == 1) {
        pieces.front().beforeTitles = false;  // no title after it: the whole text, not a title page
    }
    return pieces;
}

/// The id Word gives a style of the book in the export (DocxWriter::mapStyles())
QString exportedStyleId(const QString& id) {
    static const std::array<std::pair<const char*, const char*>, 7> builtIn{{
        {"normal", "Normal"},
        {"heading_1", "Heading1"},
        {"heading_2", "Heading2"},
        {"heading_3", "Heading3"},
        {"quote", "Quote"},
        {"caption", "Caption"},
        {"footnote", "FootnoteText"},
    }};
    for (const auto& [book, word] : builtIn) {
        if (id == QLatin1String(book)) {
            return QString::fromLatin1(word);
        }
    }
    QString result;
    bool upper = true;
    for (const QChar ch : id) {
        if (ch.isLetterOrNumber() && ch.unicode() < 128) {
            result += upper ? ch.toUpper() : ch;
            upper = false;
        } else {
            upper = true;
        }
    }
    return result;
}

/// The book's style that a style of Word, LibreOffice or Google Docs is, by its name
QString builtInStyleOf(const QString& name) {
    const QString lower = name.trimmed().toLower();
    static const std::array<std::pair<const char*, const char*>, 22> names{{
        {"normal", "normal"},
        {"default paragraph style", "normal"},
        {"default style", "normal"},
        {"standard", "normal"},
        {"text body", "normal"},
        {"body text", "normal"},
        {"body text first indent", "normal"},
        {"first paragraph", "normal"},
        {"list paragraph", "normal"},
        {"no spacing", "normal"},
        {"normalny", "normal"},
        {"title", "heading_1"},
        {"tytuł", "heading_1"},
        {"quote", "quote"},
        {"intense quote", "quote"},
        {"block text", "quote"},
        {"quotations", "quote"},
        {"cytat", "quote"},
        {"caption", "caption"},
        {"footnote text", "footnote"},
        {"endnote text", "footnote"},
        {"plain text", "code"},
    }};
    for (const auto& [word, book] : names) {
        if (lower == QLatin1String(word)) {
            return QString::fromLatin1(book);
        }
    }
    if (lower == QLatin1String("html preformatted") || lower == QLatin1String("preformatted text") ||
        lower == QLatin1String("source code")) {
        return QStringLiteral("code");
    }
    return QString();
}

}  // namespace

// =============================================================================
// The document
// =============================================================================

const ImportStyle* ImportDocument::style(const QString& key) const {
    const auto it = std::find_if(styles.begin(), styles.end(),
                                 [&key](const ImportStyle& style) { return style.key == key; });
    return it == styles.end() ? nullptr : &*it;
}

int ImportDocument::headingLevelOf(const ImportBlock& block) const {
    if (block.headingLevel > 0) {
        return block.headingLevel;
    }
    const ImportStyle* own = style(block.style);
    return own ? own->headingLevel : 0;
}

ImportDocument withChangesResolved(const ImportDocument& document, bool reject) {
    ImportDocument resolved = document;
    const ImportRevision dropped = reject ? ImportRevision::Inserted : ImportRevision::Deleted;
    resolveChanges(resolved.blocks, dropped);
    for (ImportNote& note : resolved.notes) {
        resolveChanges(note.paragraphs, dropped);
    }
    return resolved;
}

QString importPlainText(const ImportBlock& block) {
    QString text;
    for (const ImportRun& run : block.runs) {
        switch (run.kind) {
            case ImportRun::Kind::Text:
                text += run.text;
                break;
            case ImportRun::Kind::LineBreak:
            case ImportRun::Kind::Tab:
                text += QLatin1Char(' ');
                break;
            default:
                break;
        }
    }
    return text;
}

// =============================================================================
// Texts and parts
// =============================================================================

std::vector<ImportPiece> splitDocument(const ImportDocument& document, const SplitOptions& options) {
    std::vector<ImportPiece> pieces;
    switch (options.mode) {
        case SplitMode::Headings:
            pieces = splitAtHeadings(document, options);
            break;
        case SplitMode::PageBreaks:
            pieces = splitAt(document, options, true);
            break;
        case SplitMode::Marks:
            pieces = splitAt(document, options, false);
            break;
        case SplitMode::Whole:
            if (hasContent(document, 0, document.blocks.size())) {
                ImportPiece piece = pieceOf(document, 0, document.blocks.size(), options.headingAsTitle);
                if (piece.from == 0 && !document.blocks.empty() && levelOf(document, document.blocks.front()) == 0) {
                    piece.title.clear();  // no heading: the caller names it
                }
                pieces.push_back(piece);
            }
            break;
    }
    for (ImportPiece& piece : pieces) {
        if (!piece.part && piece.words == 0) {
            piece.words = wordsOf(document, piece.from, piece.to);
        }
    }
    return pieces;
}

std::map<int, int> headingCounts(const ImportDocument& document) {
    std::map<int, int> counts;
    for (const ImportBlock& block : document.blocks) {
        if (const int level = levelOf(document, block); level > 0) {
            ++counts[level];
        }
    }
    return counts;
}

SplitOptions suggestedSplit(const ImportDocument& document) {
    SplitOptions options;
    const std::map<int, int> counts = headingCounts(document);
    if (counts.empty()) {
        const bool marks = std::any_of(document.blocks.begin(), document.blocks.end(),
                                       [](const ImportBlock& block) { return block.startsText; });
        const bool pages = std::any_of(document.blocks.begin(), document.blocks.end(), [](const ImportBlock& block) {
            return block.kind == ImportBlock::Kind::PageBreak;
        });
        options.mode = marks ? SplitMode::Marks : pages ? SplitMode::PageBreaks : SplitMode::Whole;
        return options;
    }
    const auto first = counts.begin();
    options.textLevel = first->first;
    const auto second = std::next(first);
    if (second == counts.end()) {
        return options;
    }
    // One heading of the first level: the title of the book over the texts, but a part's
    if (first->second == 1 && !document.fromKalahari) {
        const auto heading = std::find_if(document.blocks.begin(), document.blocks.end(), [&](const ImportBlock& block) {
            return levelOf(document, block) == first->first;
        });
        if (heading != document.blocks.end() && !isPartTitle(titleOf(*heading))) {
            options.textLevel = second->first;
            return options;
        }
    }
    // Parts: headings of the first level with those of the next one under them, in the
    // program's own file or when such a heading names a part
    const ImportBlock* previous = nullptr;
    for (const ImportBlock& block : document.blocks) {
        const int level = levelOf(document, block);
        if (level == second->first && previous && levelOf(document, *previous) == first->first &&
            (document.fromKalahari || isPartTitle(titleOf(*previous)))) {
            options.partLevel = first->first;
            options.textLevel = second->first;
            break;
        }
        if (level > 0) {
            previous = &block;
        }
    }
    return options;
}

QString kindOfTitle(const QString& title, const std::vector<KindTitle>& kinds) {
    const QString text = title.simplified();
    for (const KindTitle& kind : kinds) {
        for (const QString& name : kind.titles) {
            const QString wanted = name.simplified();
            if (wanted.isEmpty() || !text.startsWith(wanted, Qt::CaseInsensitive)) {
                continue;
            }
            if (text.size() == wanted.size() || !text.at(wanted.size()).isLetterOrNumber()) {
                return kind.kind;
            }
        }
    }
    return QString();
}

// =============================================================================
// Styles
// =============================================================================

QString suggestedBookStyle(const ImportDocument& document, const QString& key, const ParagraphStyleSet& book) {
    QSet<QString> seen;
    QString current = key;
    while (!current.isEmpty() && !seen.contains(current)) {
        seen.insert(current);
        if (book.contains(current)) {
            return current;  // the book's own id (Markdown, plain text)
        }
        const ImportStyle* style = document.style(current);
        for (const ParagraphStyle& own : book.styles()) {
            if (exportedStyleId(own.id) == current) {
                return own.id;  // exported from the book
            }
        }
        if (!style) {
            break;
        }
        for (const ParagraphStyle& own : book.styles()) {
            if (own.name.compare(style->name, Qt::CaseInsensitive) == 0 ||
                own.id.compare(style->name, Qt::CaseInsensitive) == 0) {
                return own.id;
            }
        }
        QString builtIn = builtInStyleOf(style->name);
        if (!builtIn.isEmpty()) {
            if (book.contains(builtIn)) {
                return builtIn;
            }
            if (builtIn == QLatin1String("quote") && book.contains(QStringLiteral("block_quote"))) {
                return QStringLiteral("block_quote");
            }
        }
        if (style->headingLevel > 0) {
            const QString heading =
                QStringLiteral("heading_%1").arg(std::min(style->headingLevel, KML_HEADING_LEVELS));
            if (book.contains(heading)) {
                return heading;
            }
        }
        current = style->basedOn;
    }
    return QStringLiteral("normal");
}

}  // namespace kalahari::editor
