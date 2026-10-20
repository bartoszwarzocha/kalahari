/// @file book_project.cpp
/// @brief Book project as its .klh file saves it

#include <kalahari/core/book_project.h>
#include <kalahari/core/logger.h>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace kalahari::core {

namespace {

/// Parts of a book in reading order, with the lists of their elements
struct BookPart {
    BookPlace place;                                ///< The part
    const char* key;                                ///< Key in the .klh file
    QList<ProjectElement> ProjectBook::*elements;  ///< Elements of the part
};

const BookPart BOOK_PARTS[] = {
    {BookPlace::Front, "front", &ProjectBook::frontElements},
    {BookPlace::Main, "main", &ProjectBook::mainElements},
    {BookPlace::Back, "back", &ProjectBook::backElements},
};

const BookPart& bookPart(BookPlace place) {
    const auto* found = std::find_if(std::begin(BOOK_PARTS), std::end(BOOK_PARTS),
                                     [place](const BookPart& part) { return part.place == place; });
    Q_ASSERT(found != std::end(BOOK_PARTS));
    return *found;
}

void addProblem(QStringList& problems, const QString& field, const QString& message) {
    problems << field + QStringLiteral(": ") + message;
}

bool isWholeNumber(const QJsonValue& value) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

/// A path inside the project folder, with "/" between folders: "book/chapter_001.kchapter"
bool isPathInside(const QString& path) {
    if (path.isEmpty() || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char(':'))) {
        return false;
    }
    const QStringList segments = path.split(QLatin1Char('/'));
    return std::none_of(segments.cbegin(), segments.cend(), [](const QString& segment) {
        return segment.isEmpty() || segment == QLatin1String(".") ||
               segment == QLatin1String("..");
    });
}

/// A kind with its package: "kalahari.base:chapter"
std::optional<KindReference> readKindReference(const QJsonValue& value) {
    if (!value.isString()) {
        return std::nullopt;
    }
    std::optional<KindReference> reference = KindReference::parse(value.toString());
    if (!reference || reference->packageId.isEmpty()) {
        return std::nullopt;
    }
    return reference;
}

QString kindReferenceProblem() {
    return QStringLiteral("must be a kind with its package, e.g. \"kalahari.base:chapter\"");
}

/// Fields of one JSON object; the fields that nothing takes are the extra fields
class Fields {
public:
    Fields(const QJsonObject& object, QString field, QStringList& problems)
        : m_rest(object), m_field(std::move(field)), m_problems(problems) {}

    /// Field of @p key: "books[0].title"
    QString path(const char* key) const {
        return m_field.isEmpty() ? QString::fromLatin1(key)
                                 : m_field + QLatin1Char('.') + QLatin1String(key);
    }

    void problem(const char* key, const QString& message) {
        addProblem(m_problems, path(key), message);
    }

    /// Value of @p key; undefined when the object does not have it
    QJsonValue take(const char* key) { return m_rest.take(QLatin1String(key)); }

    /// String of @p key; empty when the object does not have it
    QString string(const char* key, bool required = false) {
        const QJsonValue value = take(key);
        if (value.isUndefined() && !required) {
            return {};
        }
        if (!value.isString() || (required && value.toString().isEmpty())) {
            problem(key, required ? QStringLiteral("must be a text that is not empty")
                                  : QStringLiteral("must be a text"));
            return {};
        }
        return value.toString();
    }

    bool boolean(const char* key, bool fallback) {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return fallback;
        }
        if (!value.isBool()) {
            problem(key, QStringLiteral("must be true or false"));
            return fallback;
        }
        return value.toBool();
    }

    /// Date and time in ISO 8601; invalid when the object does not have it
    QDateTime dateTime(const char* key) {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return {};
        }
        QDateTime result = QDateTime::fromString(value.toString(), Qt::ISODate);
        if (!value.isString() || !result.isValid()) {
            problem(key, QStringLiteral("must be a date and time such as "
                                        "\"2026-10-09T10:00:00Z\""));
            return {};
        }
        return result;
    }

    /// List of @p key; empty when the object does not have it
    QJsonArray array(const char* key) {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return {};
        }
        if (!value.isArray()) {
            problem(key, QStringLiteral("must be a list"));
            return {};
        }
        return value.toArray();
    }

    /// Fields that nothing took
    const QJsonObject& rest() const { return m_rest; }

private:
    QJsonObject m_rest;
    QString m_field;
    QStringList& m_problems;
};

/// Ids and files of the elements read so far, each with the field that has it
struct Seen {
    QHash<QString, QString> ids;
    QHash<QString, QString> files;
};

QList<ProjectElement> readElements(const QJsonArray& array, const QString& field, Seen& seen,
                                   QStringList& problems);

/// The id of the element of @p field, unique in the project
QString readId(Fields& fields, const QString& field, Seen& seen) {
    const QString id = fields.string("id", true);
    if (!id.isEmpty()) {
        const QString first = seen.ids.value(id);
        if (first.isEmpty()) {
            seen.ids.insert(id, field);
        } else {
            fields.problem("id", QStringLiteral("'%1' is already the id of %2").arg(id, first));
        }
    }
    return id;
}

/// The file of the element of @p field, inside the project folder and of no other element;
/// empty when it has none
QString readFile(Fields& fields, const QString& field, Seen& seen, bool required) {
    const QJsonValue value = fields.take("file");
    if (value.isUndefined() && !required) {
        return {};
    }
    if (!value.isString() || !isPathInside(value.toString())) {
        fields.problem("file", QStringLiteral("must be a path inside the project folder, "
                                              "e.g. \"book/chapter_001.kchapter\""));
        return {};
    }
    const QString file = value.toString();
    const QString first = seen.files.value(file);
    if (first.isEmpty()) {
        seen.files.insert(file, field);
    } else {
        fields.problem("file", QStringLiteral("'%1' is already the file of %2").arg(file, first));
    }
    return file;
}

/// The writer's own fields of an element: a name and a value each
QList<ElementField> readElementFields(const QJsonArray& array, const QString& field,
                                      QStringList& problems) {
    QList<ElementField> result;
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(field).arg(i);
        if (!array.at(i).isObject()) {
            addProblem(problems, path, QStringLiteral("must be an object with a name and a value"));
            continue;
        }
        Fields entry(array.at(i).toObject(), path, problems);
        ElementField each;
        each.name = entry.string("name");
        each.value = entry.string("value");
        each.extra = entry.rest();
        result << each;
    }
    return result;
}

/// What an element is about: ids of elements and of sections, each once
QStringList readLinks(Fields& fields) {
    QStringList links;
    for (const QJsonValue& value : fields.array("links")) {
        if (!value.isString() || value.toString().isEmpty()) {
            fields.problem("links", QStringLiteral("must be a list of ids of elements and "
                                                   "sections"));
            return {};
        }
        if (!links.contains(value.toString())) {
            links << value.toString();
        }
    }
    return links;
}

/// How the picture of an element is shown: its turn, with the fields of "view" this version
/// does not know
void readView(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue view = fields.take("view");
    if (view.isUndefined()) {
        return;
    }
    if (!view.isObject()) {
        fields.problem("view", QStringLiteral("must be an object"));
        return;
    }
    Fields viewFields(view.toObject(), fields.path("view"), problems);
    const QJsonValue rotation = viewFields.take("rotation");
    if (!rotation.isUndefined()) {
        const int degrees = isWholeNumber(rotation) ? rotation.toInt(-1) : -1;
        if (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270) {
            viewFields.problem("rotation", QStringLiteral("must be 0, 90, 180 or 270"));
        } else {
            element.rotation = degrees;
        }
    }
    element.viewExtra = viewFields.rest();
}

ProjectElement readElement(const QJsonValue& value, const QString& field, Seen& seen,
                           QStringList& problems) {
    ProjectElement element;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return element;
    }
    Fields fields(value.toObject(), field, problems);

    element.id = readId(fields, field, seen);

    const QJsonValue kind = fields.take("kind");
    if (const std::optional<KindReference> reference = readKindReference(kind)) {
        element.kind = *reference;
    } else {
        fields.problem("kind", kindReferenceProblem());
    }

    element.title = fields.string("title");
    element.file = readFile(fields, field, seen, false);
    element.status = fields.string("status");
    element.elements = readElements(fields.array("elements"), fields.path("elements"), seen,
                                    problems);
    element.description = fields.string("description");
    element.fields = readElementFields(fields.array("fields"), fields.path("fields"), problems);
    element.links = readLinks(fields);
    readView(fields, element, problems);
    element.extra = fields.rest();
    return element;
}

/// A file of a book outside its text: an id, a title and a file, without a kind and a status
ProjectElement readBookFile(const QJsonValue& value, const QString& field, Seen& seen,
                            QStringList& problems) {
    ProjectElement file;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return file;
    }
    Fields fields(value.toObject(), field, problems);
    file.id = readId(fields, field, seen);
    file.title = fields.string("title");
    file.file = readFile(fields, field, seen, true);
    file.extra = fields.rest();
    return file;
}

QList<ProjectElement> readElements(const QJsonArray& array, const QString& field, Seen& seen,
                                   QStringList& problems) {
    QList<ProjectElement> elements;
    for (qsizetype i = 0; i < array.size(); ++i) {
        elements << readElement(array.at(i), QStringLiteral("%1[%2]").arg(field).arg(i), seen,
                                problems);
    }
    return elements;
}

ProjectBook readBook(const QJsonValue& value, const QString& field, Seen& seen,
                     QStringList& problems) {
    ProjectBook book;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return book;
    }
    Fields fields(value.toObject(), field, problems);

    book.id = fields.string("id", true);
    book.title = fields.string("title");
    book.author = fields.string("author");
    book.language = fields.string("language");
    book.genre = fields.string("genre");
    book.name = fields.string("name");
    book.partsLayer = fields.boolean("partsLayer", true);

    // The id of a set of names, or the writer's own names of the three parts
    const QJsonValue sections = fields.take("sections");
    if (sections.isString() &&
        sections.toString() != QLatin1String(ProjectBook::CUSTOM_SECTIONS)) {
        book.sectionSet = sections.toString();
    } else if (sections.isObject()) {
        Fields names(sections.toObject(), fields.path("sections"), problems);
        book.sectionSet = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);
        for (const BookPart& part : BOOK_PARTS) {
            book.sectionNames << names.string(part.key, true);
        }
        book.sectionNamesExtra = names.rest();
    } else if (!sections.isUndefined()) {
        fields.problem("sections", QStringLiteral("must be the id of a set of names, e.g. "
                                                  "\"matter\", or the names of the front, "
                                                  "main and back part"));
    }

    const QJsonValue folder = fields.take("folder");
    if (!folder.isString() || !isPathInside(folder.toString())) {
        fields.problem("folder", QStringLiteral("must be a folder inside the project folder, "
                                                "e.g. \"book\""));
    } else {
        book.folder = folder.toString();
    }

    for (const BookPart& part : BOOK_PARTS) {
        book.*part.elements = readElements(fields.array(part.key), fields.path(part.key), seen,
                                           problems);
    }
    const QJsonArray files = fields.array("files");
    for (qsizetype i = 0; i < files.size(); ++i) {
        book.files << readBookFile(files.at(i),
                                   QStringLiteral("%1[%2]").arg(fields.path("files")).arg(i),
                                   seen, problems);
    }
    book.extra = fields.rest();
    return book;
}

ProjectWorkshop readWorkshop(const QJsonValue& value, Seen& seen, QStringList& problems) {
    ProjectWorkshop workshop;
    const QString field = QStringLiteral("workshop");
    if (value.isUndefined()) {
        return workshop;
    }
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return workshop;
    }
    Fields fields(value.toObject(), field, problems);

    workshop.name = fields.string("name");
    workshop.grouped = fields.boolean("grouped", false);
    workshop.elements = readElements(fields.array("elements"), fields.path("elements"), seen,
                                     problems);
    workshop.extra = fields.rest();
    return workshop;
}

std::optional<ProjectType> readType(const QJsonValue& value, QStringList& problems) {
    const QString field = QStringLiteral("type");
    if (!value.isObject()) {
        addProblem(problems, field,
                   QStringLiteral("must be an object with the id and version of the type"));
        return std::nullopt;
    }
    Fields fields(value.toObject(), field, problems);

    ProjectType type;
    type.id = fields.string("id", true);
    if (!type.id.isEmpty() && !BookTypePackage::isId(type.id)) {
        fields.problem("id", QStringLiteral("must be the id of a package, e.g. "
                                            "\"kalahari.novel\""));
    }
    type.version = fields.string("version");
    type.extra = fields.rest();
    return type;
}

// -----------------------------------------------------------------------------
// Writing
// -----------------------------------------------------------------------------

/// Set @p key to @p text, or take it out when @p text is empty
void setOrRemove(QJsonObject& json, const char* key, const QString& text) {
    if (text.isEmpty()) {
        json.remove(QLatin1String(key));
    } else {
        json.insert(QLatin1String(key), text);
    }
}

QString dateTimeText(const QDateTime& time) {
    return time.isValid() ? time.toUTC().toString(Qt::ISODate) : QString();
}

QJsonArray elementsToJson(const QList<ProjectElement>& elements);

QJsonObject elementToJson(const ProjectElement& element) {
    QJsonObject json = element.extra;
    json.insert(QLatin1String("id"), element.id);
    json.insert(QLatin1String("kind"), element.kind.toString());
    json.insert(QLatin1String("title"), element.title);
    setOrRemove(json, "file", element.file);
    setOrRemove(json, "status", element.status);
    if (element.elements.isEmpty()) {
        json.remove(QLatin1String("elements"));
    } else {
        json.insert(QLatin1String("elements"), elementsToJson(element.elements));
    }
    setOrRemove(json, "description", element.description);
    if (element.fields.isEmpty()) {
        json.remove(QLatin1String("fields"));
    } else {
        QJsonArray fields;
        for (const ElementField& field : element.fields) {
            QJsonObject each = field.extra;
            each.insert(QLatin1String("name"), field.name);
            each.insert(QLatin1String("value"), field.value);
            fields.append(each);
        }
        json.insert(QLatin1String("fields"), fields);
    }
    if (element.links.isEmpty()) {
        json.remove(QLatin1String("links"));
    } else {
        json.insert(QLatin1String("links"), QJsonArray::fromStringList(element.links));
    }
    QJsonObject view = element.viewExtra;
    if (element.rotation != 0) {
        view.insert(QLatin1String("rotation"), element.rotation);
    }
    if (view.isEmpty()) {
        json.remove(QLatin1String("view"));
    } else {
        json.insert(QLatin1String("view"), view);
    }
    return json;
}

QJsonArray elementsToJson(const QList<ProjectElement>& elements) {
    QJsonArray array;
    for (const ProjectElement& element : elements) {
        array.append(elementToJson(element));
    }
    return array;
}

QJsonObject bookToJson(const ProjectBook& book) {
    QJsonObject json = book.extra;
    json.insert(QLatin1String("id"), book.id);
    json.insert(QLatin1String("title"), book.title);
    json.insert(QLatin1String("author"), book.author);
    json.insert(QLatin1String("language"), book.language);
    json.insert(QLatin1String("genre"), book.genre);
    setOrRemove(json, "name", book.name);
    json.insert(QLatin1String("partsLayer"), book.partsLayer);
    if (book.sectionSet == QLatin1String(ProjectBook::CUSTOM_SECTIONS)) {
        QJsonObject names = book.sectionNamesExtra;
        for (qsizetype i = 0; i < std::ssize(BOOK_PARTS); ++i) {
            names.insert(QLatin1String(BOOK_PARTS[i].key), book.sectionNames.value(i));
        }
        json.insert(QLatin1String("sections"), names);
    } else {
        setOrRemove(json, "sections", book.sectionSet);
    }
    json.insert(QLatin1String("folder"), book.folder);
    for (const BookPart& part : BOOK_PARTS) {
        json.insert(QLatin1String(part.key), elementsToJson(book.*part.elements));
    }
    // The files outside the text, which have no kind and no status
    QJsonArray files;
    for (const ProjectElement& file : book.files) {
        QJsonObject fileJson = file.extra;
        fileJson.insert(QLatin1String("id"), file.id);
        fileJson.insert(QLatin1String("title"), file.title);
        fileJson.insert(QLatin1String("file"), file.file);
        files.append(fileJson);
    }
    if (files.isEmpty()) {
        json.remove(QLatin1String("files"));
    } else {
        json.insert(QLatin1String("files"), files);
    }
    return json;
}

QJsonObject workshopToJson(const ProjectWorkshop& workshop) {
    QJsonObject json = workshop.extra;
    setOrRemove(json, "name", workshop.name);
    json.insert(QLatin1String("grouped"), workshop.grouped);
    json.insert(QLatin1String("elements"), elementsToJson(workshop.elements));
    return json;
}

// -----------------------------------------------------------------------------
// Elements
// -----------------------------------------------------------------------------

/// The list of elements of @p Project: const for a const project
template <typename Project>
using ElementList = std::conditional_t<std::is_const_v<Project>, const QList<ProjectElement>,
                                       QList<ProjectElement>>;

/// The list in @p list, at any depth, that holds element @p id; @p index gets its index
template <typename List>
List* findInList(List& list, const QString& id, qsizetype& index) {
    for (qsizetype i = 0; i < list.size(); ++i) {
        auto& element = list[i];
        if (element.id == id) {
            index = i;
            return &list;
        }
        if (List* inside = findInList(element.elements, id, index)) {
            return inside;
        }
    }
    return nullptr;
}

/// The list in @p project that holds element @p id, or the files of a book outside its text
/// that hold it; @p index gets its index
template <typename Project>
ElementList<Project>* findList(Project& project, const QString& id, qsizetype& index) {
    for (auto& book : project.books) {
        for (const BookPart& part : BOOK_PARTS) {
            if (ElementList<Project>* list = findInList(book.*part.elements, id, index)) {
                return list;
            }
        }
        if (ElementList<Project>* files = findInList(book.files, id, index)) {
            return files;
        }
    }
    return findInList(project.workshop.elements, id, index);
}

void forEachElement(const QList<ProjectElement>& elements,
                    const std::function<void(const ProjectElement&)>& visit) {
    for (const ProjectElement& element : elements) {
        visit(element);
        forEachElement(element.elements, visit);
    }
}

/// Every element of the books and the Workshop of @p project, and every file of its books
/// outside their text
void forEachElement(const BookProject& project,
                    const std::function<void(const ProjectElement&)>& visit) {
    for (const ProjectBook& book : project.books) {
        for (const BookPart& part : BOOK_PARTS) {
            forEachElement(book.*part.elements, visit);
        }
        forEachElement(book.files, visit);
    }
    forEachElement(project.workshop.elements, visit);
}

/// The place of element @p id in @p list of group @p groupId, at any depth: @p place gets its
/// group and index
bool findPlaceIn(const QList<ProjectElement>& list, const QString& id, const QString& groupId,
                 ElementPlace& place) {
    for (qsizetype i = 0; i < list.size(); ++i) {
        const ProjectElement& element = list.at(i);
        if (element.id == id) {
            place.groupId = groupId;
            place.index = i;
            return true;
        }
        if (findPlaceIn(element.elements, id, element.id, place)) {
            return true;
        }
    }
    return false;
}

/// Append the elements of @p elements and of their groups, without the groups, in reading order
void appendContent(const BookTypeRegistry& registry, const QList<ProjectElement>& elements,
                   QList<const ProjectElement*>& content) {
    for (const ProjectElement& element : elements) {
        if (BookProject::formOf(registry, element) != ElementForm::Group) {
            content.append(&element);
        }
        appendContent(registry, element.elements, content);
    }
}

/// Where the elements of the kind of @p element stand; Any for a kind @p registry does not have
KindPosition positionOf(const BookTypeRegistry& registry, const ProjectElement& element) {
    const KindRef kind = BookProject::kindOf(registry, element);
    return kind ? kind.kind->position : KindPosition::Any;
}

void appendInOrder(const QList<ProjectElement>& elements, QList<const ProjectElement*>& order) {
    for (const ProjectElement& element : elements) {
        order.append(&element);
        appendInOrder(element.elements, order);
    }
}

/// Index of the book of @p project that has element @p id; -1 for an element of the Workshop
/// or for none
qsizetype bookOf(const BookProject& project, const QString& id) {
    for (qsizetype book = 0; book < project.books.size(); ++book) {
        for (const BookPart& part : BOOK_PARTS) {
            qsizetype index = -1;
            if (findInList(project.books.at(book).*part.elements, id, index)) {
                return book;
            }
        }
    }
    return -1;
}

/// The list of @p place in @p project: of a section of book @p book, of the Workshop or of a
/// group; nullptr when the project has no such book or group
QList<ProjectElement>* listAt(BookProject& project, const ElementPlace& place, qsizetype book) {
    if (!place.groupId.isEmpty()) {
        ProjectElement* group = project.findElement(place.groupId);
        return group ? &group->elements : nullptr;
    }
    if (place.place != BookPlace::Workshop && (book < 0 || book >= project.books.size())) {
        return nullptr;
    }
    return &project.elementsIn(place.place, book);
}

/// Whether an element of @p kind can be inside @p element: a group of a kind that can have it
bool canHold(const BookTypeRegistry& registry, const ProjectElement& element,
             const KindRef& kind) {
    return BookProject::formOf(registry, element) == ElementForm::Group &&
           kind.kind->allowsInside(element.kind.kindId);
}

// -----------------------------------------------------------------------------
// Kinds
// -----------------------------------------------------------------------------

bool containsKind(const QList<KindRef>& kinds, const KindRef& ref) {
    return std::any_of(kinds.cbegin(), kinds.cend(),
                       [&ref](const KindRef& other) { return other.kind == ref.kind; });
}

}  // namespace

// =============================================================================
// Names of the parts of a book
// =============================================================================

QString SectionNameSet::name(BookPlace place, const QString& language) const {
    switch (place) {
    case BookPlace::Front:
        return front.text(language);
    case BookPlace::Main:
        return main.text(language);
    case BookPlace::Back:
        return back.text(language);
    case BookPlace::Workshop:
        break;
    }
    return {};
}

const QList<SectionNameSet>& ProjectBook::sectionNameSets() {
    // Names in the language of the book, like the titles of new elements; a language without
    // names of its own gets the English ones
    static const QList<SectionNameSet> sets = [] {
        const auto text = [](const char* polish, const char* english) {
            LocalizedText localized;
            localized.values.insert(QStringLiteral("pl"), QString::fromUtf8(polish));
            localized.values.insert(QStringLiteral("en"), QString::fromUtf8(english));
            return localized;
        };
        return QList<SectionNameSet>{
            {QStringLiteral("sections"), text("Sekcja początkowa", "Front Section"),
             text("Sekcja główna", "Main Section"), text("Sekcja końcowa", "Back Section")},
            {QStringLiteral("matter"), text("Strony początkowe", "Front Matter"),
             text("Tekst główny", "Body"), text("Strony końcowe", "Back Matter")},
            {QStringLiteral("fragments"), text("Fragment początkowy", "Opening Fragment"),
             text("Fragment główny", "Main Fragment"),
             text("Fragment końcowy", "Closing Fragment")},
            {QStringLiteral("arc"), text("Otwarcie", "Opening"),
             text("Rozwinięcie", "Development"), text("Zamknięcie", "Closing")},
        };
    }();
    return sets;
}

const SectionNameSet& ProjectBook::sectionNameSet() const {
    const QList<SectionNameSet>& sets = sectionNameSets();
    const auto found = std::find_if(sets.cbegin(), sets.cend(),
                                    [this](const SectionNameSet& set) {
                                        return set.id == sectionSet;
                                    });
    return found != sets.cend() ? *found : sets.first();
}

QString ProjectBook::sectionName(BookPlace place) const {
    if (sectionSet == QLatin1String(CUSTOM_SECTIONS) && place != BookPlace::Workshop) {
        return sectionNames.value(std::distance(std::cbegin(BOOK_PARTS), &bookPart(place)));
    }
    return sectionNameSet().name(place, language);
}

BookSections ProjectBook::sections() const {
    BookSections sections;
    sections.shown = partsLayer;
    sections.set = sectionSet;
    if (sectionSet == QLatin1String(CUSTOM_SECTIONS)) {
        sections.names = sectionNames;
    }
    return sections;
}

void ProjectBook::setSections(const BookSections& sections) {
    partsLayer = sections.shown;
    sectionSet = sections.set;
    sectionNames.clear();
    if (sectionSet != QLatin1String(CUSTOM_SECTIONS)) {
        sectionNamesExtra = QJsonObject();  // the fields of names the book no longer has
        return;
    }
    const SectionNameSet& first = sectionNameSets().first();
    for (qsizetype i = 0; i < std::ssize(BOOK_PARTS); ++i) {
        const QString given = sections.names.value(i).trimmed();
        sectionNames << (given.isEmpty() ? first.name(BOOK_PARTS[i].place, language) : given);
    }
}

bool ProjectElement::operator==(const ProjectElement& other) const {
    return id == other.id && kind == other.kind && title == other.title &&
           file == other.file && status == other.status && elements == other.elements &&
           description == other.description && fields == other.fields &&
           links == other.links && rotation == other.rotation &&
           viewExtra == other.viewExtra && extra == other.extra;
}

// =============================================================================
// Elements
// =============================================================================

QList<ProjectElement>& BookProject::elementsIn(BookPlace place, qsizetype book) {
    if (place == BookPlace::Workshop) {
        return workshop.elements;
    }
    Q_ASSERT(book >= 0 && book < books.size());
    return books[book].*bookPart(place).elements;
}

const QList<ProjectElement>& BookProject::elementsIn(BookPlace place, qsizetype book) const {
    if (place == BookPlace::Workshop) {
        return workshop.elements;
    }
    Q_ASSERT(book >= 0 && book < books.size());
    return books.at(book).*bookPart(place).elements;
}

QList<const ProjectElement*> BookProject::readingOrder(qsizetype book) const {
    QList<const ProjectElement*> order;
    for (const BookPart& part : BOOK_PARTS) {
        appendInOrder(elementsIn(part.place, book), order);
    }
    return order;
}

ProjectElement* BookProject::findElement(const QString& elementId) {
    qsizetype index = -1;
    QList<ProjectElement>* list = findList(*this, elementId, index);
    return list ? &(*list)[index] : nullptr;
}

const ProjectElement* BookProject::findElement(const QString& elementId) const {
    qsizetype index = -1;
    const QList<ProjectElement>* list = findList(*this, elementId, index);
    return list ? &list->at(index) : nullptr;
}

bool BookProject::isBookFile(const QString& elementId) const {
    return !elementId.isEmpty() &&
           std::any_of(books.cbegin(), books.cend(), [&elementId](const ProjectBook& book) {
               return std::any_of(book.files.cbegin(), book.files.cend(),
                                  [&elementId](const ProjectElement& file) {
                                      return file.id == elementId;
                                  });
           });
}

QString BookProject::sectionLink(BookPlace place, qsizetype book) const {
    if (place == BookPlace::Workshop || book < 0 || book >= books.size()) {
        return {};
    }
    return books.at(book).id + QLatin1Char('/') + QLatin1String(bookPart(place).key);
}

std::optional<BookPlace> BookProject::linkedSection(const QString& link, qsizetype book) const {
    for (const BookPart& part : BOOK_PARTS) {
        if (!link.isEmpty() && link == sectionLink(part.place, book)) {
            return part.place;
        }
    }
    return std::nullopt;
}

bool BookProject::hasLinkTarget(const QString& link) const {
    if (findElement(link) != nullptr) {
        return true;
    }
    for (qsizetype book = 0; book < books.size(); ++book) {
        if (linkedSection(link, book)) {
            return true;
        }
    }
    return false;
}

QList<ProjectElement>* BookProject::listOf(const QString& elementId, qsizetype* index) {
    qsizetype found = -1;
    QList<ProjectElement>* list = findList(*this, elementId, found);
    if (list && index) {
        *index = found;
    }
    return list;
}

const QList<ProjectElement>* BookProject::listOf(const QString& elementId,
                                                 qsizetype* index) const {
    qsizetype found = -1;
    const QList<ProjectElement>* list = findList(*this, elementId, found);
    if (list && index) {
        *index = found;
    }
    return list;
}

std::optional<ElementPlace> BookProject::placeOf(const QString& elementId) const {
    ElementPlace found;
    for (const ProjectBook& book : books) {
        for (const BookPart& part : BOOK_PARTS) {
            found.place = part.place;
            if (findPlaceIn(book.*part.elements, elementId, QString(), found)) {
                return found;
            }
        }
    }
    found.place = BookPlace::Workshop;
    if (findPlaceIn(workshop.elements, elementId, QString(), found)) {
        return found;
    }
    return std::nullopt;
}

std::optional<ProjectElement> BookProject::takeElement(const QString& elementId) {
    qsizetype index = -1;
    QList<ProjectElement>* list = findList(*this, elementId, index);
    if (!list) {
        return std::nullopt;
    }
    return list->takeAt(index);
}

bool BookProject::moveElement(const QString& elementId, qsizetype index) {
    qsizetype from = -1;
    QList<ProjectElement>* list = findList(*this, elementId, from);
    if (!list || index < 0 || index >= list->size()) {
        return false;
    }
    list->move(from, index);
    return true;
}

QList<ElementPlace> BookProject::movePlacesOf(const BookTypeRegistry& registry,
                                              const QString& elementId) const {
    QList<ElementPlace> places;
    const std::optional<ElementPlace> from = placeOf(elementId);
    if (!from) {
        return places;
    }

    // The project without the element: its places are where the element goes once it has left
    // its own
    BookProject without = *this;
    const std::optional<ProjectElement> element = without.takeElement(elementId);
    const KindRef kind = element ? kindOf(registry, *element) : KindRef{};
    const qsizetype book = bookOf(*this, elementId);
    if (!kind) {
        // A kind the registry does not have: the places of its list after the title page
        const QList<ProjectElement>* list = listAt(without, *from, book);
        for (qsizetype i = 0; list && i <= list->size(); ++i) {
            if (canBeAt(registry, *list, kind, i)) {
                places.append({from->place, from->groupId, i});
            }
        }
        return places;
    }

    // In reading order: the place before an element, then the places inside it
    const std::function<void(BookPlace, const QList<ProjectElement>&, const QString&, bool)>
        addPlaces = [&](BookPlace part, const QList<ProjectElement>& elements,
                        const QString& groupId, bool here) {
            for (qsizetype i = 0; i <= elements.size(); ++i) {
                if (here && canBeAt(registry, elements, kind, i)) {
                    places.append({part, groupId, i});
                }
                if (i < elements.size() && canHold(registry, elements.at(i), kind)) {
                    addPlaces(part, elements.at(i).elements, elements.at(i).id, true);
                }
            }
        };
    if (from->place == BookPlace::Workshop) {
        addPlaces(BookPlace::Workshop, without.workshop.elements, QString(),
                  kind.kind->allows(BookPlace::Workshop));
        return places;
    }
    for (const BookPart& part : BOOK_PARTS) {
        addPlaces(part.place, without.elementsIn(part.place, book), QString(),
                  kind.kind->allows(part.place));
    }
    return places;
}

bool BookProject::moveElementTo(const BookTypeRegistry& registry, const QString& elementId,
                                const ElementPlace& to) {
    if (!movePlacesOf(registry, elementId).contains(to)) {
        return false;
    }

    // The project as it was comes back if the element finds no list there
    const BookProject previous = *this;
    const qsizetype book = bookOf(*this, elementId);
    std::optional<ProjectElement> element = takeElement(elementId);
    if (!element) {
        return false;
    }
    QList<ProjectElement>* list = listAt(*this, to, book);
    if (!list || to.index > list->size()) {
        *this = previous;
        return false;
    }
    list->insert(to.index, std::move(*element));
    return true;
}

bool BookProject::hasFile(const QString& file) const {
    bool found = false;
    if (!file.isEmpty()) {
        forEachElement(*this, [&file, &found](const ProjectElement& element) {
            found = found || element.file == file;
        });
    }
    return found;
}

QString BookProject::newElementId() const {
    QString newId;
    do {
        newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (findElement(newId) != nullptr);
    return newId;
}

// =============================================================================
// Kinds
// =============================================================================

QList<KindRef> BookProject::kindsIn(const BookTypeRegistry& registry, BookPlace place) const {
    QList<KindRef> offered = type ? registry.kindsIn(type->id, place) : QList<KindRef>{};
    for (const KindReference& reference : kinds) {
        const KindRef ref = registry.findKind(reference.packageId, reference.kindId);
        if (ref && ref.offeredIn(place) && !containsKind(offered, ref)) {
            offered.append(ref);
        }
    }
    return offered;
}

QList<KindRef> BookProject::kindsInside(const BookTypeRegistry& registry,
                                        const QString& groupKindId) const {
    QList<KindRef> offered =
        type ? registry.kindsInside(type->id, groupKindId) : QList<KindRef>{};
    for (const KindReference& reference : kinds) {
        const KindRef ref = registry.findKind(reference.packageId, reference.kindId);
        if (ref && ref.kind->allowsInside(groupKindId) && !containsKind(offered, ref)) {
            offered.append(ref);
        }
    }
    return offered;
}

KindRef BookProject::kindOf(const BookTypeRegistry& registry, const ProjectElement& element) {
    return registry.findKind(element.kind.packageId, element.kind.kindId);
}

ElementForm BookProject::formOf(const BookTypeRegistry& registry, const ProjectElement& element) {
    if (const KindRef kind = kindOf(registry, element)) {
        return kind.kind->form;
    }
    // A kind of a package that is not installed: its file tells
    if (element.file.isEmpty()) {
        return ElementForm::Group;
    }
    return element.file.endsWith(QStringLiteral(".kchapter"), Qt::CaseInsensitive)
               ? ElementForm::Text
               : ElementForm::Window;
}

qsizetype BookProject::newIndexIn(const BookTypeRegistry& registry,
                                  const QList<ProjectElement>& elements, const KindRef& kind) {
    if (kind && kind.kind->first) {
        return 0;
    }
    const qsizetype opening = afterOpeningOf(registry, elements);
    const KindPosition position = kind ? kind.kind->position : KindPosition::Any;
    if (position == KindPosition::Start) {
        return opening;
    }
    qsizetype index = elements.size();
    if (position == KindPosition::End) {
        return index;
    }
    while (index > opening &&
           positionOf(registry, elements.at(index - 1)) == KindPosition::End) {
        --index;
    }
    return index;
}

bool BookProject::canBeAt(const BookTypeRegistry& registry, const QList<ProjectElement>& elements,
                          const KindRef& kind, qsizetype index) {
    if (index < 0 || index > elements.size()) {
        return false;
    }
    if (kind && kind.kind->first) {
        return index == 0;
    }
    return index >= afterOpeningOf(registry, elements);
}

qsizetype BookProject::afterOpeningOf(const BookTypeRegistry& registry,
                                      const QList<ProjectElement>& elements) {
    for (qsizetype i = elements.size(); i > 0; --i) {
        const KindRef kind = kindOf(registry, elements.at(i - 1));
        if (kind && kind.kind->first) {
            return i;
        }
    }
    return 0;
}

QList<const ProjectElement*> BookProject::contentOf(const BookTypeRegistry& registry,
                                                    const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> content;
    appendContent(registry, elements, content);
    return content;
}

QList<const ProjectElement*> BookProject::openingElementsOf(const BookTypeRegistry& registry,
                                                            const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> opening = contentOf(registry, elements);
    const auto first = std::find_if(opening.cbegin(), opening.cend(),
                                    [&registry](const ProjectElement* element) {
                                        return positionOf(registry, *element) !=
                                               KindPosition::Start;
                                    });
    opening.erase(first, opening.cend());
    return opening;
}

QList<const ProjectElement*> BookProject::closingElementsOf(const BookTypeRegistry& registry,
                                                            const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> closing = contentOf(registry, elements);
    qsizetype start = closing.size();
    while (start > 0 && positionOf(registry, *closing.at(start - 1)) == KindPosition::End) {
        --start;
    }
    closing.remove(0, start);
    return closing;
}

QStringList BookProject::packageIds() const {
    QStringList ids;
    const auto note = [&ids](const QString& packageId) {
        if (!packageId.isEmpty() && !ids.contains(packageId)) {
            ids.append(packageId);
        }
    };
    const auto noteElement = [&note](const ProjectElement& element) {
        note(element.kind.packageId);
    };

    if (type) {
        note(type->id);
    }
    for (const KindReference& reference : kinds) {
        note(reference.packageId);
    }
    forEachElement(*this, noteElement);
    return ids;
}

QStringList BookProject::missingPackages(const BookTypeRegistry& registry) const {
    QStringList missing;
    const auto note = [&missing](const QString& packageId) {
        if (!missing.contains(packageId)) {
            missing.append(packageId);
        }
    };
    for (const QString& packageId : packageIds()) {
        if (!registry.package(packageId)) {
            note(packageId);
        }
    }
    // A package of another version, without a kind the project names
    const auto noteKind = [&registry, &note](const KindReference& reference) {
        if (!reference.kindId.isEmpty() && registry.package(reference.packageId) &&
            !registry.findKind(reference.packageId, reference.kindId)) {
            note(reference.packageId);
        }
    };
    for (const KindReference& reference : kinds) {
        noteKind(reference);
    }
    forEachElement(*this, [&noteKind](const ProjectElement& element) { noteKind(element.kind); });
    missing.sort();
    return missing;
}

// =============================================================================
// Reading and writing
// =============================================================================

QJsonObject BookProject::toJson() const {
    QJsonObject json = extra;
    json.insert(QLatin1String("format"), FORMAT);
    json.insert(QLatin1String("id"), id);
    setOrRemove(json, "created", dateTimeText(created));
    setOrRemove(json, "modified", dateTimeText(modified));

    if (type) {
        QJsonObject typeJson = type->extra;
        typeJson.insert(QLatin1String("id"), type->id);
        setOrRemove(typeJson, "version", type->version);
        json.insert(QLatin1String("type"), typeJson);
    } else {
        json.remove(QLatin1String("type"));
    }

    QJsonArray kindList;
    for (const KindReference& reference : kinds) {
        kindList.append(reference.toString());
    }
    json.insert(QLatin1String("kinds"), kindList);

    QJsonArray bookList;
    for (const ProjectBook& book : books) {
        bookList.append(bookToJson(book));
    }
    json.insert(QLatin1String("books"), bookList);
    json.insert(QLatin1String("workshop"), workshopToJson(workshop));
    return json;
}

bool BookProject::isNewerFormat(const QJsonObject& json) {
    const QJsonValue format = json.value(QLatin1String("format"));
    return isWholeNumber(format) && format.toDouble() > FORMAT;
}

bool BookProject::isNewerFormat(const QByteArray& data) {
    const QJsonDocument document = QJsonDocument::fromJson(data);
    return document.isObject() && isNewerFormat(document.object());
}

bool BookProject::isNewerFormatFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && isNewerFormat(file.readAll());
}

bool BookProject::isOlderFormat(const QJsonObject& json) {
    const QJsonValue format = json.value(QLatin1String("format"));
    return format.isUndefined() || (isWholeNumber(format) && format.toDouble() < FORMAT);
}

bool BookProject::isOlderFormatFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() && isOlderFormat(document.object());
}

std::optional<BookProject> BookProject::fromJson(const QJsonObject& json,
                                                 QStringList& problems) {
    const qsizetype problemsBefore = problems.size();
    Fields fields(json, QString(), problems);

    // The format first: another format may have other fields
    const QJsonValue format = fields.take("format");
    if (!isWholeNumber(format) || format.toInt() != FORMAT) {
        fields.problem("format", QStringLiteral("must be %1, the format this version of "
                                                "Kalahari reads")
                                     .arg(FORMAT));
        return std::nullopt;
    }

    BookProject project;
    project.id = fields.string("id", true);
    project.created = fields.dateTime("created");
    project.modified = fields.dateTime("modified");

    const QJsonValue type = fields.take("type");
    if (!type.isUndefined()) {
        project.type = readType(type, problems);
    }

    const QJsonArray kindList = fields.array("kinds");
    for (qsizetype i = 0; i < kindList.size(); ++i) {
        const QString field = QStringLiteral("kinds[%1]").arg(i);
        const std::optional<KindReference> reference = readKindReference(kindList.at(i));
        if (!reference) {
            addProblem(problems, field, kindReferenceProblem());
        } else if (project.kinds.contains(*reference)) {
            addProblem(problems, field, QStringLiteral("'%1' is already in the list")
                                            .arg(reference->toString()));
        } else {
            project.kinds.append(*reference);
        }
    }

    Seen seen;
    const QJsonValue books = fields.take("books");
    if (!books.isArray() || books.toArray().isEmpty()) {
        fields.problem("books", QStringLiteral("must be a list of at least one book"));
    } else {
        const QJsonArray bookList = books.toArray();
        QHash<QString, QString> bookIds;
        QHash<QString, QString> folders;
        for (qsizetype i = 0; i < bookList.size(); ++i) {
            const QString field = QStringLiteral("books[%1]").arg(i);
            ProjectBook book = readBook(bookList.at(i), field, seen, problems);
            if (!book.id.isEmpty()) {
                if (bookIds.contains(book.id)) {
                    addProblem(problems, field + QStringLiteral(".id"),
                               QStringLiteral("'%1' is already the id of %2")
                                   .arg(book.id, bookIds.value(book.id)));
                } else {
                    bookIds.insert(book.id, field);
                }
            }
            if (!book.folder.isEmpty()) {
                if (folders.contains(book.folder)) {
                    addProblem(problems, field + QStringLiteral(".folder"),
                               QStringLiteral("'%1' is already the folder of %2")
                                   .arg(book.folder, folders.value(book.folder)));
                } else {
                    folders.insert(book.folder, field);
                }
            }
            project.books.append(std::move(book));
        }
    }

    project.workshop = readWorkshop(fields.take("workshop"), seen, problems);
    project.extra = fields.rest();

    if (problems.size() > problemsBefore) {
        return std::nullopt;
    }
    return project;
}

bool BookProject::save(const QString& path) const {
    auto& logger = Logger::getInstance();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        logger.error("BookProject::save: cannot write {}: {}", path.toStdString(),
                     file.errorString().toStdString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        logger.error("BookProject::save: cannot save {}: {}", path.toStdString(),
                     file.errorString().toStdString());
        return false;
    }
    return true;
}

std::optional<BookProject> BookProject::load(const QString& path, QStringList& problems) {
    const QString field = QFileInfo(path).fileName();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        addProblem(problems, field, QStringLiteral("cannot be read"));
        return std::nullopt;
    }
    return read(file.readAll(), field, problems);
}

std::optional<BookProject> BookProject::read(const QByteArray& data, const QString& name,
                                             QStringList& problems) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        addProblem(problems, name,
                   QStringLiteral("not valid JSON: %1 at character %2")
                       .arg(error.errorString())
                       .arg(error.offset));
        return std::nullopt;
    }
    if (!document.isObject()) {
        addProblem(problems, name, QStringLiteral("must be a JSON object"));
        return std::nullopt;
    }
    return fromJson(document.object(), problems);
}

}  // namespace kalahari::core
