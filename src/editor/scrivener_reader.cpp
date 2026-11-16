// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file scrivener_reader.cpp
/// @brief Import of a project of Scrivener: its binder, its documents and their metadata, and
///        the texts and parts of its draft

#include "kalahari/editor/scrivener_import.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace kalahari::editor {

namespace {

constexpr int MAX_HEADING_LEVEL = 9;
constexpr int PART_LEVEL = 1;
constexpr int TEXT_LEVEL = 2;
constexpr int SECONDS_PER_MINUTE = 60;
constexpr int MINUTES_PER_HOUR = 60;
constexpr int MAX_BINDER_DEPTH = 64;  ///< Items deeper than this are left out

/// The files of a document in its folder Files/Data/<UUID>/
constexpr QLatin1String CONTENT("content.rtf");
constexpr QLatin1String SYNOPSIS("synopsis.txt");
constexpr QLatin1String NOTES("notes.rtf");
constexpr QLatin1String COMMENTS("content.comments");

/// An element of XML with its children
struct Node {
    QString name;
    QXmlStreamAttributes attributes;
    std::vector<Node> children;
    QString text;  ///< Its own characters

    const Node* child(QLatin1String local) const {
        const auto it = std::find_if(children.begin(), children.end(),
                                     [local](const Node& node) { return node.name == local; });
        return it == children.end() ? nullptr : &*it;
    }

    /// The text of its child @p local, trimmed; empty: none
    QString value(QLatin1String local) const {
        const Node* found = child(local);
        return found ? found->text.trimmed() : QString();
    }

    QString attr(QLatin1String local) const { return attributes.value(local).toString(); }
};

/// The tree of an XML document; false when it is not well-formed
bool readXml(const QByteArray& data, Node* root) {
    QXmlStreamReader reader(data);
    std::vector<Node*> open;
    bool started = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            Node* parent = open.empty() ? nullptr : open.back();
            Node* node = root;
            if (parent) {
                parent->children.emplace_back();
                node = &parent->children.back();
            } else if (started) {
                return false;  // a second root
            }
            started = true;
            node->name = reader.name().toString();
            node->attributes = reader.attributes();
            open.push_back(node);
        } else if (reader.isEndElement() && !open.empty()) {
            open.pop_back();
        } else if (reader.isCharacters() && !open.empty()) {
            open.back()->text += reader.text();
        }
    }
    return started && !reader.hasError();
}

/// The bytes of a file; empty when it cannot be read
QByteArray contentsOf(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// The plain text of a document's paragraphs, one in a line
QString plainTextOf(const ImportDocument& document) {
    QStringList lines;
    for (const ImportBlock& block : document.blocks) {
        if (block.kind == ImportBlock::Kind::Paragraph) {
            lines << importPlainText(block).trimmed();
        }
    }
    return lines.join(QLatin1Char('\n')).trimmed();
}

/// The parts of the time of a snapshot in index.xml ("2025-10-27 02:20:40 -0700"): the date,
/// the time, the sign of the offset from UTC, its hours and minutes
QRegularExpressionMatch snapshotTimeParts(const QString& text) {
    static const QRegularExpression pattern(QStringLiteral(
        "^(\\d{4}-\\d{2}-\\d{2})[ T](\\d{2}):(\\d{2}):(\\d{2})(?: ?([+-])(\\d{2}):?(\\d{2}))?$"));
    return pattern.match(text.trimmed());
}

/// The time of a snapshot in index.xml; invalid: not one
QDateTime snapshotTime(const QString& text) {
    const QRegularExpressionMatch match = snapshotTimeParts(text);
    if (!match.hasMatch()) {
        return QDateTime();
    }
    const QDate date = QDate::fromString(match.captured(1), Qt::ISODate);
    const QTime time(match.captured(2).toInt(), match.captured(3).toInt(), match.captured(4).toInt());
    if (match.captured(5).isEmpty()) {
        return QDateTime(date, time);
    }
    const int sign = match.captured(5) == QLatin1String("-") ? -1 : 1;
    const int offset =
        sign * (match.captured(6).toInt() * MINUTES_PER_HOUR + match.captured(7).toInt()) *
        SECONDS_PER_MINUTE;
    return QDateTime(date, time, QTimeZone(offset));
}

/// The name of the file of a snapshot of its time in index.xml:
/// "2025-10-27-02-20-40-0700.rtf"; empty: not a time
QString snapshotFileName(const QString& text) {
    const QRegularExpressionMatch match = snapshotTimeParts(text);
    if (!match.hasMatch()) {
        return QString();
    }
    QString name = QStringLiteral("%1-%2-%3-%4")
                       .arg(match.captured(1), match.captured(2), match.captured(3), match.captured(4));
    if (!match.captured(6).isEmpty()) {
        name += QLatin1Char('-') + match.captured(6) + match.captured(7);
    }
    return name + QStringLiteral(".rtf");
}

/// The time of a snapshot of the name of its file, without its offset from UTC (its sign is
/// not in the name); invalid: not a time
QDateTime snapshotTimeOfFile(const QString& fileName) {
    static const QRegularExpression pattern(
        QStringLiteral("^(\\d{4}-\\d{2}-\\d{2})-(\\d{2})-(\\d{2})-(\\d{2})"));
    const QRegularExpressionMatch match = pattern.match(fileName);
    if (!match.hasMatch()) {
        return QDateTime();
    }
    return QDateTime(QDate::fromString(match.captured(1), Qt::ISODate),
                     QTime(match.captured(2).toInt(), match.captured(3).toInt(), match.captured(4).toInt()));
}

/// Whether a block has something of a text: a character, a picture, a table
bool hasContent(const ImportBlock& block) {
    if (block.kind == ImportBlock::Kind::Table) {
        return true;
    }
    return std::any_of(block.runs.begin(), block.runs.end(), [](const ImportRun& run) {
        return run.kind == ImportRun::Kind::Image ||
               (run.kind == ImportRun::Kind::Text && !run.text.trimmed().isEmpty());
    });
}

/// The reader of a project's binder and of the files of its documents
class ProjectReader {
public:
    ProjectReader(const QString& folder, const Node& root)
        : m_data(QDir(folder).filePath(QStringLiteral("Files/Data"))),
          m_snapshots(QDir(folder).filePath(QStringLiteral("Snapshots"))) {
        readNames(root);
    }

    ScrivenerItem readItem(const Node& node, int depth) const {
        ScrivenerItem item;
        item.id = node.attr(QLatin1String("UUID"));
        if (item.id.isEmpty()) {
            item.id = node.attr(QLatin1String("ID"));
        }
        item.type = node.attr(QLatin1String("Type"));
        item.title = node.value(QLatin1String("Title"));
        if (item.type.contains(QLatin1String("Folder"))) {
            item.kind = ScrivenerItem::Kind::Folder;
        } else if (item.type != QLatin1String("Text")) {
            item.kind = ScrivenerItem::Kind::File;
        }
        readMetaData(node, item);
        readFiles(item);
        if (const Node* children = node.child(QLatin1String("Children"));
            children && depth < MAX_BINDER_DEPTH) {
            for (const Node& child : children->children) {
                if (child.name == QLatin1String("BinderItem")) {
                    item.children.push_back(readItem(child, depth + 1));
                }
            }
        }
        return item;
    }

private:
    /// The names of the labels, the statuses and the keywords by their ids
    void readNames(const Node& root) {
        const auto names = [](const Node* list, QLatin1String element, QHash<QString, QString>& into) {
            if (!list) {
                return;
            }
            for (const Node& node : list->children) {
                const QString id = node.attr(QLatin1String("ID"));
                if (node.name != element || id.isEmpty() || id == QLatin1String("-1")) {
                    continue;  // -1: No Label, No Status
                }
                const QString title = node.value(QLatin1String("Title"));
                into.insert(id, title.isEmpty() ? node.text.trimmed() : title);
            }
        };
        if (const Node* labels = root.child(QLatin1String("LabelSettings"))) {
            names(labels->child(QLatin1String("Labels")), QLatin1String("Label"), m_labels);
        }
        if (const Node* statuses = root.child(QLatin1String("StatusSettings"))) {
            names(statuses->child(QLatin1String("StatusItems")), QLatin1String("Status"),
                  m_statuses);
        }
        names(root.child(QLatin1String("Keywords")), QLatin1String("Keyword"), m_keywords);
    }

    void readMetaData(const Node& node, ScrivenerItem& item) const {
        if (const Node* meta = node.child(QLatin1String("MetaData"))) {
            item.compiled = meta->value(QLatin1String("IncludeInCompile"))
                                .compare(QLatin1String("No"), Qt::CaseInsensitive) != 0;
            item.label = m_labels.value(meta->value(QLatin1String("LabelID")));
            item.status = m_statuses.value(meta->value(QLatin1String("StatusID")));
        }
        // Its keywords: by their ids (KeywordID), or by their names (Keyword)
        if (const Node* keywords = node.child(QLatin1String("Keywords"))) {
            for (const Node& keyword : keywords->children) {
                const QString text = keyword.text.trimmed();
                QString name = keyword.name == QLatin1String("KeywordID") ? m_keywords.value(text)
                                                                           : text;
                if (keyword.name == QLatin1String("Keyword") && name.isEmpty()) {
                    name = m_keywords.value(keyword.attr(QLatin1String("ID")));
                }
                if (!name.isEmpty() && !item.keywords.contains(name)) {
                    item.keywords << name;
                }
            }
        }
    }

    /// Its text, synopsis, notes, comments, snapshots, and the file of a document of research
    void readFiles(ScrivenerItem& item) const {
        if (item.id.isEmpty()) {
            return;
        }
        const QDir folder(QDir(m_data).filePath(item.id));
        item.synopsis = QString::fromUtf8(contentsOf(folder.filePath(SYNOPSIS))).trimmed();
        if (const QByteArray notes = contentsOf(folder.filePath(NOTES)); !notes.isEmpty()) {
            item.notes = plainTextOf(readRtf(notes));
        }
        RtfReadOptions options;
        options.folder = folder.path();
        options.linked = readComments(folder.filePath(COMMENTS));
        if (const QByteArray content = contentsOf(folder.filePath(CONTENT)); !content.isEmpty()) {
            item.text = readRtf(content, options);
        }
        if (item.kind == ScrivenerItem::Kind::File) {
            const QStringList files = folder.entryList({QStringLiteral("content.*")}, QDir::Files, QDir::Name);
            for (const QString& name : files) {
                if (name != CONTENT && name != COMMENTS) {
                    item.file = folder.filePath(name);
                    break;
                }
            }
        }
        item.snapshots = readSnapshots(item.id, options);
    }

    /// The comments and the footnotes the text links to (scrivcmt://<ID>)
    static QHash<QString, LinkedNote> readComments(const QString& path) {
        QHash<QString, LinkedNote> linked;
        const QByteArray data = contentsOf(path);
        Node root;
        if (data.isEmpty() || !readXml(data, &root)) {
            return linked;
        }
        const std::function<void(const Node&)> collect = [&](const Node& node) {
            const QString id = node.attr(QLatin1String("ID"));
            if (node.name == QLatin1String("Comment") && !id.isEmpty()) {
                LinkedNote note;
                note.footnote = node.attr(QLatin1String("Footnote"))
                                    .compare(QLatin1String("Yes"), Qt::CaseInsensitive) == 0;
                const ImportDocument text = readRtf(node.text.trimmed().toUtf8());
                for (const ImportBlock& block : text.blocks) {
                    if (block.kind == ImportBlock::Kind::Paragraph) {
                        note.paragraphs.push_back(block);
                    }
                }
                linked.insert(id, note);
                return;
            }
            for (const Node& child : node.children) {
                collect(child);
            }
        };
        collect(root);
        return linked;
    }

    /// Its snapshots, from the oldest: those index.xml names, each in the file of its time,
    /// else the files of RTF in the order of their names (of their times)
    std::vector<ScrivenerSnapshot> readSnapshots(const QString& id, const RtfReadOptions& options) const {
        std::vector<ScrivenerSnapshot> snapshots;
        const QDir folder(QDir(m_snapshots).filePath(id + QStringLiteral(".snapshots")));
        if (!folder.exists()) {
            return snapshots;
        }
        QStringList files = folder.entryList({QStringLiteral("*.rtf")}, QDir::Files, QDir::Name);
        Node index;
        const QByteArray data = contentsOf(folder.filePath(QStringLiteral("index.xml")));
        std::vector<std::pair<QString, QString>> entries;  // the title, the time
        if (!data.isEmpty() && readXml(data, &index)) {
            for (const Node& node : index.children) {
                if (node.name == QLatin1String("Snapshot")) {
                    entries.emplace_back(node.value(QLatin1String("Title")),
                                         node.value(QLatin1String("Date")));
                }
            }
        }
        // Each entry in the file of its time, else in the file of its place (when they are as
        // many as the files)
        const bool byPlace = entries.size() == static_cast<size_t>(files.size());
        const QStringList all = files;
        for (size_t i = 0; i < entries.size(); ++i) {
            QString name = snapshotFileName(entries[i].second);
            if (!files.contains(name)) {
                name = byPlace && files.contains(all[static_cast<qsizetype>(i)]) ? all[static_cast<qsizetype>(i)]
                                                                                  : QString();
            }
            if (name.isEmpty()) {
                continue;
            }
            files.removeAll(name);
            ScrivenerSnapshot snapshot;
            snapshot.title = entries[i].first;
            snapshot.created = snapshotTime(entries[i].second);
            snapshot.text = readRtf(contentsOf(folder.filePath(name)), options);
            snapshots.push_back(std::move(snapshot));
        }
        for (const QString& name : files) {
            ScrivenerSnapshot snapshot;
            snapshot.created = snapshotTimeOfFile(name);
            snapshot.text = readRtf(contentsOf(folder.filePath(name)), options);
            snapshots.push_back(std::move(snapshot));
        }
        const auto timeOf = [](const ScrivenerSnapshot& snapshot) {
            return snapshot.created.isValid() ? snapshot.created.toMSecsSinceEpoch()
                                              : std::numeric_limits<qint64>::min();
        };
        std::stable_sort(snapshots.begin(), snapshots.end(),
                         [&timeOf](const ScrivenerSnapshot& a, const ScrivenerSnapshot& b) {
                             return timeOf(a) < timeOf(b);
                         });
        return snapshots;
    }

    QString m_data;
    QString m_snapshots;
    QHash<QString, QString> m_labels;
    QHash<QString, QString> m_statuses;
    QHash<QString, QString> m_keywords;
};

/// The documents of an item, with it
int countItems(const std::vector<ScrivenerItem>& items) {
    int count = 0;
    for (const ScrivenerItem& item : items) {
        count += 1 + countItems(item.children);
    }
    return count;
}

// =============================================================================
// The texts of the draft
// =============================================================================

/// How far a status of the book is: "draft" 0, "revision" 1, "final" 2; -1: none
int progressOf(const QString& status) {
    static const QStringList order{QStringLiteral("draft"), QStringLiteral("revision"),
                                   QStringLiteral("final")};
    return static_cast<int>(order.indexOf(scrivenerBookStatus(status)));
}

/// The builder of the draft's document and of its texts
class DraftBuilder {
public:
    DraftBuilder(const ScrivenerProject& project, const ScrivenerDraftOptions& options)
        : m_options(options) {
        m_draft.document.title = project.title;
        m_draft.document.author = project.author;
    }

    ScrivenerDraft build(const std::vector<ScrivenerItem>& items) {
        addTop(items);
        closeText();
        return std::move(m_draft);
    }

private:
    bool included(const ScrivenerItem& item) const {
        return item.kind != ScrivenerItem::Kind::File && (item.compiled || !m_options.compiledOnly);
    }

    static bool isContainer(const ScrivenerItem& item) { return !item.children.empty(); }

    /// The items at the top of the draft: parts and texts; an item left out gives its place to
    /// its documents
    void addTop(const std::vector<ScrivenerItem>& items) {
        for (const ScrivenerItem& item : items) {
            if (!included(item)) {
                countLeftOut(item);
                addTop(item.children);
            } else if (std::any_of(item.children.begin(), item.children.end(), isContainer)) {
                addPart(item);
            } else {
                addText(item, false);
            }
        }
    }

    void addPart(const ScrivenerItem& item) {
        closeText();
        m_draft.document.blocks.push_back(heading(item.title, PART_LEVEL));
        ScrivenerText part = metadataOf(item);
        part.piece.part = true;
        part.piece.title = item.title;
        part.piece.from = m_draft.document.blocks.size();
        part.piece.to = part.piece.from;
        m_draft.texts.push_back(std::move(part));
        // The part's own text: a text of the part with its title
        if (item.hasText()) {
            ScrivenerText own;
            own.piece.inPart = true;
            own.piece.title = item.title;
            startText(std::move(own));
            appendScene(item.text);
        }
        addInPart(item.children);
    }

    void addInPart(const std::vector<ScrivenerItem>& items) {
        for (const ScrivenerItem& item : items) {
            if (!included(item)) {
                countLeftOut(item);
                addInPart(item.children);
            } else {
                addText(item, true);
            }
        }
    }

    /// A text of an item: its own text, and those of the items in it (its scenes)
    void addText(const ScrivenerItem& item, bool inPart) {
        std::vector<const ScrivenerItem*> scenes;
        collectScenes(item.children, scenes);
        if (item.kind == ScrivenerItem::Kind::Folder && scenes.empty() && !item.hasText()) {
            return;  // an empty folder
        }
        closeText();
        m_draft.document.blocks.push_back(heading(item.title, TEXT_LEVEL));
        ScrivenerText text = metadataOf(item);
        text.piece.inPart = inPart;
        text.piece.title = item.title;
        int progress = progressOf(item.status);
        for (const ScrivenerItem* scene : scenes) {
            text.scenes.push_back({scene->title, scene->synopsis, scene->notes, scene->label,
                                   scene->status, scene->keywords});
            const int sceneProgress = progressOf(scene->status);
            if (item.status.isEmpty() && sceneProgress >= 0 && (progress < 0 || sceneProgress < progress)) {
                progress = sceneProgress;
                text.status = scene->status;
            }
        }
        if (scenes.empty()) {
            text.snapshots = item.snapshots;
        } else {
            m_draft.sceneSnapshots += static_cast<int>(item.snapshots.size());
            for (const ScrivenerItem* scene : scenes) {
                m_draft.sceneSnapshots += static_cast<int>(scene->snapshots.size());
            }
        }
        startText(std::move(text));
        appendScene(item.text);
        for (const ScrivenerItem* scene : scenes) {
            appendScene(scene->text);
        }
    }

    /// The documents in a text, in the order of the binder; those left out give their place
    /// to theirs
    void collectScenes(const std::vector<ScrivenerItem>& items, std::vector<const ScrivenerItem*>& into) {
        for (const ScrivenerItem& item : items) {
            if (included(item)) {
                into.push_back(&item);
            } else {
                countLeftOut(item);
            }
            collectScenes(item.children, into);
        }
    }

    void countLeftOut(const ScrivenerItem& item) {
        if (item.kind != ScrivenerItem::Kind::File) {
            ++m_draft.excluded;
        }
    }

    ScrivenerText metadataOf(const ScrivenerItem& item) const {
        ScrivenerText text;
        text.synopsis = item.synopsis;
        text.notes = item.notes;
        text.label = item.label;
        text.status = item.status;
        text.keywords = item.keywords;
        return text;
    }

    static ImportBlock heading(const QString& title, int level) {
        ImportBlock block;
        block.headingLevel = level;
        ImportRun run;
        run.text = title;
        block.runs.push_back(std::move(run));
        return block;
    }

    /// A text starts after its heading (the text before it is closed)
    void startText(ScrivenerText text) {
        text.piece.from = m_draft.document.blocks.size();
        m_text = std::move(text);
        m_scenes = 0;
    }

    void closeText() {
        if (!m_text) {
            return;
        }
        m_text->piece.to = m_draft.document.blocks.size();
        m_text->piece.words = importWords(m_draft.document, m_text->piece.from, m_text->piece.to);
        m_draft.texts.push_back(std::move(*m_text));
        m_text.reset();
    }

    /// A document of the text, after a scene break when another one is before it; its
    /// headings a level lower than the titles
    void appendScene(const ImportDocument& scene) {
        if (!hasText(scene)) {
            return;
        }
        ImportDocument copy = scene;
        for (ImportBlock& block : copy.blocks) {
            if (const int level = copy.headingLevelOf(block); level > 0) {
                block.headingLevel = std::min(level + TEXT_LEVEL, MAX_HEADING_LEVEL);
            }
        }
        if (m_scenes > 0) {
            ImportBlock sceneBreak;
            sceneBreak.kind = ImportBlock::Kind::SceneBreak;
            m_draft.document.blocks.push_back(std::move(sceneBreak));
        }
        ++m_scenes;
        appendImportDocument(m_draft.document, std::move(copy));
    }

    static bool hasText(const ImportDocument& document) {
        return std::any_of(document.blocks.begin(), document.blocks.end(),
                           [](const ImportBlock& block) { return hasContent(block); });
    }

    const ScrivenerDraftOptions& m_options;
    ScrivenerDraft m_draft;
    std::optional<ScrivenerText> m_text;  ///< The text being written
    int m_scenes = 0;                     ///< The documents in it
};

/// The indexes of a document appended to another: its notes, images, annotations and lists
/// after theirs, its styles' keys as the other's
struct Offsets {
    int notes = 0;
    int images = 0;
    int annotations = 0;
    int lists = 0;
    QHash<QString, QString> styles;
};

void shiftBlocks(std::vector<ImportBlock>& blocks, const Offsets& offsets) {
    for (ImportBlock& block : blocks) {
        if (!block.style.isEmpty()) {
            block.style = offsets.styles.value(block.style, block.style);
        }
        if (block.list >= 0) {
            block.list += offsets.lists;
        }
        for (ImportRun& run : block.runs) {
            if (run.kind == ImportRun::Kind::Note && run.index >= 0) {
                run.index += offsets.notes;
            } else if (run.kind == ImportRun::Kind::Image && run.index >= 0) {
                run.index += offsets.images;
            } else if (run.kind == ImportRun::Kind::Annotation && run.index >= 0) {
                run.index += offsets.annotations;
            }
            for (int& annotation : run.annotations) {
                annotation += offsets.annotations;
            }
        }
        if (block.kind == ImportBlock::Kind::Table && block.table) {
            // A copy of the table: the document it came from may share it
            block.table = std::make_shared<ImportTable>(*block.table);
            for (ImportRow& row : block.table->rows) {
                for (ImportCell& cell : row.cells) {
                    shiftBlocks(cell.blocks, offsets);
                }
            }
        }
    }
}

}  // namespace

bool ScrivenerItem::hasText() const {
    return std::any_of(text.blocks.begin(), text.blocks.end(),
                       [](const ImportBlock& block) { return hasContent(block); });
}

QString scrivenerProjectFile(const QString& path) {
    const QFileInfo info(path);
    if (info.isFile()) {
        return info.suffix().compare(QLatin1String("scrivx"), Qt::CaseInsensitive) == 0
                   ? info.absoluteFilePath()
                   : QString();
    }
    if (!info.isDir()) {
        return QString();
    }
    const QDir folder(info.absoluteFilePath());
    QString own = folder.filePath(info.completeBaseName() + QStringLiteral(".scrivx"));
    if (QFileInfo(own).isFile()) {
        return own;
    }
    const QStringList files = folder.entryList({QStringLiteral("*.scrivx")}, QDir::Files, QDir::Name);
    return files.isEmpty() ? QString() : folder.filePath(files.first());
}

bool readScrivenerProject(const QString& path, ScrivenerProject* project, ImportProblem* problem) {
    *project = ScrivenerProject();
    *problem = ImportProblem();
    const QString file = scrivenerProjectFile(path);
    if (file.isEmpty()) {
        problem->kind = ImportProblem::Kind::NotScrivener;
        problem->detail = QStringLiteral("No .scrivx file");
        return false;
    }
    QFile scrivx(file);
    if (!scrivx.open(QIODevice::ReadOnly)) {
        problem->kind = ImportProblem::Kind::CannotOpen;
        problem->detail = scrivx.errorString();
        return false;
    }
    Node root;
    const Node* binder = nullptr;
    if (readXml(scrivx.readAll(), &root) && root.name == QLatin1String("ScrivenerProject")) {
        binder = root.child(QLatin1String("Binder"));
    }
    if (!binder) {
        problem->kind = ImportProblem::Kind::Damaged;
        problem->detail = QStringLiteral("The .scrivx file is not a binder of Scrivener");
        return false;
    }
    const QFileInfo info(file);
    project->title = QFileInfo(info.absolutePath()).completeBaseName();
    if (const Node* properties = root.child(QLatin1String("ProjectProperties"))) {
        if (const QString title = properties->value(QLatin1String("ProjectTitle")); !title.isEmpty()) {
            project->title = title;
        }
        project->author = properties->value(QLatin1String("FullName"));
    }
    if (!info.absolutePath().endsWith(QLatin1String(".scriv"), Qt::CaseInsensitive) &&
        project->title == QFileInfo(info.absolutePath()).completeBaseName()) {
        project->title = info.completeBaseName();  // a .scrivx file outside its folder
    }
    const ProjectReader reader(info.absolutePath(), root);
    std::vector<ScrivenerItem> others;
    for (const Node& node : binder->children) {
        if (node.name != QLatin1String("BinderItem")) {
            continue;
        }
        ScrivenerItem item = reader.readItem(node, 0);
        if (item.type == QLatin1String("DraftFolder")) {
            project->draftName = item.title;
            project->draft = std::move(item.children);
        } else if (item.type == QLatin1String("ResearchFolder")) {
            for (ScrivenerItem& child : item.children) {
                project->research.push_back(std::move(child));
            }
        } else if (item.type == QLatin1String("TrashFolder")) {
            project->trash = countItems(item.children);
        } else {
            others.push_back(std::move(item));  // Characters, Places, Notes...
        }
    }
    for (ScrivenerItem& item : others) {
        project->research.push_back(std::move(item));
    }
    return true;
}

QString scrivenerBookStatus(const QString& status) {
    static const QHash<QString, QString> statuses{
        {QStringLiteral("to do"), QStringLiteral("draft")},
        {QStringLiteral("todo"), QStringLiteral("draft")},
        {QStringLiteral("first draft"), QStringLiteral("draft")},
        {QStringLiteral("draft"), QStringLiteral("draft")},
        {QStringLiteral("revised draft"), QStringLiteral("revision")},
        {QStringLiteral("revised"), QStringLiteral("revision")},
        {QStringLiteral("revision"), QStringLiteral("revision")},
        {QStringLiteral("second draft"), QStringLiteral("revision")},
        {QStringLiteral("final draft"), QStringLiteral("final")},
        {QStringLiteral("final"), QStringLiteral("final")},
        {QStringLiteral("done"), QStringLiteral("final")},
    };
    return statuses.value(status.simplified().toLower());
}

ScrivenerDraft scrivenerDraft(const ScrivenerProject& project, const ScrivenerDraftOptions& options) {
    DraftBuilder builder(project, options);
    return builder.build(project.draft);
}

void appendImportDocument(ImportDocument& into, ImportDocument from) {
    Offsets offsets;
    offsets.notes = static_cast<int>(into.notes.size());
    offsets.images = static_cast<int>(into.images.size());
    offsets.annotations = static_cast<int>(into.annotations.size());
    offsets.lists = static_cast<int>(into.lists.size());
    // The styles: one of the same key and name is the same; another of the same key gets one
    // of its own
    QSet<QString> keys;
    for (const ImportStyle& style : into.styles) {
        keys.insert(style.key);
    }
    std::vector<ImportStyle> added;
    for (const ImportStyle& style : from.styles) {
        const auto same = std::find_if(into.styles.begin(), into.styles.end(), [&style](const ImportStyle& own) {
            return own.key == style.key && own.name == style.name;
        });
        if (same != into.styles.end()) {
            same->paragraphs += style.paragraphs;
            continue;
        }
        QString key = style.key;
        for (int n = 2; keys.contains(key); ++n) {
            key = QStringLiteral("%1_%2").arg(style.key).arg(n);
        }
        keys.insert(key);
        offsets.styles.insert(style.key, key);
        added.push_back(style);
        added.back().key = key;
    }
    for (ImportStyle& style : added) {
        style.basedOn = offsets.styles.value(style.basedOn, style.basedOn);
        style.next = offsets.styles.value(style.next, style.next);
        into.styles.push_back(std::move(style));
    }
    shiftBlocks(from.blocks, offsets);
    for (ImportNote& note : from.notes) {
        shiftBlocks(note.paragraphs, offsets);
    }
    std::move(from.blocks.begin(), from.blocks.end(), std::back_inserter(into.blocks));
    std::move(from.notes.begin(), from.notes.end(), std::back_inserter(into.notes));
    std::move(from.images.begin(), from.images.end(), std::back_inserter(into.images));
    std::move(from.annotations.begin(), from.annotations.end(), std::back_inserter(into.annotations));
    std::move(from.lists.begin(), from.lists.end(), std::back_inserter(into.lists));
    into.trackedChanges += from.trackedChanges;
    for (const auto& [omission, count] : from.omissions) {
        into.omissions[omission] += count;
    }
}

}  // namespace kalahari::editor
