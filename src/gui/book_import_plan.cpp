// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_plan.cpp
/// @brief What an import will add to a book: the files read, their texts and parts with the
///        kinds and titles they get, and the new styles of the book made of the files' styles

#include <kalahari/gui/book_import_plan.h>

#include <kalahari/core/extension_points.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/gui/panels/navigator_panel.h>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <array>
#include <exception>
#include <functional>
#include <map>
#include <utility>

namespace kalahari::gui {

/// @brief The context of the translations of what an import adds to a book
struct BookImportPlan {
    Q_DECLARE_TR_FUNCTIONS(BookImportPlan)
};

namespace {

constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                     core::BookPlace::Back};

/// The id of the kind of a title page in the packages of the program
const QString TITLE_PAGE_KIND = QStringLiteral("title_page");

/// The ids of the program's own formats, which the formats of the plugins cannot take
const QStringList OWN_FORMATS{QStringLiteral("docx"), QStringLiteral("markdown"),
                              QStringLiteral("text"), QStringLiteral("scrivener")};

/// The styles of headings of Markdown: heading_1 to heading_3
constexpr int MARKDOWN_HEADING_STYLES = 3;
/// The deepest level of a heading
constexpr int DEEPEST_HEADING = 9;

/// The key of the style of a heading of @p level in Markdown
QString headingStyle(int level) {
    return QStringLiteral("heading_%1").arg(std::min(level, MARKDOWN_HEADING_STYLES));
}

/// The key of a style of Markdown for a heading a level lower; another style's key as it is
QString lowerStyle(const QString& key) {
    return key.startsWith(QLatin1String("heading_")) ? headingStyle(key.mid(8).toInt() + 1) : key;
}

/// Where the parts of a document go when it is added after another one
struct Offsets {
    int notes = 0;
    int images = 0;
    int annotations = 0;
    int lists = 0;
};

/// A block of a document added after another one: its headings a level lower, its notes,
/// images, annotations and lists after those of the other document
void lowerBlock(editor::ImportBlock& block, const Offsets& offsets) {
    if (block.headingLevel > 0) {
        block.headingLevel = std::min(block.headingLevel + 1, DEEPEST_HEADING);
    }
    block.style = lowerStyle(block.style);
    if (block.list >= 0) {
        block.list += offsets.lists;
    }
    for (editor::ImportRun& run : block.runs) {
        switch (run.kind) {
            case editor::ImportRun::Kind::Note:
                run.index += offsets.notes;
                break;
            case editor::ImportRun::Kind::Image:
                run.index += offsets.images;
                break;
            case editor::ImportRun::Kind::Annotation:
                run.index += offsets.annotations;
                break;
            default:
                break;
        }
        for (int& annotation : run.annotations) {
            annotation += offsets.annotations;
        }
    }
    if (block.table) {
        for (editor::ImportRow& row : block.table->rows) {
            for (editor::ImportCell& cell : row.cells) {
                for (editor::ImportBlock& each : cell.blocks) {
                    lowerBlock(each, offsets);
                }
            }
        }
    }
}

/// Add @p text after what @p document has, its headings a level lower; @p paragraphs counts
/// the paragraphs of each style
void appendLower(editor::ImportDocument& document, editor::ImportDocument text,
                 std::map<QString, int>& paragraphs) {
    const Offsets offsets{static_cast<int>(document.notes.size()),
                          static_cast<int>(document.images.size()),
                          static_cast<int>(document.annotations.size()),
                          static_cast<int>(document.lists.size())};
    for (editor::ImportBlock& block : text.blocks) {
        lowerBlock(block, offsets);
        document.blocks.push_back(std::move(block));
    }
    for (editor::ImportNote& note : text.notes) {
        for (editor::ImportBlock& block : note.paragraphs) {
            lowerBlock(block, offsets);
        }
        document.notes.push_back(std::move(note));
    }
    std::move(text.images.begin(), text.images.end(), std::back_inserter(document.images));
    std::move(text.annotations.begin(), text.annotations.end(),
              std::back_inserter(document.annotations));
    std::move(text.lists.begin(), text.lists.end(), std::back_inserter(document.lists));
    for (const editor::ImportStyle& style : text.styles) {
        paragraphs[lowerStyle(style.key)] += style.paragraphs;
    }
    for (const auto& [omission, count] : text.omissions) {
        document.omissions[omission] += count;
    }
}

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

// =============================================================================
// A project of Scrivener
// =============================================================================

/// Append a document of a project after its draft (a snapshot, a text of the research): its
/// paragraphs, what it left out and its tracked changes are not counted with the draft's
void appendAfterDraft(editor::ImportDocument& document, editor::ImportDocument added, size_t* from,
                      size_t* to) {
    for (editor::ImportStyle& style : added.styles) {
        style.paragraphs = 0;
    }
    added.omissions.clear();
    added.trackedChanges = 0;
    *from = document.blocks.size();
    editor::appendImportDocument(document, std::move(added));
    *to = document.blocks.size();
}

/// The kind of a material of a file of the research of its type in the binder
ImportMaterial::Kind materialKindOf(const QString& type) {
    if (type.compare(QLatin1String("Image"), Qt::CaseInsensitive) == 0) {
        return ImportMaterial::Kind::Picture;
    }
    if (type.compare(QLatin1String("PDF"), Qt::CaseInsensitive) == 0) {
        return ImportMaterial::Kind::Document;
    }
    if (type.compare(QLatin1String("WebArchive"), Qt::CaseInsensitive) == 0) {
        return ImportMaterial::Kind::WebPage;
    }
    return ImportMaterial::Kind::File;
}

/// The documents of the research, those in folders too, in the binder's order: the files, and
/// the texts (those of the folders) with something in them
void addResearch(const std::vector<editor::ScrivenerItem>& items, editor::ImportDocument& document,
                 std::vector<ImportMaterial>& research) {
    for (const editor::ScrivenerItem& item : items) {
        ImportMaterial material;
        material.title = item.title.trimmed();
        material.synopsis = item.synopsis.trimmed();
        material.notes = item.notes.trimmed();
        if (item.kind == editor::ScrivenerItem::Kind::File) {
            material.kind = materialKindOf(item.type);
            material.file = item.file;
            research.push_back(std::move(material));
        } else if (item.hasText() || !material.synopsis.isEmpty() || !material.notes.isEmpty()) {
            material.kind = ImportMaterial::Kind::Note;
            appendAfterDraft(document, item.text, &material.from, &material.to);
            research.push_back(std::move(material));
        }
        addResearch(item.children, document, research);
    }
}

/// The document of a project: its draft, then the snapshots of its texts and its research
editor::ImportDocument projectDocument(const editor::ScrivenerProject& project,
                                       const editor::ScrivenerDraftOptions& options,
                                       ImportProject* read) {
    editor::ScrivenerDraft draft = editor::scrivenerDraft(project, options);
    read->title = project.title.trimmed();
    read->draftName = project.draftName.trimmed();
    read->excluded = draft.excluded;
    read->sceneSnapshots = draft.sceneSnapshots;
    read->trash = project.trash;
    editor::ImportDocument document = std::move(draft.document);
    read->draftBlocks = document.blocks.size();
    for (editor::ScrivenerText& text : draft.texts) {
        read->pieces.push_back(text.piece);
        ImportTextDetails details;
        details.synopsis = text.synopsis.trimmed();
        details.notes = text.notes.trimmed();
        details.label = text.label;
        details.status = text.status;
        details.keywords = text.keywords;
        details.scenes = std::move(text.scenes);
        for (editor::ScrivenerSnapshot& snapshot : text.snapshots) {
            ImportSnapshot version;
            version.title = snapshot.title.trimmed();
            version.created = snapshot.created;
            appendAfterDraft(document, std::move(snapshot.text), &version.from, &version.to);
            details.snapshots.push_back(std::move(version));
        }
        read->details.push_back(std::move(details));
    }
    addResearch(project.research, document, read->research);
    return document;
}

/// The label, a status of the project the book has no status of, and the keywords, a line each
QStringList aboutOf(const QString& label, const QString& status, const QStringList& keywords) {
    QStringList lines;
    if (!label.trimmed().isEmpty()) {
        //: The label of a document of Scrivener, in the notes of an imported text. In Polish:
        //: Etykieta: %1
        lines << BookImportPlan::tr("Label: %1").arg(label.trimmed());
    }
    if (!status.trimmed().isEmpty() && editor::scrivenerBookStatus(status).isEmpty()) {
        //: A status of Scrivener the book has no status of, in the notes of an imported text.
        //: In Polish: Stan: %1
        lines << BookImportPlan::tr("Status: %1").arg(status.trimmed());
    }
    if (!keywords.isEmpty()) {
        //: The keywords of a document of Scrivener, in the notes of an imported text. In
        //: Polish: Słowa kluczowe: %1
        lines << BookImportPlan::tr("Keywords: %1").arg(keywords.join(QStringLiteral(", ")));
    }
    return lines;
}

/// What a project says of a text or a part, as @p take has it: its notes, status and versions
void addDetails(ImportedText& text, const ImportFile& file, const ImportRow& row,
                const editor::ImportKmlOptions& options, const ImportDetails& take) {
    if (!file.project || !file.document || row.text < 0 ||
        static_cast<size_t>(row.text) >= file.project->details.size()) {
        return;
    }
    const ImportTextDetails& details = file.project->details[static_cast<size_t>(row.text)];
    text.notes = importNotes(details, take);
    if (row.piece.part) {
        return;
    }
    if (take.statuses) {
        text.status = editor::scrivenerBookStatus(details.status);
    }
    if (take.snapshots) {
        for (const ImportSnapshot& snapshot : details.snapshots) {
            editor::ImportPiece piece;
            piece.from = snapshot.from;
            piece.to = snapshot.to;
            text.versions.push_back(
                {editor::importKml(*file.document, piece, options), snapshot.title, snapshot.created});
        }
    }
}

}  // namespace

int ImportProject::snapshots() const {
    int count = 0;
    for (const ImportTextDetails& each : details) {
        count += static_cast<int>(each.snapshots.size());
    }
    return count;
}

QString importNotes(const ImportTextDetails& details, const ImportDetails& take) {
    QStringList paragraphs;
    if (take.synopses && !details.synopsis.trimmed().isEmpty()) {
        //: Before the synopsis of a document of Scrivener, at the start of the notes of an
        //: imported text. In Polish: Streszczenie:
        paragraphs << BookImportPlan::tr("Synopsis:") + QLatin1Char('\n') + details.synopsis.trimmed();
    }
    if (take.notes && !details.notes.trimmed().isEmpty()) {
        paragraphs << details.notes.trimmed();
    }
    if (const QStringList about = take.notes ? aboutOf(details.label, details.status, details.keywords)
                                             : QStringList();
        !about.isEmpty()) {
        paragraphs << about.join(QLatin1Char('\n'));
    }
    QStringList scenes;
    for (const editor::ScrivenerScene& scene : details.scenes) {
        QStringList lines;
        if (take.synopses && !scene.synopsis.trimmed().isEmpty()) {
            lines << scene.synopsis.trimmed();
        }
        if (take.notes) {
            if (!scene.notes.trimmed().isEmpty()) {
                lines << scene.notes.trimmed();
            }
            lines += aboutOf(scene.label, scene.status, scene.keywords);
        }
        if (lines.isEmpty()) {
            continue;
        }
        //: A scene of Scrivener without a title, in the notes of an imported text. In Polish:
        //: Scena bez tytułu
        const QString title = scene.title.trimmed().isEmpty() ? BookImportPlan::tr("Untitled scene")
                                                              : scene.title.trimmed();
        lines.prepend(title);
        scenes << lines.join(QLatin1Char('\n'));
    }
    if (!scenes.isEmpty()) {
        //: The heading of what the scenes of Scrivener say of themselves, in the notes of an
        //: imported text. In Polish: Sceny:
        paragraphs << BookImportPlan::tr("Scenes:") + QStringLiteral("\n\n") +
                          scenes.join(QStringLiteral("\n\n"));
    }
    return paragraphs.join(QStringLiteral("\n\n"));
}

QString materialNotes(const ImportMaterial& material) {
    QStringList paragraphs;
    for (const QString& each : {material.synopsis, material.notes}) {
        if (!each.trimmed().isEmpty()) {
            paragraphs << each.trimmed();
        }
    }
    return paragraphs.join(QStringLiteral("\n\n"));
}

QStringList importExtensions(ImportFormat format, const QString& plugin) {
    switch (format) {
        case ImportFormat::Docx:
            return {QStringLiteral(".docx")};
        case ImportFormat::Markdown:
            return {QStringLiteral(".md"), QStringLiteral(".markdown")};
        case ImportFormat::PlainText:
            return {QStringLiteral(".txt")};
        case ImportFormat::Scrivener:
            return {QStringLiteral(".scrivx")};
        case ImportFormat::Plugin:
            for (const PluginImportFormat& each : pluginImportFormats()) {
                if (each.id == plugin) {
                    return each.extensions;
                }
            }
            break;
    }
    return {};
}

std::vector<PluginImportFormat> pluginImportFormats() {
    std::vector<PluginImportFormat> formats;
    QStringList taken = OWN_FORMATS;
    for (const auto& importer :
         core::ExtensionPointRegistry::getInstance().getPluginsOfType<core::IImporter>()) {
        std::vector<core::ImportFormatInfo> infos;
        try {
            infos = importer->getFormats();
        } catch (const std::exception& error) {
            core::Logger::getInstance().error("Import: the formats of the plugin {}: {}",
                                              importer->getPluginId(), error.what());
            continue;
        }
        for (const core::ImportFormatInfo& info : infos) {
            PluginImportFormat format;
            format.id = QString::fromStdString(info.id).trimmed();
            format.name = QString::fromStdString(info.name).trimmed();
            for (const std::string& each : info.extensions) {
                QString extension = QString::fromStdString(each).trimmed().toLower();
                while (extension.startsWith(QLatin1Char('.'))) {
                    extension.remove(0, 1);
                }
                if (!extension.isEmpty()) {
                    format.extensions << QLatin1Char('.') + extension;
                }
            }
            format.extensions.removeDuplicates();
            if (format.id.isEmpty() || format.name.isEmpty() || format.extensions.isEmpty() ||
                taken.contains(format.id)) {
                continue;
            }
            taken << format.id;
            format.pluginId = QString::fromStdString(importer->getPluginId());
            formats.push_back(std::move(format));
        }
    }
    return formats;
}

std::shared_ptr<core::IImporter> importerOf(const QString& id) {
    for (const PluginImportFormat& each : pluginImportFormats()) {
        if (each.id == id) {
            return core::ExtensionPointRegistry::getInstance().getPluginAs<core::IImporter>(
                each.pluginId.toStdString());
        }
    }
    return nullptr;
}

editor::ImportDocument pluginDocument(const std::vector<core::ImportTextInfo>& texts,
                                      const QString& folder, const QString& fileTitle) {
    editor::ImportDocument document;
    std::map<QString, int> paragraphs;
    for (size_t i = 0; i < texts.size(); ++i) {
        const QString title = QString::fromStdString(texts[i].title).simplified();
        // The title as a heading of the first level; the first text without one is the text
        // before the titles
        if (!title.isEmpty() || i > 0) {
            editor::ImportBlock heading;
            heading.style = headingStyle(1);
            heading.headingLevel = 1;
            editor::ImportRun run;
            run.text = title.isEmpty() ? fileTitle : title;
            heading.runs.push_back(std::move(run));
            ++paragraphs[heading.style];
            document.blocks.push_back(std::move(heading));
        }
        appendLower(document,
                    editor::readMarkdown(QString::fromStdString(texts[i].markdown), folder),
                    paragraphs);
    }
    // The styles in the order of Markdown's
    for (const QString& key : {QStringLiteral("normal"), headingStyle(1), headingStyle(2),
                               headingStyle(3), QStringLiteral("quote"), QStringLiteral("code")}) {
        if (const auto found = paragraphs.find(key); found != paragraphs.end() && found->second > 0) {
            editor::ImportStyle style;
            style.key = key;
            style.name = key;
            style.headingLevel = key.startsWith(QLatin1String("heading_")) ? key.mid(8).toInt() : 0;
            style.paragraphs = found->second;
            document.styles.push_back(std::move(style));
        }
    }
    return document;
}

ImportFile readImportFile(const QString& path, const ImportReading& reading) {
    ImportFile file;
    file.path = path;
    auto document = std::make_shared<editor::ImportDocument>();
    if (reading.format == ImportFormat::Plugin) {
        // The plugin divides the file into its texts
        const std::shared_ptr<core::IImporter> importer = importerOf(reading.plugin);
        if (!importer) {
            file.problem.kind = editor::ImportProblem::Kind::Plugin;
            file.problem.detail =
                QStringLiteral("%1: no plugin reads the format %2").arg(path, reading.plugin);
            return file;
        }
        std::vector<core::ImportTextInfo> texts;
        try {
            texts = importer->importDocument(reading.plugin.toStdString(), path.toStdString());
        } catch (const std::exception& error) {
            file.problem.kind = editor::ImportProblem::Kind::Plugin;
            file.problem.detail = QStringLiteral("%1: %2").arg(path, QString::fromUtf8(error.what()));
            return file;
        }
        const QFileInfo info(path);
        *document = pluginDocument(texts, info.absolutePath(), info.completeBaseName());
    } else if (reading.format == ImportFormat::Docx) {
        if (!editor::readDocx(path, document.get(), &file.problem)) {
            return file;
        }
    } else if (reading.format == ImportFormat::Scrivener) {
        editor::ScrivenerProject project;
        if (!editor::readScrivenerProject(path, &project, &file.problem)) {
            return file;
        }
        auto read = std::make_shared<ImportProject>();
        *document = projectDocument(project, reading.scrivener, read.get());
        file.project = std::move(read);
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
        const std::vector<editor::ImportPiece> pieces =
            file.project ? file.project->pieces
                         : editor::splitDocument(*file.document, wholeFiles ? whole : split);
        for (size_t p = 0; p < pieces.size(); ++p) {
            const editor::ImportPiece& piece = pieces[p];
            ImportRow row;
            row.file = static_cast<int>(i);
            row.piece = piece;
            row.text = file.project ? static_cast<int>(p) : -1;
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
                                        const std::vector<editor::ImportKmlOptions>& options,
                                        const ImportDetails& details) {
    std::vector<ImportedText> texts;
    const editor::ImportKmlOptions none;
    const auto optionsOf = [&](size_t file) -> const editor::ImportKmlOptions& {
        return file < options.size() ? options[file] : none;
    };
    const auto textOf = [&](const ImportRow& row) {
        ImportedText text;
        text.title = row.title.trimmed();
        const ImportKind* kind = kinds.find(row.kind);
        text.kind = kind ? kind->kind : kinds.main;
        const size_t file = static_cast<size_t>(row.file);
        if (file < files.size() && files[file].isRead()) {
            text.kml = editor::importKml(*files[file].document, row.piece, optionsOf(file));
            addDetails(text, files[file], row, optionsOf(file), details);
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
        if (const size_t file = static_cast<size_t>(row.file); file < files.size()) {
            addDetails(part, files[file], row, optionsOf(file), details);
        }
        texts.push_back(std::move(part));
    }
    return texts;
}

std::vector<ImportedMaterial> importedMaterials(const std::vector<ImportFile>& files,
                                                const std::vector<editor::ImportKmlOptions>& options) {
    std::vector<ImportedMaterial> materials;
    for (size_t i = 0; i < files.size(); ++i) {
        const ImportFile& file = files[i];
        if (!file.isRead() || !file.project) {
            continue;
        }
        const editor::ImportKmlOptions none;
        const editor::ImportKmlOptions& written = i < options.size() ? options[i] : none;
        for (const ImportMaterial& material : file.project->research) {
            ImportedMaterial made;
            made.title = material.title;
            if (made.title.isEmpty()) {
                made.title = QFileInfo(material.file).completeBaseName();
            }
            if (made.title.isEmpty()) {
                //: The title of a document of the research of Scrivener without one. In Polish:
                //: Bez tytułu
                made.title = BookImportPlan::tr("Untitled");
            }
            switch (material.kind) {
                case ImportMaterial::Kind::Note: {
                    made.kind = QStringLiteral("work_note");
                    editor::ImportPiece piece;
                    piece.from = material.from;
                    piece.to = material.to;
                    made.kml = editor::importKml(*file.document, piece, written);
                    made.notes = materialNotes(material);
                    break;
                }
                case ImportMaterial::Kind::Picture:
                    made.kind = QStringLiteral("picture");
                    break;
                case ImportMaterial::Kind::Document:
                    made.kind = QStringLiteral("document");
                    break;
                case ImportMaterial::Kind::WebPage:
                    made.kind = QStringLiteral("web_page");
                    break;
                case ImportMaterial::Kind::File:
                    made.kind = QStringLiteral("other_file");
                    break;
            }
            if (material.kind != ImportMaterial::Kind::Note) {
                made.file = material.file;
                made.description = materialNotes(material);
            }
            materials.push_back(std::move(made));
        }
    }
    return materials;
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
