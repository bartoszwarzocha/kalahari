// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list.h
/// @brief The lists of the materials of the Workshop on their tabs: the kinds of the Materials,
/// the columns of each kind and what a material shows in them

#ifndef KALAHARI_GUI_MATERIAL_LIST_H
#define KALAHARI_GUI_MATERIAL_LIST_H

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief A column of a list of materials
struct MaterialColumn {
    QString id;            ///< MaterialList::TITLE..., or FIELD_PREFIX and the name of a field
    QString title;         ///< Its header; empty for the thumbnail
    bool shown = true;     ///< Shown while the writer has not chosen the columns of the list
    bool sortable = true;  ///< false: the thumbnail
    bool editable = false; ///< Its cells are edited in the table (F2): the fields of a card
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
    QString group;                   ///< Of a material of the cover: its part
                                     ///< (core::CoverMaterial::BLURB...); the group of its row
    QString state;                   ///< Of a material of the cover: its state (CHOSEN...)
    QString place;                   ///< Of a material of the cover: its place on the cover
    QString body;                    ///< Of a material of the cover: its text; empty: none
    QString heading;                 ///< A row that names a group of the list of the cover,
                                     ///< not a material: its name and number; empty: a
                                     ///< material
    bool clipping = false;           ///< A clipping (core::ClippingData)
    bool originLive = false;         ///< Of a clipping: the text it was cut from is in the book
    bool card = false;               ///< A card of a character, a location or an item
                                     ///< (CardList)
    QString color;                   ///< Of a card: its color ("#RRGGBB"); empty: none
    QString summary;                 ///< Of a card: In Short, on its tile
    QString volume;                  ///< Of a material of the cover of a series: the name
                                     ///< of its volume, or of the whole series
                                     ///< (VolumeNames::nameOf()); empty: no series
    int volumeOrder = 0;             ///< Its place: 0 the whole series, 1 the first volume...
                                     ///< (VolumeNames::orderOf()); the groups of a series go
                                     ///< by it first
    bool volumeHeading = false;      ///< A row that names a volume of a series (or the whole
                                     ///< series) above the groups of its materials: it
                                     ///< collapses and expands them
};

/// @brief The lists of the materials of the book open in @p book: the kinds of the Materials of its
/// Workshop, the columns of each list and the rows of their materials
///
/// A list is named by the kind of its materials with its package ("kalahari.base:quote");
/// ALL is the list of All Materials, the working notes too, and SOURCES that of the materials
/// with a bibliographic description, of every kind. A list of a kind of cards (characters,
/// locations, items) has its columns and rows from CardList. The writer chooses the columns of
/// each list and their order, and between the table and the tiles of the pictures, the
/// documents and the publications; the program keeps it for each list (LISTS_SETTING).
class MaterialList {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::MaterialList)

public:
    static constexpr const char* ALL = "";  ///< The list of All Materials
    /// @brief The list of the sources: the materials with a bibliographic description
    static constexpr const char* SOURCES = "sources";

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
    static constexpr const char* COVER_TEXT = "coverText";     ///< The cover: text or title
    static constexpr const char* COVER_PART = "coverPart";     ///< The cover: what it is
    static constexpr const char* COVER_AUTHOR = "coverAuthor"; ///< The cover: who made it
    static constexpr const char* COVER_PLACE = "coverPlace";   ///< The cover: where it goes
    static constexpr const char* COVER_STATE = "coverState";   ///< The cover: chosen...
    static constexpr const char* COVER_LENGTH = "coverLength"; ///< The cover: of the text
    static constexpr const char* COVER_SOURCE = "coverSource"; ///< The cover: of an opinion
    static constexpr const char* COVER_DATE = "coverDate";     ///< The cover: of an opinion
    static constexpr const char* COVER_LICENSE = "coverLicense";  ///< The cover: of a picture
    /// The cover of a series: the volume, or the whole series
    static constexpr const char* VOLUME = "volume";
    static constexpr const char* CLIPPING_FROM = "from";  ///< A clipping: the text it was cut from
    static constexpr const char* CLIPPING_CUT = "cut";    ///< A clipping: when it was cut
    static constexpr const char* CLIPPING_NOTE = "note";  ///< A clipping: its note
    static constexpr const char* CLIPPING_INSERTED = "inserted";  ///< A clipping: inserted last
    static constexpr const char* SOURCE_KIND = "sourceKind";        ///< A source: a book...
    static constexpr const char* SOURCE_AUTHORS = "sourceAuthors";  ///< A source: its authors
    static constexpr const char* SOURCE_YEAR = "sourceYear";        ///< A source: its year
    static constexpr const char* SOURCE_INCLUSION = "sourceInclusion";  ///< A source: whether
                                                                        ///< the bibliography
                                                                        ///< has it
    static constexpr const char* SOURCE_PLACES = "sourcePlaces";  ///< A source: its places in
                                                                  ///< the text
    static constexpr const char* QUOTE_SOURCE = "quoteSource";    ///< A quote: its source
    /// @}

    /// @brief The setting of the columns and the view of each list, an object: list ->
    /// {"columns": [ids shown, in their order], "tiles": true for the tiles, "plan": true
    /// for the plan of the cover, "groups": false for the list of the cover without its
    /// groups}
    static constexpr const char* LISTS_SETTING = "materials.lists";

    /// @brief The setting of the kinds of materials that the Navigator does not show, in every
    /// book: a list of kinds with their packages ("kalahari.base:cover"). Their materials stay
    /// in the books and in All Materials.
    static constexpr const char* HIDDEN_SETTING = "materials.hiddenKinds";

    /// @brief The setting of what a kind clicked in the Navigator does: "own", the list of
    /// the kind on a tab of its own (the usual), or "one", the one tab of the lists shows it
    static constexpr const char* TABS_SETTING = "materials.typeTabs";

    /// @brief The lists of the book open in @p book besides ALL, as the Navigator has them in its
    /// Materials: the working notes and the kinds of materials, in the order of the book's
    /// type, without the kinds hidden (HIDDEN_SETTING)
    static QStringList kinds(const core::ProjectManager& book);

    /// @brief The kinds of materials of the book open in @p book as the Navigator would have them
    /// with none hidden, in its order
    static QStringList allKinds(const core::ProjectManager& book);

    /// @brief The kinds Settings > Navigator shows or hides: those of the book open in @p book
    /// (allKinds()), else the materials of the base package, then the kinds hidden that are neither
    static QStringList settingsKinds(const core::ProjectManager& book);

    /// @brief How many materials of @p list the Workshop of the book open in @p book has; -1
    /// without a book
    static int countOf(const core::ProjectManager& book, const QString& list);

    /// @brief The kinds hidden in the Navigator (HIDDEN_SETTING)
    static QStringList hiddenKinds();

    /// @brief Whether the Navigator does not show @p list, a kind of materials
    static bool isHidden(const QString& list);

    /// @brief Hide @p list, a kind of materials, in the Navigator of every book, or show it
    /// again (HIDDEN_SETTING)
    static void setHidden(const QString& list, bool hidden);

    /// @brief Whether @p list is the list of the materials of the cover
    static bool isCover(const QString& list);

    /// @brief The kinds Add of @p list offers: those of the Materials for ALL, those that can be
    /// a source for SOURCES (a publication first), else the kind of the list
    static QStringList addKindsOf(const core::ProjectManager& book, const QString& list);

    /// @brief Name of @p list: the plural of its kind, "Quotes"; "All Materials" for ALL,
    /// "Sources" for SOURCES
    static QString nameOf(const QString& list);

    /// @brief Icon of @p list: that of its kind in the Navigator, of Materials for ALL
    static QString iconIdOf(const QString& list);

    /// @brief Whether @p list has tiles besides the table: the pictures, the documents, the
    /// publications and the cards (CardList)
    static bool hasTiles(const QString& list);

    /// @brief Columns of @p list, then one for each field of the writer's that its materials
    /// have (hidden at first), by name
    static QList<MaterialColumn> columnsOf(const core::ProjectManager& book, const QString& list);

    /// @brief Add to @p columns one column for each field of the writer's that @p elements have,
    /// hidden at first, sorted by name (FIELD_PREFIX)
    static void appendFieldColumns(QList<MaterialColumn>& columns,
                                   const QList<const core::ProjectElement*>& elements);

    /// @brief Column @p list sorts by at first: the quote of the quotes, the kind of All
    /// Materials, the time of the cut of the clippings, else the title
    static QString firstSortOf(const QString& list);

    /// @brief The order @p list sorts in at first: the clippings the last cut first, the others
    /// from A to Z
    static Qt::SortOrder firstSortOrderOf(const QString& list);

    /// @brief Whether @p list is the list of the clippings
    static bool isClippings(const QString& list);

    /// @brief Rows of the materials of @p list in the book open in @p book, in the order of the
    /// Workshop
    static QList<MaterialRow> rowsOf(const core::ProjectManager& book, const QString& list);

    /// @brief Columns of @p list the writer shows, in their order; those shown at first while
    /// the writer has not chosen
    static QStringList shownColumns(const core::ProjectManager& book, const QString& list);

    /// @brief Show @p columns of @p list, in their order (LISTS_SETTING)
    static void setShownColumns(const QString& list, const QStringList& columns);

    /// @brief Whether @p list shows its tiles, not its table (LISTS_SETTING)
    static bool tilesShown(const QString& list);

    /// @brief Show the tiles of @p list, or its table (LISTS_SETTING)
    static void setTilesShown(const QString& list, bool tiles);

    /// @brief Whether the list of the cover shows the plan of the cover, the list of the texts
    /// for publishers its set (LISTS_SETTING)
    static bool planShown(const QString& list);

    /// @brief Show the plan of the cover or the set for a publisher in @p list, or its table
    /// or tiles (LISTS_SETTING)
    static void setPlanShown(const QString& list, bool plan);

    /// @brief Whether the table of the list of the cover, or of the texts for publishers, has
    /// its materials in groups by their parts, as it has at first (LISTS_SETTING)
    static bool groupsShown(const QString& list);

    /// @brief Show the materials of the list of the cover, or of the texts for publishers, in
    /// groups, or in one table
    static void setGroupsShown(const QString& list, bool groups);

    /// @brief Whether a kind clicked in the Navigator has a tab of its own (TABS_SETTING)
    static bool ownTabs();

    /// @brief Kind of a document of @p format ("PDF", "DOCX"): PDF, text, spreadsheet,
    /// presentation, e-book, or other
    static QString documentKindOf(const QString& format);

    /// @brief What Copy of a material puts on the clipboard: the words of a quote, the address
    /// of a page or of a picture on the web without a copy, the path of a file, else the title;
    /// in SOURCES the entry of a source in the bibliography of the book (rowsOf())
    static QString copyTextOf(const core::ProjectManager& book, const QString& elementId);

    /// @brief Whether the rows of @p list can be in groups by what they are: the list of the
    /// cover
    static bool hasGroups(const QString& list);

    /// @brief The column of @p list that says what its rows are, which the groups hide:
    /// COVER_PART; empty for a list without groups
    static QString partColumnOf(const QString& list);

    /// @brief The columns of a kind @p kindId with columns of its own: the clippings; none for
    /// another kind
    static QList<MaterialColumn> ownColumnsOf(const QString& kindId);

    /// @brief The column of the volume of the list of the cover (VOLUME) in @p columns of
    /// @p list, before what the rows are (partColumnOf()), when the book open in @p book is a
    /// series
    static void addVolumeColumn(const core::ProjectManager& book, const QString& list,
                                QList<MaterialColumn>& columns);

    /// @brief What material @p element of the cover of the series open in @p book shows of its
    /// volume; nothing for another material, or in a book
    /// that is no series
    static void setVolumeTexts(MaterialRow& row, const core::ProjectManager& book,
                               const core::ProjectElement& element);

    /// @brief Columns of the list of the clippings: the title, where it comes from, when it
    /// was cut, the words, the note and when it was inserted (shown when a clipping inserted
    /// stays in the Workshop), the time of the file and the links
    static QList<MaterialColumn> clippingColumns();

    /// @brief What clipping @p element of the book open in @p book shows in the columns of its
    /// list; nothing for an element that is not a clipping
    static void setClippingTexts(MaterialRow& row, const core::ProjectManager& book,
                                 const core::ProjectElement& element);
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_MATERIAL_LIST_H
