/// @file book_type_package.cpp
/// @brief Book type packages: reading booktype.json and the styles file

#include <kalahari/core/book_type_package.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace kalahari::core {

namespace {

const char* const PLACE_NAMES[] = {"front", "main", "back", "workshop"};

/// Names of the groups of the Workshop, from WorkshopGroup::Libraries on
const char* const WORKSHOP_GROUP_NAMES[] = {"libraries", "resources"};

void addProblem(QStringList& problems, const QString& field, const QString& message) {
    problems << field + QStringLiteral(": ") + message;
}

bool matches(const QString& text, const char* pattern) {
    const QRegularExpression expression(QString::fromLatin1(pattern));
    return expression.match(text).hasMatch();
}

/// Kind, style and window ids: "chapter", "heading_1", "mindmap"
bool isIdentifier(const QString& text) {
    return matches(text, "^[a-z][a-z0-9_]*$");
}

/// Package ids: "kalahari.novel"
bool isPackageId(const QString& text) {
    return matches(text, "^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+$");
}

/// Kinds in the lists of a package: "chapter", or "kalahari.nonfiction:bibliography"
bool isKindName(const QString& text) {
    return KindReference::parse(text).has_value();
}

bool isVersion(const QString& text) {
    return matches(text, "^[0-9]+\\.[0-9]+(\\.[0-9]+)?$");
}

/// "en", "pl", "pt_BR"
bool isLanguageCode(const QString& text) {
    return matches(text, "^[a-z]{2,3}(_[A-Z]{2})?$");
}

/// "#RRGGBB" or "#AARRGGBB", as StyleResolver reads them
bool isColor(const QString& text) {
    return matches(text, "^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$");
}

bool isWholeNumber(const QJsonValue& value) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

/// Report keys of @p object that are not in @p known (a misspelt key would be ignored)
void checkKeys(const QJsonObject& object, const QString& field,
               std::initializer_list<const char*> known, QStringList& problems) {
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString key = it.key();
        const bool isKnown = std::any_of(known.begin(), known.end(), [&key](const char* name) {
            return key == QLatin1String(name);
        });
        if (!isKnown) {
            addProblem(problems, field, QStringLiteral("unknown key '%1'").arg(key));
        }
    }
}

std::optional<QJsonObject> readJsonObject(const QString& path, const QString& field,
                                          QStringList& problems) {
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
    return document.object();
}

LocalizedText readText(const QJsonValue& value, const QString& field, bool required,
                       QStringList& problems) {
    LocalizedText text;
    if (value.isUndefined()) {
        if (required) {
            addProblem(problems, field, QStringLiteral("is missing"));
        }
        return text;
    }
    if (!value.isObject()) {
        addProblem(problems, field,
                   QStringLiteral("must be texts by language, e.g. "
                                  "{\"en\": \"...\", \"pl\": \"...\"}"));
        return text;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!isLanguageCode(it.key())) {
            addProblem(problems, field,
                       QStringLiteral("'%1' is not a language code such as \"en\" or \"pt_BR\"")
                           .arg(it.key()));
        } else if (!it.value().isString() || it.value().toString().trimmed().isEmpty()) {
            addProblem(problems, field + QLatin1Char('.') + it.key(),
                       QStringLiteral("must be a text that is not empty"));
        } else {
            text.values.insert(it.key(), it.value().toString());
        }
    }
    if (!object.contains(QStringLiteral("en"))) {
        addProblem(problems, field,
                   QStringLiteral("the English text (\"en\") is missing; it is shown in "
                                  "languages the package does not have"));
    }
    return text;
}

/// Optional icon id
QString readIcon(const QJsonValue& value, const QString& field, QStringList& problems) {
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isString() || value.toString().isEmpty()) {
        addProblem(problems, field, QStringLiteral("must be an icon id"));
        return {};
    }
    return value.toString();
}

/// List of ids; @p unique reports an id listed twice
QStringList readIdList(const QJsonValue& value, const QString& field,
                       bool (*isValid)(const QString&), const QString& what, bool unique,
                       QStringList& problems) {
    QStringList list;
    if (value.isUndefined()) {
        return list;
    }
    if (!value.isArray()) {
        addProblem(problems, field, QStringLiteral("must be a list of %1s").arg(what));
        return list;
    }
    const QJsonArray array = value.toArray();
    for (const auto& item : array) {
        const QString id = item.toString();
        if (!item.isString() || !isValid(id)) {
            addProblem(problems, field, QStringLiteral("'%1' is not a %2").arg(id, what));
        } else if (unique && list.contains(id)) {
            addProblem(problems, field, QStringLiteral("'%1' is listed twice").arg(id));
        } else {
            list << id;
        }
    }
    return list;
}

/// A file of the package: a relative path inside its folder, with @p suffix, that exists
bool checkPackageFile(const QString& directory, const QJsonValue& value, const QString& suffix,
                      const QString& field, QStringList& problems) {
    const QString path = value.toString();
    const QStringList segments = path.split(QRegularExpression(QStringLiteral("[/\\\\]")));
    if (!value.isString() || path.isEmpty() || QDir::isAbsolutePath(path) ||
        segments.contains(QStringLiteral(".."))) {
        addProblem(problems, field, QStringLiteral("must be a path inside the package folder"));
        return false;
    }
    if (!path.endsWith(suffix, Qt::CaseInsensitive)) {
        addProblem(problems, field, QStringLiteral("must be a %1 file").arg(suffix));
        return false;
    }
    if (!QFileInfo(QDir(directory).filePath(path)).isFile()) {
        addProblem(problems, field, QStringLiteral("file '%1' is not in the package").arg(path));
        return false;
    }
    return true;
}

std::optional<ElementKind> readKind(const QString& id, const QJsonValue& value,
                                    const QString& directory, QStringList& problems) {
    const QString field = QStringLiteral("kinds.") + id;
    if (!isIdentifier(id)) {
        addProblem(problems, field,
                   QStringLiteral("a kind id has lowercase letters, digits and _, and starts "
                                  "with a letter"));
        return std::nullopt;
    }
    if (bookPlaceFromName(id)) {
        addProblem(problems, field, QStringLiteral("is the name of a place, not a kind id"));
        return std::nullopt;
    }
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    checkKeys(object, field,
              {"form", "name", "plural", "icon", "places", "limit", "title", "numbering",
               "template", "editor", "generated", "settings", "workshopGroup"},
              problems);

    ElementKind kind;
    kind.id = id;

    const QString form = object.value(QStringLiteral("form")).toString();
    if (form == QLatin1String("text")) {
        kind.form = ElementForm::Text;
    } else if (form == QLatin1String("group")) {
        kind.form = ElementForm::Group;
    } else if (form == QLatin1String("window")) {
        kind.form = ElementForm::Window;
    } else {
        addProblem(problems, field + QStringLiteral(".form"),
                   QStringLiteral("must be \"text\", \"group\" or \"window\""));
    }

    kind.name = readText(object.value(QStringLiteral("name")), field + QStringLiteral(".name"),
                         true, problems);
    kind.plural = readText(object.value(QStringLiteral("plural")),
                           field + QStringLiteral(".plural"), true, problems);
    kind.icon = readIcon(object.value(QStringLiteral("icon")), field + QStringLiteral(".icon"),
                         problems);

    const QString placesField = field + QStringLiteral(".places");
    const QJsonValue places = object.value(QStringLiteral("places"));
    kind.places = readIdList(places, placesField, isIdentifier,
                             QStringLiteral("place or group kind"), true, problems);
    if (places.isUndefined() || (places.isArray() && places.toArray().isEmpty())) {
        addProblem(problems, placesField,
                   QStringLiteral("must list where the kind can be: front, main, back, "
                                  "workshop or groups it can be inside"));
    }
    if (kind.form == ElementForm::Group) {
        for (const QString& place : kind.places) {
            const auto bookPlace = bookPlaceFromName(place);
            if (bookPlace && *bookPlace != BookPlace::Main) {
                addProblem(problems, placesField,
                           QStringLiteral("a group can only be in the main part or inside "
                                          "another group, not in '%1'")
                               .arg(place));
            }
        }
    }

    const QJsonValue limit = object.value(QStringLiteral("limit"));
    if (!limit.isUndefined()) {
        if (!isWholeNumber(limit) || limit.toDouble() < 1) {
            addProblem(problems, field + QStringLiteral(".limit"),
                       QStringLiteral("must be a whole number of at least 1"));
        } else {
            kind.limit = limit.toInt();
        }
    }

    kind.title = readText(object.value(QStringLiteral("title")), field + QStringLiteral(".title"),
                          false, problems);
    const auto numbered = std::count_if(
        kind.title.values.cbegin(), kind.title.values.cend(),
        [](const QString& title) { return title.contains(QStringLiteral("%n")); });
    if (numbered > 0 && numbered < kind.title.values.size()) {
        addProblem(problems, field + QStringLiteral(".title"),
                   QStringLiteral("\"%n\" must be in the title in every language or in none"));
    }

    const QJsonValue numbering = object.value(QStringLiteral("numbering"));
    if (!numbering.isUndefined()) {
        if (numbering.toString() == QLatin1String("arabic")) {
            kind.numbering = TitleNumbering::Arabic;
        } else if (numbering.toString() == QLatin1String("roman")) {
            kind.numbering = TitleNumbering::Roman;
        } else {
            addProblem(problems, field + QStringLiteral(".numbering"),
                       QStringLiteral("must be \"arabic\" or \"roman\""));
        }
        if (numbered == 0) {
            addProblem(problems, field + QStringLiteral(".numbering"),
                       QStringLiteral("needs \"%n\" in the title"));
        }
    }

    const QJsonValue templateFile = object.value(QStringLiteral("template"));
    if (!templateFile.isUndefined()) {
        if (kind.form != ElementForm::Text) {
            addProblem(problems, field + QStringLiteral(".template"),
                       QStringLiteral("only a text kind has a template"));
        } else if (checkPackageFile(directory, templateFile, QStringLiteral(".kchapter"),
                                    field + QStringLiteral(".template"), problems)) {
            kind.templateFile = templateFile.toString();
        }
    }

    const QJsonValue editor = object.value(QStringLiteral("editor"));
    if (kind.form == ElementForm::Window) {
        if (!editor.isString() || !isIdentifier(editor.toString())) {
            addProblem(problems, field + QStringLiteral(".editor"),
                       QStringLiteral("a window kind needs the id of the window that opens it, "
                                      "e.g. \"mindmap\""));
        } else {
            kind.editor = editor.toString();
        }
    } else if (!editor.isUndefined()) {
        addProblem(problems, field + QStringLiteral(".editor"),
                   QStringLiteral("only a window kind has an editor"));
    }

    const QJsonValue generated = object.value(QStringLiteral("generated"));
    if (!generated.isUndefined()) {
        if (!generated.isBool()) {
            addProblem(problems, field + QStringLiteral(".generated"),
                       QStringLiteral("must be true or false"));
        } else if (kind.form != ElementForm::Window) {
            addProblem(problems, field + QStringLiteral(".generated"),
                       QStringLiteral("only a window kind can be made by a tool"));
        } else {
            kind.generated = generated.toBool();
        }
    }

    const QJsonValue settings = object.value(QStringLiteral("settings"));
    if (!settings.isUndefined()) {
        if (!settings.isObject()) {
            addProblem(problems, field + QStringLiteral(".settings"),
                       QStringLiteral("must be an object"));
        } else if (kind.form != ElementForm::Window) {
            addProblem(problems, field + QStringLiteral(".settings"),
                       QStringLiteral("only a window kind has settings"));
        } else {
            kind.settings = settings.toObject();
        }
    }

    const QJsonValue workshopGroup = object.value(QStringLiteral("workshopGroup"));
    if (!workshopGroup.isUndefined()) {
        const auto group = workshopGroupFromName(workshopGroup.toString());
        if (!workshopGroup.isString() || !group) {
            addProblem(problems, field + QStringLiteral(".workshopGroup"),
                       QStringLiteral("must be \"libraries\" or \"resources\""));
        } else if (!kind.allows(BookPlace::Workshop)) {
            addProblem(problems, field + QStringLiteral(".workshopGroup"),
                       QStringLiteral("only a kind of the Workshop goes to a group of the "
                                      "Workshop"));
        } else {
            kind.workshopGroup = *group;
        }
    }

    return kind;
}

// -----------------------------------------------------------------------------
// Styles
// -----------------------------------------------------------------------------

/// Value types of style properties
enum class PropertyType { Text, Size, Number, Distance, Factor, Flag, Color, Alignment };

struct PropertySpec {
    const char* name;
    PropertyType type;
};

/// Properties that StyleResolver applies to paragraphs (sizes and distances in points)
constexpr PropertySpec PARAGRAPH_PROPERTIES[] = {
    {"fontFamily", PropertyType::Text},        {"fontSize", PropertyType::Size},
    {"bold", PropertyType::Flag},              {"italic", PropertyType::Flag},
    {"underline", PropertyType::Flag},         {"textColor", PropertyType::Color},
    {"alignment", PropertyType::Alignment},    {"firstLineIndent", PropertyType::Number},
    {"leftMargin", PropertyType::Distance},    {"rightMargin", PropertyType::Distance},
    {"spaceBefore", PropertyType::Distance},   {"spaceAfter", PropertyType::Distance},
    {"lineHeight", PropertyType::Factor},
};

/// Properties that StyleResolver applies to characters
constexpr PropertySpec CHARACTER_PROPERTIES[] = {
    {"fontFamily", PropertyType::Text},   {"fontSize", PropertyType::Size},
    {"bold", PropertyType::Flag},         {"italic", PropertyType::Flag},
    {"underline", PropertyType::Flag},    {"strikethrough", PropertyType::Flag},
    {"textColor", PropertyType::Color},   {"backgroundColor", PropertyType::Color},
};

/// What is wrong with @p value as a property of @p type; empty when it is right
QString propertyError(PropertyType type, const QJsonValue& value) {
    switch (type) {
        case PropertyType::Text:
            return value.isString() && !value.toString().isEmpty()
                       ? QString()
                       : QStringLiteral("must be a text that is not empty");
        case PropertyType::Size:
            return isWholeNumber(value) && value.toDouble() >= 1
                       ? QString()
                       : QStringLiteral("must be a whole number of points, at least 1");
        case PropertyType::Number:
            return value.isDouble() ? QString() : QStringLiteral("must be a number of points");
        case PropertyType::Distance:
            return value.isDouble() && value.toDouble() >= 0
                       ? QString()
                       : QStringLiteral("must be a number of points, at least 0");
        case PropertyType::Factor:
            return value.isDouble() && value.toDouble() > 0
                       ? QString()
                       : QStringLiteral("must be a number greater than 0");
        case PropertyType::Flag:
            return value.isBool() ? QString() : QStringLiteral("must be true or false");
        case PropertyType::Color:
            return value.isString() && isColor(value.toString())
                       ? QString()
                       : QStringLiteral("must be a color \"#RRGGBB\" or \"#AARRGGBB\"");
        case PropertyType::Alignment: {
            const QString alignment = value.toString();
            const bool known = alignment == QLatin1String("left") ||
                               alignment == QLatin1String("center") ||
                               alignment == QLatin1String("right") ||
                               alignment == QLatin1String("justify");
            return value.isString() && known
                       ? QString()
                       : QStringLiteral("must be \"left\", \"center\", \"right\" or \"justify\"");
        }
    }
    return {};
}

template <std::size_t N>
QVariantMap readProperties(const QJsonValue& value, const QString& field,
                           const PropertySpec (&specs)[N], QStringList& problems) {
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object of style properties"));
        return {};
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString key = it.key();
        const auto* spec = std::find_if(std::begin(specs), std::end(specs),
                                        [&key](const PropertySpec& property) {
                                            return key == QLatin1String(property.name);
                                        });
        if (spec == std::end(specs)) {
            addProblem(problems, field, QStringLiteral("unknown property '%1'").arg(key));
            continue;
        }
        const QString error = propertyError(spec->type, it.value());
        if (!error.isEmpty()) {
            addProblem(problems, field + QLatin1Char('.') + key, error);
        }
    }
    return object.toVariantMap();
}

/// Field of a style: "paragraph_styles.heading_1", or with its position while the id is wrong
QString styleField(const char* list, const QString& id, qsizetype index) {
    return isIdentifier(id) ? QStringLiteral("%1.%2").arg(QLatin1String(list), id)
                            : QStringLiteral("%1[%2]").arg(QLatin1String(list)).arg(index);
}

void readStyles(const QString& directory, const QString& file, BookTypePackage& package,
                QStringList& problems) {
    const auto object = readJsonObject(QDir(directory).filePath(file), file, problems);
    if (!object) {
        return;
    }
    checkKeys(*object, file, {"paragraph_styles", "character_styles"}, problems);

    const QJsonValue paragraphs = object->value(QStringLiteral("paragraph_styles"));
    if (!paragraphs.isUndefined() && !paragraphs.isArray()) {
        addProblem(problems, QStringLiteral("paragraph_styles"),
                   QStringLiteral("must be a list of styles"));
    }
    const QJsonArray paragraphList = paragraphs.toArray();
    for (qsizetype i = 0; i < paragraphList.size(); ++i) {
        const QJsonObject entry = paragraphList.at(i).toObject();
        PackageParagraphStyle style;
        style.id = entry.value(QStringLiteral("id")).toString();
        const QString field = styleField("paragraph_styles", style.id, i);
        if (!paragraphList.at(i).isObject()) {
            addProblem(problems, field, QStringLiteral("must be an object"));
            continue;
        }
        checkKeys(entry, field, {"id", "name", "base_style", "next_style", "properties"},
                  problems);
        if (!isIdentifier(style.id)) {
            addProblem(problems, field,
                       QStringLiteral("needs an id of lowercase letters, digits and _"));
        } else if (std::any_of(package.paragraphStyles.cbegin(), package.paragraphStyles.cend(),
                               [&style](const PackageParagraphStyle& other) {
                                   return other.id == style.id;
                               })) {
            addProblem(problems, field, QStringLiteral("is defined twice"));
        }
        style.name = readText(entry.value(QStringLiteral("name")), field + QStringLiteral(".name"),
                              true, problems);
        for (const auto& [key, member] :
             {std::pair{"base_style", &PackageParagraphStyle::baseStyle},
              std::pair{"next_style", &PackageParagraphStyle::nextStyle}}) {
            const QJsonValue reference = entry.value(QLatin1String(key));
            if (reference.isUndefined()) {
                continue;
            }
            if (!reference.isString() || !isIdentifier(reference.toString())) {
                addProblem(problems, field + QLatin1Char('.') + QLatin1String(key),
                           QStringLiteral("must be the id of a paragraph style"));
            } else {
                style.*member = reference.toString();
            }
        }
        if (!style.baseStyle.isEmpty() && style.baseStyle == style.id) {
            addProblem(problems, field + QStringLiteral(".base_style"),
                       QStringLiteral("a style cannot inherit from itself"));
        }
        style.properties = readProperties(entry.value(QStringLiteral("properties")),
                                          field + QStringLiteral(".properties"),
                                          PARAGRAPH_PROPERTIES, problems);
        package.paragraphStyles.append(style);
    }

    const QJsonValue characters = object->value(QStringLiteral("character_styles"));
    if (!characters.isUndefined() && !characters.isArray()) {
        addProblem(problems, QStringLiteral("character_styles"),
                   QStringLiteral("must be a list of styles"));
    }
    const QJsonArray characterList = characters.toArray();
    for (qsizetype i = 0; i < characterList.size(); ++i) {
        const QJsonObject entry = characterList.at(i).toObject();
        PackageCharacterStyle style;
        style.id = entry.value(QStringLiteral("id")).toString();
        const QString field = styleField("character_styles", style.id, i);
        if (!characterList.at(i).isObject()) {
            addProblem(problems, field, QStringLiteral("must be an object"));
            continue;
        }
        checkKeys(entry, field, {"id", "name", "properties"}, problems);
        if (!isIdentifier(style.id)) {
            addProblem(problems, field,
                       QStringLiteral("needs an id of lowercase letters, digits and _"));
        } else if (std::any_of(package.characterStyles.cbegin(), package.characterStyles.cend(),
                               [&style](const PackageCharacterStyle& other) {
                                   return other.id == style.id;
                               })) {
            addProblem(problems, field, QStringLiteral("is defined twice"));
        }
        style.name = readText(entry.value(QStringLiteral("name")), field + QStringLiteral(".name"),
                              true, problems);
        style.properties = readProperties(entry.value(QStringLiteral("properties")),
                                          field + QStringLiteral(".properties"),
                                          CHARACTER_PROPERTIES, problems);
        package.characterStyles.append(style);
    }
}

QString toRoman(int number) {
    static const std::pair<int, const char*> NUMERALS[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
        {40, "XL"},  {10, "X"},   {9, "IX"},  {5, "V"},    {4, "IV"},  {1, "I"}};
    QString roman;
    for (const auto& [value, letters] : NUMERALS) {
        while (number >= value) {
            roman += QLatin1String(letters);
            number -= value;
        }
    }
    return roman;
}

}  // namespace

// =============================================================================
// LocalizedText, places, ElementKind
// =============================================================================

QString LocalizedText::text(const QString& language) const {
    QString exact = values.value(language);
    if (!exact.isEmpty()) {
        return exact;
    }
    const qsizetype separator = language.indexOf(QRegularExpression(QStringLiteral("[_-]")));
    if (separator > 0) {
        QString general = values.value(language.left(separator));
        if (!general.isEmpty()) {
            return general;
        }
    }
    return values.value(QStringLiteral("en"));
}

QString bookPlaceName(BookPlace place) {
    return QLatin1String(PLACE_NAMES[static_cast<int>(place)]);
}

std::optional<BookPlace> bookPlaceFromName(const QString& name) {
    for (int i = 0; i < static_cast<int>(std::size(PLACE_NAMES)); ++i) {
        if (name == QLatin1String(PLACE_NAMES[i])) {
            return static_cast<BookPlace>(i);
        }
    }
    return std::nullopt;
}

QString workshopGroupName(WorkshopGroup group) {
    return group == WorkshopGroup::None
               ? QString()
               : QLatin1String(WORKSHOP_GROUP_NAMES[static_cast<int>(group) - 1]);
}

std::optional<WorkshopGroup> workshopGroupFromName(const QString& name) {
    for (int i = 0; i < static_cast<int>(std::size(WORKSHOP_GROUP_NAMES)); ++i) {
        if (name == QLatin1String(WORKSHOP_GROUP_NAMES[i])) {
            return static_cast<WorkshopGroup>(i + 1);
        }
    }
    return std::nullopt;
}

std::optional<KindReference> KindReference::parse(const QString& text) {
    const qsizetype separator = text.indexOf(QLatin1Char(':'));
    KindReference reference;
    if (separator >= 0) {
        reference.packageId = text.left(separator);
        if (!isPackageId(reference.packageId)) {
            return std::nullopt;
        }
    }
    reference.kindId = text.mid(separator + 1);
    if (!isIdentifier(reference.kindId)) {
        return std::nullopt;
    }
    return reference;
}

QString KindReference::toString() const {
    return packageId.isEmpty() ? kindId : packageId + QLatin1Char(':') + kindId;
}

bool ElementKind::allows(BookPlace place) const {
    return places.contains(bookPlaceName(place));
}

bool ElementKind::allowsInside(const QString& groupKindId) const {
    return !bookPlaceFromName(groupKindId) && places.contains(groupKindId);
}

QString ElementKind::defaultTitle(const QString& language, int number) const {
    QString text = title.isEmpty() ? name.text(language) : title.text(language);
    if (text.contains(QStringLiteral("%n"))) {
        const bool roman = numbering == TitleNumbering::Roman && number >= 1 && number <= 3999;
        text.replace(QStringLiteral("%n"), roman ? toRoman(number) : QString::number(number));
    }
    return text;
}

// =============================================================================
// BookTypePackage
// =============================================================================

const ElementKind* BookTypePackage::ownKind(const QString& kindId) const {
    const auto it = std::find_if(kinds.cbegin(), kinds.cend(),
                                 [&kindId](const ElementKind& kind) { return kind.id == kindId; });
    return it == kinds.cend() ? nullptr : &*it;
}

const QStringList& BookTypePackage::kindsIn(BookPlace place) const {
    switch (place) {
        case BookPlace::Front:
            return frontKinds;
        case BookPlace::Main:
            return mainKinds;
        case BookPlace::Back:
            return backKinds;
        case BookPlace::Workshop:
            return workshopKinds;
    }
    return mainKinds;
}

std::optional<BookTypePackage> BookTypePackage::read(const QString& directory,
                                                     QStringList& problems) {
    const qsizetype problemsBefore = problems.size();
    const QDir folder(directory);
    const QString manifestFile = QString::fromLatin1(MANIFEST_FILE);
    const auto manifest = readJsonObject(folder.filePath(manifestFile), manifestFile, problems);
    if (!manifest) {
        return std::nullopt;
    }

    // The format first: another format may have other keys
    const QJsonValue format = manifest->value(QStringLiteral("format"));
    if (!isWholeNumber(format) || format.toInt() != FORMAT) {
        addProblem(problems, QStringLiteral("format"),
                   QStringLiteral("must be %1, the format this version of Kalahari reads")
                       .arg(FORMAT));
        return std::nullopt;
    }
    checkKeys(*manifest, manifestFile,
              {"format", "id", "version", "role", "name", "description", "icon", "uses", "kinds",
               "front", "main", "back", "workshop", "primary", "start", "partsLayer", "styles"},
              problems);

    BookTypePackage package;
    package.directory = folder.absolutePath();

    package.id = manifest->value(QStringLiteral("id")).toString();
    if (!isPackageId(package.id)) {
        addProblem(problems, QStringLiteral("id"),
                   QStringLiteral("must be lowercase words joined by dots, e.g. "
                                  "\"kalahari.novel\""));
    }

    package.version = manifest->value(QStringLiteral("version")).toString();
    if (!isVersion(package.version)) {
        addProblem(problems, QStringLiteral("version"),
                   QStringLiteral("must be a version such as \"1.0\" or \"1.2.3\""));
    }

    const QString roleName = manifest->value(QStringLiteral("role")).toString();
    if (roleName == QLatin1String("type")) {
        package.role = PackageRole::Type;
    } else if (roleName == QLatin1String("shared")) {
        package.role = PackageRole::Shared;
    } else {
        addProblem(problems, QStringLiteral("role"),
                   QStringLiteral("must be \"type\" (a book type) or \"shared\" (kinds and "
                                  "styles for types)"));
    }

    package.name = readText(manifest->value(QStringLiteral("name")), QStringLiteral("name"), true,
                            problems);
    package.description = readText(manifest->value(QStringLiteral("description")),
                                   QStringLiteral("description"), false, problems);
    package.icon = readIcon(manifest->value(QStringLiteral("icon")), QStringLiteral("icon"),
                            problems);

    package.uses = readIdList(manifest->value(QStringLiteral("uses")), QStringLiteral("uses"),
                              isPackageId, QStringLiteral("package id"), true, problems);
    if (package.uses.contains(package.id)) {
        addProblem(problems, QStringLiteral("uses"), QStringLiteral("a package cannot use itself"));
    }

    const QJsonValue kindsValue = manifest->value(QStringLiteral("kinds"));
    if (!kindsValue.isUndefined() && !kindsValue.isObject()) {
        addProblem(problems, QStringLiteral("kinds"),
                   QStringLiteral("must be an object of kinds by their ids"));
    }
    const QJsonObject kindObject = kindsValue.toObject();
    for (auto it = kindObject.constBegin(); it != kindObject.constEnd(); ++it) {
        if (auto kind = readKind(it.key(), it.value(), package.directory, problems)) {
            package.kinds.append(std::move(*kind));
        }
    }

    for (const BookPlace place :
         {BookPlace::Front, BookPlace::Main, BookPlace::Back, BookPlace::Workshop}) {
        const QString placeName = bookPlaceName(place);
        const QStringList list = readIdList(manifest->value(placeName), placeName, isKindName,
                                            QStringLiteral("kind id"), true, problems);
        switch (place) {
            case BookPlace::Front:
                package.frontKinds = list;
                break;
            case BookPlace::Main:
                package.mainKinds = list;
                break;
            case BookPlace::Back:
                package.backKinds = list;
                break;
            case BookPlace::Workshop:
                package.workshopKinds = list;
                break;
        }
    }

    const QJsonValue primary = manifest->value(QStringLiteral("primary"));
    if (primary.isUndefined()) {
        if (package.role == PackageRole::Type) {
            addProblem(problems, QStringLiteral("primary"),
                       QStringLiteral("a book type needs its main text kind, e.g. \"chapter\""));
        }
    } else if (!primary.isString() || !isKindName(primary.toString())) {
        addProblem(problems, QStringLiteral("primary"), QStringLiteral("must be a kind id"));
    } else {
        package.primaryKind = primary.toString();
    }

    package.startKinds = readIdList(manifest->value(QStringLiteral("start")),
                                    QStringLiteral("start"), isKindName,
                                    QStringLiteral("kind id"), false, problems);
    if (package.role == PackageRole::Shared && !package.startKinds.isEmpty()) {
        addProblem(problems, QStringLiteral("start"),
                   QStringLiteral("only a book type has elements to start with"));
    }

    const QJsonValue partsLayer = manifest->value(QStringLiteral("partsLayer"));
    if (!partsLayer.isUndefined()) {
        if (!partsLayer.isBool()) {
            addProblem(problems, QStringLiteral("partsLayer"),
                       QStringLiteral("must be true or false"));
        } else if (package.role == PackageRole::Shared) {
            addProblem(problems, QStringLiteral("partsLayer"),
                       QStringLiteral("only a book type says whether a new book shows its parts"));
        } else {
            package.partsLayer = partsLayer.toBool();
        }
    }

    const QJsonValue styles = manifest->value(QStringLiteral("styles"));
    if (!styles.isUndefined() && checkPackageFile(package.directory, styles,
                                                  QStringLiteral(".json"),
                                                  QStringLiteral("styles"), problems)) {
        package.stylesFile = styles.toString();
        readStyles(package.directory, package.stylesFile, package, problems);
    }

    if (problems.size() > problemsBefore) {
        return std::nullopt;
    }
    return package;
}

}  // namespace kalahari::core
