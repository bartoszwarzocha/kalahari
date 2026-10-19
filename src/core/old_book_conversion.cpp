/// @file old_book_conversion.cpp
/// @brief A book of the first .klh format, converted to the book project of this version

#include <kalahari/core/old_book_conversion.h>
#include <kalahari/core/chapter_document.h>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QUuid>
#include <algorithm>
#include <array>
#include <functional>

namespace kalahari::core {

namespace {

/// Type of a converted book: the first format had a novel's parts and chapters
constexpr const char* NOVEL = "kalahari.novel";

/// Names of the sections that the first format showed: Front Matter, Body, Back Matter
constexpr const char* MATTER_SECTIONS = "matter";

/// Extension of the chapter files
constexpr const char* CHAPTER_EXTENSION = ".kchapter";

/// The places of the elements of a book, in reading order
constexpr std::array BOOK_PLACES{BookPlace::Front, BookPlace::Main, BookPlace::Back};

/// A type of the first format whose kind has another id, or is not of a novel's packages
struct OldType {
    const char* type;     ///< Type of the first format
    const char* package;  ///< Package of the kind
    const char* kind;     ///< Id of the kind
};

constexpr std::array OLD_TYPES{
    OldType{"epigraph", "kalahari.base", "motto"},
    OldType{"foreword", "kalahari.base", "preface"},
    OldType{"appendix", "kalahari.nonfiction", "annex"},
};

/// Status of the element whose chapter file is @p file: the status in the file, or a draft
QString statusOf(const QString& projectDir, const QString& file) {
    const QString path = QDir(projectDir).filePath(file);
    if (!file.isEmpty() && QFileInfo(path).isFile()) {
        if (const std::optional<ChapterDocument> chapter = ChapterDocument::load(path)) {
            const QString status = chapter->status();
            if (status == QLatin1String("draft") || status == QLatin1String("revision") ||
                status == QLatin1String("final")) {
                return status;
            }
        }
    }
    return QStringLiteral("draft");
}

/// The chapter file @p file of an element of the first format, relative to @p projectDir with
/// "/" between folders; empty, with a problem of @p field, when it is outside the folder
///
/// The first format read the file with the extension .kchapter, as
/// std::filesystem::path::replace_extension() gave it: "chapter_003" and "chapter_003.txt"
/// were "chapter_003.kchapter", and ".kchapter" was ".kchapter.kchapter".
QString chapterFile(const QString& file, const QString& projectDir, const QString& field,
                    QStringList& problems) {
    QString path = file;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (QDir::isAbsolutePath(path)) {
        path = QDir(projectDir).relativeFilePath(path);
    }
    path = QDir::cleanPath(path);

    // The extension is the name from its last dot, unless that dot starts the name
    const qsizetype nameStart = path.lastIndexOf(QLatin1Char('/')) + 1;
    const qsizetype dot = path.lastIndexOf(QLatin1Char('.'));
    const qsizetype extensionStart = dot > nameStart ? dot : path.size();
    if (QStringView(path).mid(extensionStart) != QLatin1String(CHAPTER_EXTENSION)) {
        path = path.left(extensionStart) + QLatin1String(CHAPTER_EXTENSION);
    }

    if (path == QLatin1String("..") || path.startsWith(QLatin1String("../")) ||
        QDir::isAbsolutePath(path) || path.contains(QLatin1Char(':'))) {
        problems.append(QStringLiteral("%1: the file %2 is outside the folder of the book")
                            .arg(field, file));
        return {};
    }
    return path;
}

/// Builds the converted book, element by element in the order of the old file
class Converter {
public:
    Converter(const QString& projectDir, const BookTypeRegistry& registry,
              const BookTypePackage& novel, OldBookConversion& result)
        : m_projectDir(projectDir), m_registry(registry), m_novel(novel), m_result(result),
          m_partKind(registry.findKind(novel.id, QStringLiteral("part"))),
          m_chapterKind(registry.findKind(novel.id, QStringLiteral("chapter"))) {}

    /// The project of @p json, the .klh file of a book of the first format, before the
    /// checks of this version; problems go to the result
    BookProject convert(const QJsonObject& json) {
        if (!m_partKind || !m_chapterKind) {
            m_result.problems.append(QStringLiteral("%1: the book type is not installed")
                                         .arg(m_novel.id));
            return {};
        }

        // The fields of the file the conversion does not use stay in the project
        QJsonObject rest = json;
        rest.remove(QLatin1String("format"));
        QJsonObject document = object(rest, "document", QString());
        QJsonObject structure = object(rest, "structure", QString());

        m_project.type = ProjectType{m_novel.id, m_novel.version, {}};
        m_project.id = text(document, "id", QStringLiteral("document"));
        if (m_project.id.isEmpty()) {
            m_project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        m_project.created = dateTime(document, "created");
        m_project.modified = dateTime(document, "modified");

        // The data of the book; the fields of "document" the conversion does not use stay in
        // the book
        ProjectBook newBook;
        newBook.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newBook.title = text(document, "title", QStringLiteral("document"));
        newBook.author = text(document, "author", QStringLiteral("document"));
        newBook.language =
            text(document, "language", QStringLiteral("document"), QStringLiteral("en"));
        newBook.genre = text(document, "genre", QStringLiteral("document"));
        newBook.partsLayer = true;
        newBook.sectionSet = QString::fromLatin1(MATTER_SECTIONS);
        newBook.folder = QString::fromLatin1(BookProject::BOOK_FOLDER);
        newBook.extra = document;
        m_project.books.append(newBook);

        // The elements in the order of the old file, whose matter types defaulted to a preface
        // and an epilogue
        const QJsonArray front = array(structure, "frontmatter", QStringLiteral("structure"));
        const QJsonArray body = array(structure, "body", QStringLiteral("structure"));
        const QJsonArray back = array(structure, "backmatter", QStringLiteral("structure"));
        for (qsizetype i = 0; i < front.size(); ++i) {
            placeElement(front.at(i), BookPlace::Front, QStringLiteral("preface"),
                         QStringLiteral("structure.frontmatter[%1]").arg(i));
        }
        for (qsizetype i = 0; i < body.size(); ++i) {
            placePart(body.at(i), QStringLiteral("structure.body[%1]").arg(i));
        }
        for (qsizetype i = 0; i < back.size(); ++i) {
            placeElement(back.at(i), BookPlace::Back, QStringLiteral("epilogue"),
                         QStringLiteral("structure.backmatter[%1]").arg(i));
        }

        if (!structure.isEmpty()) {
            rest.insert(QLatin1String("structure"), structure);
        }
        m_project.extra = rest;
        giveIds();
        return m_project;
    }

private:
    ProjectBook& book() { return m_project.books.first(); }

    void problem(const QString& field, const char* key, const QString& message) {
        const QString path = field.isEmpty() ? QString::fromLatin1(key)
                                             : field + QLatin1Char('.') + QLatin1String(key);
        m_result.problems.append(QStringLiteral("%1: %2").arg(path, message));
    }

    /// The text of @p key, taken out of @p fields; @p fallback when there is none
    QString text(QJsonObject& fields, const char* key, const QString& field,
                 const QString& fallback = QString()) {
        const QJsonValue value = fields.take(QLatin1String(key));
        if (value.isUndefined() || value.isNull()) {
            return fallback;
        }
        if (!value.isString()) {
            problem(field, key, QStringLiteral("must be a text"));
            return fallback;
        }
        return value.toString();
    }

    /// The date and time of @p key in "document", taken out of it; invalid when there is none
    QDateTime dateTime(QJsonObject& document, const char* key) {
        const QString value = text(document, key, QStringLiteral("document"));
        if (value.isEmpty()) {
            return {};
        }
        const QDateTime time = QDateTime::fromString(value, Qt::ISODate);
        if (!time.isValid()) {
            problem(QStringLiteral("document"), key,
                    QStringLiteral("must be a date and time such as \"2026-10-09T10:00:00Z\""));
        }
        return time;
    }

    /// The object of @p key, taken out of @p fields; empty when there is none
    QJsonObject object(QJsonObject& fields, const char* key, const QString& field) {
        const QJsonValue value = fields.take(QLatin1String(key));
        if (!value.isUndefined() && !value.isObject()) {
            problem(field, key, QStringLiteral("must be an object"));
        }
        return value.toObject();
    }

    /// The list of @p key, taken out of @p fields; empty when there is none
    QJsonArray array(QJsonObject& fields, const char* key, const QString& field) {
        const QJsonValue value = fields.take(QLatin1String(key));
        if (!value.isUndefined() && !value.isArray()) {
            problem(field, key, QStringLiteral("must be a list"));
        }
        return value.toArray();
    }

    /// The chapter file of an element, taken out of @p fields; empty when it has none
    QString file(QJsonObject& fields, const QString& field) {
        const QString path = text(fields, "file", field);
        return path.isEmpty() ? QString()
                              : chapterFile(path, m_projectDir, field + QStringLiteral(".file"),
                                            m_result.problems);
    }

    /// The kind of a type of the first format: a text kind of a novel's packages, or of the
    /// package that OLD_TYPES names; none for another type
    KindRef kindOf(const QString& type) const {
        KindRef kind;
        const auto old = std::find_if(OLD_TYPES.cbegin(), OLD_TYPES.cend(),
                                      [&type](const OldType& known) {
                                          return type == QLatin1String(known.type);
                                      });
        if (old != OLD_TYPES.cend()) {
            kind = m_registry.findKind(QString::fromLatin1(old->package),
                                       QString::fromLatin1(old->kind));
        } else if (!type.isEmpty() && !type.contains(QLatin1Char(':'))) {
            kind = m_registry.findKind(m_novel.id, type);
        }
        return kind && kind.kind->form == ElementForm::Text ? kind : KindRef{};
    }

    /// Where an element of @p kind that was in @p oldPlace goes: there, else to a place where
    /// the kind is offered (the main part for a prologue, an epilogue or a chapter); none for a
    /// kind of the Workshop
    static std::optional<BookPlace> placeOf(const KindRef& kind, BookPlace oldPlace) {
        if (kind.offeredIn(oldPlace)) {
            return oldPlace;
        }
        for (const BookPlace place : {BookPlace::Main, BookPlace::Front, BookPlace::Back}) {
            if (kind.offeredIn(place)) {
                return place;
            }
        }
        return std::nullopt;
    }

    /// Whether the book has as many elements of @p kind as a book can have
    bool limitReached(const KindRef& kind) {
        if (kind.kind->limit <= 0) {
            return false;
        }
        const KindReference reference{kind.package->id, kind.kind->id};
        qsizetype count = 0;
        const std::function<void(const QList<ProjectElement>&)> countIn =
            [&reference, &count, &countIn](const QList<ProjectElement>& elements) {
                for (const ProjectElement& element : elements) {
                    count += element.kind == reference ? 1 : 0;
                    countIn(element.elements);
                }
            };
        for (const BookPlace place : BOOK_PLACES) {
            countIn(m_project.elementsIn(place));
        }
        return count >= kind.kind->limit;
    }

    /// A kind that the type does not offer in @p place is taken into the project
    void takeKind(const KindRef& kind, BookPlace place) {
        const QList<KindRef> offered = m_registry.kindsIn(m_novel.id, place);
        const KindReference reference{kind.package->id, kind.kind->id};
        if (std::none_of(offered.cbegin(), offered.cend(),
                         [&kind](const KindRef& other) { return other.kind == kind.kind; }) &&
            !m_project.kinds.contains(reference)) {
            m_project.kinds.append(reference);
        }
    }

    /// An element of the front or back matter, into a place of its kind or among the files
    /// of the book outside its text
    void placeElement(const QJsonValue& value, BookPlace oldPlace, const QString& defaultType,
                      const QString& field) {
        if (!value.isObject()) {
            m_result.problems.append(QStringLiteral("%1: must be an object").arg(field));
            return;
        }
        QJsonObject fields = value.toObject();
        ProjectElement element;
        element.id = text(fields, "id", field);
        element.title = text(fields, "title", field);
        element.file = file(fields, field);
        const QJsonValue type = fields.take(QLatin1String("type"));

        const KindRef kind = kindOf(type.isString() ? type.toString() : defaultType);
        const std::optional<BookPlace> place = kind ? placeOf(kind, oldPlace) : std::nullopt;
        if (place && !limitReached(kind)) {
            element.kind = KindReference{kind.package->id, kind.kind->id};
            element.status = statusOf(m_projectDir, element.file);
            element.extra = fields;
            takeKind(kind, *place);
            QList<ProjectElement>& list = m_project.elementsIn(*place);
            list.insert(BookProject::newIndexIn(m_registry, list, kind), element);
            return;
        }

        // Among the files of the book outside its text, with the type it had
        if (!type.isUndefined()) {
            fields.insert(QLatin1String("type"), type);
        }
        element.extra = fields;
        if (element.file.isEmpty()) {
            m_result.problems.append(
                QStringLiteral("%1: an element without a file whose type has no kind in a "
                               "novel, so it can be neither in the book nor among its files")
                    .arg(field));
            return;
        }
        book().files.append(element);
        m_result.otherFiles.append(element.title);
    }

    /// A part of the body with its chapters, into the main part
    void placePart(const QJsonValue& value, const QString& field) {
        if (!value.isObject()) {
            m_result.problems.append(QStringLiteral("%1: must be an object").arg(field));
            return;
        }
        QJsonObject fields = value.toObject();
        ProjectElement part;
        part.id = text(fields, "id", field);
        part.title = text(fields, "title", field);
        part.kind = KindReference{m_partKind.package->id, m_partKind.kind->id};

        const QJsonArray chapters = array(fields, "chapters", field);
        for (qsizetype i = 0; i < chapters.size(); ++i) {
            const QString chapterField = QStringLiteral("%1.chapters[%2]").arg(field).arg(i);
            if (!chapters.at(i).isObject()) {
                m_result.problems.append(QStringLiteral("%1: must be an object").arg(chapterField));
                continue;
            }
            QJsonObject chapterFields = chapters.at(i).toObject();
            ProjectElement chapter;
            chapter.id = text(chapterFields, "id", chapterField);
            chapter.title = text(chapterFields, "title", chapterField);
            chapter.file = file(chapterFields, chapterField);
            chapter.kind = KindReference{m_chapterKind.package->id, m_chapterKind.kind->id};
            chapter.status = statusOf(m_projectDir, chapter.file);
            chapter.extra = chapterFields;
            part.elements.append(chapter);
        }
        part.extra = fields;

        QList<ProjectElement>& main = m_project.elementsIn(BookPlace::Main);
        main.insert(BookProject::newIndexIn(m_registry, main, m_partKind), part);
    }

    /// Ids for the elements without one, which the first format did not need
    void giveIds() {
        const std::function<void(QList<ProjectElement>&)> give =
            [this, &give](QList<ProjectElement>& elements) {
                for (ProjectElement& element : elements) {
                    if (element.id.isEmpty()) {
                        element.id = m_project.newElementId();
                    }
                    give(element.elements);
                }
            };
        for (const BookPlace place : BOOK_PLACES) {
            give(m_project.elementsIn(place));
        }
        give(book().files);
    }

    const QString& m_projectDir;
    const BookTypeRegistry& m_registry;
    const BookTypePackage& m_novel;
    OldBookConversion& m_result;
    const KindRef m_partKind;
    const KindRef m_chapterKind;
    BookProject m_project;
};

}  // namespace

OldBookConversion OldBookConversion::convert(const QJsonObject& json, const QString& projectDir,
                                             const BookTypeRegistry& registry) {
    OldBookConversion result;
    const BookTypePackage* novel = registry.package(QString::fromLatin1(NOVEL));
    if (!novel) {
        result.problems.append(QStringLiteral("%1: the book type is not installed")
                                   .arg(QString::fromLatin1(NOVEL)));
        return result;
    }
    if (!json.value(QLatin1String("document")).isObject()) {
        result.problems.append(QStringLiteral("document: must be the data of the book, which "
                                              "a book of the first format has"));
        return result;
    }

    Converter converter(projectDir, registry, *novel, result);
    const BookProject project = converter.convert(json);

    // The same checks as for every .klh file: e.g. two elements with one id or one file
    QStringList found;
    std::optional<BookProject> read;
    if (result.problems.isEmpty()) {
        read = BookProject::fromJson(project.toJson(), found);
        result.problems.append(found);
    }
    if (!read) {
        result.otherFiles.clear();
        return result;
    }
    result.project = std::move(read);
    return result;
}

QString OldBookConversion::copyPathFor(const QString& manifestPath) {
    const QFileInfo manifest(manifestPath);
    const QString base =
        QDir(manifest.absolutePath()).filePath(manifest.completeBaseName() +
                                               QStringLiteral(".format1"));
    const QString extension = QStringLiteral(".klh.bak");
    QString path = base + extension;
    for (int number = 2; QFileInfo::exists(path); ++number) {
        path = base + QStringLiteral("-%1").arg(number) + extension;
    }
    return path;
}

}  // namespace kalahari::core
