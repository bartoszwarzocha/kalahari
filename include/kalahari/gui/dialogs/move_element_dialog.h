// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file move_element_dialog.h
/// @brief Move: a new place in the book for an element, with the elements inside it

#ifndef KALAHARI_GUI_DIALOGS_MOVE_ELEMENT_DIALOG_H
#define KALAHARI_GUI_DIALOGS_MOVE_ELEMENT_DIALOG_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QList>
#include <QString>

class QComboBox;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

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
///
/// In a series, a list of the volumes above the book chooses the volume the element goes to,
/// its own at first; another volume shows its places as it is (movePlacesIn()), with the
/// element where an element of its kind is added to its main section.
class MoveElementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief A volume of a series that the element can go to
    struct Volume {
        QString id;                     ///< Id of its book
        QString name;                   ///< Its name, as the Navigator shows it ("Volume 2:
                                        ///< Dunes")
        QList<PickerSection> sections;  ///< Its front, main and back section; in the volume of
                                        ///< the element, without the element
    };

    /// @brief The element and the book without it
    struct Book {
        core::ProjectElement element;   ///< The element, with the elements inside it
        core::ElementPlace from;        ///< Its place, in the book without it
        QList<PickerSection> sections;  ///< The front, main and back section of the book
                                        ///< without it; none: the element is not in the book
        QList<Volume> volumes;          ///< The volumes of a series, the element's own one
                                        ///< among them; none for a project with one book
        qsizetype volume = 0;           ///< The index of the element's volume in volumes
    };

    /// @brief Element @p elementId of the book open in @p projects, and the book
    /// without it; no sections when the book has no element @p elementId
    static Book openBook(const core::ProjectManager& projects, const QString& elementId);

    /// @brief Create the dialog
    /// @param book The element and the book without it
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    MoveElementDialog(const Book& book, const core::BookTypeRegistry& registry,
                      QWidget* parent = nullptr);

    /// @brief The chosen place, in the book without the element, or in the chosen volume
    /// (core::ProjectManager::moveElementTo())
    [[nodiscard]] core::ElementPlace place() const;

    /// @brief The id of the chosen volume of a series (core::ProjectManager::moveElementTo());
    /// empty for a project with one book
    [[nodiscard]] QString volume() const;

    /// @brief Whether the chosen place or volume is another than the element's
    [[nodiscard]] bool movesElement() const;

    /// @brief The list of the volumes of a series; nullptr for a project with one book
    [[nodiscard]] QComboBox* volumeBox() const { return m_volumeBox; }

    /// @brief The place of the element in the book
    [[nodiscard]] ElementPlacePicker* picker() const { return m_picker; }

private:
    /// @brief Where @p place of the book without the element is, as a part of a sentence: "at
    /// the end of the main section", "in the part "Part One", before "Chapter 3""
    [[nodiscard]] QString placeText(const core::ElementPlace& place) const;

    /// @brief For a kind of the start or of the end, its places as options: the start or the
    /// end of each part of the book it can be in, and first in its first group there or last
    /// in its last one; in the volume the list shows
    [[nodiscard]] QList<PlaceOption> edgeOptions() const;

    /// @brief The list of the volumes above the book, for a series
    void addVolumeBox();

    /// @brief Show volume @p index of the series: the element at its place in its own volume,
    /// where an element of its kind is added to the main section of another one
    void showVolume(int index);

    /// @brief @p text with the name of the element's kind for "{kind}", and of the kind of
    /// @p group for "{group}"
    QString named(const QString& text, const core::ProjectElement* group = nullptr) const;

    /// @brief Group @p id of the book without the element, or nullptr
    [[nodiscard]] const core::ProjectElement* groupOf(const QString& id) const;

    /// @brief The part @p place of the book without the element, or nullptr
    [[nodiscard]] const PickerSection* sectionOf(core::BookPlace place) const;

    /// @brief The dialog can be accepted only with another place
    void updateAcceptButton();

    Book m_book;
    QList<PickerSection> m_shown;  ///< The sections the list shows: of the chosen volume
    const core::BookTypeRegistry* m_registry = nullptr;
    QComboBox* m_volumeBox = nullptr;
    ElementPlacePicker* m_picker = nullptr;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_MOVE_ELEMENT_DIALOG_H
