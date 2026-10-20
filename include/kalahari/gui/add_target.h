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
/// The window offers the kinds of the place it was opened on: the whole book, a section of it,
/// a group, the list of an element, or the Workshop. It never offers the kinds of the book
/// together with those of the Workshop.
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

} // namespace gui
} // namespace kalahari

Q_DECLARE_METATYPE(kalahari::gui::AddTarget)
