// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file own_fields.h
/// @brief The writer's own fields of a book and of its series, as the windows show and change
///        them (core::BookCustomField in the .klh file)
///
/// An own field has a name ("Narrator's Name"), a value ("Ilias") and a short description for
/// the tooltips. A field of the book is the book's alone; in a series a field can also be
/// one of all the volumes, and a volume can give it a value of its own. The fields in the text
/// (editor::FieldType::Custom) show the value by the field's id, so the name and the value
/// can change.

#ifndef KALAHARI_GUI_OWN_FIELDS_H
#define KALAHARI_GUI_OWN_FIELDS_H

#include "kalahari/editor/text_fields.h"

#include <QList>
#include <QMap>
#include <QString>

#include <optional>

namespace kalahari::core {
class ProjectManager;
struct ProjectBook;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief An own field of a book, as the windows show it
struct OwnField {
    QString id;           ///< Unique among the fields of the book and of its series
    QString name;         ///< "Narrator's Name"
    QString value;        ///< Its value in the book: of a field of the series the volume's own
                          ///< value, else the series'
    QString description;  ///< For the tooltips; may be empty
    bool series = false;  ///< A field of all the volumes of the series
    QString seriesValue;  ///< Of a field of the series: its value in the series
    bool ownValue = false;  ///< Of a field of the series: the volume gives it its own value

    bool operator==(const OwnField&) const = default;
};

namespace own_fields {

/// @brief The own fields of a book: its own, then those of its series, each by name
/// @param bookId The book of a series; empty: the current book
[[nodiscard]] QList<OwnField> of(const core::ProjectManager& book,
                                 const QString& bookId = QString());

/// @brief The own fields of a volume as the fields in its texts show them
[[nodiscard]] QMap<QString, editor::CustomFieldValue> values(const core::ProjectManager& book,
                                                             const core::ProjectBook& volume);

/// @brief The field of an id among of(); nothing for an id no field has
[[nodiscard]] std::optional<OwnField> find(const core::ProjectManager& book, const QString& id,
                                           const QString& bookId = QString());

/// @brief Give the book (with OwnField::series its series) a new field, and save the book
/// @return false without a book, in one open read-only, for a field of the series outside a
///         series, or when the book cannot be saved
bool add(core::ProjectManager& book, const OwnField& field, const QString& bookId = QString());

/// @brief Change a field of the book or of its series (by its id): its name, value,
///        description; of a field of the series, whether the volume gives it its own value
/// @return false without such a field, or as add()
bool change(core::ProjectManager& book, const OwnField& field,
            const QString& bookId = QString());

/// @brief Change only the value of a field in the book: of a field of the series the value of
///        the series, or the volume's own one when it has it (the Properties panel)
bool setValue(core::ProjectManager& book, const QString& id, const QString& value,
              const QString& bookId = QString());

/// @brief Take a field away from the book or from its series
/// @return false without such a field, or as add()
bool remove(core::ProjectManager& book, const QString& id, const QString& bookId = QString());

/// @brief An id no field has yet
[[nodiscard]] QString newId();

/// @brief Whether a name is that of another field of the book (case not told apart)
[[nodiscard]] bool isNameTaken(const core::ProjectManager& book, const QString& name,
                               const QString& exceptId = QString(),
                               const QString& bookId = QString());

}  // namespace own_fields

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_OWN_FIELDS_H
