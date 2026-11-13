/// @file markdown_writer.cpp
/// @brief Export of a book to Markdown (GitHub Flavored Markdown)
///
/// The file reads as plain text too: headings with #, lists with their numbers, tables of
/// pipes, the notes as footnotes at its end. What Markdown has no way to write (the fonts and
/// sizes, the paragraphs' own spacing, the page breaks, the annotations) stays only in the
/// book; underlined, raised and lowered text is HTML (<u>, <sup>, <sub>), as GitHub shows it.

#include "book_export_internal.h"

#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_tables.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextTable>

#include <cmath>

namespace kalahari::editor {

namespace {

using namespace exporting;

/// The characters a backslash keeps from being Markdown anywhere in a text
const QString SPECIAL_CHARACTERS = QStringLiteral("\\`*_[]<>~|");

/// A text with the characters of Markdown escaped
QString escaped(const QString& text) {
    QString result;
    result.reserve(text.size() + text.size() / 8);
    for (const QChar ch : text) {
        if (SPECIAL_CHARACTERS.contains(ch)) {
            result += QLatin1Char('\\');
        }
        result += ch;
    }
    return result;
}

/// The lines of a block with what would start Markdown at their starts escaped (a heading, a
/// list, a quote, a thematic break, a code block of four spaces)
QString escapedLineStarts(const QString& text) {
    static const QRegularExpression orderedItem(QStringLiteral("^(\\d{1,9})([.)])"));
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        qsizetype spaces = 0;
        while (spaces < line.size() && (line.at(spaces) == u' ' || line.at(spaces) == u'\t')) {
            ++spaces;
        }
        line.remove(0, spaces);  // four spaces would make it code
        if (line.isEmpty()) {
            continue;
        }
        const QChar first = line.at(0);
        if (first == u'#' || first == u'+' || first == u'-' || first == u'=') {
            line.prepend(QLatin1Char('\\'));
        } else if (const QRegularExpressionMatch match = orderedItem.match(line); match.hasMatch()) {
            line.insert(match.capturedLength(1), QLatin1Char('\\'));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

/// An address as a destination of a link: in angle brackets when it has spaces or brackets
QString destination(const QString& address) {
    QString result = address;
    result.replace(QLatin1Char('<'), QLatin1String("%3C")).replace(QLatin1Char('>'), QLatin1String("%3E"));
    if (result.contains(QLatin1Char(' ')) || result.contains(QLatin1Char('(')) ||
        result.contains(QLatin1Char(')'))) {
        return QLatin1Char('<') + result + QLatin1Char('>');
    }
    return result;
}

/// The anchor GitHub gives a heading: its letters, digits, hyphens and underscores in lower
/// case, its spaces hyphens
QString headingSlug(const QString& text) {
    QString slug;
    for (const QChar ch : text.trimmed().toLower()) {
        if (ch.isLetterOrNumber() || ch.isMark() || ch == u'-' || ch == u'_') {
            slug += ch;
        } else if (ch == u' ') {
            slug += QLatin1Char('-');
        }
    }
    return slug;
}

/// The formatting of a piece of text that Markdown writes
struct Marks {
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;

    explicit Marks(const QTextCharFormat& format)
        : bold(format.fontWeight() > QFont::Normal), italic(format.fontItalic()),
          strike(format.fontStrikeOut()), underline(format.fontUnderline()),
          superscript(format.verticalAlignment() == QTextCharFormat::AlignSuperScript),
          subscript(format.verticalAlignment() == QTextCharFormat::AlignSubScript) {}

    QString open() const {
        QString result;
        if (bold) result += QStringLiteral("**");
        if (italic) result += QStringLiteral("*");
        if (strike) result += QStringLiteral("~~");
        if (underline) result += QStringLiteral("<u>");
        if (superscript) result += QStringLiteral("<sup>");
        if (subscript) result += QStringLiteral("<sub>");
        return result;
    }

    QString close() const {
        QString result;
        if (subscript) result += QStringLiteral("</sub>");
        if (superscript) result += QStringLiteral("</sup>");
        if (underline) result += QStringLiteral("</u>");
        if (strike) result += QStringLiteral("~~");
        if (italic) result += QStringLiteral("*");
        if (bold) result += QStringLiteral("**");
        return result;
    }
};

/// Text in its marks, the spaces at its ends outside them (Markdown does not take "** a**")
QString marked(const QString& text, const Marks& marks) {
    qsizetype start = 0;
    while (start < text.size() && text.at(start).isSpace()) {
        ++start;
    }
    qsizetype end = text.size();
    while (end > start && text.at(end - 1).isSpace()) {
        --end;
    }
    if (start == end) {
        return text;
    }
    return text.left(start) + marks.open() + text.mid(start, end - start) + marks.close() + text.mid(end);
}

/// How a block of the file is written: what separates it from the block before
enum class BlockKind { Paragraph, ListItem, Quote, Code, Table, Other };

/// A heading of the file with its anchor
struct Heading {
    int level = 1;
    QString text;
    QString slug;
    bool contents = false;  ///< The title of the table of contents (not in it)
};

/// The writer of the file
class MarkdownWriter {
public:
    MarkdownWriter(const BookExport& book, const QString& path, const QString& imagesFolderName,
                   const ExportProgress& progress)
        : m_book(book), m_progress(progress), m_imagesFolderName(imagesFolderName) {
        const QFileInfo file(path);
        m_folder = file.absolutePath();
        m_images.setFolder(book.imagesFolder);
        m_textWidth = book.page.textWidthPoints();
    }

    /// @brief The file's text, the images written to their folder
    bool build(QString* markdown, ExportResult* result, ExportProblem* problem);

private:
    void prepare();
    void writeItem(size_t index);
    void writeContents();
    void writeBlocks(const ExportText& text, size_t index);
    void writeTable(const ExportText& text, size_t index, const QTextTable& table);
    QString inlineOf(const ExportText& text, size_t index, const QTextBlock& block, bool code);
    QString imageOf(const ImageProperties& image);
    QString noteLabel(size_t index, const NotePlace& place) const;
    QString noteContent(const Footnote& note) const;
    /// @param newList An item that starts another list, though of the same list id (Markdown
    ///        numbers a list on from its first number only)
    void addBlock(BlockKind kind, const QString& text, int listId = 0, bool newList = false);
    void closeCode();

    const BookExport& m_book;
    ExportProgress m_progress;
    QString m_imagesFolderName;
    QString m_folder;  ///< Of the file
    ImageStore m_images;
    double m_textWidth = 0.0;

    std::vector<std::unique_ptr<ExportText>> m_texts;
    std::vector<bool> m_withTitle;
    std::vector<Heading> m_headings;
    QHash<QPair<int, int>, int> m_headingAt;  ///< (item, block; -1 its title): its heading

    QString m_out;
    BlockKind m_last = BlockKind::Other;
    bool m_empty = true;
    bool m_inCode = false;
    int m_lastListId = 0;
    int m_lastTopNumber = 0;  ///< The number of the last item of a list at its first level

    struct ListLevelState {
        int level = 0;
        int indent = 0;  ///< Where its marker starts
        int width = 0;   ///< The width of its marker with the space after it
    };
    std::vector<ListLevelState> m_listStack;

    /// The labels of the notes of the file, by item and note
    QHash<QPair<int, int>, QString> m_noteLabels;
    QStringList m_definitions;  ///< The texts of the notes, in the order of the file

    bool m_imagesFolderMade = false;
    bool m_failed = false;
    ExportProblem m_problem;
    QHash<QString, QString> m_imageFiles;  ///< The file written for each picture (its key)
    QSet<QString> m_takenNames;            ///< The names of the image files, in lower case
    int m_imagesWritten = 0;
    int m_missingImages = 0;
};

void MarkdownWriter::prepare() {
    QHash<QString, int> slugCounts;
    const auto addHeading = [&](int item, int block, int level, const QString& text, bool contents) {
        QString slug = headingSlug(text);
        const int seen = slugCounts.value(slug, 0);
        slugCounts.insert(slug, seen + 1);
        if (seen > 0) {
            slug += QStringLiteral("-%1").arg(seen);
        }
        m_headingAt.insert(qMakePair(item, block), static_cast<int>(m_headings.size()));
        m_headings.push_back({level, text, slug, contents});
    };

    // The labels of the notes: their numbers when every one is unique and plain, else with the
    // place of their text, else counted through the file
    std::vector<std::tuple<int, int, QString>> notes;  // item, note, number
    for (size_t index = 0; index < m_book.items.size(); ++index) {
        const ExportItem& item = m_book.items[index];
        m_texts.push_back(std::make_unique<ExportText>(item));
        const ExportText& text = *m_texts.back();
        const bool titled = m_book.titleHeadings && !item.title.trimmed().isEmpty() &&
                            (item.kind == ExportItemKind::Contents ||
                             !(item.kind == ExportItemKind::Text &&
                               startsWithHeading(text.document(), m_book.styles)));
        m_withTitle.push_back(titled || (item.kind == ExportItemKind::Contents && !item.title.trimmed().isEmpty()));
        if (m_withTitle.back()) {
            const int level = item.kind == ExportItemKind::Contents
                                  ? 1
                                  : std::max(1, headingLevelOfStyle(m_book.styles, titleStyleOf(item, m_book.styles)));
            addHeading(static_cast<int>(index), -1, level, item.title.simplified(),
                       item.kind == ExportItemKind::Contents);
        }
        for (QTextBlock block = text.document().begin(); block.isValid(); block = block.next()) {
            if (tableOf(block) != nullptr || blockObject(block) != TextObject::None ||
                listItemOf(block.blockFormat())) {
                continue;
            }
            const int level = headingLevelOfStyle(m_book.styles, shownStyleOf(m_book.styles, block));
            const QString heading = level > 0 ? plainTextOf(block, text) : QString();
            if (!heading.isEmpty()) {
                addHeading(static_cast<int>(index), block.blockNumber(), level, heading, false);
            }
        }
        for (size_t note = 0; note < text.notes().notes().size(); ++note) {
            notes.emplace_back(static_cast<int>(index), static_cast<int>(note), text.notes().number(note));
        }
    }
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9-]+$"));
    const auto unique = [&](const std::function<QString(int, int, const QString&)>& labelOf) {
        QSet<QString> labels;
        for (const auto& [item, note, shown] : notes) {
            const QString label = labelOf(item, note, shown);
            if (!plain.match(label).hasMatch() || labels.contains(label)) {
                return false;
            }
            labels.insert(label);
        }
        return true;
    };
    std::function<QString(int, int, const QString&)> labelOf = [](int, int, const QString& shown) { return shown; };
    if (!unique(labelOf)) {
        labelOf = [](int item, int, const QString& shown) { return QStringLiteral("%1-%2").arg(item + 1).arg(shown); };
        if (!unique(labelOf)) {
            labelOf = nullptr;
        }
    }
    int counted = 0;
    for (const auto& [item, note, shown] : notes) {
        m_noteLabels.insert(qMakePair(item, note), labelOf ? labelOf(item, note, shown) : QString::number(++counted));
    }
}

void MarkdownWriter::closeCode() {
    if (m_inCode) {
        m_out += QStringLiteral("\n```");
        m_inCode = false;
    }
}

void MarkdownWriter::addBlock(BlockKind kind, const QString& text, int listId, bool newList) {
    if (kind != BlockKind::ListItem) {
        m_listStack.clear();
    }
    if (kind != BlockKind::Code) {
        closeCode();
    }
    if (!m_empty) {
        if (kind == BlockKind::Code && m_inCode) {
            m_out += QLatin1Char('\n');
        } else if (kind == BlockKind::ListItem && m_last == BlockKind::ListItem && listId == m_lastListId &&
                   !newList) {
            m_out += QLatin1Char('\n');
        } else if (kind == BlockKind::ListItem && m_last == BlockKind::ListItem) {
            m_out += QStringLiteral("\n\n<!-- -->\n\n");  // another list: not one with the one before
        } else if (kind == BlockKind::Quote && m_last == BlockKind::Quote) {
            m_out += QStringLiteral("\n>\n");
        } else {
            m_out += QStringLiteral("\n\n");
        }
    }
    if (kind == BlockKind::Code && !m_inCode) {
        m_out += QStringLiteral("```\n");
        m_inCode = true;
    }
    m_out += text;
    m_empty = false;
    m_last = kind;
    m_lastListId = listId;
}

QString MarkdownWriter::noteContent(const Footnote& note) const {
    QStringList paragraphs;
    for (const NoteParagraph& paragraph : noteParagraphs(note.content)) {
        std::vector<FormatRun> runs = paragraph.formats;
        std::sort(runs.begin(), runs.end(), [](const FormatRun& a, const FormatRun& b) { return a.start < b.start; });
        QString text;
        size_t at = 0;
        const size_t length = static_cast<size_t>(paragraph.text.size());
        const auto write = [&](size_t from, size_t to, const QTextCharFormat& format) {
            if (to <= from) {
                return;
            }
            QString part = escaped(paragraph.text.mid(static_cast<qsizetype>(from), static_cast<qsizetype>(to - from)));
            part.replace(QChar::LineSeparator, QStringLiteral("\\\n"));
            text += marked(part, Marks(format));
        };
        for (const FormatRun& run : runs) {
            const size_t start = std::min(run.start, length);
            const size_t end = std::min(run.end, length);
            if (start < at) {
                continue;
            }
            write(at, start, QTextCharFormat());
            write(start, end, run.format);
            at = end;
        }
        write(at, length, QTextCharFormat());
        paragraphs << escapedLineStarts(text);
    }
    // The paragraphs after the first indented, as Markdown takes them into the note
    for (qsizetype i = 1; i < paragraphs.size(); ++i) {
        paragraphs[i] = QStringLiteral("    ") + QString(paragraphs[i]).replace(QLatin1Char('\n'), QStringLiteral("\n    "));
    }
    return paragraphs.join(QStringLiteral("\n\n"));
}

QString MarkdownWriter::imageOf(const ImageProperties& image) {
    if (m_book.imagesFolder.isEmpty()) {
        ++m_missingImages;
        return QString();
    }
    const QString source = m_images.filePath(image.source);
    if (source.isEmpty() || !m_images.shownPixelSize(normalizedImage(image))) {
        ++m_missingImages;
        return QString();
    }
    const QString alt = escaped(image.alt.isEmpty() ? image.name : image.alt);
    if (!m_book.copyImages) {
        // The file of the book, from the folder of the Markdown file
        QString path = QDir(m_folder).relativeFilePath(source);
        ++m_imagesWritten;
        return QStringLiteral("![") + alt + QStringLiteral("](") + destination(QDir::fromNativeSeparators(path)) +
               QLatin1Char(')');
    }

    // A copy in the folder of the images: the file itself when it is shown as it is, else the
    // picture as the editor shows it
    const bool plain = image.hasPlainPicture() && !image.hasFrame();
    const QString key = plain ? source : source + QLatin1Char('#') + pictureLookKey(image) +
                                             QStringLiteral("#%1").arg(static_cast<int>(image.borderLine)) +
                                             (image.shadow ? QStringLiteral("s") : QString());
    QString name = m_imageFiles.value(key);
    if (name.isEmpty()) {
        QByteArray data;
        QString wanted;
        if (plain) {
            QFile file(source);
            if (file.open(QIODevice::ReadOnly)) {
                data = file.readAll();
            }
            wanted = QFileInfo(source).fileName();
        } else {
            const ExportPicture picture = exportPicture(m_images, image, m_textWidth, 0.0);
            data = picture.data;
            wanted = QFileInfo(source).completeBaseName() + QLatin1Char('.') +
                     (picture.extension == QLatin1String("jpeg") ? QStringLiteral("jpg") : picture.extension);
        }
        if (data.isEmpty()) {
            ++m_missingImages;
            return QString();
        }
        const QString folder = m_folder + QLatin1Char('/') + m_imagesFolderName;
        if (!m_imagesFolderMade) {
            if (!QDir().mkpath(folder)) {
                m_failed = true;
                m_problem.kind = ExportProblem::Kind::CannotWrite;
                m_problem.lines << QStringLiteral("%1: the folder cannot be created").arg(folder);
                return QString();
            }
            m_imagesFolderMade = true;
        }
        name = wanted;
        const QFileInfo parts(wanted);
        for (int n = 2; m_takenNames.contains(name.toLower()); ++n) {
            name = parts.completeBaseName() + QStringLiteral("-%1.").arg(n) + parts.suffix();
        }
        if (!writeExportFile(folder + QLatin1Char('/') + name, data, &m_problem)) {
            m_failed = true;
            return QString();
        }
        m_takenNames.insert(name.toLower());
        m_imageFiles.insert(key, name);
    }
    ++m_imagesWritten;
    return QStringLiteral("![") + alt + QStringLiteral("](") + destination(m_imagesFolderName + QLatin1Char('/') + name) +
           QLatin1Char(')');
}

QString MarkdownWriter::inlineOf(const ExportText& text, size_t index, const QTextBlock& block, bool code) {
    QString result;
    QString linkText;  // the text of the link open
    QString link;
    const auto closeLink = [&]() {
        if (!link.isEmpty()) {
            result += QLatin1Char('[') + linkText + QStringLiteral("](") + destination(link) + QLatin1Char(')');
            link.clear();
            linkText.clear();
        }
    };
    const std::vector<NotePlace>& notes = text.notes().notes();
    for (const Piece& piece : piecesOf(block, text)) {
        if (code) {
            if (piece.kind == Piece::Kind::Text) {
                result += QString(piece.text).replace(QChar::LineSeparator, QLatin1Char('\n'));
            } else if (piece.kind == Piece::Kind::Field) {
                result += piece.text;
            }
            continue;
        }
        if (piece.link != link) {
            closeLink();
            link = piece.link;
        }
        QString part;
        switch (piece.kind) {
            case Piece::Kind::Text: {
                QString characters = escaped(piece.text);
                characters.replace(QChar::LineSeparator, QStringLiteral("\\\n"));
                characters.replace(QLatin1Char('\t'), QLatin1Char(' '));
                part = marked(characters, Marks(piece.format));
                break;
            }
            case Piece::Kind::Field:
                part = marked(escaped(piece.text), Marks(piece.format));
                break;
            case Piece::Kind::Image:
                part = imageOf(piece.image);
                break;
            case Piece::Kind::Note: {
                for (size_t i = 0; i < notes.size(); ++i) {
                    if (notes[i].position == piece.position) {
                        const QString label = m_noteLabels.value(qMakePair(static_cast<int>(index), static_cast<int>(i)));
                        part = QStringLiteral("[^") + label + QLatin1Char(']');
                        m_definitions << QStringLiteral("[^") + label + QStringLiteral("]: ") + noteContent(notes[i].note);
                        break;
                    }
                }
                break;
            }
        }
        if (link.isEmpty()) {
            result += part;
        } else {
            linkText += part;
        }
    }
    closeLink();
    return result;
}

void MarkdownWriter::writeTable(const ExportText& text, size_t index, const QTextTable& table) {
    const TableShape shape = tableShapeOf(table);
    const int columns = std::max(1, shape.columns);
    const QTextDocument& document = text.document();
    const auto rowOf = [&](int row) {
        QStringList cells;
        for (int column = 0; column < columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (!cell.isValid() || cell.row() != row || cell.column() != column) {
                cells << QString();  // covered by a merged cell: its text is in its first column
                continue;
            }
            QStringList paragraphs;
            for (QTextBlock block = document.findBlock(cell.firstPosition());
                 block.isValid() && block.position() <= cell.lastPosition(); block = block.next()) {
                if (blockObject(block) != TextObject::None) {
                    continue;
                }
                QString paragraph = inlineOf(text, index, block, false);
                paragraph.replace(QStringLiteral("\\\n"), QStringLiteral("<br>"));
                paragraph.replace(QLatin1Char('\n'), QLatin1Char(' '));
                if (const std::optional<ListItem> item = listItemOf(block.blockFormat()); item && item->numbered) {
                    const int number = text.listNumber(block);
                    paragraph.prepend(item->levelFormat().isBullet() ? QStringLiteral("• ")
                                                                     : QStringLiteral("%1. ").arg(std::max(0, number)));
                }
                paragraphs << paragraph.trimmed();
            }
            while (!paragraphs.isEmpty() && paragraphs.constLast().isEmpty()) {
                paragraphs.removeLast();
            }
            cells << paragraphs.join(QStringLiteral("<br>"));
        }
        return QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    };
    QStringList lines;
    int first = 0;
    if (shape.headerRows > 0) {
        lines << rowOf(0);
        first = 1;
    } else {
        QStringList empty;
        for (int column = 0; column < columns; ++column) {
            empty << QString();
        }
        lines << QStringLiteral("| ") + empty.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    }
    QStringList rule;
    for (int column = 0; column < columns; ++column) {
        rule << QStringLiteral("---");
    }
    lines << QStringLiteral("| ") + rule.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    for (int row = first; row < shape.rows; ++row) {
        lines << rowOf(row);
    }
    addBlock(BlockKind::Table, lines.join(QLatin1Char('\n')));
}

void MarkdownWriter::writeBlocks(const ExportText& text, size_t index) {
    const QTextDocument& document = text.document();
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (const QTextTable* table = tableOf(block)) {
            writeTable(text, index, *table);
            block = document.findBlock(table->lastPosition());
            continue;
        }
        switch (blockObject(block)) {
            case TextObject::PageBreak:
                continue;  // Markdown has no pages
            case TextObject::HorizontalRule:
                addBlock(BlockKind::Other, QStringLiteral("---"));
                continue;
            case TextObject::SceneBreak: {
                const QString symbol = sceneBreakText(m_book.sceneBreak);
                if (m_book.sceneBreak == SceneBreakLook::Line) {
                    addBlock(BlockKind::Other, QStringLiteral("---"));
                } else {
                    addBlock(BlockKind::Paragraph, symbol.isEmpty() ? QStringLiteral("&nbsp;") : escaped(symbol));
                }
                continue;
            }
            case TextObject::FigureList: {
                QStringList rows;
                for (const FigureListRow& row : text.captions().figureList()) {
                    rows << QStringLiteral("- ") + escaped(row.text);
                }
                if (!rows.isEmpty()) {
                    addBlock(BlockKind::Other, rows.join(QLatin1Char('\n')));
                }
                continue;
            }
            case TextObject::None:
                break;
        }

        const QString style = shownStyleOf(m_book.styles, block);
        const std::optional<ListItem> item = listItemOf(block.blockFormat());
        if (item) {
            // An item of a list: its number or a hyphen, its level by its indent
            while (!m_listStack.empty() && m_listStack.back().level >= item->level &&
                   !(m_listStack.back().level == item->level && !item->numbered)) {
                m_listStack.pop_back();
            }
            if (!item->numbered) {
                // Without a number: a paragraph of the item before, at its text
                const int indent = m_listStack.empty() ? 0 : m_listStack.back().indent + m_listStack.back().width;
                QString paragraph = escapedLineStarts(inlineOf(text, index, block, false));
                paragraph.replace(QLatin1Char('\n'), QLatin1Char('\n') + QString(indent, QLatin1Char(' ')));
                if (m_last == BlockKind::ListItem) {
                    m_out += QStringLiteral("\n\n") + QString(indent, QLatin1Char(' ')) + paragraph;
                } else {
                    addBlock(BlockKind::Paragraph, paragraph);
                }
                continue;
            }
            const int number = text.listNumber(block);
            const QString marker = item->levelFormat().isBullet() ? QStringLiteral("- ")
                                                                  : QStringLiteral("%1. ").arg(std::max(0, number));
            const int indent = m_listStack.empty() ? 0 : m_listStack.back().indent + m_listStack.back().width;
            const int listId = item->id;
            QString paragraph = escapedLineStarts(inlineOf(text, index, block, false));
            paragraph.replace(QLatin1Char('\n'), QLatin1Char('\n') + QString(indent + static_cast<int>(marker.size()), QLatin1Char(' ')));
            // An item of the first level numbered other than on from the one before starts a list
            // of its own number
            const bool numbered = !item->levelFormat().isBullet();
            const bool newList = m_listStack.empty() && numbered && m_last == BlockKind::ListItem &&
                                 listId == m_lastListId && number != m_lastTopNumber + 1;
            if (m_listStack.empty() && numbered) {
                m_lastTopNumber = number;
            }
            const std::vector<ListLevelState> stack = m_listStack;
            addBlock(BlockKind::ListItem, QString(indent, QLatin1Char(' ')) + marker + paragraph, listId, newList);
            m_listStack = stack;
            m_listStack.push_back({item->level, indent, static_cast<int>(marker.size())});
            continue;
        }

        const int level = headingLevelOfStyle(m_book.styles, style);
        if (level > 0) {
            QString heading = inlineOf(text, index, block, false);
            heading.replace(QStringLiteral("\\\n"), QStringLiteral(" "));
            if (!heading.trimmed().isEmpty()) {
                addBlock(BlockKind::Paragraph, QString(level, QLatin1Char('#')) + QLatin1Char(' ') + heading.trimmed());
            }
            continue;
        }
        if (isStyleBasedOn(m_book.styles, style, QStringLiteral("code"))) {
            addBlock(BlockKind::Code, inlineOf(text, index, block, true));
            continue;
        }
        QString paragraph = escapedLineStarts(inlineOf(text, index, block, false));
        if (paragraph.trimmed().isEmpty()) {
            continue;  // an empty paragraph: nothing in Markdown
        }
        if (isStyleBasedOn(m_book.styles, style, QStringLiteral("quote")) ||
            isStyleBasedOn(m_book.styles, style, QStringLiteral("block_quote"))) {
            addBlock(BlockKind::Quote, QStringLiteral("> ") + paragraph.replace(QLatin1Char('\n'), QStringLiteral("\n> ")));
            continue;
        }
        addBlock(BlockKind::Paragraph, paragraph);
    }
}

void MarkdownWriter::writeContents() {
    QStringList lines;
    for (const Heading& heading : m_headings) {
        if (heading.contents) {
            continue;
        }
        lines << QString((std::clamp(heading.level, 1, HEADING_LEVELS) - 1) * 2, QLatin1Char(' ')) +
                     QStringLiteral("- [") + escaped(heading.text) + QStringLiteral("](#") + heading.slug +
                     QLatin1Char(')');
    }
    if (!lines.isEmpty()) {
        addBlock(BlockKind::Other, lines.join(QLatin1Char('\n')));
    }
}

void MarkdownWriter::writeItem(size_t index) {
    const ExportItem& item = m_book.items[index];
    if (m_withTitle[index]) {
        const int heading = m_headingAt.value(qMakePair(static_cast<int>(index), -1), -1);
        const int level = heading >= 0 ? m_headings[static_cast<size_t>(heading)].level : 1;
        addBlock(BlockKind::Paragraph, QString(level, QLatin1Char('#')) + QLatin1Char(' ') + escaped(item.title.simplified()));
    }
    if (item.kind == ExportItemKind::Contents) {
        writeContents();
    } else if (item.kind == ExportItemKind::Text) {
        writeBlocks(*m_texts[index], index);
    }
}

bool MarkdownWriter::build(QString* markdown, ExportResult* result, ExportProblem* problem) {
    prepare();
    const size_t count = m_book.items.size();
    for (size_t index = 0; index < count && !m_failed; ++index) {
        writeItem(index);
        if (m_progress) {
            m_progress(static_cast<int>(95 * (index + 1) / std::max<size_t>(1, count)));
        }
    }
    closeCode();
    if (m_failed) {
        *problem = m_problem;
        return false;
    }
    if (!m_definitions.isEmpty()) {
        m_out += QStringLiteral("\n\n") + m_definitions.join(QStringLiteral("\n\n"));
    }
    m_out += QLatin1Char('\n');
    *markdown = m_out;
    if (result) {
        result->images = m_imagesWritten;
        result->missingImages = m_missingImages;
        result->imagesFolder = m_imagesFolderMade ? m_folder + QLatin1Char('/') + m_imagesFolderName : QString();
    }
    return true;
}

}  // namespace

bool exportMarkdown(const BookExport& book, const QString& path, const QString& imagesFolderName,
                    const ExportProgress& progress, ExportResult* result, ExportProblem* problem) {
    ExportProblem ignored;
    ExportProblem* reported = problem ? problem : &ignored;
    *reported = ExportProblem();
    ExportResult written;
    MarkdownWriter writer(book, path, imagesFolderName, progress);
    QString markdown;
    if (!writer.build(&markdown, &written, reported)) {
        return false;
    }
    if (!writeExportFile(path, markdown.toUtf8(), reported)) {
        return false;
    }
    written.file = path;
    if (result) {
        *result = written;
    }
    if (progress) {
        progress(100);
    }
    return true;
}

}  // namespace kalahari::editor
