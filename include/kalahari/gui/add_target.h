/// @file add_target.h
/// @brief Where the Add Element window adds a new element: the node of the Navigator or the
/// element in the editor it was opened on

#pragma once

#include "kalahari/core/book_type_registry.h"

#include <QList>
#include <QMetaType>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief Where the Add Element window adds a new element, and which kinds it offers there
///
/// The window starts at the place it was opened on: the whole book, a section of it, a group,
/// the list of an element, or the Workshop. Without a choice at its top (AddChooser::None) it
/// offers the kinds of that place only. It never offers the kinds of the book together with
/// those of the Workshop.
struct AddTarget {
    /// @brief The place the window was opened on
    enum class Scope {
        Book,      ///< The book: the kinds of all its sections (the book's node, Book > Add...)
        Section,   ///< A section of the book: its kinds
        Group,     ///< A group of the book (a part): the kinds inside it
        Element,   ///< An element of the book: the kinds of its list, right after it
        Workshop,  ///< The Workshop, a group of it or an element of it: the kinds of the Workshop
    };

    Scope scope = Scope::Book;
    core::BookPlace place = core::BookPlace::Main;  ///< Section: which one
    QString id;                                     ///< Group, Element: its id
    /// Workshop: the only kinds offered (those of a group or of an element of the Workshop);
    /// empty: all kinds of the Workshop
    QList<core::KindRef> kinds;
    core::KindRef kind;  ///< The kind chosen at the start; none: the usual one of the place
};

/// @brief What the top of the Add Element window chooses from, as Settings > Navigator sets it
/// (NavigatorPanel::ADD_WINDOW_SETTING)
///
/// With a choice the place the window was opened on only chooses where it starts: the group,
/// the kind and the place; the kinds of the other groups are a click or a key away. The kinds
/// of the book and those of the Workshop are never in one list.
enum class AddChooser {
    Sections,  ///< "sections", the usual one: a section of the book or the Workshop, one at a time
    Book,      ///< "book": the book with the kinds of all its sections, or the Workshop
    None,      ///< "place": no choice; only the kinds of the place the window was opened on
};

} // namespace gui
} // namespace kalahari

Q_DECLARE_METATYPE(kalahari::gui::AddTarget)
