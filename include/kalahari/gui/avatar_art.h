// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file avatar_art.h
/// @brief The drawings that portraits of parts are put together from: a set of layers read from
/// a file, each drawn for some choices of the parts, and the portrait they make as an image

#ifndef KALAHARI_GUI_AVATAR_ART_H
#define KALAHARI_GUI_AVATAR_ART_H

#include <QColor>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <array>
#include <utility>

class QJsonObject;
class QPainter;
class QSize;

namespace kalahari::gui {

/// @brief What a portrait of parts shows
enum class AvatarFrame {
    Bust,  ///< The head and the shoulders, as on the card
    Full,  ///< The whole figure, with the build and the height
    Face   ///< The face close up, for the tiles of the eyes, the nose and the mouth
};

/// @brief How a portrait of parts is drawn: its style, its frame and what it takes from the card
struct AvatarLook {
    /// @brief The style of colored shapes, which reads well in a small size
    static constexpr const char* FLAT = "flat";
    /// @brief The style of a drawing in ink with light colors
    static constexpr const char* LINE = "line";

    QString style = QString::fromLatin1(FLAT);  ///< FLAT or LINE
    AvatarFrame frame = AvatarFrame::Bust;      ///< What it shows
    QColor card;                                ///< The color of the card, for its background
    QString sign;                               ///< A sign in its corner ("♀"); empty: none
};

/// @brief A set of drawings of the parts of a portrait (resources/portraits/drawn.json)
///
/// The set lists layers in the order they are drawn. A layer is drawn when the choices of the
/// portrait match its conditions ("when": { "hair": ["long", "curly"], "clothes": { "not":
/// ["robe"] } }; a choice of many, "extras", matches when it has the value) and the frame is
/// one of its frames. Its shapes have colors of the portrait: "{skin}", "{hair}", "{eyes}",
/// "{clothes}", "{bottom}" and "{card}", darker ("{skin-10}"), lighter ("{skin+30}") or
/// mixed with another color ("{skin>a03a35@60}"). A layer can be made wider or narrower by
/// a choice ("fit": ["face"]: the factor of the chosen face in "fit"), around the middle of
/// the portrait; in the whole figure everything but the "fixed" layers grows by the factor
/// "height" around the ground. The style LINE draws the shapes in light colors with an outline
/// in ink, unless a shape says otherwise ("line": { ... }, "raw", "outline", "style"). A shape
/// can be drawn again mirrored around the middle ("mirror") or moved to the right ("copies":
/// [28]), and moved ("move": [0, 3]) or made narrower around the middle ("scaleX": 0.9)
/// itself.
class AvatarArt {
public:
    /// @brief The set of the program; empty when its file cannot be read
    [[nodiscard]] static const AvatarArt& drawn();

    /// @brief The set in @p path
    /// @param problems Gets what is wrong in the file, one line each
    [[nodiscard]] static AvatarArt fromFile(const QString& path, QStringList& problems);

    /// @brief The set in @p json
    /// @param problems Gets what is wrong in it, one line each
    [[nodiscard]] static AvatarArt fromJson(const QJsonObject& json, QStringList& problems);

    /// @brief Whether it has no layers
    [[nodiscard]] bool isEmpty() const { return m_layers.isEmpty(); }

    /// @brief The part of the drawing that @p frame shows
    [[nodiscard]] QRectF view(AvatarFrame frame) const;

    /// @brief The values of the parts that some layer is drawn for, by the part ("hair")
    [[nodiscard]] QHash<QString, QStringList> drawnValues() const;

    /// @brief The SVG of the portrait with the choices @p choices: each part ("hair") with
    /// its value ("long"), each color ("skin") with its "#rrggbb"; "extras" is a list
    [[nodiscard]] QString svg(const QJsonObject& choices, const AvatarLook& look) const;

    /// @brief The portrait drawn in @p target of @p painter, as large as it fits, in the middle
    void paint(QPainter& painter, const QRectF& target, const QJsonObject& choices,
               const AvatarLook& look) const;

    /// @brief The portrait as an image of @p pixels, transparent where it has no background
    [[nodiscard]] QImage image(const QJsonObject& choices, const AvatarLook& look,
                               const QSize& pixels) const;

private:
    /// @brief A condition of a layer: the part and the values it is drawn for, or not drawn for
    struct Condition {
        QString key;         ///< The part: "hair"
        QStringList values;  ///< Its values
        bool negated = false;
    };

    /// @brief A layer of the set
    struct Layer {
        QList<Condition> when;  ///< All of them hold when it is drawn
        QStringList frames;     ///< "bust", "full", "face"; empty: all
        QStringList fit;        ///< The parts whose factors make it wider or narrower
        QString style;          ///< The style it is drawn in; empty: both
        bool fixed = false;     ///< Not grown with the height
        QJsonArray draw;        ///< Its shapes
    };

    /// @brief How a choice makes layers wider or narrower
    struct Fit {
        QString part;                    ///< The part whose value it reads: "build"
        QHash<QString, double> factors;  ///< By the value: "slim" 0.86
    };

    /// @brief What drawing a portrait needs while it puts the SVG together
    struct Drawing;

    /// @brief The frames there are, in the order of AvatarFrame
    static constexpr std::size_t FRAMES = 3;

    [[nodiscard]] static bool holds(const Condition& condition, const QJsonObject& choices);
    [[nodiscard]] static bool drawn(const Layer& layer, const QJsonObject& choices,
                                    const AvatarLook& look);
    [[nodiscard]] double factor(const QString& fit, const QJsonObject& choices) const;
    [[nodiscard]] QString layerSvg(const Layer& layer, Drawing& drawing) const;
    [[nodiscard]] QString shapeSvg(const QJsonObject& given, Drawing& drawing) const;
    /// @brief The fill of @p shape: its color or gradient, light in the style LINE (@p line);
    /// "none" without one
    [[nodiscard]] QString fillSvg(const QJsonObject& shape, Drawing& drawing, bool line) const;
    /// @brief The outline of @p shape filled with @p fill: its color (empty: none) and width;
    /// in the style LINE (@p line) the ink
    [[nodiscard]] std::pair<QString, double> strokeOf(const QJsonObject& shape,
                                                      const QString& fill, bool line) const;
    [[nodiscard]] QString paintSvg(const QJsonObject& shape, Drawing& drawing) const;
    [[nodiscard]] QString signSvg(const AvatarLook& look) const;
    void readLayers(const QJsonArray& layers, QStringList& problems);

    QList<Layer> m_layers;
    std::array<QRectF, FRAMES> m_views;  ///< By the frame
    QHash<QString, Fit> m_fits;          ///< By their names: "face", "shoulders", "height"
    QJsonArray m_overrides;  ///< Colors a choice gives ("armor": the clothes are steel)
    double m_axis = 0;       ///< x of the middle of the portrait
    double m_ground = 0;     ///< y of the ground of the whole figure
    QString m_ink;           ///< The color of the outlines of the style LINE
    double m_lineWidth = 1;  ///< Their width
    int m_lighten = 0;       ///< How much lighter the colors of the style LINE are, in percent
    QString m_signColor;     ///< The color of the sign in the corner, in the style FLAT
    double m_signSize = 0;   ///< Its height, a part of the width of the view
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_AVATAR_ART_H
