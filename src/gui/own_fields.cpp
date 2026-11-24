// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file own_fields.cpp
/// @brief The writer's own fields of a book and of its series

#include "kalahari/gui/own_fields.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"

#include <QUuid>

#include <algorithm>

namespace kalahari::gui::own_fields {

namespace {

/// @brief The length of the random part of a new id
constexpr int ID_LENGTH = 12;

/// @brief A field of the book as the windows show it
OwnField ownOf(const core::BookCustomField& field)
{
    OwnField own;
    own.id = field.id;
    own.name = field.name;
    own.value = field.value;
    own.description = field.description;
    return own;
}

/// @brief A field of the series as a volume with these values shows it
OwnField ownOf(const core::BookCustomField& field, const QMap<QString, QString>& volumeValues)
{
    OwnField own = ownOf(field);
    own.series = true;
    own.seriesValue = field.value;
    own.ownValue = volumeValues.contains(field.id);
    own.value = own.ownValue ? volumeValues.value(field.id) : field.value;
    return own;
}

/// @brief By name, as the lists show them
void sortByName(QList<OwnField>& fields)
{
    std::stable_sort(fields.begin(), fields.end(), [](const OwnField& a, const OwnField& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
}

/// @brief The fields of the book, of the series and the volume's values, to change and save
struct Fields {
    QList<core::BookCustomField> book;
    QList<core::BookCustomField> series;
    QMap<QString, QString> values;

    Fields(const core::ProjectManager& manager, const QString& bookId)
        : book(manager.bookCustomFields(bookId))
        , series(manager.seriesCustomFields())
        , values(manager.seriesFieldValues(bookId))
    {
    }

    bool save(core::ProjectManager& manager, const QString& bookId) const
    {
        return manager.setCustomFields(book, series, values, bookId);
    }
};

/// @brief The field of an id in a list; null: none
core::BookCustomField* fieldIn(QList<core::BookCustomField>& fields, const QString& id)
{
    const auto found = std::find_if(fields.begin(), fields.end(),
                                    [&id](const core::BookCustomField& field) { return field.id == id; });
    return found != fields.end() ? &*found : nullptr;
}

}  // namespace

QList<OwnField> of(const core::ProjectManager& book, const QString& bookId)
{
    QList<OwnField> own;
    for (const core::BookCustomField& field : book.bookCustomFields(bookId)) {
        own.append(ownOf(field));
    }
    sortByName(own);
    if (!book.isSeries()) {
        return own;
    }
    QList<OwnField> series;
    const QMap<QString, QString> volumeValues = book.seriesFieldValues(bookId);
    for (const core::BookCustomField& field : book.seriesCustomFields()) {
        series.append(ownOf(field, volumeValues));
    }
    sortByName(series);
    return own + series;
}

QMap<QString, editor::CustomFieldValue> values(const core::ProjectManager& book,
                                               const core::ProjectBook& volume)
{
    QMap<QString, editor::CustomFieldValue> shown;
    if (book.isSeries()) {
        for (const core::BookCustomField& field : book.seriesCustomFields()) {
            const auto own = volume.seriesFieldValues.constFind(field.id);
            shown.insert(field.id,
                         {field.name,
                          own != volume.seriesFieldValues.cend() ? *own : field.value,
                          field.description});
        }
    }
    for (const core::BookCustomField& field : volume.customFields) {
        shown.insert(field.id, {field.name, field.value, field.description});
    }
    return shown;
}

std::optional<OwnField> find(const core::ProjectManager& book, const QString& id,
                             const QString& bookId)
{
    for (const OwnField& field : of(book, bookId)) {
        if (field.id == id) {
            return field;
        }
    }
    return std::nullopt;
}

bool add(core::ProjectManager& book, const OwnField& field, const QString& bookId)
{
    if (field.id.isEmpty() || (field.series && !book.isSeries()) || find(book, field.id, bookId)) {
        return false;
    }
    Fields fields(book, bookId);
    core::BookCustomField added;
    added.id = field.id;
    added.name = field.name.trimmed();
    added.description = field.description.trimmed();
    if (field.series) {
        added.value = field.seriesValue;
        fields.series.append(added);
        if (field.ownValue) {
            fields.values.insert(field.id, field.value);
        }
    } else {
        added.value = field.value;
        fields.book.append(added);
    }
    return fields.save(book, bookId);
}

bool change(core::ProjectManager& book, const OwnField& field, const QString& bookId)
{
    Fields fields(book, bookId);
    if (core::BookCustomField* own = fieldIn(fields.book, field.id)) {
        own->name = field.name.trimmed();
        own->value = field.value;
        own->description = field.description.trimmed();
        return fields.save(book, bookId);
    }
    core::BookCustomField* series = book.isSeries() ? fieldIn(fields.series, field.id) : nullptr;
    if (series == nullptr) {
        return false;
    }
    series->name = field.name.trimmed();
    series->value = field.seriesValue;
    series->description = field.description.trimmed();
    if (field.ownValue) {
        fields.values.insert(field.id, field.value);
    } else {
        fields.values.remove(field.id);
    }
    return fields.save(book, bookId);
}

bool setValue(core::ProjectManager& book, const QString& id, const QString& value,
              const QString& bookId)
{
    std::optional<OwnField> field = find(book, id, bookId);
    if (!field) {
        return false;
    }
    field->value = value;
    if (field->series && !field->ownValue) {
        field->seriesValue = value;
    }
    return change(book, *field, bookId);
}

bool remove(core::ProjectManager& book, const QString& id, const QString& bookId)
{
    Fields fields(book, bookId);
    const auto byId = [&id](const core::BookCustomField& field) { return field.id == id; };
    const qsizetype before = fields.book.size() + fields.series.size();
    fields.book.removeIf(byId);
    fields.series.removeIf(byId);
    if (fields.book.size() + fields.series.size() == before) {
        return false;
    }
    fields.values.remove(id);
    return fields.save(book, bookId);
}

QString newId()
{
    return QStringLiteral("field-") +
           QUuid::createUuid().toString(QUuid::Id128).left(ID_LENGTH);
}

bool isNameTaken(const core::ProjectManager& book, const QString& name, const QString& exceptId,
                 const QString& bookId)
{
    const QString wanted = name.simplified();
    const QList<OwnField> fields = of(book, bookId);
    return std::any_of(fields.cbegin(), fields.cend(), [&](const OwnField& field) {
        return field.id != exceptId &&
               field.name.simplified().compare(wanted, Qt::CaseInsensitive) == 0;
    });
}

}  // namespace kalahari::gui::own_fields
