// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_card_editor.h
/// @brief A card of the plot board edited where it is: its title, description, point of view,
/// place and time of the action

#ifndef KALAHARI_GUI_WIDGETS_PLOT_CARD_EDITOR_H
#define KALAHARI_GUI_WIDGETS_PLOT_CARD_EDITOR_H

#include "kalahari/gui/plot_board.h"

#include <QFrame>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace kalahari::gui {

struct PlotCardEdit;

/// @brief The fields of a card of the plot board over the card (F2)
///
/// Tab goes from the title to the description, the point of view, the place and the time of
/// the action; the point of view and the place suggest the characters and the places of the
/// Workshop, and take any other words too. Ctrl+Enter, or Enter in a field of one line, keeps
/// what was written (accepted()); Esc leaves the card as it was (rejected()), as the frame of
/// an annotation does.
class PlotCardEditor : public QFrame {
    Q_OBJECT

public:
    /// @param card The card edited
    /// @param povs The points of view to choose from
    /// @param places The places to choose from
    /// @param parent The viewport of the board
    PlotCardEditor(const PlotCard& card, const QStringList& povs, const QStringList& places,
                   QWidget* parent = nullptr);

    /// @brief The id of the card's text
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief What the fields have now
    [[nodiscard]] PlotCardEdit edit() const;

    /// @brief The fields
    [[nodiscard]] QLineEdit* titleEdit() const { return m_title; }
    [[nodiscard]] QPlainTextEdit* descriptionEdit() const { return m_description; }
    [[nodiscard]] QLineEdit* povEdit() const { return m_pov; }
    [[nodiscard]] QLineEdit* placeEdit() const { return m_place; }
    [[nodiscard]] QLineEdit* timeEdit() const { return m_time; }

    /// @brief The keys go to the title, all of it chosen
    void focusTitle();

signals:
    /// @brief Keep what was written (Ctrl+Enter, the Save button)
    void accepted();
    /// @brief Leave the card as it was (Esc, the Cancel button)
    void rejected();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

private:
    QString m_elementId;
    core::ElementPlot m_plot;  ///< As the card had it: the color and the unknown fields stay
    QLineEdit* m_title = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QLineEdit* m_pov = nullptr;
    QLineEdit* m_place = nullptr;
    QLineEdit* m_time = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_cancel = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_PLOT_CARD_EDITOR_H
