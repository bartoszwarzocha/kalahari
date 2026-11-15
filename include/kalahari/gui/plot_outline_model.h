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
#include <QVariant>

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
    // The columns
    /// @brief The place of the text among the texts of the board; "3.2" for the second scene
    /// of the third text
    static constexpr int NUMBER_COLUMN = 0;
    static constexpr int TITLE_COLUMN = 1;        ///< Its title; of a part, the part's
    static constexpr int DESCRIPTION_COLUMN = 2;  ///< Its short description
    static constexpr int POV_COLUMN = 3;          ///< Point of view
    static constexpr int PLACE_COLUMN = 4;        ///< Place of the action
    static constexpr int TIME_COLUMN = 5;         ///< Time of the action
    static constexpr int WORDS_COLUMN = 6;        ///< Its words; of a part, the words of its texts
    static constexpr int STATUS_COLUMN = 7;       ///< Its state
    static constexpr int COLUMN_COUNT = 8;        ///< How many columns the outline has

    static constexpr int ID_ROLE = Qt::UserRole + 1;    ///< The id of the text or the part
    static constexpr int PART_ROLE = Qt::UserRole + 2;  ///< Whether the row is a part's

    /// @brief An outline without rows
    explicit PlotOutlineModel(QObject* parent = nullptr);

    /// @brief Show @p bands
    void setBands(const QList<PlotBand>& bands);

    /// @brief Whether the book is open read-only: no cell can be edited
    void setReadOnly(bool readOnly);

    /// @brief Show the rows of the scenes of the texts under the rows of the texts, or not
    void setShowScenes(bool show);

    /// @brief The name of @p column in its setting (PlotBoard::outlineSetting()): "number",
    /// "description", "pov", "place", "time", "words", "status"; nullptr for the title, which
    /// is always shown, and for no column
    [[nodiscard]] static const char* columnName(int column);

    /// @brief The title of @p column; empty: none
    [[nodiscard]] static QString columnTitle(int column);

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
    /// @brief The writer changed cell @p column (TITLE_COLUMN...) of text or scene @p elementId
    /// to @p value
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
    /// @brief The rows of the bands shown
    [[nodiscard]] QList<Row> rowsOf() const;
    void fill();
    /// @brief What cell @p column of the row of a part shows in @p role
    [[nodiscard]] static QVariant partData(const Row& row, int column, int role);
    /// @brief What cell @p column of the row of @p card shows in @p role
    [[nodiscard]] static QVariant cardData(const PlotCard& card, int column, int role);

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
    /// @brief The editors without suggestions
    explicit PlotOutlineDelegate(QObject* parent = nullptr);

    /// @brief The points of view and the places suggested
    void setSuggestions(const QStringList& povs, const QStringList& places);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

private:
    /// @brief The points of view or the places suggested in @p column; empty: none
    [[nodiscard]] QStringList suggestionsFor(int column) const;

    QStringList m_povs;
    QStringList m_places;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_OUTLINE_MODEL_H
