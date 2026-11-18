// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file avatar_art.cpp
/// @brief The drawings of the parts of a portrait and the portrait they make

#include "kalahari/gui/avatar_art.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/resource_paths.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPainter>
#include <QRegularExpression>
#include <QSize>
#include <QSvgRenderer>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::gui {

namespace {

/// The names of the frames in the file, in the order of AvatarFrame
constexpr std::array<const char*, 3> FRAME_NAMES = {"bust", "full", "face"};

/// The percent of a mix that is all of the other color
constexpr double PERCENT = 100.0;

/// The names of the colors in the shapes ("{hair}") and the choices that give them
constexpr std::array<std::pair<const char*, const char*>, 5> COLOR_KEYS = {{
    {"skin", "skin"},
    {"hair", "hairColor"},
    {"eyes", "eyeColor"},
    {"clothes", "clothesColor"},
    {"bottom", "bottomColor"},
}};

/// The numbers of a rectangle of a shape: x, y, width and height, with the radius of its
/// corners when it has one
constexpr qsizetype RECT_NUMBERS = 4;

/// The significant digits of a number of the SVG
constexpr int SVG_DIGITS = 6;

/// The middle of a shape, in its parts: where a radial gradient is, and how far it goes
constexpr double MIDDLE = 0.5;

/// The groups of a color of the portrait in a token: "{skin}", "{skin-10}", "{skin>a03a35@60}"
constexpr int TOKEN_NAME = 1;
constexpr int TOKEN_SIGN = 2;
constexpr int TOKEN_PERCENT = 3;
constexpr int TOKEN_MIXED = 4;
constexpr int TOKEN_MIXED_PERCENT = 5;

/// A number for the SVG: short, without a needless fraction
QString number(double value) {
    return QString::number(value, 'g', SVG_DIGITS);
}

/// The name of @p frame in the file
QString frameName(AvatarFrame frame) {
    return QString::fromLatin1(FRAME_NAMES.at(static_cast<std::size_t>(frame)));
}

/// @p color moved toward @p other by @p percent
QColor mixed(const QColor& color, const QColor& other, double percent) {
    const double part = std::clamp(percent, 0.0, PERCENT) / PERCENT;
    const auto channel = [part](int from, int to) {
        return qRound(from + ((to - from) * part));
    };
    return {channel(color.red(), other.red()), channel(color.green(), other.green()),
            channel(color.blue(), other.blue())};
}

/// A rectangle of four numbers in @p json; null when it is not one
QRectF rectOf(const QJsonValue& json) {
    const QJsonArray numbers = json.toArray();
    if (numbers.size() != 4) {
        return {};
    }
    return {numbers.at(0).toDouble(), numbers.at(1).toDouble(), numbers.at(2).toDouble(),
            numbers.at(3).toDouble()};
}

/// The strings of @p json, a string or a list of them
QStringList stringsOf(const QJsonValue& json) {
    if (json.isString()) {
        return {json.toString()};
    }
    QStringList strings;
    for (const auto& each : json.toArray()) {
        strings << each.toString();
    }
    return strings;
}

/// @p shape with the keys of its "line" put over its own
QJsonObject inLine(const QJsonObject& shape) {
    QJsonObject merged = shape;
    const QJsonObject line = shape.value(QLatin1String("line")).toObject();
    for (auto it = line.constBegin(); it != line.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return merged;
}

}  // namespace

/// What drawing a portrait needs while it puts the SVG together
struct AvatarArt::Drawing {
    QJsonObject choices;          ///< The parts and the colors
    AvatarLook look;              ///< The style, the frame and the card
    QHash<QString, QColor> colors;  ///< The colors of the portrait by their names
    QString definitions;          ///< The gradients
    int gradients = 0;            ///< How many there are

    /// Whether it is drawn in the style LINE
    [[nodiscard]] bool line() const { return look.style == QLatin1String(AvatarLook::LINE); }

    /// A gradient of the shapes, given as "fill": { "gradient": "radial", "stops": [[0,
    /// "{skin+8}"], [1, "{skin-10}", 0.9]], "center": [0.4, 0.35], "radius": 0.7 } or as a
    /// "linear" one "from" a point "to" another, in parts of the shape; its paint "url(#g1)"
    [[nodiscard]] QString gradient(const QJsonObject& json);

    /// The color @p token names: "#rrggbb", "{skin}", "{skin-10}", "{skin+30}",
    /// "{skin>a03a35@60}"; invalid when it names none
    [[nodiscard]] QColor color(const QString& token) const {
        static const QRegularExpression pattern(QStringLiteral(
            R"(^\{([a-z]+)(?:([+-])([0-9]{1,3})|>([0-9a-fA-F]{6})@([0-9]{1,3}))?\}$)"));
        const QRegularExpressionMatch match = pattern.match(token);
        if (!match.hasMatch()) {
            return QColor::isValidColorName(token) ? QColor(token) : QColor();
        }
        const QColor base = colors.value(match.captured(TOKEN_NAME));
        if (!base.isValid() ||
            (match.captured(TOKEN_SIGN).isEmpty() && match.captured(TOKEN_MIXED).isEmpty())) {
            return base;
        }
        if (!match.captured(TOKEN_MIXED).isEmpty()) {
            return mixed(base, QColor(QLatin1Char('#') + match.captured(TOKEN_MIXED)),
                         match.captured(TOKEN_MIXED_PERCENT).toDouble());
        }
        const bool lighter = match.captured(TOKEN_SIGN) == QLatin1String("+");
        return mixed(base, lighter ? QColor(Qt::white) : QColor(Qt::black),
                     match.captured(TOKEN_PERCENT).toDouble());
    }
};

const AvatarArt& AvatarArt::drawn() {
    static const AvatarArt art = [] {
        QStringList problems;
        const QString path = core::ResourcePaths::getInstance().getResourcesDir() +
                             QStringLiteral("/portraits/drawn.json");
        AvatarArt read = fromFile(path, problems);
        for (const QString& problem : problems) {
            core::Logger::getInstance().warn("Portraits: {}: {}", path.toStdString(),
                                             problem.toStdString());
        }
        return read;
    }();
    return art;
}

AvatarArt AvatarArt::fromFile(const QString& path, QStringList& problems) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        problems << QStringLiteral("cannot be read: %1").arg(file.errorString());
        return {};
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (!document.isObject()) {
        problems << QStringLiteral("is not a JSON object: %1").arg(error.errorString());
        return {};
    }
    return fromJson(document.object(), problems);
}

AvatarArt AvatarArt::fromJson(const QJsonObject& json, QStringList& problems) {
    AvatarArt art;
    const QJsonObject frames = json.value(QLatin1String("frames")).toObject();
    for (std::size_t i = 0; i < FRAMES; ++i) {
        art.m_views.at(i) = rectOf(frames.value(QLatin1String(FRAME_NAMES.at(i))));
        if (art.m_views.at(i).isEmpty()) {
            problems << QStringLiteral("frames.%1: must be [x, y, width, height]")
                            .arg(QLatin1String(FRAME_NAMES.at(i)));
        }
    }
    art.m_axis = json.value(QLatin1String("axis")).toDouble();
    art.m_ground = json.value(QLatin1String("ground")).toDouble();
    const QJsonObject fits = json.value(QLatin1String("fit")).toObject();
    for (auto fit = fits.constBegin(); fit != fits.constEnd(); ++fit) {
        Fit& read = art.m_fits[fit.key()];
        read.part = fit.value().toObject().value(QLatin1String("part")).toString();
        const QJsonObject values = fit.value().toObject().value(QLatin1String("values")).toObject();
        for (auto value = values.constBegin(); value != values.constEnd(); ++value) {
            read.factors.insert(value.key(), value.value().toDouble(1.0));
        }
    }
    art.m_overrides = json.value(QLatin1String("overrides")).toArray();
    const QJsonObject line = json.value(QLatin1String("lineStyle")).toObject();
    art.m_ink = line.value(QLatin1String("ink")).toString();
    art.m_lineWidth = line.value(QLatin1String("width")).toDouble(1.0);
    art.m_lighten = line.value(QLatin1String("lighten")).toInt();
    const QJsonObject sign = json.value(QLatin1String("sign")).toObject();
    art.m_signColor = sign.value(QLatin1String("color")).toString();
    art.m_signSize = sign.value(QLatin1String("size")).toDouble();
    art.readLayers(json.value(QLatin1String("layers")).toArray(), problems);
    return art;
}

void AvatarArt::readLayers(const QJsonArray& layers, QStringList& problems) {
    for (qsizetype i = 0; i < layers.size(); ++i) {
        const QJsonObject json = layers.at(i).toObject();
        Layer layer;
        const QJsonObject when = json.value(QLatin1String("when")).toObject();
        for (auto it = when.constBegin(); it != when.constEnd(); ++it) {
            Condition condition;
            condition.key = it.key();
            const QJsonValue not_ = it.value().toObject().value(QLatin1String("not"));
            condition.negated = !not_.isUndefined();
            condition.values = stringsOf(condition.negated ? not_ : it.value());
            layer.when << condition;
        }
        layer.frames = stringsOf(json.value(QLatin1String("frames")));
        layer.fit = stringsOf(json.value(QLatin1String("fit")));
        layer.style = json.value(QLatin1String("style")).toString();
        layer.fixed = json.value(QLatin1String("fixed")).toBool();
        layer.draw = json.value(QLatin1String("draw")).toArray();
        if (layer.draw.isEmpty()) {
            problems << QStringLiteral("layers[%1]: has nothing to draw").arg(i);
            continue;
        }
        m_layers << layer;
    }
}

QRectF AvatarArt::view(AvatarFrame frame) const {
    return m_views.at(static_cast<std::size_t>(frame));
}

QHash<QString, QStringList> AvatarArt::drawnValues() const {
    QHash<QString, QStringList> values;
    for (const Layer& layer : m_layers) {
        for (const Condition& condition : layer.when) {
            if (condition.negated) {
                continue;
            }
            QStringList& known = values[condition.key];
            for (const QString& value : condition.values) {
                if (!known.contains(value)) {
                    known << value;
                }
            }
        }
    }
    return values;
}

bool AvatarArt::holds(const Condition& condition, const QJsonObject& choices) {
    const QJsonValue chosen = choices.value(condition.key);
    const QStringList values = stringsOf(chosen);
    const bool any = std::any_of(values.cbegin(), values.cend(), [&condition](const QString& v) {
        return condition.values.contains(v);
    });
    return any != condition.negated;
}

bool AvatarArt::drawn(const Layer& layer, const QJsonObject& choices,
                      const AvatarLook& look) {
    if (!layer.style.isEmpty() && layer.style != look.style) {
        return false;
    }
    if (!layer.frames.isEmpty() && !layer.frames.contains(frameName(look.frame))) {
        return false;
    }
    return std::all_of(layer.when.cbegin(), layer.when.cend(),
                       [&choices](const Condition& each) { return holds(each, choices); });
}

double AvatarArt::factor(const QString& fit, const QJsonObject& choices) const {
    const Fit found = m_fits.value(fit);
    return found.factors.value(choices.value(found.part).toString(), 1.0);
}

QString AvatarArt::svg(const QJsonObject& choices, const AvatarLook& look) const {
    Drawing drawing;
    drawing.choices = choices;
    drawing.look = look;
    for (const auto& [name, key] : COLOR_KEYS) {
        drawing.colors.insert(QLatin1String(name),
                              QColor(choices.value(QLatin1String(key)).toString()));
    }
    drawing.colors.insert(QStringLiteral("card"), look.card);
    for (const auto& each : m_overrides) {
        const QJsonObject override_ = each.toObject();
        Condition condition;
        condition.key = override_.value(QLatin1String("part")).toString();
        condition.values = stringsOf(override_.value(QLatin1String("is")));
        if (holds(condition, choices)) {
            drawing.colors.insert(override_.value(QLatin1String("color")).toString(),
                                  QColor(override_.value(QLatin1String("value")).toString()));
        }
    }

    const QRectF box = view(look.frame);
    QString fixed;
    QString grown;
    for (const Layer& layer : m_layers) {
        if (drawn(layer, choices, look)) {
            (layer.fixed ? fixed : grown) += layerSvg(layer, drawing);
        }
    }
    if (look.frame == AvatarFrame::Full) {
        const QString height = number(factor(QStringLiteral("height"), choices));
        grown = QStringLiteral("<g transform=\"translate(%1 %2) scale(%3) translate(%4 %5)\">")
                    .arg(number(m_axis), number(m_ground), height, number(-m_axis),
                         number(-m_ground)) +
                grown + QStringLiteral("</g>");
    }
    return QStringLiteral(
               "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%1 %2 %3 %4\" "
               "width=\"%3\" height=\"%4\"><defs>%5</defs>")
               .arg(number(box.x()), number(box.y()), number(box.width()),
                    number(box.height()), drawing.definitions) +
           fixed + grown + signSvg(look) + QStringLiteral("</svg>");
}

QString AvatarArt::layerSvg(const Layer& layer, Drawing& drawing) const {
    QString shapes;
    for (const auto& each : layer.draw) {
        shapes += shapeSvg(each.toObject(), drawing);
    }
    for (const QString& fit : layer.fit) {
        const double scale = factor(fit, drawing.choices);
        if (!qFuzzyCompare(scale, 1.0)) {
            shapes = QStringLiteral("<g transform=\"translate(%1 0) scale(%2 1) translate(%3 0)\">")
                         .arg(number(m_axis), number(scale), number(-m_axis)) +
                     shapes + QStringLiteral("</g>");
        }
    }
    return shapes;
}

namespace {

/// The start of the element of the shape @p shape, without its paint: "<path d=\"...\"";
/// empty when it has no shape. @p view is the rectangle of a shape that fills the view.
QString geometryOf(const QJsonObject& shape, const QRectF& view) {
    if (const QJsonValue path = shape.value(QLatin1String("path")); path.isString()) {
        return QStringLiteral("<path d=\"%1\"").arg(path.toString());
    }
    if (const QJsonArray e = shape.value(QLatin1String("ellipse")).toArray(); e.size() == 4) {
        return QStringLiteral("<ellipse cx=\"%1\" cy=\"%2\" rx=\"%3\" ry=\"%4\"")
            .arg(number(e.at(0).toDouble()), number(e.at(1).toDouble()),
                 number(e.at(2).toDouble()), number(e.at(3).toDouble()));
    }
    if (const QJsonArray c = shape.value(QLatin1String("circle")).toArray(); c.size() == 3) {
        return QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"%3\"")
            .arg(number(c.at(0).toDouble()), number(c.at(1).toDouble()),
                 number(c.at(2).toDouble()));
    }
    QRectF rect = view;
    double radius = 0;
    if (const QJsonArray r = shape.value(QLatin1String("rect")).toArray(); r.size() >= 4) {
        rect = QRectF(r.at(0).toDouble(), r.at(1).toDouble(), r.at(2).toDouble(),
                      r.at(3).toDouble());
        radius = r.size() > RECT_NUMBERS ? r.at(RECT_NUMBERS).toDouble() : 0;
    } else if (!shape.value(QLatin1String("view")).toBool()) {
        return {};
    }
    return QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"%5\"")
        .arg(number(rect.x()), number(rect.y()), number(rect.width()), number(rect.height()),
             number(radius));
}

}  // namespace

QString AvatarArt::Drawing::gradient(const QJsonObject& json) {
    const QString id = QStringLiteral("g%1").arg(++gradients);
    QString stops;
    for (const auto& each : json.value(QLatin1String("stops")).toArray()) {
        const QJsonArray stop = each.toArray();
        stops += QStringLiteral("<stop offset=\"%1\" stop-color=\"%2\" stop-opacity=\"%3\"/>")
                     .arg(number(stop.at(0).toDouble()), color(stop.at(1).toString()).name(),
                          number(stop.at(2).toDouble(1.0)));
    }
    if (json.value(QLatin1String("gradient")).toString() == QLatin1String("radial")) {
        const QJsonArray center = json.value(QLatin1String("center")).toArray();
        definitions += QStringLiteral("<radialGradient id=\"%1\" cx=\"%2\" cy=\"%3\" r=\"%4\">")
                           .arg(id, number(center.at(0).toDouble(MIDDLE)),
                                number(center.at(1).toDouble(MIDDLE)),
                                number(json.value(QLatin1String("radius")).toDouble(MIDDLE))) +
                       stops + QStringLiteral("</radialGradient>");
    } else {
        const QJsonArray from = json.value(QLatin1String("from")).toArray();
        const QJsonArray to = json.value(QLatin1String("to")).toArray();
        definitions += QStringLiteral("<linearGradient id=\"%1\" x1=\"%2\" y1=\"%3\" x2=\"%4\" "
                                      "y2=\"%5\">")
                           .arg(id, number(from.at(0).toDouble(0)), number(from.at(1).toDouble(0)),
                                number(to.at(0).toDouble(0)), number(to.at(1).toDouble(1.0))) +
                       stops + QStringLiteral("</linearGradient>");
    }
    return QStringLiteral("url(#%1)").arg(id);
}

QString AvatarArt::fillSvg(const QJsonObject& shape, Drawing& drawing, bool line) const {
    const QJsonValue fillJson = shape.value(QLatin1String("fill"));
    if (fillJson.isObject()) {
        const QJsonObject gradient = fillJson.toObject();
        return line ? mixed(drawing.color(gradient.value(QLatin1String("base")).toString()),
                            Qt::white, m_lighten)
                          .name()
                    : drawing.gradient(gradient);
    }
    const QColor color = drawing.color(fillJson.toString());
    if (!color.isValid()) {
        return QStringLiteral("none");
    }
    return (line ? mixed(color, Qt::white, m_lighten) : color).name();
}

std::pair<QString, double> AvatarArt::strokeOf(const QJsonObject& shape, const QString& fill,
                                               bool line) const {
    const QString stroke = shape.value(QLatin1String("stroke")).toString();
    const double width = shape.value(QLatin1String("width")).toDouble(1.0);
    if (!line) {
        return {stroke, width};
    }
    if (!stroke.isEmpty()) {
        return {shape.value(QLatin1String("ink")).toBool(true) ? m_ink : stroke, width};
    }
    if (fill != QLatin1String("none") && shape.value(QLatin1String("outline")).toBool(true)) {
        return {m_ink, m_lineWidth};
    }
    return {stroke, width};
}

QString AvatarArt::paintSvg(const QJsonObject& shape, Drawing& drawing) const {
    const bool line = drawing.line() && !shape.value(QLatin1String("raw")).toBool();
    const QString fill = fillSvg(shape, drawing, line);
    const auto [stroke, width] = strokeOf(shape, fill, line);
    QString paint = QStringLiteral(" fill=\"%1\"").arg(fill);
    if (const QJsonValue opacity = shape.value(QLatin1String("fillOpacity")); opacity.isDouble()) {
        paint += QStringLiteral(" fill-opacity=\"%1\"").arg(number(opacity.toDouble()));
    }
    if (const QColor color = drawing.color(stroke); color.isValid()) {
        paint += QStringLiteral(" stroke=\"%1\" stroke-width=\"%2\" stroke-linecap=\"round\" "
                                "stroke-linejoin=\"round\"")
                     .arg(color.name(), number(width));
    }
    if (const QJsonValue opacity = shape.value(QLatin1String("opacity")); opacity.isDouble()) {
        paint += QStringLiteral(" opacity=\"%1\"").arg(number(opacity.toDouble()));
    }
    return paint;
}

QString AvatarArt::shapeSvg(const QJsonObject& given, Drawing& drawing) const {
    const QString style = given.value(QLatin1String("style")).toString();
    if (!style.isEmpty() && style != drawing.look.style) {
        return {};
    }
    const QJsonObject shape = drawing.line() ? inLine(given) : given;
    const QString geometry = geometryOf(shape, view(drawing.look.frame));
    if (geometry.isEmpty()) {
        return {};
    }
    QString one = geometry + paintSvg(shape, drawing) + QStringLiteral("/>");
    if (const QJsonValue scale = shape.value(QLatin1String("scaleX")); scale.isDouble()) {
        one = QStringLiteral("<g transform=\"translate(%1 0) scale(%2 1) translate(%3 0)\">")
                  .arg(number(m_axis), number(scale.toDouble()), number(-m_axis)) +
              one + QStringLiteral("</g>");
    }
    if (const QJsonArray move = shape.value(QLatin1String("move")).toArray(); move.size() == 2) {
        one = QStringLiteral("<g transform=\"translate(%1 %2)\">")
                  .arg(number(move.at(0).toDouble()), number(move.at(1).toDouble())) +
              one + QStringLiteral("</g>");
    }
    QString all = one;
    if (shape.value(QLatin1String("mirror")).toBool()) {
        all += QStringLiteral("<g transform=\"matrix(-1 0 0 1 %1 0)\">").arg(number(2 * m_axis)) +
               one + QStringLiteral("</g>");
    }
    for (const auto& dx : shape.value(QLatin1String("copies")).toArray()) {
        all += QStringLiteral("<g transform=\"translate(%1 0)\">").arg(number(dx.toDouble())) +
               one + QStringLiteral("</g>");
    }
    return all;
}

QString AvatarArt::signSvg(const AvatarLook& look) const {
    if (look.sign.isEmpty() || look.frame == AvatarFrame::Face) {
        return {};
    }
    const QRectF box = view(look.frame);
    const double size = box.width() * m_signSize;
    const bool line = look.style == QLatin1String(AvatarLook::LINE);
    return QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"end\" font-size=\"%3\" "
                          "fill=\"%4\" fill-opacity=\"0.85\">%5</text>")
        .arg(number(box.right() - (size / 2)), number(box.top() + size), number(size),
             line ? m_ink : m_signColor, look.sign.toHtmlEscaped());
}

void AvatarArt::paint(QPainter& painter, const QRectF& target, const QJsonObject& choices,
                      const AvatarLook& look) const {
    if (isEmpty() || target.isEmpty()) {
        return;
    }
    const QRectF box = view(look.frame);
    QSizeF size = box.size();
    size.scale(target.size(), Qt::KeepAspectRatio);
    const QRectF fitted(target.center().x() - (size.width() / 2),
                        target.center().y() - (size.height() / 2), size.width(),
                        size.height());
    // The renderer draws all of the drawing; only the view of the frame is shown
    QSvgRenderer renderer(svg(choices, look).toUtf8());
    painter.save();
    painter.setClipRect(fitted, Qt::IntersectClip);
    renderer.render(&painter, fitted);
    painter.restore();
}

QImage AvatarArt::image(const QJsonObject& choices, const AvatarLook& look,
                        const QSize& pixels) const {
    QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    paint(painter, QRectF(QPointF(0, 0), QSizeF(pixels)), choices, look);
    return image;
}

}  // namespace kalahari::gui
