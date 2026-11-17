// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_fields_box.h
/// @brief The group of the Properties panel with what the plot board says of a text: its
/// description, point of view, place and time of the action, and the color of its card

#ifndef KALAHARI_GUI_PANELS_PLOT_FIELDS_BOX_H
#define KALAHARI_GUI_PANELS_PLOT_FIELDS_BOX_H

#include <QGroupBox>
#include <QString>

class QComboBox;
class QEvent;
class QLineEdit;
class QObject;
class QPlainTextEdit;
class QStringListModel;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief What the plot board says of a text of the main section, changed in the Properties
/// panel as on its card (F2) and in the outline
///
/// The point of view and the place suggest the names of the characters and places of the
/// Workshop, and take any text. A field keeps what was written when the keys leave it (Enter
/// in a field of one line), the color at once; the book saves it (core::ProjectManager::
/// setPlot()). A change made elsewhere (on the board) shows here at once, but in a field being
/// written in.
class PlotFieldsBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the book @p book, without a text
    explicit PlotFieldsBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show what the board says of element @p elementId: only a text of the main
    /// section has a card there; any other element hides the group
    void showElement(const QString& elementId);

    /// @brief The element shown; empty: none
    [[nodiscard]] QString elementId() const { return m_elementId; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The fields from the book again, but the one with the keys
    void fill();
    /// @brief The colors of the cards of the current theme on the items of the color
    void fillColors();
    /// @brief Keep what the fields say, when it is not what the book has
    void save();

    core::ProjectManager& m_book;
    QString m_elementId;
    QPlainTextEdit* m_description = nullptr;
    QLineEdit* m_pov = nullptr;
    QLineEdit* m_place = nullptr;
    QLineEdit* m_time = nullptr;
    QComboBox* m_color = nullptr;
    QStringListModel* m_povs = nullptr;    ///< The suggestions of the point of view
    QStringListModel* m_places = nullptr;  ///< The suggestions of the place
    bool m_filling = false;  ///< The fields are being filled: no change to save
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PLOT_FIELDS_BOX_H
