/// @file kind_palette.h
/// @brief The palette: the kinds of elements of all book types, which the writer chooses for a
/// user project

#pragma once

#include "kalahari/core/book_type_registry.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QWidget>

class QEvent;
class QLabel;

namespace kalahari {
namespace core {
struct PaletteChoice;
} // namespace core

namespace gui {

class KindTile;

/// @brief The kinds of elements of all book types, as tiles in four groups: the front, main
/// and back part of the book and the Workshop
///
/// +- Front Section 8 ------------------------------------------------+
/// | [x][icon]   [ ][icon]   ...                                       |
/// |  Title page  Copyright                                            |
/// |  Base        Base                                                 |
/// | [ ] Create now                                                    |
/// +-------------------------------------------------------------------+
///
/// Each tile shows the kind's icon, its name and the book type it comes from, with a box that
/// checks the kind for the book, and on a checked kind "Create now", which makes an element of
/// the kind with the book. A click on the tile checks or unchecks its kind. A kind that can be
/// in more parts of the book has a tile in each, checked together; "Create now" makes the
/// element in the part of its tile, also in the Workshop. Kinds that open in a window the
/// program does not have yet cannot be created now, and neither can materials, which are copies
/// of files chosen when they are added.
///
/// The palette of a book that is already there (Project Properties) has no "Create now": its
/// tiles show how many elements of their kinds the book has, and the kinds of the book's type
/// are checked for good.
class KindPalette : public QWidget {
    Q_OBJECT

public:
    /// @brief The palette of the kinds of @p registry, none of them checked
    /// @param registry Packages of the kinds; they must stay loaded while the palette is there
    /// @param parent Parent widget
    explicit KindPalette(const core::BookTypeRegistry& registry, QWidget* parent = nullptr);

    /// @brief Name the group of @p place: a part of the book or the Workshop
    void setGroupName(core::BookPlace place, const QString& name);

    /// @brief The name of the group of @p place
    QString groupName(core::BookPlace place) const;

    /// @brief Check the kinds of @p choice and only them, and the locked kinds, with "Create
    ///        now" on the tiles of its elements
    void setChoice(const core::PaletteChoice& choice);

    /// @brief The checked kinds, in the order of the palette, and the elements of the tiles with
    ///        "Create now", in the order of the tiles
    core::PaletteChoice choice() const;

    /// @brief Check every kind; "Create now" stays as it was
    void checkAll();

    /// @brief Check the kinds of package @p packageId and only them, and the locked kinds; the
    ///        other kinds lose "Create now"
    void checkOnly(const QString& packageId);

    /// @brief Uncheck every kind but the locked ones, also "Create now"
    void uncheckAll();

    /// @brief Never show "Create now": the palette of a book that is already there
    void hideCreateNow();

    /// @brief Check @p kinds for good: the writer cannot uncheck them (the kinds of the book's
    ///        type); @p tooltip, on their tiles, says why. The kinds locked before can be
    ///        unchecked again, and stay checked.
    void setLocked(const QList<core::KindRef>& kinds, const QString& tooltip);

    /// @brief Whether @p kind is checked for good
    bool isLocked(const core::ElementKind* kind) const;

    /// @brief Show on each tile the number of the book's elements of its kind; none for 0
    void setElementCounts(const QHash<const core::ElementKind*, int>& counts);

    /// @brief Whether @p kind is checked
    bool isChecked(const core::ElementKind* kind) const;

    /// @brief The number of checked kinds
    int checkedCount() const;

    /// @brief The kinds of the tiles of the group of @p place, in their order
    QList<core::KindRef> kindsIn(core::BookPlace place) const;

    /// @brief Whether "Create now" can be checked on the tile of @p kind: the program can open
    ///        its elements, and it makes them without a file chosen (not a material)
    static bool canCreateNow(const core::KindRef& kind);

signals:
    /// @brief The writer checked or unchecked a kind or "Create now"
    void choiceChanged();

protected:
    /// @brief A new font gives the tiles a new size
    void changeEvent(QEvent* event) override;

private:
    /// @brief One group: its heading and its tiles
    struct Group {
        core::BookPlace place = core::BookPlace::Main;
        QLabel* name = nullptr;
        QLabel* count = nullptr;
        QList<KindTile*> tiles;
    };

    /// @brief Check or uncheck @p kind on all its tiles; unchecked, it loses "Create now"
    void setChecked(const core::ElementKind* kind, bool checked);

    /// @brief Give all the tiles the width of the font and the height of the highest of them
    void sizeTiles();

    QList<Group> m_groups;                            ///< Front, Main, Back, Workshop
    QList<core::KindRef> m_kinds;                     ///< The kinds of the tiles, in order
    QHash<const core::ElementKind*, bool> m_checked;  ///< Kind -> checked
    QSet<const core::ElementKind*> m_locked;          ///< Kinds checked for good
    bool m_createNowShown = true;                     ///< "Create now" is on the tiles
};

} // namespace gui
} // namespace kalahari
