// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file markdown_reader.cpp
/// @brief Import of Markdown: Qt's reading of GitHub Flavored Markdown, with what it does not
///        read itself (footnotes, a YAML header, hard line breaks, underline) marked before it

#include <kalahari/editor/book_import.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>
#include <QUrl>

#include <algorithm>
#include <map>

namespace kalahari::editor {

namespace {

/// The marks of what Qt does not read, private characters the text cannot have
constexpr char16_t MARK_UNDERLINE_START = 0xE000;
constexpr char16_t MARK_UNDERLINE_END = 0xE001;
constexpr char16_t MARK_LINE_BREAK = 0xE002;
constexpr char16_t MARK_SCENE_BREAK = 0xE003;
constexpr char16_t MARK_NOTE_START = 0xE004;  ///< Followed by the note's index and MARK_NOTE_END
constexpr char16_t MARK_NOTE_END = 0xE005;
constexpr char16_t LAST_MARK = 0xE00F;

/// The deepest heading of KML
constexpr int KML_HEADING_LEVELS = 3;

/// The longest YAML header the reader looks for, in lines
constexpr qsizetype MAX_HEADER_LINES = 50;

/// Whether a line opens or closes a fenced code block (``` or ~~~)
bool isFence(const QString& line) {
    static const QRegularExpression fence(QStringLiteral("^ {0,3}(```|~~~)"));
    return fence.match(line).hasMatch();
}

/// The value of a line of YAML ("title: Szklane miasto"), without its quotation marks
QString yamlValue(const QString& line) {
    QString value = line.section(QLatin1Char(':'), 1).trimmed();
    if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\''))) &&
        value.endsWith(value.at(0))) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

/// A line with the marks of its spans outside code spans: the references of the footnotes,
/// <u>, </u> and <br>
QString markedLine(const QString& line, const QHash<QString, int>& labels, std::vector<QString>& references) {
    static const QRegularExpression span(
        QStringLiteral("(`+)|(\\\\.)|\\[\\^([^\\]\\s]+)\\]|<(/?)u>|<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    QString result;
    qsizetype from = 0;
    QString codeTicks;
    QRegularExpressionMatchIterator it = span.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedStart() < from) {
            continue;
        }
        if (!codeTicks.isEmpty()) {
            if (match.captured(1) == codeTicks) {
                codeTicks.clear();
            }
            continue;
        }
        if (!match.captured(1).isEmpty()) {
            // A code span opens when its backticks close somewhere after it
            if (line.indexOf(match.captured(1), match.capturedEnd()) >= 0) {
                codeTicks = match.captured(1);
            }
            continue;
        }
        if (!match.captured(2).isEmpty()) {
            continue;  // an escaped character stays
        }
        QString mark;
        if (!match.captured(3).isEmpty()) {
            if (!labels.contains(match.captured(3))) {
                continue;
            }
            mark = QChar(MARK_NOTE_START) + QString::number(references.size()) + QChar(MARK_NOTE_END);
            references.push_back(match.captured(3));
        } else if (match.captured(0).startsWith(QLatin1String("<u"), Qt::CaseInsensitive) ||
                   match.captured(0).startsWith(QLatin1String("</u"), Qt::CaseInsensitive)) {
            mark = QChar(match.captured(4).isEmpty() ? MARK_UNDERLINE_START : MARK_UNDERLINE_END);
        } else {
            mark = QChar(MARK_LINE_BREAK);
        }
        result += line.mid(from, match.capturedStart() - from) + mark;
        from = match.capturedEnd();
    }
    return result + line.mid(from);
}

/// The Markdown of the file with its marks, its YAML header and the texts of its footnotes
/// taken out
struct MarkedMarkdown {
    QString text;
    QString title;
    QString author;
    QString language;
    std::vector<QString> notes;  ///< The Markdown of each reference's note, in their order
    bool html = false;           ///< It has HTML the reader leaves out
};

MarkedMarkdown marked(const QString& source) {
    MarkedMarkdown result;
    QString text = source;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    text.removeIf([](QChar ch) { return ch.unicode() >= MARK_UNDERLINE_START && ch.unicode() <= LAST_MARK; });
    QStringList lines = text.split(QLatin1Char('\n'));

    // The YAML header
    if (!lines.isEmpty() && lines.front().trimmed() == QLatin1String("---")) {
        for (qsizetype i = 1; i < lines.size() && i < MAX_HEADER_LINES; ++i) {
            const QString line = lines[i].trimmed();
            if (line == QLatin1String("---") || line == QLatin1String("...")) {
                for (qsizetype k = 1; k < i; ++k) {
                    const QString key = lines[k].section(QLatin1Char(':'), 0, 0).trimmed().toLower();
                    if (key == QLatin1String("title")) {
                        result.title = yamlValue(lines[k]);
                    } else if (key == QLatin1String("author")) {
                        result.author = yamlValue(lines[k]);
                    } else if (key == QLatin1String("lang") || key == QLatin1String("language")) {
                        result.language = yamlValue(lines[k]).section(QLatin1Char('-'), 0, 0).toLower();
                    }
                }
                lines = lines.mid(i + 1);
                break;
            }
        }
    }

    // The texts of the footnotes: "[^1]: text" and the indented lines after it
    static const QRegularExpression definition(QStringLiteral("^ {0,3}\\[\\^([^\\]\\s]+)\\]:\\s?(.*)$"));
    QHash<QString, QString> definitions;
    QStringList body;
    bool fenced = false;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (isFence(line)) {
            fenced = !fenced;
        }
        const QRegularExpressionMatch match = fenced ? QRegularExpressionMatch() : definition.match(line);
        if (!match.hasMatch()) {
            body << line;
            continue;
        }
        QStringList note{match.captured(2)};
        while (i + 1 < lines.size()) {
            const QString& next = lines[i + 1];
            const bool indented = next.startsWith(QStringLiteral("    ")) || next.startsWith(QLatin1Char('\t'));
            const bool blankThenIndented = next.trimmed().isEmpty() && i + 2 < lines.size() &&
                                           (lines[i + 2].startsWith(QStringLiteral("    ")) ||
                                            lines[i + 2].startsWith(QLatin1Char('\t')));
            if (!indented && !blankThenIndented) {
                break;
            }
            note << next.trimmed();
            ++i;
        }
        definitions.insert(match.captured(1), note.join(QLatin1Char('\n')));
    }

    // The marks
    QHash<QString, int> labels;
    for (auto it = definitions.begin(); it != definitions.end(); ++it) {
        labels.insert(it.key(), 0);
    }
    std::vector<QString> references;
    static const QRegularExpression starBreak(QStringLiteral("^ {0,3}\\*[ \\t]*\\*[ \\t]*\\*[ \\t*]*$"));
    static const QRegularExpression htmlTag(QStringLiteral("<(?!/?u>|br\\s*/?>|/?sub>|/?sup>|!--)[a-zA-Z/][^>]*>"),
                                            QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    fenced = false;
    for (qsizetype i = 0; i < body.size(); ++i) {
        QString line = body[i];
        if (isFence(line)) {
            fenced = !fenced;
            out << line;
            continue;
        }
        if (fenced) {
            out << line;
            continue;
        }
        if (starBreak.match(line).hasMatch()) {
            out << QString() << QString(QChar(MARK_SCENE_BREAK)) << QString();
            continue;
        }
        result.html = result.html || htmlTag.match(line).hasMatch();
        line = markedLine(line, labels, references);
        // A hard line break: a backslash or two spaces at the end of a line of a paragraph
        const bool more = i + 1 < body.size() && !body[i + 1].trimmed().isEmpty();
        if (more && line.endsWith(QLatin1Char('\\')) && !line.endsWith(QStringLiteral("\\\\"))) {
            line.chop(1);
            line += QChar(MARK_LINE_BREAK);
        } else if (more && line.endsWith(QStringLiteral("  ")) && !line.trimmed().isEmpty()) {
            while (line.endsWith(QLatin1Char(' '))) {
                line.chop(1);
            }
            line += QChar(MARK_LINE_BREAK);
        }
        out << line;
    }
    result.text = out.join(QLatin1Char('\n'));
    for (const QString& label : references) {
        result.notes.push_back(definitions.value(label));
    }
    return result;
}

/// The reader of a document Qt made of Markdown
class MarkdownWalker {
public:
    MarkdownWalker(ImportDocument& document, const QString& folder) : m_document(document), m_folder(folder) {}

    /// The blocks of a document; @p notes: the marks of the notes are those of the document's
    void walk(const QTextDocument& text, std::vector<ImportBlock>& blocks, bool inNote) {
        QTextFrame* root = text.rootFrame();
        walkFrame(*root, blocks, inNote);
    }

    std::vector<int> noteOfReference;  ///< The note of each reference of the text

private:
    void walkFrame(QTextFrame& frame, std::vector<ImportBlock>& blocks, bool inNote) {
        for (QTextFrame::iterator it = frame.begin(); !it.atEnd(); ++it) {
            if (QTextFrame* child = it.currentFrame()) {
                if (auto* table = qobject_cast<QTextTable*>(child)) {
                    blocks.push_back(tableOf(*table, inNote));
                    m_lastList = nullptr;
                } else {
                    walkFrame(*child, blocks, inNote);
                }
            } else if (const QTextBlock block = it.currentBlock();
                       block.isValid() && (block.length() > 1 || block.textList() != nullptr ||
                                           block.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))) {
                addBlock(block, blocks, inNote, false);  // an empty paragraph is Qt's, after a table
            }
        }
    }

    ImportBlock tableOf(QTextTable& table, bool inNote) {
        ImportBlock block;
        block.kind = ImportBlock::Kind::Table;
        auto result = std::make_shared<ImportTable>();
        for (int r = 0; r < table.rows(); ++r) {
            ImportRow row;
            row.header = r == 0;  // a table of GitHub always has its header row
            for (int c = 0; c < table.columns(); ++c) {
                const QTextTableCell cell = table.cellAt(r, c);
                if (cell.row() != r || cell.column() != c) {
                    continue;
                }
                ImportCell importCell;
                importCell.columnSpan = cell.columnSpan();
                importCell.rowSpan = cell.rowSpan();
                for (QTextFrame::iterator it = cell.begin(); !it.atEnd(); ++it) {
                    if (it.currentBlock().isValid()) {
                        addBlock(it.currentBlock(), importCell.blocks, inNote, r == 0);
                    }
                }
                row.cells.push_back(std::move(importCell));
            }
            result->rows.push_back(std::move(row));
        }
        block.table = std::move(result);
        return block;
    }

    void addBlock(const QTextBlock& block, std::vector<ImportBlock>& blocks, bool inNote, bool header) {
        const QTextBlockFormat format = block.blockFormat();
        if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            ImportBlock line;
            line.kind = ImportBlock::Kind::HorizontalLine;
            blocks.push_back(line);
            m_lastList = nullptr;
            return;
        }
        ImportBlock paragraph;
        paragraph.style = QStringLiteral("normal");
        if (const int level = format.headingLevel(); level > 0) {
            paragraph.headingLevel = level;
            paragraph.style = QStringLiteral("heading_%1").arg(std::min(level, KML_HEADING_LEVELS));
        } else if (format.hasProperty(QTextFormat::BlockCodeFence) || format.nonBreakableLines() ||
                   format.hasProperty(QTextFormat::BlockCodeLanguage)) {
            paragraph.style = QStringLiteral("code");
        } else if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            paragraph.style = QStringLiteral("quote");
        }
        if (const Qt::Alignment align = format.alignment(); format.hasProperty(QTextFormat::BlockAlignment)) {
            if (align & Qt::AlignHCenter) {
                paragraph.align = QStringLiteral("center");
            } else if (align & Qt::AlignRight) {
                paragraph.align = QStringLiteral("right");
            }
        }
        readRuns(block, paragraph, inNote, header);

        if (QTextList* list = block.textList(); list && !inNote) {
            addListItem(*list, block, format, paragraph);
        } else {
            m_lastList = nullptr;
        }

        // The marks of a scene break: the export's no-break space and a line of asterisks
        const QString text = importPlainText(paragraph);
        if (paragraph.list < 0 && paragraph.style == QLatin1String("normal") &&
            (text == QString(QChar(MARK_SCENE_BREAK)) || (!text.isEmpty() && text.trimmed().isEmpty() &&
                                                          text.contains(QChar(0x00A0))))) {
            ImportBlock scene;
            scene.kind = ImportBlock::Kind::SceneBreak;
            blocks.push_back(scene);
            return;
        }
        if (!inNote) {
            ++m_used[paragraph.style];
        }
        blocks.push_back(std::move(paragraph));
    }

    void addListItem(QTextList& list, const QTextBlock& block, const QTextBlockFormat& format, ImportBlock& paragraph) {
        const QTextListFormat listFormat = list.format();
        const int level = std::clamp(listFormat.indent() - 1, 0, LIST_LEVELS - 1);
        // The lists of Qt one after another are one list of the document, each of a level of
        // it; another list of a level that has one starts a new list
        const auto owner = m_levelOwners.find(level);
        if (!m_lastList || (owner != m_levelOwners.end() && owner->second != &list)) {
            m_document.lists.push_back(ListDefinition::numbered());
            m_currentList = static_cast<int>(m_document.lists.size()) - 1;
            m_levelLists.clear();
            m_levelOwners.clear();
        }
        m_levelOwners.erase(m_levelOwners.upper_bound(level), m_levelOwners.end());
        m_levelOwners[level] = &list;
        if (!m_levelLists.contains(&list)) {
            m_levelLists.insert(&list);
            ListLevel& levelFormat = m_document.lists[static_cast<size_t>(m_currentList)].levels[static_cast<size_t>(level)];
            switch (listFormat.style()) {
                case QTextListFormat::ListDisc:
                    levelFormat.format = ListNumberFormat::Bullet;
                    levelFormat.bullet = QString(QChar(BULLET_DISC));
                    break;
                case QTextListFormat::ListCircle:
                    levelFormat.format = ListNumberFormat::Bullet;
                    levelFormat.bullet = QString(QChar(BULLET_CIRCLE));
                    break;
                case QTextListFormat::ListSquare:
                    levelFormat.format = ListNumberFormat::Bullet;
                    levelFormat.bullet = QString(QChar(BULLET_SQUARE));
                    break;
                case QTextListFormat::ListLowerAlpha:
                    levelFormat.format = ListNumberFormat::LowerAlpha;
                    break;
                case QTextListFormat::ListUpperAlpha:
                    levelFormat.format = ListNumberFormat::UpperAlpha;
                    break;
                case QTextListFormat::ListLowerRoman:
                    levelFormat.format = ListNumberFormat::LowerRoman;
                    break;
                case QTextListFormat::ListUpperRoman:
                    levelFormat.format = ListNumberFormat::UpperRoman;
                    break;
                default:
                    levelFormat.format = ListNumberFormat::Decimal;
                    break;
            }
            if (levelFormat.format != ListNumberFormat::Bullet) {
                levelFormat.bullet.clear();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
                levelFormat.start = std::max(0, listFormat.start());
#endif
            }
        }
        m_lastList = &list;
        paragraph.list = m_currentList;
        paragraph.listLevel = level;
        // A task: its box before its text
        if (format.marker() == QTextBlockFormat::MarkerType::Checked ||
            format.marker() == QTextBlockFormat::MarkerType::Unchecked) {
            ImportRun box;
            box.text = format.marker() == QTextBlockFormat::MarkerType::Checked ? QStringLiteral("☑ ")
                                                                                : QStringLiteral("☐ ");
            paragraph.runs.insert(paragraph.runs.begin(), box);
        }
        Q_UNUSED(block);
    }

    void readRuns(const QTextBlock& block, ImportBlock& paragraph, bool inNote, bool header) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            ImportCharFormat look;
            look.bold = format.fontWeight() >= QFont::Bold && !header && paragraph.headingLevel == 0;
            // Qt reads _text_ as underline: emphasis, as Markdown has it (the marks are underline)
            look.italic = format.fontItalic() || format.fontUnderline();
            look.strike = format.fontStrikeOut();
            look.superscript = format.verticalAlignment() == QTextCharFormat::AlignSuperScript;
            look.subscript = format.verticalAlignment() == QTextCharFormat::AlignSubScript;
            const QString link = format.isAnchor() ? format.anchorHref() : QString();

            if (format.isImageFormat()) {
                if (!inNote) {
                    addImage(format.toImageFormat(), paragraph, link);
                }
                continue;
            }
            const QString text = fragment.text();
            QString characters;
            const auto flush = [&]() {
                if (!characters.isEmpty()) {
                    ImportRun run;
                    run.text = characters;
                    run.format = look;
                    run.format.underline = m_underline;
                    run.link = link;
                    paragraph.runs.push_back(std::move(run));
                    characters.clear();
                }
            };
            for (qsizetype i = 0; i < text.size(); ++i) {
                const QChar ch = text.at(i);
                switch (ch.unicode()) {
                    case MARK_UNDERLINE_START:
                    case MARK_UNDERLINE_END:
                        flush();
                        m_underline = ch.unicode() == MARK_UNDERLINE_START;
                        break;
                    case MARK_LINE_BREAK:
                    case QChar::LineSeparator: {
                        flush();
                        ImportRun run;
                        run.kind = ImportRun::Kind::LineBreak;
                        paragraph.runs.push_back(run);
                        // The space Qt makes of the line's end after the mark
                        if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char(' ')) {
                            ++i;
                        }
                        break;
                    }
                    case MARK_NOTE_START: {
                        flush();
                        const qsizetype end = text.indexOf(QChar(MARK_NOTE_END), i);
                        const int reference = end > i ? text.mid(i + 1, end - i - 1).toInt() : -1;
                        if (reference >= 0 && static_cast<size_t>(reference) < noteOfReference.size() && !inNote) {
                            ImportRun run;
                            run.kind = ImportRun::Kind::Note;
                            run.index = noteOfReference[static_cast<size_t>(reference)];
                            run.format = look;
                            paragraph.runs.push_back(run);
                        }
                        i = end > i ? end : i;
                        break;
                    }
                    default:
                        if (ch == QLatin1Char('\t')) {
                            flush();
                            ImportRun run;
                            run.kind = ImportRun::Kind::Tab;
                            paragraph.runs.push_back(run);
                        } else if (ch.category() != QChar::Other_Control) {
                            characters += ch;
                        }
                        break;
                }
            }
            flush();
        }
    }

    void addImage(const QTextImageFormat& format, ImportBlock& paragraph, const QString& link) {
        ImportImage image;
        const QString address = QUrl::fromPercentEncoding(format.name().toUtf8());
        image.address = address;
        image.look.alt = format.stringProperty(QTextFormat::ImageAltText).trimmed();
        const QUrl url(format.name());
        if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
            image.web = true;
            image.fileName = QFileInfo(url.path()).fileName();
        } else {
            const QString path = url.isLocalFile() ? url.toLocalFile() : address;
            const QString file = QFileInfo(path).isAbsolute() || m_folder.isEmpty() ? path : QDir(m_folder).filePath(path);
            image.fileName = QFileInfo(file).fileName();
            QFile reader(file);
            if (!m_folder.isEmpty() && isImageFileName(image.fileName) && reader.open(QIODevice::ReadOnly)) {
                image.data = reader.readAll();
            }
            image.missing = image.data.isEmpty();
        }
        const int index = static_cast<int>(m_document.images.size());
        m_document.images.push_back(std::move(image));
        ImportRun run;
        run.kind = ImportRun::Kind::Image;
        run.index = index;
        run.link = link;
        paragraph.runs.push_back(run);
    }

public:
    QHash<QString, int> m_used;  ///< The paragraphs of each style

private:
    ImportDocument& m_document;
    QString m_folder;
    bool m_underline = false;
    const QTextList* m_lastList = nullptr;
    int m_currentList = -1;
    QSet<const QTextList*> m_levelLists;
    std::map<int, const QTextList*> m_levelOwners;  ///< The list of Qt of each level
};

}  // namespace

ImportDocument readMarkdown(const QString& text, const QString& folder) {
    ImportDocument document;
    const MarkedMarkdown markdown = marked(text);
    document.title = markdown.title;
    document.author = markdown.author;
    document.language = markdown.language;
    if (markdown.html) {
        ++document.omissions[ImportOmission::Html];
    }

    MarkdownWalker walker(document, folder);
    // The notes first: their texts, each in a paragraph or more
    for (const QString& note : markdown.notes) {
        QTextDocument noteText;
        noteText.setMarkdown(marked(note).text, QTextDocument::MarkdownDialectGitHub);
        ImportNote importNote;
        walker.walk(noteText, importNote.paragraphs, true);
        walker.noteOfReference.push_back(static_cast<int>(document.notes.size()));
        document.notes.push_back(std::move(importNote));
    }

    QTextDocument textDocument;
    textDocument.setMarkdown(markdown.text, QTextDocument::MarkdownDialectGitHub);
    walker.walk(textDocument, document.blocks, false);

    for (const QString& key : {QStringLiteral("normal"), QStringLiteral("heading_1"), QStringLiteral("heading_2"),
                              QStringLiteral("heading_3"), QStringLiteral("quote"), QStringLiteral("code")}) {
        if (const int count = walker.m_used.value(key); count > 0) {
            ImportStyle style;
            style.key = key;
            style.name = key;
            style.headingLevel = key.startsWith(QLatin1String("heading_")) ? key.mid(8).toInt() : 0;
            style.paragraphs = count;
            document.styles.push_back(std::move(style));
        }
    }
    return document;
}

}  // namespace kalahari::editor
