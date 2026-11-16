// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_plan.cpp
/// @brief What an import will add to a book: the files read, their texts and parts with the
///        kinds and titles they get, and the new styles of the book made of the files' styles

#include <kalahari/gui/book_import_plan.h>

#include <kalahari/core/project_manager.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/gui/panels/navigator_panel.h>

#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                     core::BookPlace::Back};

/// The id of the kind of a title page in the packages of the program
const QString TITLE_PAGE_KIND = QStringLiteral("title_page");

/// The most bytes of a file its preview reads
constexpr qint64 PREVIEW_BYTES = 64 * 1024;
/// The most characters of a line of the preview
constexpr int PREVIEW_LINE = 160;

/// The kinds of @p kinds offered in the parts of a book, as the import offers them
ImportKinds kindsFrom(const std::function<QList<core::KindRef>(core::BookPlace)>& kindsIn,
                      const std::function<int(const core::KindRef&)>& roomOf) {
    ImportKinds kinds;
    QSet<QString> seen;
    const QString language = programLanguage();
    for (const core::BookPlace place : BOOK_PLACES) {
        for (const core::KindRef& kind : kindsIn(place)) {
            if (!kind || seen.contains(kind.reference())) {
                continue;
            }
            seen.insert(kind.reference());
            if (kind.kind->form == core::ElementForm::Group) {
                if (!kinds.part && place == core::BookPlace::Main) {
                    kinds.part = kind;
                    kinds.partName = kind.kind->name.text(language);
                }
                continue;
            }
            if (kind.kind->form != core::ElementForm::Text || kind.kind->isCard()) {
                continue;
            }
            kinds.kinds.append({kind, kind.kind->name.text(language),
                                NavigatorPanel::iconIdOf(kind), roomOf(kind)});
            if (kind.kind->id == TITLE_PAGE_KIND && !kinds.titlePage) {
                kinds.titlePage = kind;
            }
        }
    }
    return kinds;
}

/// Whether two names of styles are the same, in any letter case
bool sameName(const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

/// The letters without a decomposition into a Latin letter and a mark, as Latin letters
QString latinOf(QChar character) {
    switch (character.toLower().unicode()) {
        case 0x0142:  // ł
            return QStringLiteral("l");
        case 0x00F8:  // ø
            return QStringLiteral("o");
        case 0x0111:  // đ
            return QStringLiteral("d");
        case 0x00DF:  // ß
            return QStringLiteral("ss");
        case 0x00E6:  // æ
            return QStringLiteral("ae");
        case 0x0153:  // œ
            return QStringLiteral("oe");
        default:
            return QString();
    }
}

/// The letters and digits of @p name in ASCII, lower case, the others as "_"
QString asciiId(const QString& name) {
    QString id;
    for (const QChar character : name.normalized(QString::NormalizationForm_KD)) {
        if (character.unicode() < 128 && character.isLetterOrNumber()) {
            id += character.toLower();
        } else if (const QString latin = latinOf(character); !latin.isEmpty()) {
            id += latin;
        } else if (character.isSpace() || character == QLatin1Char('_') ||
                   character == QLatin1Char('-')) {
            if (!id.isEmpty() && !id.endsWith(QLatin1Char('_'))) {
                id += QLatin1Char('_');
            }
        }
    }
    while (id.endsWith(QLatin1Char('_'))) {
        id.chop(1);
    }
    return id;
}

/// The digits of @p text from @p at, the index after them in @p at
QStringView digitsAt(const QString& text, qsizetype* at) {
    const qsizetype from = *at;
    while (*at < text.size() && text.at(*at).isDigit()) {
        ++*at;
    }
    return QStringView(text).mid(from, *at - from);
}

/// Two names of files in the order of their letters, the numbers in them as numbers ("2"
/// before "10"), in any letter case; QCollator's numeric mode needs ICU, which a build may not
/// have
int compareNames(const QString& a, const QString& b) {
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        if (a.at(i).isDigit() && b.at(j).isDigit()) {
            QStringView first = digitsAt(a, &i);
            QStringView second = digitsAt(b, &j);
            while (first.size() > 1 && first.front() == QLatin1Char('0')) {
                first = first.mid(1);
            }
            while (second.size() > 1 && second.front() == QLatin1Char('0')) {
                second = second.mid(1);
            }
            if (first.size() != second.size()) {
                return first.size() < second.size() ? -1 : 1;
            }
            if (const int order = first.compare(second); order != 0) {
                return order;
            }
            continue;
        }
        qsizetype endA = i;
        while (endA < a.size() && !a.at(endA).isDigit()) {
            ++endA;
        }
        qsizetype endB = j;
        while (endB < b.size() && !b.at(endB).isDigit()) {
            ++endB;
        }
        const QString textA = a.mid(i, endA - i).toLower();
        const QString textB = b.mid(j, endB - j).toLower();
        if (const int order = QString::localeAwareCompare(textA, textB); order != 0) {
            return order;
        }
        i = endA;
        j = endB;
    }
    return (i < a.size()) - (j < b.size());
}

}  // namespace

QStringList importExtensions(ImportFormat format) {
    switch (format) {
        case ImportFormat::Docx:
            return {QStringLiteral(".docx")};
        case ImportFormat::Markdown:
            return {QStringLiteral(".md"), QStringLiteral(".markdown")};
        case ImportFormat::PlainText:
            return {QStringLiteral(".txt")};
    }
    return {};
}

ImportFile readImportFile(const QString& path, const ImportReading& reading) {
    ImportFile file;
    file.path = path;
    auto document = std::make_shared<editor::ImportDocument>();
    if (reading.format == ImportFormat::Docx) {
        if (!editor::readDocx(path, document.get(), &file.problem)) {
            return file;
        }
    } else {
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly)) {
            file.problem.kind = editor::ImportProblem::Kind::CannotOpen;
            file.problem.detail = QStringLiteral("%1: %2").arg(path, source.errorString());
            return file;
        }
        const QByteArray data = source.readAll();
        file.encoding = reading.encoding.value_or(editor::detectTextEncoding(data));
        const QString text = editor::decodeText(data, file.encoding);
        *document = reading.format == ImportFormat::Markdown
                        ? editor::readMarkdown(text, QFileInfo(path).absolutePath())
                        : editor::readPlainText(text, reading.text);
    }
    file.read = document;
    resolveChanges(file, reading.rejectChanges);
    return file;
}

void resolveChanges(ImportFile& file, bool reject) {
    if (!file.read) {
        return;
    }
    file.document = file.read->trackedChanges > 0
                        ? std::make_shared<const editor::ImportDocument>(
                              editor::withChangesResolved(*file.read, reject))
                        : file.read;
}

QString textPreview(const QString& path, editor::TextEncoding encoding, int lines) {
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QString text = editor::decodeText(source.read(PREVIEW_BYTES), encoding);
    QStringList shown;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        shown << (trimmed.size() > PREVIEW_LINE ? trimmed.left(PREVIEW_LINE) + QChar(0x2026)
                                                : trimmed);
        if (shown.size() >= lines) {
            break;
        }
    }
    return shown.join(QLatin1Char('\n'));
}

QStringList sortedByName(QStringList paths) {
    std::stable_sort(paths.begin(), paths.end(), [](const QString& a, const QString& b) {
        return compareNames(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });
    return paths;
}

// =============================================================================
// The kinds of the texts
// =============================================================================

const ImportKind* ImportKinds::find(const QString& reference) const {
    const auto found = std::find_if(kinds.cbegin(), kinds.cend(),
                                    [&reference](const ImportKind& kind) {
                                        return kind.kind.reference() == reference;
                                    });
    return found != kinds.cend() ? &*found : nullptr;
}

ImportKinds importKindsOf(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    const core::ProjectBook* first = book.book();
    if (!project || !first) {
        return {};
    }
    const core::BookTypeRegistry& registry = book.bookTypes();
    // The elements of each kind the book has now
    std::map<const core::ElementKind*, int> counts;
    const std::function<void(const QList<core::ProjectElement>&)> count =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                if (const core::KindRef kind = core::BookProject::kindOf(registry, element)) {
                    ++counts[kind.kind];
                }
                count(element.elements);
            }
        };
    count(first->frontElements);
    count(first->mainElements);
    count(first->backElements);
    ImportKinds kinds = kindsFrom(
        [project, &registry](core::BookPlace place) { return project->kindsIn(registry, place); },
        [&counts](const core::KindRef& kind) {
            return kind.kind->limit > 0 ? std::max(0, kind.kind->limit - counts[kind.kind]) : -1;
        });
    kinds.main = book.mainTextKind();
    kinds.titles = importKindTitles(book);
    return kinds;
}

ImportKinds importKindsOf(const core::BookTypeRegistry& registry, const QString& typeId) {
    ImportKinds kinds = kindsFrom(
        [&registry, &typeId](core::BookPlace place) { return registry.kindsIn(typeId, place); },
        [](const core::KindRef& kind) { return kind.kind->limit > 0 ? kind.kind->limit : -1; });
    if (const core::BookTypePackage* type = registry.package(typeId)) {
        kinds.main = registry.findKind(type->id, type->primaryKind);
    }
    if (!kinds.main && !kinds.kinds.isEmpty()) {
        kinds.main = kinds.kinds.front().kind;
    }
    QList<core::KindRef> all;
    for (const ImportKind& kind : std::as_const(kinds.kinds)) {
        all.append(kind.kind);
    }
    kinds.titles = importKindTitles(all, kinds.main);
    return kinds;
}

// =============================================================================
// The texts and the parts
// =============================================================================

std::vector<ImportRow> importRows(const std::vector<ImportFile>& files,
                                  const editor::SplitOptions& split, bool wholeFiles,
                                  const ImportKinds& kinds) {
    std::vector<ImportRow> rows;
    editor::SplitOptions whole = split;
    whole.mode = editor::SplitMode::Whole;
    for (size_t i = 0; i < files.size(); ++i) {
        const ImportFile& file = files[i];
        if (!file.isRead()) {
            continue;
        }
        const QString fileTitle = QFileInfo(file.path).completeBaseName();
        for (const editor::ImportPiece& piece :
             editor::splitDocument(*file.document, wholeFiles ? whole : split)) {
            ImportRow row;
            row.file = static_cast<int>(i);
            row.piece = piece;
            row.title = piece.title.isEmpty() ? fileTitle : piece.title;
            if (piece.part) {
                row.kind = kinds.part.reference();
            } else if (piece.beforeTitles && kinds.titlePage && piece.words <= TITLE_PAGE_WORDS &&
                       i == 0) {
                // A short text before the first title of the first file: a title page
                row.kind = kinds.titlePage.reference();
                if (const ImportKind* kind = kinds.find(row.kind); kind && piece.title.isEmpty()) {
                    row.title = kind->name;
                }
            } else if (const QString named = editor::kindOfTitle(row.title, kinds.titles);
                       !named.isEmpty() && kinds.find(named)) {
                row.kind = named;
            } else {
                row.kind = kinds.main.reference();
            }
            rows.push_back(std::move(row));
        }
    }

    // A kind the book can have no more of: its texts are unchecked
    std::map<QString, int> taken;
    for (ImportRow& row : rows) {
        const ImportKind* kind = row.piece.part ? nullptr : kinds.find(row.kind);
        if (!kind || kind->room < 0) {
            continue;
        }
        if (taken[row.kind] >= kind->room) {
            row.checked = false;
            row.full = true;
        } else {
            ++taken[row.kind];
        }
    }
    return rows;
}

QStringList fullKinds(const std::vector<ImportRow>& rows, const ImportKinds& kinds) {
    QStringList full;
    std::map<QString, int> taken;
    for (const ImportRow& row : rows) {
        const ImportKind* kind = row.piece.part || !row.checked ? nullptr : kinds.find(row.kind);
        if (!kind || kind->room < 0) {
            continue;
        }
        if (++taken[row.kind] > kind->room && !full.contains(row.kind)) {
            full << row.kind;
        }
    }
    return full;
}

bool isChecked(const std::vector<ImportRow>& rows, size_t row) {
    if (row >= rows.size()) {
        return false;
    }
    if (!rows[row].piece.part) {
        return rows[row].checked;
    }
    for (size_t i = row + 1;
         i < rows.size() && rows[i].piece.inPart && rows[i].file == rows[row].file; ++i) {
        if (rows[i].checked) {
            return true;
        }
    }
    return false;
}

std::vector<ImportedText> importedTexts(const std::vector<ImportFile>& files,
                                        const std::vector<ImportRow>& rows,
                                        const ImportKinds& kinds,
                                        const std::vector<editor::ImportKmlOptions>& options) {
    std::vector<ImportedText> texts;
    const auto textOf = [&](const ImportRow& row) {
        ImportedText text;
        text.title = row.title.trimmed();
        const ImportKind* kind = kinds.find(row.kind);
        text.kind = kind ? kind->kind : kinds.main;
        const size_t file = static_cast<size_t>(row.file);
        if (file < files.size() && files[file].isRead()) {
            text.kml = editor::importKml(*files[file].document, row.piece,
                                         file < options.size() ? options[file]
                                                               : editor::ImportKmlOptions());
        }
        return text;
    };
    for (size_t i = 0; i < rows.size(); ++i) {
        const ImportRow& row = rows[i];
        if (row.piece.inPart) {
            continue;  // with its part
        }
        if (!row.piece.part) {
            if (row.checked) {
                texts.push_back(textOf(row));
            }
            continue;
        }
        std::vector<ImportedText> inside;
        for (size_t j = i + 1; j < rows.size() && rows[j].piece.inPart && rows[j].file == row.file;
             ++j) {
            if (rows[j].checked) {
                inside.push_back(textOf(rows[j]));
            }
        }
        if (inside.empty()) {
            continue;
        }
        if (!kinds.part) {
            // A book without parts: the texts of the part one after another
            std::move(inside.begin(), inside.end(), std::back_inserter(texts));
            continue;
        }
        ImportedText part;
        part.title = row.title.trimmed();
        part.kind = kinds.part;
        part.texts = std::move(inside);
        texts.push_back(std::move(part));
    }
    return texts;
}

int checkedWords(const std::vector<ImportRow>& rows) {
    int words = 0;
    for (const ImportRow& row : rows) {
        if (!row.piece.part && row.checked) {
            words += row.piece.words;
        }
    }
    return words;
}

// =============================================================================
// The styles
// =============================================================================

std::vector<ImportFileStyle> importFileStyles(const std::vector<ImportFile>& files) {
    std::vector<ImportFileStyle> styles;
    for (const ImportFile& file : files) {
        if (!file.isRead()) {
            continue;
        }
        for (const editor::ImportStyle& style : file.document->styles) {
            if (style.paragraphs <= 0) {
                continue;
            }
            const QString name = style.name.isEmpty() ? style.key : style.name;
            const auto same = std::find_if(styles.begin(), styles.end(),
                                           [&name](const ImportFileStyle& each) {
                                               return sameName(each.name, name);
                                           });
            if (same != styles.end()) {
                same->paragraphs += style.paragraphs;
            } else {
                styles.push_back({style.key, name, style.paragraphs, &style});
            }
        }
    }
    return styles;
}

QString newStyleId(const QString& name, const QStringList& taken) {
    QString base = asciiId(name);
    base = QStringLiteral("imported_") + (base.isEmpty() ? QStringLiteral("style") : base);
    QString id = base;
    for (int number = 2; taken.contains(id); ++number) {
        id = QStringLiteral("%1_%2").arg(base).arg(number);
    }
    return id;
}

QList<core::BookParagraphStyle> newBookStyles(const std::vector<ImportFile>& files,
                                              QHash<QString, QString>* mapping,
                                              const editor::ParagraphStyleSet& book) {
    QList<core::BookParagraphStyle> made;
    if (!mapping) {
        return made;
    }
    QStringList taken;
    for (const editor::ParagraphStyle& style : book.styles()) {
        taken << style.id;
    }
    const std::vector<ImportFileStyle> styles = importFileStyles(files);
    // In the order of the names, so the same choices make the same ids
    QStringList names = mapping->keys();
    names.sort(Qt::CaseInsensitive);
    for (const QString& name : std::as_const(names)) {
        const auto it = mapping->find(name);
        if (!it.value().startsWith(QLatin1String(NEW_STYLE_PREFIX))) {
            continue;
        }
        const auto style = std::find_if(styles.cbegin(), styles.cend(),
                                        [&name](const ImportFileStyle& each) {
                                            return sameName(each.name, name);
                                        });
        const editor::ImportDocument* document = nullptr;
        for (const ImportFile& file : files) {
            if (file.isRead() && style != styles.cend() &&
                file.document->style(style->key) == style->style) {
                document = file.document.get();
                break;
            }
        }
        if (style == styles.cend() || !document) {
            it.value() = QString();  // a style no file has: the suggestion
            continue;
        }
        core::BookParagraphStyle entry;
        entry.id = newStyleId(style->name, taken);
        entry.name = style->name;
        entry.baseStyle = editor::suggestedBookStyle(*document, style->key, book);
        entry.properties = style->style->properties;
        taken << entry.id;
        made.append(entry);
        it.value() = entry.id;
    }
    return made;
}

QHash<QString, QString> fileMapping(const editor::ImportDocument& document,
                                    const QHash<QString, QString>& byName) {
    QHash<QString, QString> mapping;
    for (const editor::ImportStyle& style : document.styles) {
        const QString name = style.name.isEmpty() ? style.key : style.name;
        for (auto it = byName.cbegin(); it != byName.cend(); ++it) {
            if (sameName(it.key(), name) && !it.value().isEmpty()) {
                mapping.insert(style.key, it.value());
                break;
            }
        }
    }
    return mapping;
}

}  // namespace kalahari::gui
