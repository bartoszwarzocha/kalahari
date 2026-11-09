/// @file material_list.h
/// @brief The lists of the materials of the Workshop on their tabs: the kinds of the Materials,
/// the columns of each kind and what a material shows in them

#pragma once

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace kalahari {
namespace gui {

/// @brief A column of a list of materials
struct MaterialColumn {
    QString id;            ///< MaterialList::TITLE..., or FIELD_PREFIX and the name of a field
    QString title;         ///< Its header; empty for the thumbnail
    bool shown = true;     ///< Shown while the writer has not chosen the columns of the list
    bool sortable = true;  ///< false: the thumbnail
};

/// @brief A material of the Workshop as a row of a list shows it
struct MaterialRow {
    QString id;                      ///< The element
    QString kind;                    ///< Its kind with its package (KindRef::reference())
    QString iconId;                  ///< Its icon, as the Navigator shows it
    QHash<QString, QString> texts;   ///< The text of each column it has something in
    QHash<QString, QVariant> order;  ///< What a column sorts by, where not by its text
    QStringList links;               ///< What it is linked with (ElementLink::to)
    QString picture;                 ///< Absolute path of its picture; empty: none
    int rotation = 0;                ///< Turn of its picture (ProjectElement::rotation)
    QString copy;                    ///< What Copy puts on the clipboard (copyTextOf())
    QString search;                  ///< All its texts in small letters, for the search
    bool text = false;               ///< A working note: a text that opens in the editor
};

/// @brief The lists of the materials of the open book: the kinds of the Materials of its
/// Workshop, the columns of each list and the rows of their materials
///
/// A list is named by the kind of its materials with its package ("kalahari.base:quote");
/// ALL is the list of All Materials, the working notes too. The writer chooses the columns of
/// each list and their order, and between the table and the tiles of the pictures, the
/// documents and the publications; the program keeps it for each list (LISTS_SETTING).
class MaterialList {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::MaterialList)

public:
    static constexpr const char* ALL = "";  ///< The list of All Materials

    /// @name Ids of the columns
    /// @{
    static constexpr const char* THUMBNAIL = "thumbnail";      ///< A picture's, small
    static constexpr const char* TITLE = "title";
    static constexpr const char* QUOTE = "quote";              ///< A quote's words
    static constexpr const char* WORDS = "words";              ///< Of a working note
    static constexpr const char* STATUS = "status";            ///< Of a working note
    static constexpr const char* MODIFIED = "modified";        ///< Of the file
    static constexpr const char* SITE = "site";                ///< The host of the address
    static constexpr const char* ADDRESS = "address";
    static constexpr const char* FORMAT = "format";            ///< Of the file: "PNG"
    static constexpr const char* DIMENSIONS = "dimensions";    ///< Of a picture, in pixels
    static constexpr const char* ORIGIN = "origin";            ///< Of a picture: disk, web
    static constexpr const char* DOCUMENT_KIND = "kind";       ///< Of a document: PDF, text...
    static constexpr const char* SIZE = "size";                ///< Of the file
    static constexpr const char* FILE = "file";                ///< The name of the file
    static constexpr const char* DESCRIPTION = "description";
    static constexpr const char* LINKS = "links";              ///< What it is linked with, how
    static constexpr const char* TYPE = "type";                ///< All Materials: the kind
    static constexpr const char* DETAILS = "details";          ///< All Materials: a summary
    static constexpr const char* FIELD_PREFIX = "field:";      ///< A field of the writer's
    /// @}

    /// @brief The setting of the columns and the view of each list, an object: list ->
    /// {"columns": [ids shown, in their order], "tiles": true for the tiles}
    static constexpr const char* LISTS_SETTING = "materials.lists";

    /// @brief The setting of what a kind clicked in the Navigator does: "own", the list of
    /// the kind on a tab of its own (the usual), or "one", the one tab of the lists shows it
    static constexpr const char* TABS_SETTING = "materials.typeTabs";

    /// @brief The lists of the open book besides ALL, as the Navigator has them in its
    /// Materials: the working notes and the kinds of materials, in the order of the book's type
    static QStringList kinds();

    /// @brief Name of @p list: the plural of its kind, "Quotes"; "All Materials" for ALL
    static QString nameOf(const QString& list);

    /// @brief Icon of @p list: that of its kind in the Navigator, of Materials for ALL
    static QString iconIdOf(const QString& list);

    /// @brief Whether @p list has tiles besides the table: the pictures, the documents and the
    /// publications
    static bool hasTiles(const QString& list);

    /// @brief Columns of @p list, then one for each field of the writer's that its materials
    /// have (hidden at first), by name
    static QList<MaterialColumn> columnsOf(const QString& list);

    /// @brief Column @p list sorts by at first: the quote of the quotes, the kind of All
    /// Materials, else the title
    static QString firstSortOf(const QString& list);

    /// @brief Rows of the materials of @p list in the open book, in the order of the Workshop
    static QList<MaterialRow> rowsOf(const QString& list);

    /// @brief Columns of @p list the writer shows, in their order; those shown at first while
    /// the writer has not chosen
    static QStringList shownColumns(const QString& list);

    /// @brief Show @p columns of @p list, in their order (LISTS_SETTING)
    static void setShownColumns(const QString& list, const QStringList& columns);

    /// @brief Whether @p list shows its tiles, not its table (LISTS_SETTING)
    static bool tilesShown(const QString& list);

    /// @brief Show the tiles of @p list, or its table (LISTS_SETTING)
    static void setTilesShown(const QString& list, bool tiles);

    /// @brief Whether a kind clicked in the Navigator has a tab of its own (TABS_SETTING)
    static bool ownTabs();

    /// @brief Kind of a document of @p format ("PDF", "DOCX"): PDF, text, spreadsheet,
    /// presentation, e-book, or other
    static QString documentKindOf(const QString& format);

    /// @brief What Copy of a material puts on the clipboard: the words of a quote, the address
    /// of a page or of a picture on the web without a copy, the path of a file, else the title
    static QString copyTextOf(const QString& elementId);
};

} // namespace gui
} // namespace kalahari
