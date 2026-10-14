/// @file sections_field.h
/// @brief Fields of the sections of a book: a set of their names or no sections, and the
/// writer's own names

#pragma once

#include "kalahari/core/book_project.h"

#include <QComboBox>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>

class QLineEdit;

namespace kalahari {
namespace gui {

/// @brief The front, main and back section, in the order of the names of a set
inline constexpr std::array<core::BookPlace, 3> SECTION_PLACES = {
    core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back};

/// @brief Field choosing how the Navigator divides a book: a set of names of its front, main
/// and back section in the language of the book, the writer's own names, or no sections
///
/// The New Book dialog and the Properties panel of a book have it. Each item holds its value:
/// the id of a set, core::ProjectBook::CUSTOM_SECTIONS or NO_SECTIONS.
class SectionsComboBox : public QComboBox {
    Q_OBJECT

public:
    /// @brief Item of a book without sections
    static constexpr const char* NO_SECTIONS = "none";

    /// @brief Create the field with the names of the sets in English
    explicit SectionsComboBox(QWidget* parent = nullptr);

    /// @brief Name the sets in language @p language; the chosen item stays, without a signal
    void setLanguage(const QString& language);

    /// @brief The chosen item: the id of a set, core::ProjectBook::CUSTOM_SECTIONS or
    /// NO_SECTIONS
    QString choice() const;

    /// @brief Choose item @p item; the first set when the field has no such item
    void choose(const QString& item);

    /// @brief The item that shows sections @p sections of a book
    static QString itemOf(const core::BookSections& sections);

    /// @brief Whether item @p item is a set of names
    static bool isNameSet(const QString& item);

protected:
    /// @brief A set of names wider than the field ends with an ellipsis
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief Fill the items in the language of the names
    void populate();

    QString m_language;  ///< Language of the names of the sets
};

/// @brief Fields of the writer's own names of the front, main and back section
///
/// An empty field shows the name of the first set, which a section without a name gets.
class SectionNamesEdit : public QWidget {
    Q_OBJECT

public:
    /// @brief Create the fields next to each other (Qt::Horizontal) or one below the other
    explicit SectionNamesEdit(Qt::Orientation orientation, QWidget* parent = nullptr);

    /// @brief Show the names of the first set in language @p language in the empty fields
    void setLanguage(const QString& language);

    /// @brief The names in the fields, without the spaces at their ends
    QStringList names() const;

    /// @brief Put names @p names in the fields, without a signal
    void setNames(const QStringList& names);

    /// @brief The field of the first name, the buddy of a label
    QLineEdit* firstField() const;

signals:
    /// @brief The writer typed in a field
    void edited();

    /// @brief The writer pressed Enter in a field, or left a field after changing it
    void editingFinished();

private:
    std::array<QLineEdit*, SECTION_PLACES.size()> m_fields{};
};

}  // namespace gui
}  // namespace kalahari
