// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file palette_edit.h
/// @brief A palette of colors to change in the Settings: its swatches in rows, "+" after them

#ifndef KALAHARI_GUI_WIDGETS_PALETTE_EDIT_H
#define KALAHARI_GUI_WIDGETS_PALETTE_EDIT_H

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QToolButton;

namespace kalahari::gui {

class FlowLayout;

/// @brief The colors of a palette, in order, as swatches in rows with "+" after them
///
/// A click, Space or Enter on a swatch opens the color window for it; Delete takes the swatch
/// with the keys out (the last color stays); "+" or Insert adds a color at the end. The arrows
/// move among the swatches as Tab does. Each swatch tells its color in its tooltip and to
/// screen readers.
class PaletteEdit : public QWidget {
    Q_OBJECT

public:
    /// @param description What the colors are for, in the title of the color window: "A color
    ///        of the palette of the cards"
    /// @param parent Parent widget
    explicit PaletteEdit(QString description, QWidget* parent = nullptr);

    /// @brief Show @p colors ("#rrggbb"), in order
    void setColors(const QStringList& colors);

    /// @brief The colors shown, in order, "#rrggbb"
    [[nodiscard]] QStringList colors() const { return m_colors; }

    /// @brief The swatches, in order
    [[nodiscard]] QList<QToolButton*> swatches() const { return m_swatches; }

    /// @brief "+": a new color at the end
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }

    /// @brief The swatch of @p color, @p side pixels on a side at @p pixelRatio, framed with
    /// @p rim; without a color, an empty frame crossed out
    [[nodiscard]] static QPixmap swatchOf(const QColor& color, int side, qreal pixelRatio,
                                          const QColor& rim);

public slots:
    /// @brief Add a color at the end, chosen in the color window
    void addColor();

    /// @brief Change color @p index in the color window
    void changeColor(int index);

    /// @brief Take color @p index out; the last color stays
    void removeColor(int index);

signals:
    /// @brief The writer changed the colors
    void colorsChanged();

protected:
    /// @brief The keys of the swatches: Delete, Insert, the arrows
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The swatches of the colors, then "+"; a swatch stays, changed, as one of them
    /// may be what calls
    void rebuild();

    /// @brief A new swatch, of color @p index
    QToolButton* newSwatch(int index);

    /// @brief Show color @p index on its swatch
    void showSwatch(int index);

    /// @brief Do what key @p action asks of swatch @p index
    void act(int action, int index);

    /// @brief Give the keys to swatch @p index, or to "+" past the last one
    void focusSwatch(int index);

    QString m_description;  ///< What the colors are for
    QStringList m_colors;
    QList<QToolButton*> m_swatches;
    QToolButton* m_addButton = nullptr;
    FlowLayout* m_layout = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_PALETTE_EDIT_H
