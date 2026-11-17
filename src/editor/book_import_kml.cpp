// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_kml.cpp
/// @brief Import of a file into a book: the KML of a text, with the book's styles, the file's
///        formatting it keeps and the cleanups of its text

#include "book_export_internal.h"

#include <kalahari/editor/book_import.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_tables.h>

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

using exporting::xmlEscaped;

/// The deepest level of a heading of KML
constexpr int KML_HEADING_LEVELS = 3;

/// The character of an object of a line in the text autocorrect reads
constexpr char16_t OBJECT_CHARACTER = 0xFFFC;

/// An attribute of KML with a leading space
QString attribute(const QString& name, const QString& value) {
    return QLatin1Char(' ') + name + QStringLiteral("=\"") + xmlEscaped(value) + QLatin1Char('"');
}

// =============================================================================
// Cleanups
// =============================================================================

/// Whether a character is white space at the start of a paragraph
bool isLeadingSpace(QChar ch) {
    return ch == QLatin1Char(' ') || ch == QLatin1Char('\t') || ch == QChar(0x00A0) || ch == QChar(0x3000);
}

/// Whether a run is something of a text: characters other than spaces, or an object
bool hasContent(const ImportRun& run) {
    switch (run.kind) {
        case ImportRun::Kind::Text:
            return !run.text.trimmed().isEmpty() &&
                   std::any_of(run.text.begin(), run.text.end(), [](QChar ch) { return !ch.isSpace(); });
        case ImportRun::Kind::LineBreak:
        case ImportRun::Kind::Tab:
        case ImportRun::Kind::Annotation:
            return false;
        default:
            return true;
    }
}

/// Whether a paragraph has nothing in it but white space
bool isEmptyParagraph(const ImportBlock& block) {
    return block.kind == ImportBlock::Kind::Paragraph &&
           std::none_of(block.runs.begin(), block.runs.end(), [](const ImportRun& run) { return hasContent(run); });
}

/// Whether a paragraph is a mark of a scene break alone: "* * *", "***", "#", "⁂", "❦" or the
/// short line the program copies ("───"), with any white space ("*\u2003*\u2003*" of the export)
bool isSceneMarker(const ImportBlock& block) {
    if (block.kind != ImportBlock::Kind::Paragraph || block.list >= 0) {
        return false;
    }
    QString text;
    for (const ImportRun& run : block.runs) {
        if (run.kind == ImportRun::Kind::Text) {
            text += run.text;
        } else if (run.kind != ImportRun::Kind::LineBreak && run.kind != ImportRun::Kind::Tab &&
                   run.kind != ImportRun::Kind::Annotation) {
            return false;
        }
    }
    text.removeIf([](QChar ch) { return ch.isSpace(); });
    static const QRegularExpression marks(QStringLiteral("^(?:\\*{3,}|#|\u2042|\u2766|\u2500{3,})$"));
    return marks.match(text).hasMatch();
}

/// The runs without the white space at their start
void removeLeadingSpaces(std::vector<ImportRun>& runs) {
    while (!runs.empty()) {
        ImportRun& run = runs.front();
        if (run.kind == ImportRun::Kind::Tab) {
            runs.erase(runs.begin());
            continue;
        }
        if (run.kind != ImportRun::Kind::Text) {
            return;
        }
        qsizetype spaces = 0;
        while (spaces < run.text.size() && isLeadingSpace(run.text.at(spaces))) {
            ++spaces;
        }
        if (spaces < run.text.size()) {
            run.text.remove(0, spaces);
            return;
        }
        runs.erase(runs.begin());
    }
}

/// The runs with each run of spaces one space
void removeDoubleSpaces(std::vector<ImportRun>& runs) {
    bool afterSpace = false;
    for (ImportRun& run : runs) {
        if (run.kind == ImportRun::Kind::Annotation) {
            continue;
        }
        if (run.kind != ImportRun::Kind::Text) {
            afterSpace = false;
            continue;
        }
        QString text;
        text.reserve(run.text.size());
        for (const QChar ch : run.text) {
            const bool space = ch == QLatin1Char(' ');
            if (!(space && afterSpace)) {
                text += ch;
            }
            afterSpace = space;
        }
        run.text = text;
    }
    std::erase_if(runs, [](const ImportRun& run) { return run.kind == ImportRun::Kind::Text && run.text.isEmpty(); });
}

/// What autocorrect changes in an imported text: only the typography
AutoCorrectOptions typographyOptions(AutoCorrectOptions options) {
    options.replacements.clear();
    options.sentenceCapitals = false;
    options.twoInitialCapitals = false;
    options.capsLock = false;
    options.links = false;
    options.sceneBreak = false;
    options.horizontalRule = false;
    options.bulletedLists = false;
    options.numberedLists = false;
    return options;
}

/// The text of a run with its double quotation marks straight, for those of the book's language
QString withStraightQuotes(QString text) {
    for (QChar& ch : text) {
        if (ch == QChar(0x201C) || ch == QChar(0x201D) || ch == QChar(0x201E)) {  // “ ” „
            ch = QLatin1Char('"');
        }
    }
    return text;
}

/// The runs with the quotation marks, apostrophes, dashes and ellipses autocorrect makes, as if
/// the paragraph were typed; the double quotation marks of another language (“…”) become
/// those of the book's
std::vector<ImportRun> withTypography(const std::vector<ImportRun>& runs, const AutoCorrectOptions& options,
                                      const QuoteMarks& marks) {
    // The text with the run of each character; an object of the line is one character
    QString text;
    std::vector<size_t> origin;
    for (size_t i = 0; i < runs.size(); ++i) {
        const ImportRun& run = runs[i];
        switch (run.kind) {
            case ImportRun::Kind::Text:
                text += options.quotes ? withStraightQuotes(run.text) : run.text;
                origin.insert(origin.end(), static_cast<size_t>(run.text.size()), i);
                break;
            case ImportRun::Kind::LineBreak:
                text += QChar::LineSeparator;
                origin.push_back(i);
                break;
            case ImportRun::Kind::Tab:
                text += QLatin1Char('\t');
                origin.push_back(i);
                break;
            default:
                text += QChar(OBJECT_CHARACTER);
                origin.push_back(i);
                break;
        }
    }

    QString typed;
    std::vector<size_t> typedOrigin;
    const auto apply = [&](const std::optional<AutoCorrection>& change) {
        if (!change || change->start < 0 || change->end > typed.size() || change->start > change->end) {
            return;
        }
        const auto start = static_cast<size_t>(change->start);
        const auto end = static_cast<size_t>(change->end);
        // A change never takes an object with it
        const bool objects = std::any_of(typedOrigin.begin() + static_cast<std::ptrdiff_t>(start),
                                         typedOrigin.begin() + static_cast<std::ptrdiff_t>(end),
                                         [&runs](size_t run) { return runs[run].kind != ImportRun::Kind::Text; });
        if (objects) {
            return;
        }
        const size_t run = start < typedOrigin.size() ? typedOrigin[start] : typedOrigin.back();
        typed.replace(change->start, change->end - change->start, change->text);
        typedOrigin.erase(typedOrigin.begin() + static_cast<std::ptrdiff_t>(start),
                          typedOrigin.begin() + static_cast<std::ptrdiff_t>(end));
        typedOrigin.insert(typedOrigin.begin() + static_cast<std::ptrdiff_t>(start),
                           static_cast<size_t>(change->text.size()), run);
    };
    const auto quoteMarks = [&marks]() { return marks; };
    for (qsizetype k = 0; k < text.size(); ++k) {
        const QChar ch = text.at(k);
        typed += ch;
        typedOrigin.push_back(origin[static_cast<size_t>(k)]);
        if (runs[origin[static_cast<size_t>(k)]].kind == ImportRun::Kind::Text && startsAutoCorrection(ch)) {
            apply(autoCorrection(typed, options, quoteMarks));
        }
    }
    // Enter at the end of the paragraph: its last word ("1939--1945")
    if (!typed.isEmpty()) {
        apply(autoCorrectionAtParagraphEnd(typed, options));
    }

    // The runs again, the characters in the runs they came from
    std::vector<ImportRun> result;
    std::optional<size_t> last;
    for (qsizetype k = 0; k < typed.size(); ++k) {
        const size_t run = typedOrigin[static_cast<size_t>(k)];
        if (runs[run].kind != ImportRun::Kind::Text) {
            result.push_back(runs[run]);
            last.reset();
            continue;
        }
        if (last != run) {
            ImportRun piece = runs[run];
            piece.text.clear();
            result.push_back(std::move(piece));
            last = run;
        }
        result.back().text += typed.at(k);
    }
    return result;
}

// =============================================================================
// The writer
// =============================================================================

/// The KML of a text of a document
class KmlWriter {
public:
    KmlWriter(const ImportDocument& document, const ImportKmlOptions& options)
        : m_document(document), m_options(options), m_typography(typographyOptions(options.cleanup.autoCorrect)) {}

    QString write(const ImportPiece& piece) {
        std::vector<ImportBlock> blocks;
        for (size_t i = piece.from; i < piece.to && i < m_document.blocks.size(); ++i) {
            blocks.push_back(m_document.blocks[i]);
        }
        blocks = cleaned(std::move(blocks), true);
        collectAnnotations(blocks);

        QString body = blocksKml(blocks, false);
        QString kml = QStringLiteral("<kml>");
        if (!m_annotationOrder.empty()) {
            kml += QStringLiteral("<annotations>");
            for (const int index : m_annotationOrder) {
                kml += annotationKml(index);
            }
            kml += QStringLiteral("</annotations>");
        }
        return kml + body + QStringLiteral("</kml>");
    }

private:
    // -------------------------------------------------------------------------
    // Styles
    // -------------------------------------------------------------------------

    /// The book's style of a paragraph
    QString bookStyleOf(const ImportBlock& block) {
        QString style;
        const bool chosen = m_options.styles.contains(block.style);
        if (chosen) {
            style = m_options.styles.value(block.style);
        } else if (!block.style.isEmpty()) {
            auto it = m_suggested.find(block.style);
            if (it == m_suggested.end()) {
                it = m_suggested.insert(block.style,
                                        suggestedBookStyle(m_document, block.style, m_options.bookStyles));
            }
            style = it.value();
        }
        if (style.isEmpty()) {
            style = QStringLiteral("normal");  // a style the book lacks is kept and shown as Normal
        }
        // A paragraph that is a heading of its own (its outline level)
        if (!chosen && block.headingLevel > 0 && exporting::headingLevelOfStyle(m_options.bookStyles, style) == 0) {
            style = QStringLiteral("heading_%1").arg(std::min(block.headingLevel, KML_HEADING_LEVELS));
        }
        return style;
    }

    bool isCode(const QString& style) const {
        return exporting::isStyleBasedOn(m_options.bookStyles, style, QStringLiteral("code"));
    }

    // -------------------------------------------------------------------------
    // Cleanups
    // -------------------------------------------------------------------------

    /// The blocks with the cleanups of their text
    /// @param topLevel Of the text (not of a cell): its empty paragraphs and scene marks
    std::vector<ImportBlock> cleaned(std::vector<ImportBlock> blocks, bool topLevel) {
        const ImportCleanup& cleanup = m_options.cleanup;
        std::vector<ImportBlock> result;
        result.reserve(blocks.size());
        bool pendingScene = false;  // empty paragraphs before (EmptyParagraphs::SceneBreak)
        for (ImportBlock& block : blocks) {
            if (block.kind == ImportBlock::Kind::Table && block.table) {
                auto table = std::make_shared<ImportTable>(*block.table);
                for (ImportRow& row : table->rows) {
                    for (ImportCell& cell : row.cells) {
                        cell.blocks = cleaned(std::move(cell.blocks), false);
                    }
                }
                block.table = std::move(table);
            }
            if (block.kind == ImportBlock::Kind::Paragraph) {
                block.style = bookStyleOf(block);  // from here on the book's
                if (!isCode(block.style)) {
                    cleanParagraph(block);
                }
                if (topLevel && cleanup.sceneMarkers && isSceneMarker(block)) {
                    block = sceneBreak();
                }
            }
            if (isEmptyParagraph(block) && topLevel) {
                if (cleanup.empty == EmptyParagraphs::Remove) {
                    continue;
                }
                if (cleanup.empty == EmptyParagraphs::SceneBreak) {
                    pendingScene = true;
                    continue;
                }
            }
            if (pendingScene) {
                pendingScene = false;
                const bool afterBreak = !result.empty() && result.back().kind == ImportBlock::Kind::SceneBreak;
                if (!result.empty() && !afterBreak && block.kind != ImportBlock::Kind::SceneBreak &&
                    block.kind != ImportBlock::Kind::PageBreak) {
                    result.push_back(sceneBreak());
                }
            }
            result.push_back(std::move(block));
        }
        if (topLevel) {
            // A text starts on a new page and the next one too
            while (!result.empty() && result.front().kind == ImportBlock::Kind::PageBreak) {
                result.erase(result.begin());
            }
            while (!result.empty() && result.back().kind == ImportBlock::Kind::PageBreak) {
                result.pop_back();
            }
        }
        return result;
    }

    static ImportBlock sceneBreak() {
        ImportBlock block;
        block.kind = ImportBlock::Kind::SceneBreak;
        return block;
    }

    void cleanParagraph(ImportBlock& block) {
        const ImportCleanup& cleanup = m_options.cleanup;
        if (cleanup.leadingSpaces) {
            removeLeadingSpaces(block.runs);
        }
        if (cleanup.doubleSpaces) {
            removeDoubleSpaces(block.runs);
        }
        if (cleanup.typography) {
            block.runs = withTypography(block.runs, m_typography, cleanup.quoteMarks);
        }
        if (m_options.dialogueStyle && block.style == QLatin1String("normal") && block.list < 0 &&
            m_options.bookStyles.contains(QStringLiteral("dialogue"))) {
            const QString text = importPlainText(block);
            qsizetype start = 0;
            while (start < text.size() && isLeadingSpace(text.at(start))) {
                ++start;
            }
            if (start < text.size() && (text.at(start) == QChar(0x2014) || text.at(start) == QChar(0x2013))) {
                block.style = QStringLiteral("dialogue");
            }
        }
    }

    // -------------------------------------------------------------------------
    // Annotations
    // -------------------------------------------------------------------------

    void collectAnnotations(const std::vector<ImportBlock>& blocks) {
        if (!m_options.annotations) {
            return;
        }
        for (const ImportBlock& block : blocks) {
            for (const ImportRun& run : block.runs) {
                for (const int index : run.annotations) {
                    addAnnotation(index);
                }
                if (run.kind == ImportRun::Kind::Annotation) {
                    addAnnotation(run.index);
                }
            }
            if (block.table) {
                for (const ImportRow& row : block.table->rows) {
                    for (const ImportCell& cell : row.cells) {
                        collectAnnotations(cell.blocks);
                    }
                }
            }
        }
    }

    void addAnnotation(int index) {
        if (index >= 0 && index < static_cast<int>(m_document.annotations.size()) && !m_annotationIds.contains(index)) {
            m_annotationIds.insert(index, newAnnotationId());
            m_annotationOrder.push_back(index);
        }
    }

    QString annotationKml(int index) const {
        const ImportAnnotation& annotation = m_document.annotations[static_cast<size_t>(index)];
        AnnotationKind kind = annotation.kind;
        QString text = annotation.text;
        if (kind == AnnotationKind::Comment) {
            for (const auto& [labelKind, label] : m_options.annotationLabels) {
                const QString word = label.trimmed();
                if (!word.isEmpty() && text.startsWith(word, Qt::CaseInsensitive) && text.size() > word.size() &&
                    text.at(word.size()) == QLatin1Char(':')) {
                    kind = labelKind;
                    text = text.mid(word.size() + 1).trimmed();
                    break;
                }
            }
        }
        QString kml = QStringLiteral("<annotation") + attribute(QStringLiteral("id"), m_annotationIds.value(index)) +
                      attribute(QStringLiteral("kind"), annotationKindName(kind));
        if (!annotation.author.isEmpty()) {
            kml += attribute(QStringLiteral("author"), annotation.author);
        }
        if (annotation.created.isValid()) {
            kml += attribute(QStringLiteral("created"), annotation.created.toUTC().toString(Qt::ISODate));
        }
        if (annotation.done) {
            kml += QStringLiteral(" done=\"true\"");
        }
        return kml + QLatin1Char('>') + xmlEscaped(text) + QStringLiteral("</annotation>");
    }

    // -------------------------------------------------------------------------
    // Blocks
    // -------------------------------------------------------------------------

    struct OpenList {
        int level = 0;
        ListLevel format;
        bool itemOpen = false;
    };

    /// The KML of blocks; in a cell only paragraphs and lists
    QString blocksKml(const std::vector<ImportBlock>& blocks, bool inCell) {
        QString kml;
        std::vector<OpenList> open;
        int openList = -1;
        int lastList = -1;
        const auto closeList = [&]() {
            if (open.back().itemOpen) {
                kml += QStringLiteral("</li>");
            }
            kml += QStringLiteral("</") + kmlListElementOf(open.back().format) + QLatin1Char('>');
            open.pop_back();
        };
        const auto closeLists = [&]() {
            if (!open.empty()) {
                lastList = openList;
            }
            while (!open.empty()) {
                closeList();
            }
        };

        for (const ImportBlock& block : blocks) {
            const bool item = block.kind == ImportBlock::Kind::Paragraph && block.list >= 0 &&
                              block.list < static_cast<int>(m_document.lists.size());
            if (!item) {
                closeLists();
                kml += blockKml(block, inCell);
                continue;
            }
            const ListDefinition& definition = m_document.lists[static_cast<size_t>(block.list)];
            // Another list at a deeper level is inside the item before it, as Word shows it
            if (!open.empty() && openList != block.list && block.listLevel == 0) {
                closeLists();
            }
            const int deepest = open.empty() ? 0 : open.back().level + 1;
            const int level = std::clamp(block.listLevel, 0, std::min(deepest, LIST_LEVELS - 1));
            const ListLevel& format = definition.levels[static_cast<size_t>(level)];
            while (!open.empty() && open.back().level > level) {
                closeList();
            }
            if (!open.empty() && open.back().level == level) {
                if (open.back().format != format) {
                    closeList();
                } else if (open.back().itemOpen) {
                    kml += QStringLiteral("</li>");
                    open.back().itemOpen = false;
                }
            }
            for (int next = open.empty() ? 0 : open.back().level + 1; next <= level; ++next) {
                const ListLevel& nextFormat = definition.levels[static_cast<size_t>(next)];
                kml += QLatin1Char('<') + kmlListElementOf(nextFormat) + listLevelToKml(nextFormat, next);
                if (next == 0 && block.list == lastList) {
                    kml += QStringLiteral(" continue=\"true\"");
                }
                kml += QLatin1Char('>');
                open.push_back({next, nextFormat, false});
            }
            if (level == 0) {
                openList = block.list;
            }
            kml += QStringLiteral("<li") + paragraphAttributes(block, true);
            if (block.value) {
                kml += attribute(QStringLiteral("value"), QString::number(std::clamp(*block.value, 0, 32767)));
            }
            if (!block.numbered) {
                kml += QStringLiteral(" numbered=\"false\"");
            }
            kml += QLatin1Char('>') + runsKml(block.runs, false);
            open.back().itemOpen = true;
        }
        closeLists();
        return kml;
    }

    QString blockKml(const ImportBlock& block, bool inCell) {
        switch (block.kind) {
            case ImportBlock::Kind::HorizontalLine:
                return inCell ? QString() : QStringLiteral("<hr/>");
            case ImportBlock::Kind::SceneBreak:
                return inCell ? QString() : QStringLiteral("<scene-break/>");
            case ImportBlock::Kind::PageBreak:
                return inCell ? QString() : QStringLiteral("<page-break/>");
            case ImportBlock::Kind::Table:
                return block.table ? tableKml(*block.table, inCell) : QString();
            case ImportBlock::Kind::Paragraph:
                break;
        }
        QString element = QStringLiteral("p");
        for (int level = 1; level <= KML_HEADING_LEVELS; ++level) {
            if (block.style == QStringLiteral("heading_%1").arg(level)) {
                element = QStringLiteral("h%1").arg(level);
            }
        }
        return QLatin1Char('<') + element + paragraphAttributes(block, false) + QLatin1Char('>') +
               runsKml(block.runs, false) + QStringLiteral("</") + element + QLatin1Char('>');
    }

    /// The attributes of a paragraph: its style, its alignment and its own formatting
    QString paragraphAttributes(const ImportBlock& block, bool item) const {
        QString kml;
        const QString& style = block.style;
        const bool heading = !item && style.startsWith(QLatin1String("heading_")) && style.size() == 9 &&
                             style.at(8).isDigit() && style.at(8).digitValue() <= KML_HEADING_LEVELS;
        if (style != QLatin1String("normal") && !heading) {
            kml += attribute(QStringLiteral("style"), style);
        }
        if (m_options.formatting == ImportFormatting::StylesOnly) {
            return kml;
        }
        const bool ownAlign = m_options.formatting == ImportFormatting::All ||
                              block.align == QLatin1String("center") || block.align == QLatin1String("right");
        if (!block.align.isEmpty() && ownAlign) {
            QString shown = m_options.bookStyles.property(style, QStringLiteral("alignment")).toString();
            if (shown.isEmpty()) {
                shown = QStringLiteral("justify");  // the editor's default
            }
            if (block.align != shown) {
                kml += attribute(QStringLiteral("align"), block.align);
            }
        }
        if (m_options.formatting == ImportFormatting::All) {
            ParagraphFormat own = block.own;
            own.pageBreakBefore.reset();  // a block of its own
            if (item) {
                own.leftIndent.reset();  // those of the list's level
                own.firstLineIndent.reset();
            }
            for (const auto& [name, value] : paragraphFormatToKml(own)) {
                kml += attribute(name, value);
            }
        }
        return kml;
    }

    QString tableKml(const ImportTable& table, bool inCell) {
        if (inCell) {
            // A table in a cell: its rows of text, its cells separated by tabs
            QString kml;
            for (const ImportRow& row : table.rows) {
                std::vector<ImportRun> runs;
                for (size_t c = 0; c < row.cells.size(); ++c) {
                    if (c > 0) {
                        ImportRun tab;
                        tab.kind = ImportRun::Kind::Tab;
                        runs.push_back(tab);
                    }
                    for (size_t p = 0; p < row.cells[c].blocks.size(); ++p) {
                        if (p > 0) {
                            ImportRun space;
                            space.text = QStringLiteral(" ");
                            runs.push_back(space);
                        }
                        const ImportBlock& paragraph = row.cells[c].blocks[p];
                        runs.insert(runs.end(), paragraph.runs.begin(), paragraph.runs.end());
                    }
                }
                kml += QStringLiteral("<p>") + runsKml(runs, false) + QStringLiteral("</p>");
            }
            return kml;
        }
        if (table.rows.empty()) {
            return QString();
        }
        QString kml = QStringLiteral("<table");
        if (!table.align.isEmpty()) {
            kml += attribute(QStringLiteral("align"), table.align);
        }
        kml += QLatin1Char('>');
        double total = 0.0;
        for (const double width : table.columns) {
            total += std::max(0.0, width);
        }
        if (total > 0.0) {
            for (const double width : table.columns) {
                kml += QStringLiteral("<col width=\"") + kmlNumber(std::max(0.0, width) * 100.0 / total) +
                       QStringLiteral("%\"/>");
            }
        }
        bool headerRows = true;
        for (const ImportRow& row : table.rows) {
            headerRows = headerRows && row.header;
            kml += QStringLiteral("<tr");
            if (row.height > 0.0) {
                kml += attribute(QStringLiteral("height"), kmlLength(row.height));
            }
            kml += QLatin1Char('>');
            const QString cellElement = headerRows ? QStringLiteral("th") : QStringLiteral("td");
            for (const ImportCell& cell : row.cells) {
                kml += QLatin1Char('<') + cellElement;
                if (cell.columnSpan > 1) {
                    kml += attribute(QStringLiteral("colspan"), QString::number(std::min(cell.columnSpan, 100)));
                }
                if (cell.rowSpan > 1) {
                    kml += attribute(QStringLiteral("rowspan"), QString::number(std::min(cell.rowSpan, 100)));
                }
                if (!cell.valign.isEmpty()) {
                    kml += attribute(QStringLiteral("valign"), cell.valign);
                }
                if (!cell.background.isEmpty() && m_options.formatting != ImportFormatting::StylesOnly) {
                    kml += attribute(QStringLiteral("background"), cell.background);
                }
                kml += QLatin1Char('>');
                const QString content = blocksKml(cell.blocks, true);
                kml += content.isEmpty() ? QStringLiteral("<p></p>") : content;
                kml += QStringLiteral("</") + cellElement + QLatin1Char('>');
            }
            kml += QStringLiteral("</tr>");
        }
        return kml + QStringLiteral("</table>");
    }

    // -------------------------------------------------------------------------
    // Runs
    // -------------------------------------------------------------------------

    /// The runs with the neighbouring text of one format, link and annotations as one
    static std::vector<ImportRun> merged(const std::vector<ImportRun>& runs) {
        std::vector<ImportRun> result;
        for (const ImportRun& run : runs) {
            if (run.kind == ImportRun::Kind::Text && run.text.isEmpty()) {
                continue;
            }
            if (!result.empty() && run.kind == ImportRun::Kind::Text && result.back().kind == ImportRun::Kind::Text &&
                result.back().format == run.format && result.back().link == run.link &&
                result.back().annotations == run.annotations) {
                result.back().text += run.text;
                continue;
            }
            result.push_back(run);
        }
        return result;
    }

    /// The format of a run the text keeps
    ImportCharFormat keptFormat(const ImportCharFormat& format, bool inNote) const {
        ImportCharFormat kept;
        if (m_options.formatting == ImportFormatting::StylesOnly) {
            return kept;
        }
        kept.bold = format.bold;
        kept.italic = format.italic;
        kept.underline = format.underline;
        kept.strike = format.strike;
        kept.subscript = format.subscript;
        kept.superscript = format.superscript && !format.subscript;
        if (m_options.formatting == ImportFormatting::All && !inNote) {
            kept.font = format.font;
            kept.size = format.size;
            kept.color = format.color;
            kept.background = format.background;
            kept.caps = format.caps;
            kept.spacing = format.spacing;
        }
        return kept;
    }

    /// The elements of a format: the opening ones in their order, the closing ones
    static std::pair<QString, QString> formatElements(const ImportCharFormat& format) {
        QStringList elements;
        if (format.bold) elements << QStringLiteral("b");
        if (format.italic) elements << QStringLiteral("i");
        if (format.underline) elements << QStringLiteral("u");
        if (format.strike) elements << QStringLiteral("s");
        if (format.subscript) {
            elements << QStringLiteral("sub");
        } else if (format.superscript) {
            elements << QStringLiteral("sup");
        }
        QString style;
        if (!format.font.isEmpty()) style += attribute(QStringLiteral("font"), format.font);
        if (format.size > 0.0) style += attribute(QStringLiteral("size"), kmlNumber(format.size));
        if (!format.color.isEmpty()) style += attribute(QStringLiteral("color"), format.color);
        if (!format.background.isEmpty()) style += attribute(QStringLiteral("bg"), format.background);
        if (!format.caps.isEmpty()) style += attribute(QStringLiteral("caps"), format.caps);
        if (format.spacing != 0.0) {
            style += attribute(QStringLiteral("spacing"), kmlNumber(std::clamp(format.spacing, -20.0, 100.0)));
        }
        if (elements.isEmpty() && !style.isEmpty()) {
            elements << QStringLiteral("span");
        }
        QString open;
        QString close;
        for (qsizetype i = 0; i < elements.size(); ++i) {
            open += QLatin1Char('<') + elements[i] + (i == 0 ? style : QString()) + QLatin1Char('>');
            close.prepend(QStringLiteral("</") + elements[i] + QLatin1Char('>'));
        }
        return {open, close};
    }

    /// The KML of the runs of a paragraph, or of a note's paragraph (@p inNote: only emphasis,
    /// links and line breaks)
    QString runsKml(const std::vector<ImportRun>& runs, bool inNote) {
        QString kml;
        for (const ImportRun& run : merged(runs)) {
            QString content;
            switch (run.kind) {
                case ImportRun::Kind::Text:
                    content = xmlEscaped(run.text);
                    break;
                case ImportRun::Kind::LineBreak:
                    kml += QStringLiteral("<br/>");
                    continue;
                case ImportRun::Kind::Tab:
                    kml += QLatin1Char('\t');
                    continue;
                case ImportRun::Kind::Annotation:
                    if (!inNote && m_annotationIds.contains(run.index)) {
                        kml += QStringLiteral("<anchor") + attribute(QStringLiteral("ref"), m_annotationIds.value(run.index)) +
                               QStringLiteral("/>");
                    }
                    continue;
                case ImportRun::Kind::Note:
                    if (inNote) {
                        continue;
                    }
                    content = noteKml(run.index);
                    break;
                case ImportRun::Kind::Image:
                    if (inNote) {
                        continue;
                    }
                    content = imageKml(run.index);
                    break;
                case ImportRun::Kind::Field:
                    if (inNote || run.text.isEmpty()) {
                        continue;
                    }
                    content = QStringLiteral("<field") + attribute(QStringLiteral("type"), run.text) +
                              (run.field.isEmpty() ? QString() : attribute(QStringLiteral("format"), run.field)) +
                              QStringLiteral("/>");
                    break;
                case ImportRun::Kind::CaptionNumber:
                    if (inNote || run.text.trimmed().isEmpty()) {
                        continue;
                    }
                    content = QStringLiteral("<caption-number") +
                              attribute(QStringLiteral("label"), run.text.trimmed().left(40)) + QStringLiteral("/>");
                    break;
            }
            if (content.isEmpty()) {
                continue;
            }
            const auto [open, close] = formatElements(keptFormat(run.format, inNote));
            content = open + content + close;
            if (!run.link.isEmpty() && isOpenableLinkAddress(run.link)) {
                content = QStringLiteral("<a") + attribute(QStringLiteral("href"), run.link) + QLatin1Char('>') +
                          content + QStringLiteral("</a>");
            }
            if (!inNote) {
                for (auto it = run.annotations.rbegin(); it != run.annotations.rend(); ++it) {
                    if (m_annotationIds.contains(*it)) {
                        content = QStringLiteral("<anchor") + attribute(QStringLiteral("ref"), m_annotationIds.value(*it)) +
                                  QLatin1Char('>') + content + QStringLiteral("</anchor>");
                    }
                }
            }
            kml += content;
        }
        return kml;
    }

    QString noteKml(int index) {
        if (index < 0 || index >= static_cast<int>(m_document.notes.size())) {
            return QString();
        }
        const ImportNote& note = m_document.notes[static_cast<size_t>(index)];
        std::vector<ImportBlock> paragraphs = note.paragraphs;
        for (ImportBlock& paragraph : paragraphs) {
            if (paragraph.style.isEmpty()) {
                paragraph.style = QStringLiteral("footnote");
            }
            cleanParagraph(paragraph);
        }
        std::erase_if(paragraphs, [](const ImportBlock& paragraph) {
            return paragraph.kind != ImportBlock::Kind::Paragraph || isEmptyParagraph(paragraph);
        });
        const QString id = newFootnoteId(m_noteIds);
        m_noteIds.insert(id);
        const QString element = note.endnote ? QStringLiteral("endnote") : QStringLiteral("footnote");
        QString content;
        if (paragraphs.size() == 1) {
            content = runsKml(paragraphs.front().runs, true);
        } else {
            for (const ImportBlock& paragraph : paragraphs) {
                content += QStringLiteral("<p>") + runsKml(paragraph.runs, true) + QStringLiteral("</p>");
            }
        }
        return QLatin1Char('<') + element + attribute(QStringLiteral("id"), id) + QLatin1Char('>') + content +
               QStringLiteral("</") + element + QLatin1Char('>');
    }

    QString imageKml(int index) const {
        if (index < 0 || index >= static_cast<int>(m_document.images.size())) {
            return QString();
        }
        const ImportImage& image = m_document.images[static_cast<size_t>(index)];
        const QString source = static_cast<size_t>(index) < m_options.imageSources.size()
                                   ? m_options.imageSources[static_cast<size_t>(index)]
                                   : QString();
        if (source.isEmpty()) {
            const QString alt = image.look.alt.trimmed();
            return alt.isEmpty() ? QString() : xmlEscaped(QLatin1Char('[') + alt + QLatin1Char(']'));
        }
        ImageProperties look = image.look;
        look.source = source;
        return editor::imageKml(look);
    }

    const ImportDocument& m_document;
    const ImportKmlOptions& m_options;
    const AutoCorrectOptions m_typography;
    QHash<QString, QString> m_suggested;  ///< The book's style of each style of the file
    QHash<int, QString> m_annotationIds;  ///< The KML id of each annotation of the text
    std::vector<int> m_annotationOrder;   ///< In the order of their places
    QSet<QString> m_noteIds;
};

}  // namespace

QString importKml(const ImportDocument& document, const ImportPiece& piece, const ImportKmlOptions& options) {
    if (piece.part) {
        return QStringLiteral("<kml></kml>");
    }
    KmlWriter writer(document, options);
    return writer.write(piece);
}

}  // namespace kalahari::editor
