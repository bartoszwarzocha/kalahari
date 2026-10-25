/// @file material_list_panel.h
/// @brief The list of a kind of materials of the Workshop on a tab in the middle of the
/// window: a table or tiles, with a search, filters and the commands of a material

#pragma once

#include "kalahari/gui/material_list.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

class QAbstractItemView;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QToolButton;

namespace kalahari {
namespace gui {

class MaterialListModel;
class MaterialListFilter;
class FilterHeader;

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
class MaterialListPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int ID_ROLE = Qt::UserRole + 1;     ///< The id of the row's material
    static constexpr int SORT_ROLE = Qt::UserRole + 2;   ///< What the column sorts by
    static constexpr int DETAIL_ROLE = Qt::UserRole + 3; ///< A tile's line under the title
    static constexpr int TILE_ROLE = Qt::UserRole + 4;   ///< A tile's picture or icon (QPixmap)

    /// @brief The list @p list (MaterialList::kinds(), MaterialList::ALL)
    explicit MaterialListPanel(const QString& list, QWidget* parent = nullptr);
    ~MaterialListPanel() override;

    /// @brief The list it shows
    QString list() const { return m_list; }

    /// @brief Show list @p list, with what it had chosen and its filters when it showed it
    /// before
    void setList(const QString& list);

    /// @brief Show the materials of the open book again, keeping the one chosen
    void refresh();

    /// @brief The icons of the list and of its materials again (another theme)
    void refreshIcons();

    /// @brief The material chosen; empty: none
    QString currentMaterial() const;

    /// @brief Choose material @p elementId, shown in the list; false when the list does not
    /// show it
    bool selectMaterial(const QString& elementId);

    /// @brief Give the keys to the search (Ctrl+F)
    void focusSearch();

    /// @brief Give the keys to the table or the tiles
    void focusList();

    /// @brief The table or the tiles, whichever is shown
    QAbstractItemView* view() const;

    /// @brief The table
    QTableView* table() const { return m_table; }

    /// @brief The tiles
    QListView* tiles() const { return m_tiles; }

    /// @brief The search
    QLineEdit* searchField() const { return m_search; }

    /// @brief "Linked with:" of the filters
    QComboBox* linkedWithBox() const { return m_linkedWith; }

    /// @brief "Without links" of the filters
    QCheckBox* withoutLinksBox() const { return m_withoutLinks; }

    /// @brief The rows shown, sorted and filtered
    QSortFilterProxyModel* rows() const;

    /// @brief Show the tiles of a list that has them, or its table; kept for the list
    void showTiles(bool tiles);

    /// @brief Show the fields that filter the columns under their headers, or hide them
    void showColumnFilters(bool shown);

    /// @brief Filter column @p columnId by @p text; shows the fields of the columns
    void setColumnFilter(const QString& columnId, const QString& text);

    /// @brief Show column @p columnId or hide it; kept for the list
    void showColumn(const QString& columnId, bool shown);

    /// @brief Ids of the columns shown, in their order
    QStringList shownColumns() const;

    /// @brief Fill @p menu with the commands of the material chosen (the menu of a row)
    void fillMenu(QMenu& menu);

    /// @brief Copy the material chosen (MaterialList::copyTextOf())
    void copyCurrent();

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
        QString sortColumn;                    ///< Empty: MaterialList::firstSortOf()
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
    QString idAt(const QModelIndex& index) const;
    int columnOf(const QString& columnId) const;

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
    QToolButton* m_deleteButton = nullptr;
    QToolButton* m_tableButton = nullptr;
    QToolButton* m_tilesButton = nullptr;
    QToolButton* m_columnsButton = nullptr;
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

    bool m_loading = false;  ///< The list is being filled: no material is chosen by the writer
};

} // namespace gui
} // namespace kalahari
