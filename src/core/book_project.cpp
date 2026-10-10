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

ProjectElement readElement(const QJsonValue& value, const QString& field, Seen& seen,
                           QStringList& problems) {
    ProjectElement element;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return element;
    }
    Fields fields(value.toObject(), field, problems);

    element.id = fields.string("id", true);
    if (!element.id.isEmpty()) {
        const QString first = seen.ids.value(element.id);
        if (first.isEmpty()) {
            seen.ids.insert(element.id, field);
        } else {
            fields.problem("id", QStringLiteral("'%1' is already the id of %2")
                                     .arg(element.id, first));
        }
    }

    const QJsonValue kind = fields.take("kind");
    if (const std::optional<KindReference> reference = readKindReference(kind)) {
        element.kind = *reference;
    } else {
        fields.problem("kind", kindReferenceProblem());
    }

    element.title = fields.string("title");

    const QJsonValue file = fields.take("file");
    if (!file.isUndefined()) {
        if (!file.isString() || !isPathInside(file.toString())) {
            fields.problem("file", QStringLiteral("must be a path inside the project folder, "
                                                  "e.g. \"book/chapter_001.kchapter\""));
        } else {
            element.file = file.toString();
            const QString first = seen.files.value(element.file);
            if (first.isEmpty()) {
                seen.files.insert(element.file, field);
            } else {
                fields.problem("file", QStringLiteral("'%1' is already the file of %2")
                                           .arg(element.file, first));
            }
        }
    }

    element.status = fields.string("status");
    element.elements = readElements(fields.array("elements"), fields.path("elements"), seen,
                                    problems);
    element.extra = fields.rest();
    return element;
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

/// The list in @p project that holds element @p id; @p index gets its index
template <typename Project>
ElementList<Project>* findList(Project& project, const QString& id, qsizetype& index) {
    for (auto& book : project.books) {
        for (const BookPart& part : BOOK_PARTS) {
            if (ElementList<Project>* list = findInList(book.*part.elements, id, index)) {
                return list;
            }
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

/// Every element of the books and the Workshop of @p project
void forEachElement(const BookProject& project,
                    const std::function<void(const ProjectElement&)>& visit) {
    for (const ProjectBook& book : project.books) {
        for (const BookPart& part : BOOK_PARTS) {
            forEachElement(book.*part.elements, visit);
        }
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
           extra == other.extra;
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
        if (ref && ref.kind->allows(place) && !containsKind(offered, ref)) {
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
    const KindPosition position = kind ? kind.kind->position : KindPosition::Any;
    if (position == KindPosition::Start) {
        return 0;
    }
    qsizetype index = elements.size();
    if (position == KindPosition::End) {
        return index;
    }
    while (index > 0 && positionOf(registry, elements.at(index - 1)) == KindPosition::End) {
        --index;
    }
    return index;
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

QStringList BookProject::missingPackages(const BookTypeRegistry& registry) const {
    QStringList missing;
    const auto note = [&registry, &missing](const QString& packageId) {
        if (!packageId.isEmpty() && !registry.package(packageId) &&
            !missing.contains(packageId)) {
            missing.append(packageId);
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
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        addProblem(problems, field,
                   QStringLiteral("not valid JSON: %1 at character %2")
                       .arg(error.errorString())
                       .arg(error.offset));
        return std::nullopt;
    }
    if (!document.isObject()) {
        addProblem(problems, field, QStringLiteral("must be a JSON object"));
        return std::nullopt;
    }
    return fromJson(document.object(), problems);
}

}  // namespace kalahari::core
