// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_outline_model.h
/// @brief The outline: the texts of the book in a table, with their description, point of
/// view, place and time of the action, words and state

#ifndef KALAHARI_GUI_PLOT_OUTLINE_MODEL_H
#define KALAHARI_GUI_PLOT_OUTLINE_MODEL_H

#include "kalahari/gui/plot_board.h"

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>

namespace kalahari::gui {

/// @brief The rows of the outline: each part over its texts, and the texts outside the parts,
/// each text over the rows of its scenes (setShowScenes())
///
/// The cells of a text and of a scene can be edited: its title, description, point of view,
/// place, time and state; a scene may have no title. The model changes nothing itself: an edit
/// asks for the change (cellEdited()), and whoever shows the model gives it the texts again
/// when the book has it.
class PlotOutlineModel : public QAbstractTableModel {
    Q_OBJECT

public:
    /// @brief The columns
    enum Column {
        NumberColumn,       ///< The place of the text among the texts of the board; "3.2"
                            ///< for the second scene of the third text
        TitleColumn,        ///< Its title; of a part, the part's
        DescriptionColumn,  ///< Its short description
        PovColumn,          ///< Point of view
        PlaceColumn,        ///< Place of the action
        TimeColumn,         ///< Time of the action
        WordsColumn,        ///< Its words; of a part, the words of its texts
        StatusColumn,       ///< Its state
        ColumnCount
    };

    static constexpr int ID_ROLE = Qt::UserRole + 1;    ///< The id of the text or the part
    static constexpr int PART_ROLE = Qt::UserRole + 2;  ///< Whether the row is a part's

    explicit PlotOutlineModel(QObject* parent = nullptr);

    /// @brief Show @p bands
    void setBands(const QList<PlotBand>& bands);

    /// @brief Whether the book is open read-only: no cell can be edited
    void setReadOnly(bool readOnly);

    /// @brief Show the rows of the scenes of the texts under the rows of the texts, or not
    void setShowScenes(bool show);

    /// @brief The row of the text, the scene (PlotBoard::sceneId()) or the part @p id; -1: none
    [[nodiscard]] int rowOf(const QString& id) const;

    /// @brief The id of the text or the scene of row @p row; empty for a part's row or no row
    [[nodiscard]] QString cardIdAt(int row) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

signals:
    /// @brief The writer changed cell @p column (Column) of text or scene @p elementId to
    /// @p value
    void cellEdited(const QString& elementId, int column, const QString& value);

private:
    /// @brief A row: a part, a text or a scene
    struct Row {
        bool part = false;
        QString partTitle;   ///< Of a part
        int partTexts = 0;   ///< Of a part: its texts
        int partWords = 0;   ///< Of a part: the words of its texts
        QString partId;
        PlotCard card;       ///< Of a text or a scene
    };
    void fill();

    QList<PlotBand> m_bands;
    QList<Row> m_rows;
    bool m_readOnly = false;
    bool m_showScenes = true;
};

/// @brief The editors of the cells of the outline: a list of the states, and the points of
/// view and places of the Workshop suggested
class PlotOutlineDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit PlotOutlineDelegate(QObject* parent = nullptr);

    /// @brief The points of view and the places suggested
    void setSuggestions(const QStringList& povs, const QStringList& places);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

private:
    QStringList m_povs;
    QStringList m_places;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_OUTLINE_MODEL_H
