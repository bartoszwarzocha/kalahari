// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file chapter_scenes.cpp
/// @brief The scenes of a text's KML (ChapterDocument): what their breaks say, and the scenes
///        moved, added and deleted, also to another text with their annotations

#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/text_footnotes.h>

#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QXmlStreamReader>

#include <algorithm>
#include <optional>

namespace kalahari::core {

namespace {

/// @brief An element of the root element of a KML: a paragraph, a list, a break...
struct TopElement {
    QString name;
    qsizetype start = 0;  ///< Its first character in the KML
    qsizetype end = 0;    ///< The character after it
    QXmlStreamAttributes attributes;
};

/// @brief The elements of the root element of @p content (with its root element:
///        KmlFormatRegistry::withRootElement()), in their order; those read before an error
QList<TopElement> topElementsOf(const QString& content)
{
    QList<TopElement> elements;
    QXmlStreamReader reader(content);
    if (!reader.readNextStartElement()) {
        return elements;  // no root element
    }
    auto before = static_cast<qsizetype>(reader.characterOffset());
    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType token = reader.readNext();
        if (token == QXmlStreamReader::EndElement || token == QXmlStreamReader::Invalid) {
            break;  // the end of the root element, or an error
        }
        if (token == QXmlStreamReader::StartElement) {
            // After text the reader is past the "<" of the element already
            qsizetype start = before;
            if (start >= content.size() || content.at(start) != QLatin1Char('<')) {
                start = content.lastIndexOf(QLatin1Char('<'), start);
            }
            TopElement element{.name = reader.name().toString(), .start = start, .end = 0,
                               .attributes = reader.attributes()};
            reader.skipCurrentElement();
            if (reader.hasError()) {
                break;
            }
            element.end = static_cast<qsizetype>(reader.characterOffset());
            elements.append(element);
        }
        before = static_cast<qsizetype>(reader.characterOffset());
    }
    return elements;
}

/// @brief Whether an element of the root is a scene break
bool isSceneBreak(const TopElement& element)
{
    return element.name == QLatin1String("scene-break");
}

/// @brief What scene break @p element says of the scene after it
ScenePlot plotOfBreak(const TopElement& element)
{
    QMap<QString, QString> attributes;
    for (const QXmlStreamAttribute& attribute : element.attributes) {
        attributes.insert(attribute.name().toString(), attribute.value().toString());
    }
    return ScenePlot::fromAttributes(attributes);
}

/// @brief A scene break saying @p plot of its scene, as the editor writes it
QString sceneBreakKml(const ScenePlot& plot)
{
    QString written = QStringLiteral("<scene-break");
    const QMap<QString, QString> attributes = plot.toAttributes();
    for (const QString& name : ScenePlot::fieldNames()) {
        if (const auto value = attributes.constFind(name); value != attributes.cend()) {
            written += QLatin1Char(' ') + name + QStringLiteral("=\"") +
                       editor::KmlFormatRegistry::escapeXmlAttribute(*value) + QLatin1Char('"');
        }
    }
    written += QStringLiteral("/>");
    return written;
}

/// @brief A text cut at its scene breaks
struct SceneParts {
    QString head;            ///< Before its first scene: the start of the root element, the
                             ///< tab guides, the annotations and the headings at its start
    QList<QString> bodies;   ///< The elements of each scene, without its break
    QList<ScenePlot> plots;  ///< What each scene says; of the first, SceneText::opening
    QString tail;            ///< After its last scene: the end of the root element
    qsizetype rootEnd = 0;   ///< In head: the character after the start tag of the root
    qsizetype annotationsStart = -1;  ///< In head: the <annotations> section; -1: none
    qsizetype annotationsEnd = -1;    ///< In head: the character after the section

    /// @brief The elements of the text, without its root element and its scene breaks
    [[nodiscard]] QString elements() const
    {
        return head.mid(rootEnd) + bodies.join(QString());
    }

    /// @brief The text with its scenes
    [[nodiscard]] SceneText joined() const
    {
        SceneText text;
        text.opening = plots.constFirst();
        text.kml = head + bodies.constFirst();
        for (qsizetype i = 1; i < bodies.size(); ++i) {
            text.kml += sceneBreakKml(plots.at(i)) + bodies.at(i);
        }
        text.kml += tail;
        return text;
    }
};

/// @brief A KML read to its end, with its root element
struct WholeKml {
    QString content;        ///< With its root element; an empty <kml/> as <kml></kml>
    qsizetype rootEnd = 0;  ///< The character after the start tag of the root element
};

/// @brief @p kml with its root element, read to its end; nullopt for a KML that cannot be
///        read, since what comes after an error would be lost
std::optional<WholeKml> wholeKmlOf(const QString& kml)
{
    WholeKml whole{.content = editor::KmlFormatRegistry::withRootElement(kml), .rootEnd = 0};
    QXmlStreamReader check(whole.content);
    if (!check.readNextStartElement()) {
        return std::nullopt;
    }
    const QString root = check.name().toString();
    whole.rootEnd = static_cast<qsizetype>(check.characterOffset());
    while (!check.atEnd()) {
        check.readNext();
    }
    if (check.hasError() || whole.rootEnd < 2) {
        return std::nullopt;
    }
    if (whole.content.at(whole.rootEnd - 2) == QLatin1Char('/')) {
        // <kml/>: the root element gets its end
        whole.content = whole.content.left(whole.rootEnd - 2) + QStringLiteral("></") + root +
                        QLatin1Char('>') + whole.content.mid(whole.rootEnd);
        --whole.rootEnd;
    }
    return whole;
}

/// @brief @p text cut at its scene breaks; nullopt for a KML that cannot be read
std::optional<SceneParts> scenePartsOf(const SceneText& text)
{
    const std::optional<WholeKml> whole = wholeKmlOf(text.kml);
    if (!whole) {
        return std::nullopt;
    }
    const QString& content = whole->content;
    const QList<TopElement> elements = topElementsOf(content);
    // The tab guides, the annotations and the headings at the start of the text stay there
    qsizetype bodyStart = whole->rootEnd;
    qsizetype first = 0;
    static const QStringList HEAD{QStringLiteral("tab-guides"), QStringLiteral("annotations"),
                                  QStringLiteral("h1"), QStringLiteral("h2"),
                                  QStringLiteral("h3")};
    while (first < elements.size() && HEAD.contains(elements.at(first).name)) {
        bodyStart = elements.at(first).end;
        ++first;
    }
    SceneParts parts;
    parts.head = content.left(bodyStart);
    parts.rootEnd = whole->rootEnd;
    const auto headEnd = elements.cbegin() + first;
    if (const auto section = std::find_if(elements.cbegin(), headEnd,
                                          [](const TopElement& element) {
                                              return element.name == QLatin1String("annotations");
                                          });
        section != headEnd) {
        parts.annotationsStart = section->start;
        parts.annotationsEnd = section->end;
    }
    parts.plots << text.opening;
    for (qsizetype i = first; i < elements.size(); ++i) {
        const TopElement& element = elements.at(i);
        if (isSceneBreak(element)) {
            parts.bodies << content.mid(bodyStart, element.start - bodyStart);
            parts.plots << plotOfBreak(element);
            bodyStart = element.end;
        }
    }
    const qsizetype tailStart =
        std::max(bodyStart, elements.isEmpty() ? bodyStart : elements.constLast().end);
    parts.bodies << content.mid(bodyStart, tailStart - bodyStart);
    parts.tail = content.mid(tailStart);
    return parts;
}

/// @brief An annotation of the <annotations> section of a text, as the text has it
struct AnnotationXml {
    QString id;   ///< Its id
    QString xml;  ///< Its <annotation> element
};

/// @brief The annotations of the <annotations> section of @p parts, in their order
QList<AnnotationXml> annotationsOf(const SceneParts& parts)
{
    QList<AnnotationXml> annotations;
    if (parts.annotationsStart < 0) {
        return annotations;
    }
    const QString section =
        parts.head.mid(parts.annotationsStart, parts.annotationsEnd - parts.annotationsStart);
    for (const TopElement& element : topElementsOf(section)) {
        if (element.name == QLatin1String("annotation")) {
            annotations << AnnotationXml{
                .id = element.attributes.value(QLatin1String("id")).toString(),
                .xml = section.mid(element.start, element.end - element.start)};
        }
    }
    return annotations;
}

/// @brief Give @p parts the <annotations> section of @p annotations, or no section when there
///        are none
void setAnnotations(SceneParts& parts, const QList<AnnotationXml>& annotations)
{
    QString section;
    if (!annotations.isEmpty()) {
        section = QStringLiteral("<annotations>");
        for (const AnnotationXml& annotation : annotations) {
            section += annotation.xml;
        }
        section += QStringLiteral("</annotations>");
    }
    // A new section goes first, right after the start tag of the root element
    const bool had = parts.annotationsStart >= 0;
    const qsizetype start = had ? parts.annotationsStart : parts.rootEnd;
    parts.head.replace(start, had ? parts.annotationsEnd - start : 0, section);
    parts.annotationsStart = section.isEmpty() ? -1 : start;
    parts.annotationsEnd = section.isEmpty() ? -1 : start + section.size();
}

/// @brief The ids of the annotations anchored in @p elements (elements of a text, without its
///        root element)
QSet<QString> anchoredIn(const QString& elements)
{
    QSet<QString> ids;
    QXmlStreamReader reader(QStringLiteral("<kml>") + elements + QStringLiteral("</kml>"));
    while (!reader.atEnd()) {
        if (reader.readNext() == QXmlStreamReader::StartElement &&
            reader.name() == QLatin1String("anchor")) {
            ids.insert(reader.attributes().value(QLatin1String("ref")).toString());
        }
    }
    return ids;
}

/// @brief The pattern of attribute @p name with value @p value in a start tag of element
///        @p element: the tag before the attribute in group 1
QRegularExpression attributePattern(const QString& element, const QString& name,
                                    const QString& value)
{
    return QRegularExpression(
        QStringLiteral("(<") + element + QStringLiteral(R"(\b[^>]*?)\s)") + name +
        QStringLiteral(R"(\s*=\s*(["']))") +
        QRegularExpression::escape(editor::KmlFormatRegistry::escapeXmlAttribute(value)) +
        QStringLiteral(R"(\2)"));
}

/// @brief What replaces attributePattern(): the tag before the attribute and @p name with the
///        value @p value
QString attributeReplacement(const QString& name, const QString& value)
{
    return QStringLiteral(R"(\1 )") + name + QStringLiteral(R"(=")") +
           editor::KmlFormatRegistry::escapeXmlAttribute(value) + QLatin1Char('"');
}

/// @brief @p elements with the anchors of annotation @p id referring to @p newId
QString withAnchorsRenamed(QString elements, const QString& id, const QString& newId)
{
    const QString ref = QStringLiteral("ref");
    elements.replace(attributePattern(QStringLiteral("anchor"), ref, id),
                     attributeReplacement(ref, newId));
    return elements;
}

/// @brief @p annotation with the id @p newId
AnnotationXml withId(AnnotationXml annotation, const QString& newId)
{
    const QString id = QStringLiteral("id");
    const qsizetype tagEnd = annotation.xml.indexOf(QLatin1Char('>')) + 1;
    QString tag = annotation.xml.left(tagEnd);
    tag.replace(attributePattern(QStringLiteral("annotation"), id, annotation.id),
                attributeReplacement(id, newId));
    annotation.xml = tag + annotation.xml.mid(tagEnd);
    annotation.id = newId;
    return annotation;
}

/// @brief Give @p target the annotations anchored in @p body, a scene taken from @p source: an
///        annotation anchored also in what stays of @p source, or with an id @p target has
///        already, goes as a copy with a new id
void moveAnnotations(QString& body, SceneParts& source, SceneParts& target)
{
    const QSet<QString> moved = anchoredIn(body);
    if (moved.isEmpty()) {
        return;
    }
    const QSet<QString> staying = anchoredIn(source.elements());
    QList<AnnotationXml> given = annotationsOf(target);
    QSet<QString> taken;
    for (const AnnotationXml& annotation : given) {
        taken.insert(annotation.id);
    }
    QList<AnnotationXml> kept;
    for (AnnotationXml annotation : annotationsOf(source)) {
        if (!moved.contains(annotation.id)) {
            kept << annotation;
            continue;
        }
        if (staying.contains(annotation.id) || taken.contains(annotation.id)) {
            kept << annotation;
            const QString newId = editor::newAnnotationId();
            body = withAnchorsRenamed(body, annotation.id, newId);
            annotation = withId(annotation, newId);
        }
        given << annotation;
    }
    setAnnotations(source, kept);
    setAnnotations(target, given);
}

}  // namespace

QList<KmlScene> ChapterDocument::kmlScenes(const QString& kml)
{
    // Without a break the text is one scene: its words are counted as those of the text
    if (!kml.contains(QLatin1String("<scene-break"))) {
        return {KmlScene{.plot = ScenePlot(), .words = kmlWordTally(kml)}};
    }
    const QString content = editor::KmlFormatRegistry::withRootElement(kml);
    QList<KmlScene> scenes{KmlScene()};
    QString text;  // the elements of the scene so far
    const auto finishScene = [&scenes, &text]() {
        scenes.last().words = kmlWordTally(QStringLiteral("<kml>") + text +
                                           QStringLiteral("</kml>"));
        text.clear();
    };
    for (const TopElement& element : topElementsOf(content)) {
        if (isSceneBreak(element)) {
            finishScene();
            scenes.append(KmlScene{.plot = plotOfBreak(element), .words = WordTally()});
        } else {
            text += QStringView(content).mid(element.start, element.end - element.start);
        }
    }
    finishScene();
    return scenes;
}

QString ChapterDocument::withScenePlot(const QString& kml, int scene, const ScenePlot& plot)
{
    if (scene < 1) {
        return {};
    }
    QString content = editor::KmlFormatRegistry::withRootElement(kml);
    int breaks = 0;
    for (const TopElement& element : topElementsOf(content)) {
        if (!isSceneBreak(element) || ++breaks < scene) {
            continue;
        }
        content.replace(element.start, element.end - element.start, sceneBreakKml(plot));
        return content;
    }
    return {};
}

std::optional<SceneText> ChapterDocument::withSceneMoved(const SceneText& text, int from,
                                                         int to)
{
    std::optional<SceneParts> parts = scenePartsOf(text);
    if (!parts || from < 0 || from >= parts->bodies.size() || to < 0 ||
        to >= parts->bodies.size()) {
        return std::nullopt;
    }
    parts->bodies.move(from, to);
    parts->plots.move(from, to);
    return parts->joined();
}

std::optional<SceneText> ChapterDocument::withSceneAdded(const SceneText& text, int at,
                                                         const ScenePlot& plot)
{
    std::optional<SceneParts> parts = scenePartsOf(text);
    if (!parts || at < 0 || at > parts->bodies.size()) {
        return std::nullopt;
    }
    parts->bodies.insert(at, QStringLiteral("<p></p>"));
    parts->plots.insert(at, plot);
    return parts->joined();
}

std::optional<SceneMove> ChapterDocument::withSceneMovedTo(const SceneText& from, int scene,
                                                           const SceneText& to, int at)
{
    std::optional<SceneParts> source = scenePartsOf(from);
    std::optional<SceneParts> target = scenePartsOf(to);
    if (!source || !target || source->bodies.size() < 2 || scene < 0 ||
        scene >= source->bodies.size() || at < 0 || at > target->bodies.size()) {
        return std::nullopt;
    }
    QString body = source->bodies.takeAt(scene);
    const ScenePlot plot = source->plots.takeAt(scene);
    moveAnnotations(body, *source, *target);
    target->bodies.insert(at, body);
    target->plots.insert(at, plot);
    return SceneMove{.from = source->joined(), .to = target->joined()};
}

std::optional<SceneText> ChapterDocument::withoutScene(const SceneText& text, int scene,
                                                       bool keepText)
{
    std::optional<SceneParts> parts = scenePartsOf(text);
    if (!parts || parts->bodies.size() < 2 || scene < 0 || scene >= parts->bodies.size()) {
        return std::nullopt;
    }
    if (keepText) {
        // The break goes: the text of the scene joins the one before it (the first scene: the
        // next one joins it)
        const qsizetype joined = scene == 0 ? 1 : scene;
        parts->bodies[joined - 1] += parts->bodies.at(joined);
        parts->bodies.removeAt(joined);
        parts->plots.removeAt(joined);
    } else {
        parts->bodies.removeAt(scene);
        parts->plots.removeAt(scene);
    }
    return parts->joined();
}

} // namespace kalahari::core
