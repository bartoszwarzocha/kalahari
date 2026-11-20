// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_project.cpp
/// @brief Book project as its .klh file saves it

#include <kalahari/core/book_project.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/card_field_set.h>
#include <kalahari/core/clipping.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/page_start.h>
#include <kalahari/core/publishing_texts.h>
#include <kalahari/core/source_description.h>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRectF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

namespace kalahari::core {

namespace {

/// The numbers of an area of a highlight: left, top, width, height
constexpr qsizetype AREA_NUMBERS = 4;

/// The places of a highlight are kept to a hundredth of a point
constexpr double POINT_HUNDREDTHS = 100.0;

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

/// @brief A field of the plot of a scene: its key and where ScenePlot keeps it
struct SceneField {
    const char* key;             ///< Key in the .klh file and attribute of a scene break
    QString ScenePlot::*member;  ///< The field of ScenePlot
};

/// The fields of the plot of a scene (ScenePlot::toAttributes()), in the order they are written
constexpr std::array<SceneField, 7> SCENE_FIELDS{{
    {.key = "title", .member = &ScenePlot::title},
    {.key = "description", .member = &ScenePlot::description},
    {.key = "pov", .member = &ScenePlot::pov},
    {.key = "place", .member = &ScenePlot::place},
    {.key = "time", .member = &ScenePlot::time},
    {.key = "color", .member = &ScenePlot::color},
    {.key = "status", .member = &ScenePlot::status},
}};

const BookPart& bookPart(BookPlace place) {
    const auto* found = std::find_if(std::begin(BOOK_PARTS), std::end(BOOK_PARTS),
                                     [place](const BookPart& part) { return part.place == place; });
    Q_ASSERT(found != std::end(BOOK_PARTS));
    return *found;
}

void addProblem(QStringList& problems, const QString& field, const QString& message) {
    problems << field + QStringLiteral(": ") + message;
}

/// A color of a card: "#RRGGBB"
bool isColor(const QString& text) {
    static const QRegularExpression color(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return color.match(text).hasMatch();
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
    [[nodiscard]] const QJsonObject& rest() const { return m_rest; }

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

/// The web address of the element of @p fields: an absolute one, such as "https://..."; empty
/// when it has none
QString readUrl(Fields& fields) {
    const QJsonValue value = fields.take("url");
    if (value.isUndefined()) {
        return {};
    }
    const QUrl url(value.toString(), QUrl::StrictMode);
    if (!value.isString() || !url.isValid() || url.isRelative()) {
        fields.problem("url", QStringLiteral("must be a web address, e.g. "
                                             "\"https://pl.wikipedia.org/wiki/Kalahari\""));
        return {};
    }
    return value.toString();
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

/// A link with its details: what it links, how, the paragraph and a note
std::optional<ElementLink> readLinkObject(const QJsonObject& object, const QString& field,
                                          QStringList& problems) {
    QStringList found;
    Fields fields(object, field, found);
    ElementLink link;
    link.to = fields.string("to", true);
    link.role = fields.string("role");
    const QJsonValue paragraph = fields.take("paragraph");
    if (!paragraph.isUndefined()) {
        const int number = isWholeNumber(paragraph) ? paragraph.toInt(-1) : -1;
        if (number < 0) {
            fields.problem("paragraph", QStringLiteral("must be a whole number, 0 or more"));
        } else {
            link.paragraph = number;
        }
    }
    link.excerpt = fields.string("excerpt");
    link.note = fields.string("note");
    link.extra = fields.rest();
    problems << found;
    if (!found.isEmpty()) {
        return std::nullopt;
    }
    return link;
}

/// What an element is about: ids of elements and of sections, or links with their details
/// (ElementLink), each element or section once
QList<ElementLink> readLinks(Fields& fields, QStringList& problems) {
    QList<ElementLink> links;
    const QJsonArray values = fields.array("links");
    for (qsizetype i = 0; i < values.size(); ++i) {
        const QJsonValue value = values.at(i);
        std::optional<ElementLink> link;
        if (value.isString() && !value.toString().isEmpty()) {
            link = ElementLink{value.toString(), {}, 0, {}, {}, {}};
        } else if (value.isObject()) {
            link = readLinkObject(value.toObject(),
                                  QStringLiteral("%1[%2]").arg(fields.path("links")).arg(i),
                                  problems);
            if (!link) {
                return {};
            }
        } else {
            fields.problem("links", QStringLiteral("must be a list of ids of elements and "
                                                   "sections, or of links with \"to\""));
            return {};
        }
        const bool twice =
            std::any_of(links.cbegin(), links.cend(),
                        [&link](const ElementLink& each) { return each.to == link->to; });
        if (!twice) {
            links << *link;
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

/// What a material of the cover is and where it goes, with the fields of "cover" this
/// version does not know
void readCover(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue cover = fields.take("cover");
    if (cover.isUndefined()) {
        return;
    }
    if (!cover.isObject()) {
        fields.problem("cover", QStringLiteral("must be an object"));
        return;
    }
    Fields coverFields(cover.toObject(), fields.path("cover"), problems);
    CoverData& data = element.cover;
    data.part = coverFields.string("part");
    data.place = coverFields.string("place");
    data.state = coverFields.string("state");
    data.author = coverFields.string("author");
    data.role = coverFields.string("role");
    data.source = coverFields.string("source");
    data.date = coverFields.string("date");
    data.consent = coverFields.string("consent");
    data.license = coverFields.string("license");
    data.about = coverFields.string("about");
    for (const auto& [key, target] : {std::pair{"targetCharacters", &data.targetCharacters},
                                      std::pair{"targetWords", &data.targetWords}}) {
        const QJsonValue value = coverFields.take(key);
        if (value.isUndefined()) {
            continue;
        }
        const int length = isWholeNumber(value) ? value.toInt(-1) : -1;
        if (length < 0) {
            coverFields.problem(key, QStringLiteral("must be a whole number, 0 or more"));
        } else {
            *target = length;
        }
    }
    data.extra = coverFields.rest();
}

/// Where a clipping was cut from, from the object @p value of "origin"
void readClippingOrigin(const QJsonValue& value, const QString& field, ClippingOrigin& origin,
                        QStringList& problems) {
    Fields originFields(value.toObject(), field, problems);
    origin.element = originFields.string("element");
    origin.title = originFields.string("title");
    if (const QJsonValue paragraph = originFields.take("paragraph"); !paragraph.isUndefined()) {
        const int number = isWholeNumber(paragraph) ? paragraph.toInt(-1) : -1;
        if (number < 0) {
            originFields.problem("paragraph", QStringLiteral("must be a whole number, 0 or more"));
        } else {
            origin.paragraph = number;
        }
    }
    origin.before = originFields.string("before");
    origin.after = originFields.string("after");
    origin.cut = originFields.dateTime("cut");
    origin.extra = originFields.rest();
}

/// Where a clipping was inserted, from the array "inserted" of @p clippingFields
void readClippingInsertions(Fields& clippingFields, ClippingData& data, QStringList& problems) {
    const QString insertedField = clippingFields.path("inserted");
    const QJsonArray inserted = clippingFields.array("inserted");
    for (qsizetype i = 0; i < inserted.size(); ++i) {
        const QString field = QStringLiteral("%1[%2]").arg(insertedField).arg(i);
        if (!inserted.at(i).isObject()) {
            addProblem(problems, field, QStringLiteral("must be an object"));
            continue;
        }
        Fields insertionFields(inserted.at(i).toObject(), field, problems);
        ClippingInsertion insertion;
        insertion.element = insertionFields.string("element");
        insertion.at = insertionFields.dateTime("at");
        insertion.extra = insertionFields.rest();
        data.inserted << insertion;
    }
}

/// Where a clipping was cut from and where it was inserted, with the fields of "clipping" this
/// version does not know
void readClipping(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue clipping = fields.take("clipping");
    if (clipping.isUndefined()) {
        return;
    }
    if (!clipping.isObject()) {
        fields.problem("clipping", QStringLiteral("must be an object"));
        return;
    }
    Fields clippingFields(clipping.toObject(), fields.path("clipping"), problems);
    ClippingData& data = element.clipping;
    if (const QJsonValue origin = clippingFields.take("origin"); origin.isObject()) {
        readClippingOrigin(origin, clippingFields.path("origin"), data.origin, problems);
    } else if (!origin.isUndefined()) {
        clippingFields.problem("origin", QStringLiteral("must be an object"));
    }
    readClippingInsertions(clippingFields, data, problems);
    data.extra = clippingFields.rest();
}

/// The portraits of a card, pictures of the materials or put together from parts:
/// [{"image": "e31", "label": "youth"}, {"label": "main", "style": "flat", "avatar": {...}}]
QList<CardPortrait> readPortraits(Fields& fields, QStringList& problems) {
    QList<CardPortrait> portraits;
    const QJsonArray array = fields.array("portraits");
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("portraits")).arg(i);
        if (!array.at(i).isObject()) {
            addProblem(problems, path,
                       QStringLiteral("must be an object with \"image\" or \"avatar\""));
            continue;
        }
        Fields entry(array.at(i).toObject(), path, problems);
        CardPortrait portrait;
        portrait.image = entry.string("image");
        portrait.label = entry.string("label");
        portrait.style = entry.string("style");
        if (const QJsonValue avatar = entry.take("avatar"); avatar.isObject()) {
            portrait.avatar = avatar.toObject();
        } else if (!avatar.isUndefined()) {
            entry.problem("avatar", QStringLiteral("must be an object of the parts and colors"));
        }
        portrait.extra = entry.rest();
        if (portrait.image.isEmpty() && portrait.avatar.isEmpty()) {
            addProblem(problems, path, QStringLiteral("must have \"image\" or \"avatar\""));
        } else {
            portraits << portrait;
        }
    }
    return portraits;
}

/// What a card has beside its text: the values of its fields, its color and its portraits,
/// with the fields of "card" this version does not know
void readCard(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue card = fields.take("card");
    if (card.isUndefined()) {
        return;
    }
    if (!card.isObject()) {
        fields.problem("card", QStringLiteral("must be an object"));
        return;
    }
    Fields cardFields(card.toObject(), fields.path("card"), problems);
    CardData& data = element.card;
    const QJsonValue values = cardFields.take("values");
    if (values.isObject()) {
        const QJsonObject object = values.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isString()) {
                data.values.insert(it.key(), it.value().toString());
            } else {
                cardFields.problem("values",
                                   QStringLiteral("'%1' must be a text").arg(it.key()));
            }
        }
    } else if (!values.isUndefined()) {
        cardFields.problem("values", QStringLiteral("must be an object of fields and their "
                                                    "values"));
    }
    data.color = cardFields.string("color");
    if (!data.color.isEmpty() && !isColor(data.color)) {
        cardFields.problem("color", QStringLiteral("must be a color such as \"#c0392b\""));
        data.color.clear();
    }
    data.portraits = readPortraits(cardFields, problems);
    data.extra = cardFields.rest();
}

/// What a scene says (the first scene of a text, a scene in the plan), from the fields of
/// @p fields that have it; the fields this version does not know are kept
ScenePlot readScenePlot(Fields& fields) {
    QMap<QString, QString> attributes;
    for (const SceneField& field : SCENE_FIELDS) {
        if (const QString value = fields.string(field.key); !value.isEmpty()) {
            attributes.insert(QLatin1String(field.key), value);
        }
    }
    ScenePlot plot = ScenePlot::fromAttributes(attributes);
    plot.extra = fields.rest();
    return plot;
}

/// The scenes in the plan of a text, from list @p planned of field @p field
QList<PlannedScene> readPlanned(const QJsonArray& planned, const QString& field,
                                QStringList& problems) {
    QList<PlannedScene> scenes;
    for (qsizetype i = 0; i < planned.size(); ++i) {
        const QString itemField = QStringLiteral("%1[%2]").arg(field).arg(i);
        if (!planned.at(i).isObject()) {
            addProblem(problems, itemField, QStringLiteral("must be an object"));
            continue;
        }
        Fields sceneFields(planned.at(i).toObject(), itemField, problems);
        PlannedScene scene;
        if (const QJsonValue after = sceneFields.take("after"); !after.isUndefined()) {
            scene.after = isWholeNumber(after) ? after.toInt(-1) : -1;
            if (scene.after < 0) {
                sceneFields.problem("after", QStringLiteral("must be a whole number, 0 or more"));
                scene.after = 0;
            }
        }
        scene.plot = readScenePlot(sceneFields);
        scenes << scene;
    }
    return scenes;
}

/// What the plot board says of a text of a book, with the fields of "plot" this version does
/// not know
void readPlot(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue plot = fields.take("plot");
    if (plot.isUndefined()) {
        return;
    }
    if (!plot.isObject()) {
        fields.problem("plot", QStringLiteral("must be an object"));
        return;
    }
    Fields plotFields(plot.toObject(), fields.path("plot"), problems);
    ElementPlot& data = element.plot;
    data.pov = plotFields.string("pov");
    data.place = plotFields.string("place");
    data.time = plotFields.string("time");
    data.color = plotFields.string("color");
    // The first scene of the text, before any scene break
    if (const QJsonValue opening = plotFields.take("opening"); opening.isObject()) {
        Fields openingFields(opening.toObject(), plotFields.path("opening"), problems);
        data.opening = readScenePlot(openingFields);
    } else if (!opening.isUndefined()) {
        plotFields.problem("opening", QStringLiteral("must be an object"));
    }
    data.planned = readPlanned(plotFields.array("planned"), plotFields.path("planned"), problems);
    data.extra = plotFields.rest();
}

/// The bibliographic description of a material that is a source, with the fields of "source"
/// this version does not know
void readSource(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue source = fields.take("source");
    if (source.isUndefined()) {
        return;
    }
    if (!source.isObject()) {
        fields.problem("source", QStringLiteral("must be an object"));
        return;
    }
    Fields sourceFields(source.toObject(), fields.path("source"), problems);
    SourceDescription& data = element.source;
    data.type = sourceFields.string("type");
    const QJsonArray people = sourceFields.array("people");
    for (qsizetype i = 0; i < people.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(sourceFields.path("people")).arg(i);
        if (!people.at(i).isObject()) {
            addProblem(problems, path, QStringLiteral("must be an object with a name"));
            continue;
        }
        Fields personFields(people.at(i).toObject(), path, problems);
        SourcePerson person;
        person.name = personFields.string("name");
        person.role = personFields.string("role");
        person.extra = personFields.rest();
        data.people << person;
    }
    for (const QString& name : SourceDescription::allFields()) {
        const QByteArray key = name.toLatin1();
        data.setValue(name, sourceFields.string(key.constData()));
    }
    data.bibliography = sourceFields.string("bibliography");
    data.extra = sourceFields.rest();
}

/// The place @p value of a highlight: [left, top, width, height] in points, the width and
/// the height above 0; empty for another value
QRectF areaOf(const QJsonValue& value) {
    const QJsonArray numbers = value.toArray();
    if (!value.isArray() || numbers.size() != AREA_NUMBERS ||
        !std::ranges::all_of(numbers, [](const QJsonValue& each) { return each.isDouble(); })) {
        return {};
    }
    const QRectF area(numbers.at(0).toDouble(), numbers.at(1).toDouble(),
                      numbers.at(2).toDouble(), numbers.at(3).toDouble());
    return area.width() > 0 && area.height() > 0 ? area : QRectF();
}

/// The text marked in the PDF file of a material
void readHighlights(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonArray highlights = fields.array("highlights");
    for (qsizetype i = 0; i < highlights.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("highlights")).arg(i);
        if (!highlights.at(i).isObject()) {
            addProblem(problems, path, QStringLiteral("must be an object with a page"));
            continue;
        }
        Fields highlightFields(highlights.at(i).toObject(), path, problems);
        PdfHighlight highlight;
        const QJsonValue page = highlightFields.take("page");
        highlight.page = isWholeNumber(page) ? page.toInt(0) : 0;
        if (highlight.page < 1) {
            highlightFields.problem("page", QStringLiteral("must be a whole number, 1 or more"));
            continue;
        }
        const QJsonArray areas = highlightFields.array("areas");
        for (const auto& each : areas) {
            if (const QRectF area = areaOf(each); !area.isEmpty()) {
                highlight.areas << area;
            }
        }
        if (highlight.areas.isEmpty() || highlight.areas.size() != areas.size()) {
            highlightFields.problem("areas", QStringLiteral("must be a list of [left, top, "
                                                            "width, height] in points"));
            continue;
        }
        highlight.text = highlightFields.string("text");
        highlight.color = highlightFields.string("color");
        highlight.note = highlightFields.string("note");
        highlight.extra = highlightFields.rest();
        element.highlights << highlight;
    }
}

/// The bookmarks of the file of a material and the page shown last
void readBookmarks(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonArray bookmarks = fields.array("bookmarks");
    for (qsizetype i = 0; i < bookmarks.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("bookmarks")).arg(i);
        if (!bookmarks.at(i).isObject()) {
            addProblem(problems, path, QStringLiteral("must be an object with a page"));
            continue;
        }
        Fields bookmarkFields(bookmarks.at(i).toObject(), path, problems);
        PageBookmark bookmark;
        const QJsonValue page = bookmarkFields.take("page");
        const int number = isWholeNumber(page) ? page.toInt(0) : 0;
        if (number < 1) {
            bookmarkFields.problem("page", QStringLiteral("must be a whole number, 1 or more"));
            continue;
        }
        bookmark.page = number;
        bookmark.note = bookmarkFields.string("note");
        bookmark.extra = bookmarkFields.rest();
        element.bookmarks << bookmark;
    }
    const QJsonValue lastPage = fields.take("lastPage");
    if (!lastPage.isUndefined()) {
        const int number = isWholeNumber(lastPage) ? lastPage.toInt(-1) : -1;
        if (number < 0) {
            fields.problem("lastPage", QStringLiteral("must be a whole number, 0 or more"));
        } else {
            element.lastPage = number;
        }
    }
}

/// Where a quote comes from, with the fields of "quote" this version does not know
void readQuote(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue quote = fields.take("quote");
    if (quote.isUndefined()) {
        return;
    }
    if (!quote.isObject()) {
        fields.problem("quote", QStringLiteral("must be an object"));
        return;
    }
    Fields quoteFields(quote.toObject(), fields.path("quote"), problems);
    QuoteSource& data = element.quote;
    data.source = quoteFields.string("source");
    data.page = quoteFields.string("page");
    data.author = quoteFields.string("author");
    data.work = quoteFields.string("work");
    data.extra = quoteFields.rest();
}

/// What a text for publishers is, with the fields of "publishing" this version does not know
void readPublishing(Fields& fields, ProjectElement& element, QStringList& problems) {
    const QJsonValue publishing = fields.take("publishing");
    if (publishing.isUndefined()) {
        return;
    }
    if (!publishing.isObject()) {
        fields.problem("publishing", QStringLiteral("must be an object"));
        return;
    }
    element.publishing =
        PublishingData::fromJson(publishing.toObject(), fields.path("publishing"), problems);
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
    element.url = readUrl(fields);
    element.status = fields.string("status");
    if (const std::optional<PageStart> firstPage = pageStartFromName(fields.string("firstPage"))) {
        element.firstPage = *firstPage;
    } else {
        fields.problem("firstPage", QStringLiteral("must be \"right\", \"left\" or \"next\""));
    }
    element.elements = readElements(fields.array("elements"), fields.path("elements"), seen,
                                    problems);
    element.description = fields.string("description");
    element.fields = readElementFields(fields.array("fields"), fields.path("fields"), problems);
    element.links = readLinks(fields, problems);
    readView(fields, element, problems);
    readCover(fields, element, problems);
    readClipping(fields, element, problems);
    readCard(fields, element, problems);
    readPlot(fields, element, problems);
    readSource(fields, element, problems);
    readBookmarks(fields, element, problems);
    readHighlights(fields, element, problems);
    readQuote(fields, element, problems);
    readPublishing(fields, element, problems);
    element.bookId = fields.string("book");
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

/// The number formats and separators of a numbering of captions
const QStringList& captionFormats() {
    static const QStringList formats = {QStringLiteral("1"), QStringLiteral("a"),
                                        QStringLiteral("A"), QStringLiteral("i"),
                                        QStringLiteral("I")};
    return formats;
}

const QStringList& captionSeparatorNames() {
    static const QStringList separators = {QStringLiteral("."), QStringLiteral("\u2013"),
                                           QStringLiteral(":")};
    return separators;
}

const QStringList& referenceNames() {
    static const QStringList names = {QStringLiteral("number"), QStringLiteral("label"),
                                      QStringLiteral("caption"), QStringLiteral("position")};
    return names;
}

/// The captions of a book: its labels, the numbering of each and what a new cross-reference
/// shows
BookCaptions readCaptions(const QJsonValue& value, const QString& field, QStringList& problems) {
    BookCaptions captions;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return captions;
    }
    Fields fields(value.toObject(), field, problems);

    const QJsonArray labels = fields.array("labels");
    for (qsizetype i = 0; i < labels.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("labels")).arg(i);
        const QString label = labels.at(i).isString() ? labels.at(i).toString().trimmed() : QString();
        if (label.isEmpty()) {
            addProblem(problems, path, QStringLiteral("must be a text that is not empty"));
        } else if (captions.labels.contains(label)) {
            addProblem(problems, path, QStringLiteral("'%1' is already in the list").arg(label));
        } else {
            captions.labels << label;
        }
    }

    const QJsonArray numbering = fields.array("numbering");
    for (qsizetype i = 0; i < numbering.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("numbering")).arg(i);
        if (!numbering.at(i).isObject()) {
            addProblem(problems, path, QStringLiteral("must be an object"));
            continue;
        }
        Fields entry(numbering.at(i).toObject(), path, problems);
        CaptionLabelNumbering label;
        label.label = entry.string("label", true).trimmed();
        if (const QString format = entry.string("format"); captionFormats().contains(format)) {
            label.format = format;
        } else if (!format.isEmpty()) {
            entry.problem("format", QStringLiteral("must be \"1\", \"a\", \"A\", \"i\" or \"I\""));
        }
        label.perChapter = entry.boolean("perChapter", false);
        label.withChapter = entry.boolean("withChapter", false);
        if (const QString separator = entry.string("separator");
            captionSeparatorNames().contains(separator)) {
            label.separator = separator;
        } else if (!separator.isEmpty()) {
            entry.problem("separator", QStringLiteral("must be \".\", \"\u2013\" or \":\""));
        }
        label.extra = entry.rest();
        const bool known = std::any_of(captions.numbering.cbegin(), captions.numbering.cend(),
                                       [&label](const CaptionLabelNumbering& other) {
                                           return other.label == label.label;
                                       });
        if (known) {
            entry.problem("label", QStringLiteral("'%1' already has its numbering").arg(label.label));
        } else if (!label.label.isEmpty()) {
            captions.numbering << label;
        }
    }

    if (const QString reference = fields.string("reference"); referenceNames().contains(reference)) {
        captions.reference = reference;
    } else if (!reference.isEmpty()) {
        fields.problem("reference",
                       QStringLiteral("must be \"number\", \"label\", \"caption\" or \"position\""));
    }
    captions.extra = fields.rest();
    return captions;
}

/// The number formats of a numbering of notes
const QStringList& noteFormats() {
    static const QStringList formats = {QStringLiteral("1"), QStringLiteral("a"),
                                        QStringLiteral("A"), QStringLiteral("i"),
                                        QStringLiteral("I"), QStringLiteral("*")};
    return formats;
}

/// The numbering of a book's footnotes or endnotes; @p numbering has the defaults
void readNoteNumbering(const QJsonValue& value, const QString& field, NoteNumbering& numbering,
                       QStringList& problems) {
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return;
    }
    Fields fields(value.toObject(), field, problems);
    if (const QString format = fields.string("format"); noteFormats().contains(format)) {
        numbering.format = format;
    } else if (!format.isEmpty()) {
        fields.problem("format",
                       QStringLiteral("must be \"1\", \"a\", \"A\", \"i\", \"I\" or \"*\""));
    }
    numbering.perChapter = fields.boolean("perChapter", numbering.perChapter);
    numbering.extra = fields.rest();
}

/// The notes of a book: their numbering and the place of the endnotes
BookFootnotes readFootnotes(const QJsonValue& value, const QString& field,
                            QStringList& problems) {
    BookFootnotes footnotes;
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return footnotes;
    }
    Fields fields(value.toObject(), field, problems);
    if (const QJsonValue numbering = fields.take("footnotes"); !numbering.isUndefined()) {
        readNoteNumbering(numbering, fields.path("footnotes"), footnotes.footnotes, problems);
    }
    if (const QJsonValue numbering = fields.take("endnotes"); !numbering.isUndefined()) {
        readNoteNumbering(numbering, fields.path("endnotes"), footnotes.endnotes, problems);
        // Where they are: a field of the endnotes, the rest of them saved back
        const QJsonValue place = footnotes.endnotes.extra.take(QLatin1String("place"));
        if (place == QJsonValue(QStringLiteral("book"))) {
            footnotes.endnotesAtBookEnd = true;
        } else if (!place.isUndefined() && place != QJsonValue(QStringLiteral("chapter"))) {
            addProblem(problems, fields.path("endnotes") + QStringLiteral(".place"),
                       QStringLiteral("must be \"chapter\" or \"book\""));
        }
    }
    footnotes.extra = fields.rest();
    return footnotes;
}

/// The set for publishers of a book, its field "publishingSet"; nullopt without it
std::optional<PublishingSet> readPublishingSet(Fields& fields, QStringList& problems) {
    const QJsonValue set = fields.take("publishingSet");
    if (set.isObject()) {
        return PublishingSet::fromJson(set.toObject(), fields.path("publishingSet"), problems);
    }
    if (!set.isUndefined()) {
        fields.problem("publishingSet", QStringLiteral("must be an object"));
    }
    return std::nullopt;
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
    if (const QJsonValue captions = fields.take("captions"); !captions.isUndefined()) {
        book.captions = readCaptions(captions, fields.path("captions"), problems);
    }
    if (const QJsonValue footnotes = fields.take("footnotes"); !footnotes.isUndefined()) {
        book.footnotes = readFootnotes(footnotes, fields.path("footnotes"), problems);
    }
    book.bibliographyStyle = fields.string("bibliographyStyle");
    book.publishingSet = readPublishingSet(fields, problems);
    book.isbn = fields.string("isbn");
    book.publisher = fields.string("publisher");
    book.extra = fields.rest();
    return book;
}

/// A relation between cards that the writer named (@p path), after the relations @p before
OwnRelation readOwnRelation(const QJsonObject& object, const QString& path,
                            const QList<OwnRelation>& before, QStringList& problems) {
    Fields entry(object, path, problems);
    OwnRelation relation;
    relation.id = entry.string("id", true);
    relation.kind = entry.string("kind", true);
    relation.otherKind = entry.string("other", true);
    relation.name = entry.string("name", true);
    relation.inverse = entry.string("inverse");
    if (relation.inverse.isEmpty()) {
        relation.inverse = relation.id;
    }
    relation.extra = entry.rest();
    // Its id is none of a package, and none of another one
    const QString& id = relation.id;
    if (!id.isEmpty() && !id.startsWith(QLatin1String(OwnRelation::ID_PREFIX))) {
        entry.problem("id", QStringLiteral("'%1' must start with \"%2\"")
                                .arg(id, QLatin1String(OwnRelation::ID_PREFIX)));
    } else if (std::any_of(before.cbegin(), before.cend(),
                           [&id](const OwnRelation& each) { return each.id == id; })) {
        entry.problem("id", QStringLiteral("'%1' is listed twice").arg(id));
    }
    return relation;
}

/// The problems of the inverses of relations @p relations (@p paths): each one of them,
/// between the same kinds the other way round and with the relation as its inverse
void checkOwnInverses(const QList<OwnRelation>& relations, const QStringList& paths,
                      QStringList& problems) {
    for (qsizetype i = 0; i < relations.size(); ++i) {
        const OwnRelation& relation = relations.at(i);
        const auto inverse = std::find_if(relations.cbegin(), relations.cend(),
                                          [&relation](const OwnRelation& each) {
                                              return each.id == relation.inverse;
                                          });
        if (inverse == relations.cend()) {
            addProblem(problems, paths.at(i) + QStringLiteral(".inverse"),
                       QStringLiteral("'%1' is no relation of the list").arg(relation.inverse));
        } else if (inverse->inverse != relation.id || inverse->kind != relation.otherKind ||
                   inverse->otherKind != relation.kind) {
            addProblem(problems, paths.at(i) + QStringLiteral(".inverse"),
                       QStringLiteral("'%1' must be the relation between the same kinds the "
                                      "other way round, with '%2' as its inverse")
                           .arg(relation.inverse, relation.id));
        }
    }
}

/// The relations between cards that the writer named, in the Workshop, each with its inverse
QList<OwnRelation> readOwnRelations(Fields& fields, QStringList& problems) {
    QList<OwnRelation> relations;
    QStringList paths;  // of each of the relations
    const QJsonArray array = fields.array("relations");
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString path = QStringLiteral("%1[%2]").arg(fields.path("relations")).arg(i);
        if (array.at(i).isObject()) {
            relations << readOwnRelation(array.at(i).toObject(), path, relations, problems);
            paths << path;
        } else {
            addProblem(problems, path, QStringLiteral("must be an object with \"id\""));
        }
    }
    checkOwnInverses(relations, paths, problems);
    return relations;
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
    workshop.relations = readOwnRelations(fields, problems);
    workshop.cardFields = card_fields::read(fields.take("cardFields"));
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

/// "clipping" of a clipping: where it was cut from and where it was inserted
QJsonObject clippingToJson(const ClippingData& clipping) {
    QJsonObject data = clipping.extra;
    const ClippingOrigin& origin = clipping.origin;
    if (isEmpty(origin) && origin.extra.isEmpty()) {
        data.remove(QLatin1String("origin"));
    } else {
        QJsonObject from = origin.extra;
        setOrRemove(from, "element", origin.element);
        setOrRemove(from, "title", origin.title);
        if (origin.paragraph > 0) {
            from.insert(QLatin1String("paragraph"), origin.paragraph);
        } else {
            from.remove(QLatin1String("paragraph"));
        }
        setOrRemove(from, "before", origin.before);
        setOrRemove(from, "after", origin.after);
        setOrRemove(from, "cut", dateTimeText(origin.cut));
        data.insert(QLatin1String("origin"), from);
    }
    if (clipping.inserted.isEmpty()) {
        data.remove(QLatin1String("inserted"));
    } else {
        QJsonArray inserted;
        for (const ClippingInsertion& insertion : clipping.inserted) {
            QJsonObject each = insertion.extra;
            setOrRemove(each, "element", insertion.element);
            setOrRemove(each, "at", dateTimeText(insertion.at));
            inserted.append(each);
        }
        data.insert(QLatin1String("inserted"), inserted);
    }
    return data;
}

/// What a card has beside its text, as "card" saves it
QJsonObject cardToJson(const CardData& card) {
    QJsonObject data = card.extra;
    if (card.values.isEmpty()) {
        data.remove(QLatin1String("values"));
    } else {
        QJsonObject values;
        for (auto it = card.values.constBegin(); it != card.values.constEnd(); ++it) {
            values.insert(it.key(), it.value());
        }
        data.insert(QLatin1String("values"), values);
    }
    setOrRemove(data, "color", card.color);
    if (card.portraits.isEmpty()) {
        data.remove(QLatin1String("portraits"));
    } else {
        QJsonArray portraits;
        for (const CardPortrait& portrait : card.portraits) {
            QJsonObject each = portrait.extra;
            setOrRemove(each, "image", portrait.image);
            setOrRemove(each, "label", portrait.label);
            setOrRemove(each, "style", portrait.style);
            if (portrait.avatar.isEmpty()) {
                each.remove(QLatin1String("avatar"));
            } else {
                each.insert(QLatin1String("avatar"), portrait.avatar);
            }
            portraits.append(each);
        }
        data.insert(QLatin1String("portraits"), portraits);
    }
    return data;
}

/// Where clipping @p element was cut from and where it was inserted (ClippingData) in @p json
void writeClipping(QJsonObject& json, const ProjectElement& element) {
    if (isEmpty(element.clipping)) {
        json.remove(QLatin1String("clipping"));
    } else {
        json.insert(QLatin1String("clipping"), clippingToJson(element.clipping));
    }
}

/// The data of a card beside its text (@p element: CardData) in @p json
void writeCard(QJsonObject& json, const ProjectElement& element) {
    if (element.card.isEmpty()) {
        json.remove(QLatin1String("card"));
    } else {
        json.insert(QLatin1String("card"), cardToJson(element.card));
    }
}

/// What a scene says (the first scene of a text, a scene in the plan), with the fields this
/// version does not know
QJsonObject scenePlotToJson(const ScenePlot& plot) {
    QJsonObject json = plot.extra;
    const QMap<QString, QString> attributes = plot.toAttributes();
    for (const SceneField& field : SCENE_FIELDS) {
        setOrRemove(json, field.key, attributes.value(QLatin1String(field.key)));
    }
    return json;
}

/// What the plot board says of a text ("plot"), with the fields this version does not know
QJsonObject plotToJson(const ElementPlot& plot) {
    QJsonObject json = plot.extra;
    setOrRemove(json, "pov", plot.pov);
    setOrRemove(json, "place", plot.place);
    setOrRemove(json, "time", plot.time);
    setOrRemove(json, "color", plot.color);
    if (plot.opening.isEmpty()) {
        json.remove(QLatin1String("opening"));
    } else {
        json.insert(QLatin1String("opening"), scenePlotToJson(plot.opening));
    }
    if (plot.planned.isEmpty()) {
        json.remove(QLatin1String("planned"));
    } else {
        QJsonArray planned;
        for (const PlannedScene& scene : plot.planned) {
            QJsonObject item = scenePlotToJson(scene.plot);
            item.insert(QLatin1String("after"), scene.after);
            planned.append(item);
        }
        json.insert(QLatin1String("planned"), planned);
    }
    return json;
}

/// @p source as the "source" object of an element, the fields the program does not know kept
QJsonObject sourceToJson(const SourceDescription& source) {
    QJsonObject data = source.extra;
    setOrRemove(data, "type", source.type);
    QJsonArray people;
    for (const SourcePerson& person : source.people) {
        QJsonObject each = person.extra;
        each.insert(QLatin1String("name"), person.name);
        setOrRemove(each, "role", person.role);
        people.append(each);
    }
    if (people.isEmpty()) {
        data.remove(QLatin1String("people"));
    } else {
        data.insert(QLatin1String("people"), people);
    }
    for (auto value = source.values.cbegin(); value != source.values.cend(); ++value) {
        data.insert(value.key(), value.value());
    }
    setOrRemove(data, "bibliography", source.bibliography);
    return data;
}

/// @p bookmarks as the "bookmarks" array of an element
QJsonArray bookmarksToJson(const QList<PageBookmark>& bookmarks) {
    QJsonArray array;
    for (const PageBookmark& bookmark : bookmarks) {
        QJsonObject each = bookmark.extra;
        each.insert(QLatin1String("page"), bookmark.page);
        setOrRemove(each, "note", bookmark.note);
        array.append(each);
    }
    return array;
}

/// @p value in points, to a hundredth: the file keeps no more than the screen shows
double roundedPoints(double value) {
    return std::round(value * POINT_HUNDREDTHS) / POINT_HUNDREDTHS;
}

/// @p highlights as the "highlights" array of an element
QJsonArray highlightsToJson(const QList<PdfHighlight>& highlights) {
    QJsonArray array;
    for (const PdfHighlight& highlight : highlights) {
        QJsonObject each = highlight.extra;
        each.insert(QLatin1String("page"), highlight.page);
        QJsonArray areas;
        for (const QRectF& area : highlight.areas) {
            areas.append(QJsonArray{roundedPoints(area.x()), roundedPoints(area.y()),
                                    roundedPoints(area.width()), roundedPoints(area.height())});
        }
        each.insert(QLatin1String("areas"), areas);
        setOrRemove(each, "text", highlight.text);
        setOrRemove(each, "color", highlight.color);
        setOrRemove(each, "note", highlight.note);
        array.append(each);
    }
    return array;
}

/// @p quote as the "quote" object of an element
QJsonObject quoteToJson(const QuoteSource& quote) {
    QJsonObject data = quote.extra;
    setOrRemove(data, "source", quote.source);
    setOrRemove(data, "page", quote.page);
    setOrRemove(data, "author", quote.author);
    setOrRemove(data, "work", quote.work);
    return data;
}

/// The description of a source, the bookmarks, the highlights, the page shown last and the
/// source of a quote of @p element in @p json
void writeSource(QJsonObject& json, const ProjectElement& element) {
    if (element.source.isEmpty()) {
        json.remove(QLatin1String("source"));
    } else {
        json.insert(QLatin1String("source"), sourceToJson(element.source));
    }
    if (element.bookmarks.isEmpty()) {
        json.remove(QLatin1String("bookmarks"));
    } else {
        json.insert(QLatin1String("bookmarks"), bookmarksToJson(element.bookmarks));
    }
    if (element.highlights.isEmpty()) {
        json.remove(QLatin1String("highlights"));
    } else {
        json.insert(QLatin1String("highlights"), highlightsToJson(element.highlights));
    }
    if (element.lastPage > 0) {
        json.insert(QLatin1String("lastPage"), element.lastPage);
    } else {
        json.remove(QLatin1String("lastPage"));
    }
    if (element.quote.isEmpty()) {
        json.remove(QLatin1String("quote"));
    } else {
        json.insert(QLatin1String("quote"), quoteToJson(element.quote));
    }
}

QJsonObject elementToJson(const ProjectElement& element) {
    QJsonObject json = element.extra;
    json.insert(QLatin1String("id"), element.id);
    json.insert(QLatin1String("kind"), element.kind.toString());
    json.insert(QLatin1String("title"), element.title);
    setOrRemove(json, "file", element.file);
    setOrRemove(json, "url", element.url);
    setOrRemove(json, "status", element.status);
    // Without the field the text starts as its kind says
    setOrRemove(json, "firstPage", pageStartName(element.firstPage));
    if (element.elements.isEmpty()) {
        json.remove(QLatin1String("elements"));
    } else {
        json.insert(QLatin1String("elements"), elementsToJson(element.elements));
    }
    setOrRemove(json, "description", element.description);
    setOrRemove(json, "book", element.bookId);
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
        // The id alone while a link says nothing more, as earlier versions read it
        QJsonArray links;
        for (const ElementLink& link : element.links) {
            if (!link.hasDetails()) {
                links.append(link.to);
                continue;
            }
            QJsonObject each = link.extra;
            each.insert(QLatin1String("to"), link.to);
            if (!link.role.isEmpty()) {
                each.insert(QLatin1String("role"), link.role);
            }
            if (link.paragraph > 0) {
                each.insert(QLatin1String("paragraph"), link.paragraph);
            }
            if (!link.excerpt.isEmpty()) {
                each.insert(QLatin1String("excerpt"), link.excerpt);
            }
            if (!link.note.isEmpty()) {
                each.insert(QLatin1String("note"), link.note);
            }
            links.append(each);
        }
        json.insert(QLatin1String("links"), links);
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
    const CoverData& cover = element.cover;
    if (cover.isEmpty()) {
        json.remove(QLatin1String("cover"));
    } else {
        QJsonObject data = cover.extra;
        setOrRemove(data, "part", cover.part);
        setOrRemove(data, "place", cover.place);
        setOrRemove(data, "state", cover.state);
        setOrRemove(data, "author", cover.author);
        setOrRemove(data, "role", cover.role);
        setOrRemove(data, "source", cover.source);
        setOrRemove(data, "date", cover.date);
        setOrRemove(data, "consent", cover.consent);
        setOrRemove(data, "license", cover.license);
        setOrRemove(data, "about", cover.about);
        for (const auto& [key, target] : {std::pair{"targetCharacters", cover.targetCharacters},
                                          std::pair{"targetWords", cover.targetWords}}) {
            if (target > 0) {
                data.insert(QLatin1String(key), target);
            } else {
                data.remove(QLatin1String(key));
            }
        }
        json.insert(QLatin1String("cover"), data);
    }
    writeClipping(json, element);
    if (element.plot.isEmpty()) {
        json.remove(QLatin1String("plot"));
    } else {
        json.insert(QLatin1String("plot"), plotToJson(element.plot));
    }
    writeSource(json, element);
    if (element.publishing.isEmpty()) {
        json.remove(QLatin1String("publishing"));
    } else {
        json.insert(QLatin1String("publishing"), element.publishing.toJson());
    }
    writeCard(json, element);
    return json;
}

QJsonArray elementsToJson(const QList<ProjectElement>& elements) {
    QJsonArray array;
    for (const ProjectElement& element : elements) {
        array.append(elementToJson(element));
    }
    return array;
}

QJsonObject captionsToJson(const BookCaptions& captions) {
    QJsonObject json = captions.extra;
    json.insert(QLatin1String("labels"), QJsonArray::fromStringList(captions.labels));
    QJsonArray numbering;
    for (const CaptionLabelNumbering& label : captions.numbering) {
        QJsonObject entry = label.extra;
        entry.insert(QLatin1String("label"), label.label);
        entry.insert(QLatin1String("format"), label.format);
        entry.insert(QLatin1String("perChapter"), label.perChapter);
        entry.insert(QLatin1String("withChapter"), label.withChapter);
        entry.insert(QLatin1String("separator"), label.separator);
        numbering.append(entry);
    }
    json.insert(QLatin1String("numbering"), numbering);
    json.insert(QLatin1String("reference"), captions.reference);
    return json;
}

QJsonObject noteNumberingToJson(const NoteNumbering& numbering) {
    QJsonObject json = numbering.extra;
    json.insert(QLatin1String("format"), numbering.format);
    json.insert(QLatin1String("perChapter"), numbering.perChapter);
    return json;
}

QJsonObject footnotesToJson(const BookFootnotes& footnotes) {
    QJsonObject json = footnotes.extra;
    json.insert(QLatin1String("footnotes"), noteNumberingToJson(footnotes.footnotes));
    QJsonObject endnotes = noteNumberingToJson(footnotes.endnotes);
    endnotes.insert(QLatin1String("place"), footnotes.endnotesAtBookEnd ? QStringLiteral("book")
                                                                        : QStringLiteral("chapter"));
    json.insert(QLatin1String("endnotes"), endnotes);
    return json;
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
    if (book.captions) {
        json.insert(QLatin1String("captions"), captionsToJson(*book.captions));
    } else {
        json.remove(QLatin1String("captions"));
    }
    if (book.footnotes) {
        json.insert(QLatin1String("footnotes"), footnotesToJson(*book.footnotes));
    } else {
        json.remove(QLatin1String("footnotes"));
    }
    setOrRemove(json, "bibliographyStyle", book.bibliographyStyle);
    if (book.publishingSet) {
        json.insert(QLatin1String("publishingSet"), book.publishingSet->toJson());
    } else {
        json.remove(QLatin1String("publishingSet"));
    }
    setOrRemove(json, "isbn", book.isbn);
    setOrRemove(json, "publisher", book.publisher);
    return json;
}

/// The paragraph styles of the project (BookParagraphStyle), each id once
QList<BookParagraphStyle> readParagraphStyles(const QJsonArray& array, QStringList& problems) {
    QList<BookParagraphStyle> styles;
    QHash<QString, QString> ids;
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString field = QStringLiteral("paragraphStyles[%1]").arg(i);
        if (!array.at(i).isObject()) {
            addProblem(problems, field, QStringLiteral("must be an object"));
            continue;
        }
        Fields fields(array.at(i).toObject(), field, problems);
        BookParagraphStyle style;
        style.id = fields.string("id", true);
        if (!style.id.isEmpty()) {
            if (const QString first = ids.value(style.id); !first.isEmpty()) {
                fields.problem("id", QStringLiteral("'%1' is already the id of %2")
                                         .arg(style.id, first));
            } else {
                ids.insert(style.id, field);
            }
        }
        style.name = fields.string("name");
        style.baseStyle = fields.string("base");
        style.nextStyle = fields.string("next");
        if (style.baseStyle == style.id && !style.id.isEmpty()) {
            fields.problem("base", QStringLiteral("a style cannot inherit from itself"));
        }
        const QJsonValue properties = fields.take("properties");
        if (properties.isObject()) {
            style.properties = properties.toObject().toVariantMap();
        } else if (!properties.isUndefined()) {
            fields.problem("properties", QStringLiteral("must be an object"));
        }
        style.extra = fields.rest();
        styles.append(std::move(style));
    }
    return styles;
}

QJsonObject paragraphStyleToJson(const BookParagraphStyle& style) {
    QJsonObject json = style.extra;
    json.insert(QLatin1String("id"), style.id);
    setOrRemove(json, "name", style.name);
    setOrRemove(json, "base", style.baseStyle);
    json.insert(QLatin1String("next"), style.nextStyle);
    json.insert(QLatin1String("properties"), QJsonObject::fromVariantMap(style.properties));
    return json;
}

QJsonObject workshopToJson(const ProjectWorkshop& workshop) {
    QJsonObject json = workshop.extra;
    setOrRemove(json, "name", workshop.name);
    json.insert(QLatin1String("grouped"), workshop.grouped);
    json.insert(QLatin1String("elements"), elementsToJson(workshop.elements));
    if (workshop.relations.isEmpty()) {
        json.remove(QLatin1String("relations"));
    } else {
        QJsonArray relations;
        for (const OwnRelation& relation : workshop.relations) {
            QJsonObject each = relation.extra;
            each.insert(QLatin1String("id"), relation.id);
            each.insert(QLatin1String("kind"), relation.kind);
            each.insert(QLatin1String("other"), relation.otherKind);
            each.insert(QLatin1String("name"), relation.name);
            setOrRemove(each, "inverse", relation.inverse == relation.id ? QString()
                                                                         : relation.inverse);
            relations.append(each);
        }
        json.insert(QLatin1String("relations"), relations);
    }
    if (const QJsonObject cardFields = card_fields::write(workshop.cardFields);
        cardFields.isEmpty()) {
        json.remove(QLatin1String("cardFields"));
    } else {
        json.insert(QLatin1String("cardFields"), cardFields);
    }
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

/// Add to @p places, in reading order, the places of an element of @p kind in @p elements of
/// @p part (the list of group @p groupId, or of the part when it is empty) and in the groups in
/// them that can have it: the places of the list itself when @p here
void addPlacesIn(const BookTypeRegistry& registry, const KindRef& kind, BookPlace part,
                 const QList<ProjectElement>& elements, const QString& groupId, bool here,
                 QList<ElementPlace>& places) {
    // The lists still being gone through, the innermost last, each with its next place
    struct Walk {
        const QList<ProjectElement>* elements = nullptr;
        QString groupId;
        bool here = false;
        qsizetype next = 0;
    };
    QList<Walk> walks{Walk{&elements, groupId, here, 0}};
    while (!walks.isEmpty()) {
        const Walk walk = walks.last();
        const qsizetype i = walk.next;
        if (i > walk.elements->size()) {
            walks.removeLast();
            continue;
        }
        walks.last().next = i + 1;
        if (walk.here && BookProject::canBeAt(registry, *walk.elements, kind, i)) {
            places.append({part, walk.groupId, i});
        }
        if (i < walk.elements->size() && canHold(registry, walk.elements->at(i), kind)) {
            const ProjectElement& group = walk.elements->at(i);
            walks.append(Walk{&group.elements, group.id, true, 0});
        }
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
// ScenePlot
// =============================================================================

QMap<QString, QString> ScenePlot::toAttributes() const {
    QMap<QString, QString> attributes;
    for (const SceneField& field : SCENE_FIELDS) {
        if (const QString& value = this->*field.member; !value.isEmpty()) {
            attributes.insert(QLatin1String(field.key), value);
        }
    }
    return attributes;
}

const QStringList& ScenePlot::fieldNames() {
    static const QStringList NAMES = [] {
        QStringList names;
        for (const SceneField& field : SCENE_FIELDS) {
            names << QLatin1String(field.key);
        }
        return names;
    }();
    return NAMES;
}

ScenePlot ScenePlot::fromAttributes(const QMap<QString, QString>& attributes) {
    ScenePlot plot;
    for (const SceneField& field : SCENE_FIELDS) {
        plot.*field.member = attributes.value(QLatin1String(field.key));
    }
    return plot;
}

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

CaptionLabelNumbering BookCaptions::numberingOf(const QString& label) const {
    for (const CaptionLabelNumbering& numbered : numbering) {
        if (numbered.label == label) {
            return numbered;
        }
    }
    CaptionLabelNumbering fallback;
    fallback.label = label;
    return fallback;
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

QStringList ProjectElement::linkTargets() const {
    QStringList targets;
    targets.reserve(links.size());
    for (const ElementLink& link : links) {
        targets << link.to;
    }
    return targets;
}

const ElementLink* ProjectElement::linkTo(const QString& target) const {
    const auto found =
        std::find_if(links.cbegin(), links.cend(),
                     [&target](const ElementLink& link) { return link.to == target; });
    return found == links.cend() ? nullptr : &*found;
}

bool ProjectElement::operator==(const ProjectElement& other) const {
    const auto fieldsOf = [](const ProjectElement& element) {
        return std::tie(element.id, element.kind, element.title, element.file, element.url,
                        element.status, element.firstPage, element.elements,
                        element.description, element.fields, element.links, element.rotation,
                        element.cover, element.clipping, element.card, element.plot,
                        element.source, element.bookmarks, element.highlights,
                        element.lastPage, element.quote, element.publishing, element.bookId,
                        element.viewExtra, element.extra);
    };
    return fieldsOf(*this) == fieldsOf(other);
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

const BookParagraphStyle* BookProject::findParagraphStyle(const QString& styleId) const {
    const auto it =
        std::find_if(paragraphStyles.cbegin(), paragraphStyles.cend(),
                     [&styleId](const BookParagraphStyle& style) { return style.id == styleId; });
    return it == paragraphStyles.cend() ? nullptr : &*it;
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
    if (from->place == BookPlace::Workshop) {
        addPlacesIn(registry, kind, BookPlace::Workshop, without.workshop.elements, QString(),
                    kind.kind->allows(BookPlace::Workshop), places);
        return places;
    }
    for (const BookPart& part : BOOK_PARTS) {
        addPlacesIn(registry, kind, part.place, without.elementsIn(part.place, book), QString(),
                    kind.kind->allows(part.place), places);
    }
    return places;
}

QList<ElementPlace> BookProject::movePlacesIn(const BookTypeRegistry& registry,
                                              const QString& elementId,
                                              const QString& bookId) const {
    const qsizetype own = bookOf(*this, elementId);
    const qsizetype target = bookIndex(bookId);
    if (own < 0 || target < 0) {
        return {};
    }
    if (target == own) {
        return movePlacesOf(registry, elementId);
    }
    const ProjectElement* element = findElement(elementId);
    const KindRef kind = element != nullptr ? kindOf(registry, *element) : KindRef{};
    QList<ElementPlace> places;
    if (!kind) {
        return places;
    }
    for (const BookPart& part : BOOK_PARTS) {
        addPlacesIn(registry, kind, part.place, elementsIn(part.place, target), QString(),
                    kind.kind->allows(part.place), places);
    }
    return places;
}

bool BookProject::moveElementTo(const BookTypeRegistry& registry, const QString& elementId,
                                const ElementPlace& to, const QString& bookId) {
    // A place in another book of a series, or in the element's own book
    const qsizetype own = bookOf(*this, elementId);
    const bool elsewhere = !bookId.isEmpty() && (own < 0 || bookIndex(bookId) != own);
    const QList<ElementPlace> places =
        elsewhere ? movePlacesIn(registry, elementId, bookId) : movePlacesOf(registry, elementId);
    if (!places.contains(to)) {
        return false;
    }

    // The project as it was comes back if the element finds no list there
    const BookProject previous = *this;
    const qsizetype book = elsewhere ? bookIndex(bookId) : own;
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

qsizetype BookProject::bookIndex(const QString& bookId) const {
    for (qsizetype book = 0; book < books.size() && !bookId.isEmpty(); ++book) {
        if (books.at(book).id == bookId) {
            return book;
        }
    }
    return -1;
}

qsizetype BookProject::bookIndexOf(const QString& elementId) const {
    return bookOf(*this, elementId);
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
    setOrRemove(json, "series", series);
    json.insert(QLatin1String("workshop"), workshopToJson(workshop));

    // Only a project with styles of its own has the field: other files stay as they were
    if (paragraphStyles.isEmpty()) {
        json.remove(QLatin1String("paragraphStyles"));
    } else {
        QJsonArray styleList;
        for (const BookParagraphStyle& style : paragraphStyles) {
            styleList.append(paragraphStyleToJson(style));
        }
        json.insert(QLatin1String("paragraphStyles"), styleList);
    }
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

    project.series = fields.string("series");
    project.workshop = readWorkshop(fields.take("workshop"), seen, problems);
    project.paragraphStyles = readParagraphStyles(fields.array("paragraphStyles"), problems);
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
