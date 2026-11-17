// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_panel.h
/// @brief The list of a kind of materials of the Workshop on a tab in the middle of the
/// window: a table or tiles, with a search, filters and the commands of a material

#ifndef KALAHARI_GUI_PANELS_MATERIAL_LIST_PANEL_H
#define KALAHARI_GUI_PANELS_MATERIAL_LIST_PANEL_H

#include "kalahari/gui/material_list.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

class QAbstractItemDelegate;
class QAbstractItemView;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QPushButton;
class QScrollArea;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class MaterialListModel;
class MaterialListFilter;
class FilterHeader;
class CoverPlan;
class CardCellDelegate;

/// @brief The list of a kind of materials of the open book (MaterialList), on a tab in the
/// middle of the window
///
/// A table with the columns of the kind, sorted by a click on a header; the pictures, the
/// documents and the publications also as tiles. Above it the commands of the material chosen
/// (Add, Edit, Link..., Copy, Delete), the table or the tiles, the columns; under them the
/// search (Ctrl+F, Esc clears it) and the filters: linked with an element, without links, the
/// fields under the headers of the columns. A material chosen shows in the Properties panel.
///
/// The keys of the list: Enter edits the material (its card, the editor of a working note), a
/// double-click opens it, Insert adds one, F2 renames, Delete deletes, Ctrl+C copies, Alt+Enter
/// goes to the Properties panel, and the menu key and Shift+F10 open the menu of the row.
///
/// The list keeps for each kind it showed what is chosen, the search, the filters and the
/// sorting, also when it shows another kind (setList()).
///
/// The list of the cover has its materials in groups by what they are (a description, a
/// tagline, an opinion...), each under a row with its name, as long as Group by Kind is down;
/// the filter Kind shows one of them. Besides the table and the tiles it has the plan of the
/// cover (CoverPlan), and the menu of a row chooses a material for the cover or rejects it.
/// A kind hidden in the Navigator (MaterialList::HIDDEN_SETTING) shows a page that says so,
/// with a button that shows it again. The gear opens the settings of the materials.
///
/// A list of a kind of cards (characters, locations, items; CardList) has a small portrait
/// beside each name, and tiles with the portraits, a stripe in the color of each card, its kind
/// and In Short. F2 edits the cell of a field or of In Short in the table (CardCellDelegate),
/// and renames the card in the column of the names; Relation... adds a relation with another
/// card, and the gear opens the settings of the cards.
class MaterialListPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int ID_ROLE = Qt::UserRole + 1;     ///< The id of the row's material
    static constexpr int SORT_ROLE = Qt::UserRole + 2;   ///< What the column sorts by
    static constexpr int DETAIL_ROLE = Qt::UserRole + 3; ///< A tile's line under the title
    static constexpr int TILE_ROLE = Qt::UserRole + 4;   ///< A tile's picture or icon (QPixmap)
    /// The name and the number of the group of a row that names one; none for a material
    static constexpr int HEADING_ROLE = Qt::UserRole + 5;
    static constexpr int STATE_ROLE = Qt::UserRole + 6;  ///< The state of a material of the cover
    static constexpr int CARD_ROLE = Qt::UserRole + 7;   ///< Whether the row is a card (bool)
    static constexpr int COLOR_ROLE = Qt::UserRole + 8;  ///< The color of a card (QColor)
    static constexpr int SUMMARY_ROLE = Qt::UserRole + 9;  ///< In Short of a card, on its tile

    /// @brief The list @p list (MaterialList::kinds(), MaterialList::ALL)
    explicit MaterialListPanel(core::ProjectManager& book, const QString& list,
                               QWidget* parent = nullptr);
    ~MaterialListPanel() override;

    /// @brief The list it shows
    [[nodiscard]] QString list() const { return m_list; }

    /// @brief Show list @p list, with what it had chosen and its filters when it showed it
    /// before
    void setList(const QString& list);

    /// @brief Show the materials of the open book again, keeping the one chosen
    void refresh();

    /// @brief The icons of the list and of its materials again (another theme)
    void refreshIcons();

    /// @brief The material chosen; empty: none
    [[nodiscard]] QString currentMaterial() const;

    /// @brief Choose material @p elementId, shown in the list; false when the list does not
    /// show it
    bool selectMaterial(const QString& elementId);

    /// @brief Give the keys to the search (Ctrl+F)
    void focusSearch();

    /// @brief Give the keys to the table or the tiles
    void focusList();

    /// @brief The table or the tiles, whichever is shown
    [[nodiscard]] QAbstractItemView* view() const;

    /// @brief The table
    [[nodiscard]] QTableView* table() const { return m_table; }

    /// @brief The tiles
    [[nodiscard]] QListView* tiles() const { return m_tiles; }

    /// @brief The plan of the cover
    [[nodiscard]] CoverPlan* plan() const { return m_plan; }

    /// @brief The button of the plan of the cover; hidden in the other lists
    [[nodiscard]] QToolButton* planButton() const { return m_planButton; }

    /// @brief Copy Entries of the list Sources; hidden in the other lists
    [[nodiscard]] QToolButton* entriesButton() const { return m_entriesButton; }

    /// @brief Group by Kind of the list of the cover; hidden in the other lists
    [[nodiscard]] QToolButton* groupsButton() const { return m_groupsButton; }

    /// @brief The filter Kind of the list of the cover; hidden in the other lists
    [[nodiscard]] QComboBox* partBox() const { return m_partBox; }

    /// @brief Add of the list
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }

    /// @brief The gear: the settings of the materials
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

    /// @brief Restore to Its Place of the list of the clippings; hidden in the other lists
    [[nodiscard]] QToolButton* restoreButton() const { return m_restoreButton; }

    /// @brief The page of a kind hidden in the Navigator; shown in place of the list
    [[nodiscard]] QWidget* hiddenPage() const { return m_hiddenPage; }

    /// @brief The search
    [[nodiscard]] QLineEdit* searchField() const { return m_search; }

    /// @brief "Linked with:" of the filters
    [[nodiscard]] QComboBox* linkedWithBox() const { return m_linkedWith; }

    /// @brief "Without links" of the filters
    [[nodiscard]] QCheckBox* withoutLinksBox() const { return m_withoutLinks; }

    /// @brief The rows shown, sorted and filtered
    [[nodiscard]] QSortFilterProxyModel* rows() const;

    /// @brief Show the tiles of a list that has them, or its table; kept for the list
    void showTiles(bool tiles);

    /// @brief Show the plan of the cover of the list of the cover, or its table or tiles as
    /// before; kept for the list
    void showPlan(bool plan);

    /// @brief Show the materials of the list of the cover in groups by what they are, or in
    /// one table; kept for the list
    void showGroups(bool groups);

    /// @brief Show only the materials of the cover of @p part (core::CoverMaterial::BLURB...);
    /// empty: all
    void setPartFilter(const QString& part);

    /// @brief Show the fields that filter the columns under their headers, or hide them
    void showColumnFilters(bool shown);

    /// @brief Filter column @p columnId by @p text; shows the fields of the columns
    void setColumnFilter(const QString& columnId, const QString& text);

    /// @brief Show column @p columnId or hide it; kept for the list
    void showColumn(const QString& columnId, bool shown);

    /// @brief Ids of the columns shown, in their order
    [[nodiscard]] QStringList shownColumns() const;

    /// @brief Fill @p menu with the commands of the material chosen (the menu of a row)
    void fillMenu(QMenu& menu);

    /// @brief Copy the material chosen (MaterialList::copyTextOf())
    void copyCurrent();

    /// @brief Copy the entries in the bibliography of all the sources the list shows, in its
    /// order, one in each line: as text and with italics (the list Sources)
    void copyEntries();

signals:
    /// @brief The writer chose material @p elementId in the list
    void materialChosen(const QString& elementId);

    /// @brief Edit material @p elementId: its card, or the editor of a working note; @p text:
    /// the keys to its text (a quote's words), else to its title
    void requestEdit(const QString& elementId, bool text);

    /// @brief Open material @p elementId as the settings say (a double-click)
    void requestOpen(const QString& elementId);

    /// @brief Show material @p elementId in the folder of its file
    void requestShowInFolder(const QString& elementId);

    /// @brief Rename material @p elementId (F2)
    void requestRename(const QString& elementId);

    /// @brief Delete material @p elementId, after asking (Delete)
    void requestDelete(const QString& elementId);

    /// @brief Choose what material @p elementId is linked with (Link...)
    void requestLink(const QString& elementId);

    /// @brief Add a material to list @p list (Insert)
    void requestAdd(const QString& list);

    /// @brief Add a material of the cover of @p part to list @p list (the menu of Add)
    void requestAddCover(const QString& list, const QString& part);

    /// @brief Give material @p elementId of the cover the state @p state: chosen for the
    /// cover, rejected, or a draft (empty)
    void requestCoverState(const QString& elementId, const QString& state);

    /// @brief Open the settings of the materials (the gear)
    void requestSettings();

    /// @brief Restore clipping @p elementId to the place it was cut from
    void requestRestore(const QString& elementId);

    /// @brief Open the text clipping @p elementId was cut from at its place
    void requestShowOrigin(const QString& elementId);

    /// @brief Show list @p list, a kind hidden in the Navigator, again
    void requestShowKind(const QString& list);

    /// @brief Bring the Properties panel to the front with the keys in it (Alt+Enter)
    void requestProperties(const QString& elementId);

    /// @brief Something to say on the status bar
    void message(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief What the list keeps of each kind it shows
    struct State {
        QString search;
        QString linkedWith;  ///< The element or the section; empty: any
        bool withoutLinks = false;
        bool columnFilters = false;
        QHash<QString, QString> columnFilter;  ///< Column -> its filter
        QString current;                       ///< The material chosen
        QString column;                        ///< The column of the current cell of the table
        QString sortColumn;                    ///< Empty: MaterialList::firstSortOf()
        QString part;                          ///< The filter Kind of the cover; empty: all
        Qt::SortOrder sortOrder = Qt::AscendingOrder;
        bool kept = false;  ///< false: the list never showed the kind
    };

    void createLayout();
    /// @brief The texts of the commands and what they do; their icons are refreshIcons()'s
    void createCommands();
    /// @brief The rows of the list from the open book, the columns, the targets of "Linked
    /// with" and the commands
    void load();
    void keepState();
    void restoreState();
    void applyColumns();
    void applyFilters();
    void fillLinkedWith();
    void updateCommands();
    void updateHeading();
    void updateFooter();
    void showMenuAt(QAbstractItemView* view, const QPoint& pos);
    void showColumnsMenu(const QPoint& globalPos);
    void keepColumns();
    /// @brief The current material changed in @p view: the other view and the Properties
    void onCurrentChanged(QAbstractItemView* view, const QModelIndex& current);
    /// @brief A key of the list: true when it did something with it
    bool handleKey(QAbstractItemView* view, QKeyEvent* key);
    /// @brief Rename... of material @p id from the keys of @p view: a list of cards edits the
    /// cell of a field, a name is renamed; nothing in a book open read-only
    void renameFromKeys(QAbstractItemView* view, const QString& id);
    [[nodiscard]] QString idAt(const QModelIndex& index) const;
    [[nodiscard]] int columnOf(const QString& columnId) const;
    /// @brief The plan of the cover from the rows shown
    void updatePlan();
    /// @brief The page of a kind hidden in the Navigator, or the list
    void updateHidden();
    /// @brief Whether the rows that name the groups of the cover are shown: the table of the
    /// list of the cover with Group by Kind down
    [[nodiscard]] bool groupsApply() const;
    /// @brief The materials shown, without the rows that name groups
    [[nodiscard]] int shownMaterials() const;
    /// @brief The choices of Kind and the menu of Add of the list of the cover
    void fillCoverChoices();
    /// @brief The texts of the list that say what it has: the materials or the cards
    void applyListTexts();
    /// @brief The height of the rows of the table, the size of its icons and of the tiles
    /// @param pictures The list has the thumbnails of its pictures
    void applyRowSizes(bool pictures);
    /// @brief The commands of a material of the cover in its menu: choose, reject, a draft
    void addCoverActions(QMenu& menu, const MaterialRow& row);
    /// @brief The menu of card @p row: Edit, Relation..., Rename..., Delete..., Properties
    void fillCardMenu(QMenu& menu, const MaterialRow& row);
    /// @brief Edit the current cell of the table of a list of cards, which F2 edits; false
    /// when it is no such cell
    bool editCell(QAbstractItemView* view);
    /// @brief The column of the table to choose a row in: that of the current cell, also when
    /// the list was filled again; the first one when it is hidden
    [[nodiscard]] int tableColumn() const;

    /// @brief The text and what Restore to Its Place of the list of the clippings does
    void createClippingCommands();

    /// @brief Restore to Its Place and the gear for list @p row of the material chosen: the
    /// button shown in the list of the clippings, enabled for a clipping whose text is in the
    /// book; the gear names the settings of the clippings there
    void updateClippingCommands(const MaterialRow* row);

    /// @brief Restore to Its Place and Go to Its Place in the menu of clipping @p row; nothing
    /// for another material
    void addClippingItems(QMenu& menu, const MaterialRow& row);

    /// @brief The gear: Settings > Editor > Clippings in the list of the clippings, else the
    /// settings of the materials (requestSettings())
    void openSettings();

    QString m_list;
    QHash<QString, State> m_states;
    QList<MaterialColumn> m_columns;

    MaterialListModel* m_model = nullptr;
    MaterialListFilter* m_filter = nullptr;

    QLabel* m_heading = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_editButton = nullptr;
    QToolButton* m_linkButton = nullptr;
    QToolButton* m_copyButton = nullptr;
    QToolButton* m_entriesButton = nullptr;
    QToolButton* m_deleteButton = nullptr;
    QToolButton* m_tableButton = nullptr;
    QToolButton* m_tilesButton = nullptr;
    QToolButton* m_columnsButton = nullptr;
    QToolButton* m_planButton = nullptr;
    QToolButton* m_groupsButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QToolButton* m_restoreButton = nullptr;  ///< Restore to Its Place: the list of the clippings
    QMenu* m_addMenu = nullptr;           ///< Add of the cover: the kind of the material
    QLabel* m_partLabel = nullptr;
    QComboBox* m_partBox = nullptr;
    QStackedWidget* m_pages = nullptr;    ///< The list, or the page of a kind hidden
    QWidget* m_hiddenPage = nullptr;
    QLabel* m_hiddenIcon = nullptr;
    QLabel* m_hiddenText = nullptr;
    QPushButton* m_showKindButton = nullptr;
    QScrollArea* m_planArea = nullptr;
    CoverPlan* m_plan = nullptr;
    QLineEdit* m_search = nullptr;
    QAction* m_searchIcon = nullptr;  ///< The icon at the start of the search
    QComboBox* m_linkedWith = nullptr;
    QCheckBox* m_withoutLinks = nullptr;
    QToolButton* m_columnFiltersButton = nullptr;
    QToolButton* m_clearButton = nullptr;
    QStackedWidget* m_stack = nullptr;
    QTableView* m_table = nullptr;
    FilterHeader* m_header = nullptr;
    QListView* m_tiles = nullptr;
    QLabel* m_footer = nullptr;
    QLabel* m_sorting = nullptr;

    QAbstractItemDelegate* m_rowDelegate = nullptr;  ///< Paints the rows of the materials
    CardCellDelegate* m_cardDelegate = nullptr;      ///< Edits the cells of the cards (F2)
    bool m_cards = false;    ///< The list is one of the cards (CardList::isCardList())
    bool m_loading = false;  ///< The list is being filled: no material is chosen by the writer
    int m_settingsListener = 0;    ///< core::SettingsManager::subscribe() id: the portraits
    core::ProjectManager& m_book;  ///< The book of its window
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_MATERIAL_LIST_PANEL_H
