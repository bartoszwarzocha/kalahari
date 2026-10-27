/// @file custom_book_type.cpp
/// @brief The writer's own book types

#include <kalahari/core/custom_book_type.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

namespace kalahari::core {

namespace {

void addProblem(QStringList& problems, const QString& field, const QString& what) {
    problems.append(field + QStringLiteral(": ") + what);
}

/// The kind named by @p reference with its package, as @p registry sees it
KindRef findKind(const BookTypeRegistry& registry, const KindReference& reference) {
    return registry.findKind(reference.packageId, reference.kindId);
}

/// Reference of a kind written with its package; nullopt when @p value is not one
std::optional<KindReference> referenceOf(const QJsonValue& value) {
    std::optional<KindReference> reference = KindReference::parse(value.toString());
    if (!reference || reference->packageId.isEmpty()) {
        return std::nullopt;
    }
    return reference;
}

}  // namespace

CustomBookType CustomBookType::of(const BookProject& project, const BookTypeRegistry& registry) {
    CustomBookType type;
    type.kinds = project.kinds;
    if (project.type) {
        type.typeId = project.type->id;
        return type;
    }
    // A user project starts with an element of its main text kind: the first text kind of its
    // main part, as ProjectManager::mainTextKind() names it
    for (const KindRef& kind : project.kindsIn(registry, BookPlace::Main)) {
        if (kind.kind->form == ElementForm::Text) {
            type.start.append(CustomStartElement{
                KindReference{kind.package->id, kind.kind->id}, BookPlace::Main});
            break;
        }
    }
    return type;
}

CustomBookType CustomBookType::of(const PaletteChoice& choice) {
    CustomBookType type;
    for (const KindRef& kind : choice.kinds) {
        const KindReference reference{kind.package->id, kind.kind->id};
        if (!type.kinds.contains(reference)) {
            type.kinds.append(reference);
        }
    }
    for (const StartElement& element : choice.elements) {
        type.start.append(CustomStartElement{
            KindReference{element.kind.package->id, element.kind.kind->id}, element.place});
    }
    return type;
}

QList<KindRef> CustomBookType::kindsIn(const BookTypeRegistry& registry) const {
    QList<KindRef> found;
    for (const KindReference& reference : kinds) {
        if (const KindRef kind = findKind(registry, reference)) {
            found.append(kind);
        }
    }
    return found;
}

QStringList CustomBookType::missingKinds(const BookTypeRegistry& registry) const {
    QStringList missing;
    for (const KindReference& reference : kinds) {
        if (!findKind(registry, reference)) {
            missing.append(reference.toString());
        }
    }
    return missing;
}

QList<StartElement> CustomBookType::startElementsIn(const BookTypeRegistry& registry) const {
    if (!isUserProject()) {
        // A book of a type makes the text elements of its type (ProjectManager::createProject())
        QList<StartElement> elements = registry.startElements(typeId);
        elements.removeIf([](const StartElement& element) {
            return !element.kind || element.kind.kind->form != ElementForm::Text;
        });
        return elements;
    }
    NewProject project;
    applyTo(project, registry);
    return project.palette ? project.palette->elements : QList<StartElement>{};
}

void CustomBookType::applyTo(NewProject& project, const BookTypeRegistry& registry) const {
    project.typeId = typeId;
    project.takenKinds.clear();
    project.palette.reset();
    const QList<KindRef> found = kindsIn(registry);
    if (!isUserProject()) {
        project.takenKinds = found;
        return;
    }
    PaletteChoice choice;
    choice.kinds = found;
    for (const CustomStartElement& element : start) {
        const KindRef kind = findKind(registry, element.kind);
        const bool ofType =
            std::any_of(found.cbegin(), found.cend(),
                        [&kind](const KindRef& other) { return other.kind == kind.kind; });
        if (kind && ofType && kind.kind->allows(element.place)) {
            choice.elements.append(StartElement{kind, element.place});
        }
    }
    project.palette = choice;
}

QJsonObject CustomBookType::toJson() const {
    QJsonObject json = extra;
    json.insert(QStringLiteral("format"), FORMAT);
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("name"), name);
    if (!description.isEmpty()) {
        json.insert(QStringLiteral("description"), description);
    } else {
        json.remove(QStringLiteral("description"));
    }
    if (!typeId.isEmpty()) {
        json.insert(QStringLiteral("type"), typeId);
    } else {
        json.remove(QStringLiteral("type"));
    }
    QJsonArray kindsArray;
    for (const KindReference& reference : kinds) {
        kindsArray.append(reference.toString());
    }
    json.insert(QStringLiteral("kinds"), kindsArray);
    QJsonArray startArray;
    for (const CustomStartElement& element : start) {
        startArray.append(QJsonObject{{QStringLiteral("kind"), element.kind.toString()},
                                      {QStringLiteral("place"), bookPlaceName(element.place)}});
    }
    json.insert(QStringLiteral("start"), startArray);
    return json;
}

std::optional<CustomBookType> CustomBookType::fromJson(const QJsonObject& json,
                                                       QStringList& problems) {
    const qsizetype problemsBefore = problems.size();
    CustomBookType type;
    QJsonObject rest = json;

    const QJsonValue format = rest.take(QStringLiteral("format"));
    if (!format.isDouble() || format.toInt() != FORMAT) {
        addProblem(problems, QStringLiteral("format"), QStringLiteral("must be %1").arg(FORMAT));
        return std::nullopt;
    }
    type.id = rest.take(QStringLiteral("id")).toString();
    if (QUuid::fromString(type.id).isNull()) {
        addProblem(problems, QStringLiteral("id"), QStringLiteral("must be a UUID"));
    }
    type.name = rest.take(QStringLiteral("name")).toString().trimmed();
    if (type.name.isEmpty()) {
        addProblem(problems, QStringLiteral("name"), QStringLiteral("must not be empty"));
    }
    type.description = rest.take(QStringLiteral("description")).toString();

    const QJsonValue typeValue = rest.take(QStringLiteral("type"));
    if (!typeValue.isUndefined()) {
        type.typeId = typeValue.toString();
        if (!BookTypePackage::isId(type.typeId)) {
            addProblem(problems, QStringLiteral("type"),
                       QStringLiteral("must be the id of a package"));
        }
    }

    const QJsonValue kindsValue = rest.take(QStringLiteral("kinds"));
    if (!kindsValue.isUndefined() && !kindsValue.isArray()) {
        addProblem(problems, QStringLiteral("kinds"), QStringLiteral("must be an array"));
    }
    const QJsonArray kindsArray = kindsValue.toArray();
    for (qsizetype index = 0; index < kindsArray.size(); ++index) {
        const std::optional<KindReference> reference = referenceOf(kindsArray.at(index));
        if (!reference) {
            addProblem(problems, QStringLiteral("kinds[%1]").arg(index),
                       QStringLiteral("must be a kind with its package"));
        } else if (!type.kinds.contains(*reference)) {
            type.kinds.append(*reference);
        }
    }
    if (type.isUserProject() && kindsArray.isEmpty()) {
        addProblem(problems, QStringLiteral("kinds"),
                   QStringLiteral("must not be empty without a type"));
    }

    const QJsonValue startValue = rest.take(QStringLiteral("start"));
    if (!startValue.isUndefined() && !startValue.isArray()) {
        addProblem(problems, QStringLiteral("start"), QStringLiteral("must be an array"));
    }
    const QJsonArray startArray = startValue.toArray();
    for (qsizetype index = 0; index < startArray.size(); ++index) {
        const QJsonObject element = startArray.at(index).toObject();
        const std::optional<KindReference> reference =
            referenceOf(element.value(QStringLiteral("kind")));
        const std::optional<BookPlace> place =
            bookPlaceFromName(element.value(QStringLiteral("place")).toString());
        if (!reference || !place) {
            addProblem(problems, QStringLiteral("start[%1]").arg(index),
                       QStringLiteral("must have a kind with its package and a place"));
            continue;
        }
        type.start.append(CustomStartElement{*reference, *place});
    }
    type.extra = rest;

    if (problems.size() > problemsBefore) {
        return std::nullopt;
    }
    return type;
}

QString CustomBookTypes::folder() {
    // The folder of the settings file: the program's, or the one a test gave it
    const std::filesystem::path settings =
        SettingsManager::getInstance().getSettingsFilePath().parent_path();
    return QDir(QString::fromStdU16String(settings.u16string()))
        .filePath(QString::fromLatin1(CustomBookType::FOLDER));
}

QList<CustomBookType> CustomBookTypes::load(const QString& folder) {
    auto& logger = Logger::getInstance();
    QList<CustomBookType> types;
    const QDir dir(folder);
    const QStringList files = dir.entryList(
        {QStringLiteral("*") + QLatin1String(CustomBookType::FILE_EXTENSION)}, QDir::Files,
        QDir::Name);
    for (const QString& name : files) {
        const QString path = dir.filePath(name);
        QFile file(path);
        QStringList problems;
        if (!file.open(QIODevice::ReadOnly)) {
            problems.append(QStringLiteral("cannot be read"));
        } else {
            QJsonParseError error{};
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                problems.append(QStringLiteral("not an object of JSON"));
            } else if (std::optional<CustomBookType> type =
                           CustomBookType::fromJson(document.object(), problems)) {
                type->file = QFileInfo(path).absoluteFilePath();
                types.append(*type);
                continue;
            }
        }
        for (const QString& problem : problems) {
            logger.warn("CustomBookTypes::load: {}: {}", name.toStdString(),
                        problem.toStdString());
        }
    }
    // By name, as the New Book window lists them
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::stable_sort(types.begin(), types.end(),
                     [&collator](const CustomBookType& one, const CustomBookType& other) {
                         return collator.compare(one.name, other.name) < 0;
                     });
    return types;
}

bool CustomBookTypes::save(CustomBookType& type, const QString& folder, QString* problem) {
    const auto fail = [problem](const QString& what) {
        Logger::getInstance().error("CustomBookTypes::save: {}", what.toStdString());
        if (problem) {
            *problem = what;
        }
        return false;
    };
    type.name = type.name.trimmed();
    if (type.name.isEmpty()) {
        return fail(QStringLiteral("the type has no name"));
    }
    if (QUuid::fromString(type.id).isNull()) {
        type.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!QDir().mkpath(folder)) {
        return fail(QStringLiteral("%1: the folder cannot be made")
                        .arg(QDir::toNativeSeparators(folder)));
    }
    const QString path =
        QDir(folder).filePath(type.id + QLatin1String(CustomBookType::FILE_EXTENSION));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path),
                                                 file.errorString()));
    }
    file.write(QJsonDocument(type.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return fail(QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path),
                                                 file.errorString()));
    }
    type.file = QFileInfo(path).absoluteFilePath();
    return true;
}

bool CustomBookTypes::remove(const CustomBookType& type) {
    if (type.file.isEmpty() || !QFile::remove(type.file)) {
        Logger::getInstance().error("CustomBookTypes::remove: {} cannot be deleted",
                                    type.file.toStdString());
        return false;
    }
    return true;
}

const CustomBookType* CustomBookTypes::named(const QList<CustomBookType>& types,
                                             const QString& name) {
    const QString wanted = name.trimmed();
    for (const CustomBookType& type : types) {
        if (type.name.compare(wanted, Qt::CaseInsensitive) == 0) {
            return &type;
        }
    }
    return nullptr;
}

}  // namespace kalahari::core
