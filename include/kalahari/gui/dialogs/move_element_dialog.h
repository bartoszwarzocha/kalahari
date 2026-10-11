/// @file move_element_dialog.h
/// @brief Move: a new place in the book for an element, with the elements inside it

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QList>
#include <QString>

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Move: a new place in the book for an element of the Navigator
///
/// The list shows the book with the element, in bold, at the place the writer chooses, and
/// only the parts of the book where its kind can be: a chapter in the main section and in its
/// parts, a part in the main section. The places are those of
/// core::ProjectManager::movePlacesOf(); the elements inside the element (the chapters of a
/// part) go with it. A kind of the start or of the end (a prologue, an epilogue) also gets its
/// places as options: the start or the end of a part of the book, and first in its first group
/// or last in its last one.
///
/// Under the list the dialog says where the element is now and, when the chosen place makes a
/// prologue stop opening its part of the book or an epilogue stop closing it, says so. The
/// dialog can be accepted only with a place other than the one the element has.
class MoveElementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The element and the book without it
    struct Book {
        core::ProjectElement element;   ///< The element, with the elements inside it
        core::ElementPlace from;        ///< Its place, in the book without it
        QList<PickerSection> sections;  ///< The front, main and back section of the book
                                        ///< without it; none: the element is not in the book
    };

    /// @brief Element @p elementId of the book open in core::ProjectManager, and the book
    /// without it; no sections when the book has no element @p elementId
    static Book openBook(const QString& elementId);

    /// @brief Create the dialog
    /// @param book The element and the book without it
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    MoveElementDialog(const Book& book, const core::BookTypeRegistry& registry,
                      QWidget* parent = nullptr);

    /// @brief The chosen place, in the book without the element
    /// (core::ProjectManager::moveElementTo())
    core::ElementPlace place() const;

    /// @brief The place of the element in the book
    ElementPlacePicker* picker() const { return m_picker; }

private:
    /// @brief Where @p place of the book without the element is, as a part of a sentence: "at
    /// the end of the main section", "in the part "Part One", before "Chapter 3""
    QString placeText(const core::ElementPlace& place) const;

    /// @brief For a kind of the start or of the end, its places as options: the start or the
    /// end of each part of the book it can be in, and first in its first group there or last
    /// in its last one
    QList<PlaceOption> edgeOptions() const;

    /// @brief @p text with the name of the element's kind for "{kind}", and of the kind of
    /// @p group for "{group}"
    QString named(const QString& text, const core::ProjectElement* group = nullptr) const;

    /// @brief Group @p id of the book without the element, or nullptr
    const core::ProjectElement* groupOf(const QString& id) const;

    /// @brief The part @p place of the book without the element, or nullptr
    const PickerSection* sectionOf(core::BookPlace place) const;

    /// @brief The dialog can be accepted only with another place
    void updateAcceptButton();

    Book m_book;
    const core::BookTypeRegistry* m_registry = nullptr;
    ElementPlacePicker* m_picker = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
