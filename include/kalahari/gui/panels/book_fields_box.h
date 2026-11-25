// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_fields_box.h
/// @brief The group of the Properties panel with the writer's own fields of the book and of
///        its series (own_fields.h), under the information of the book

#ifndef KALAHARI_GUI_PANELS_BOOK_FIELDS_BOX_H
#define KALAHARI_GUI_PANELS_BOOK_FIELDS_BOX_H

#include <QGroupBox>
#include <QList>
#include <QString>

class QEvent;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

struct OwnField;

/// @brief The own fields of the book, their values changed as the title and the author above
///
/// A row for each field, by name: the book's own, then those of its series. A value is kept
/// when the keys leave its field (Enter); the fields in the texts show it at once. Of a field
/// of the series the value is that of all the volumes, but where the volume gives its own: the
/// note under the rows names those fields. Add Field... makes one, Insert in Text... opens
/// Insert > Field... on its tab Own for the text in front; the gear opens the settings of the
/// fields. A change made elsewhere shows here at once, but in a field being written in.
class BookFieldsBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the book @p book
    explicit BookFieldsBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show the fields of the book again, but the value being written
    void refresh();

    /// @brief Whether a text of the book is in front, to insert a field in it
    void setCanInsert(bool can);

    /// @brief The field of the value of the own field @p id; nullptr: no such row
    [[nodiscard]] QLineEdit* valueEdit(const QString& id) const;

    /// @brief The ids of the fields, in the order of the rows
    [[nodiscard]] QStringList fieldIds() const;

    /// @brief The note on the fields of the series (hidden without them)
    [[nodiscard]] QLabel* seriesNote() const { return m_seriesNote; }

    /// @brief The line said when the book has no own fields
    [[nodiscard]] QLabel* emptyLabel() const { return m_empty; }

    [[nodiscard]] QPushButton* addButton() const { return m_add; }
    [[nodiscard]] QPushButton* insertButton() const { return m_insert; }
    [[nodiscard]] QToolButton* settingsButton() const { return m_settings; }

    /// @brief The field whose value had the keys last; empty: none
    [[nodiscard]] QString lastField() const { return m_lastField; }

signals:
    /// @brief Add Field...: the window of a new own field
    void requestNewField();
    /// @brief Insert in Text...: Insert > Field... on its tab Own, @p id chosen (empty: none)
    void requestInsertField(const QString& id);
    /// @brief The gear: Settings > Editor > Writing, the gray background of the fields
    void requestSettings();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The rows anew, for other fields
    void rebuild(const QList<OwnField>& fields);
    /// @brief Keep the value of the field @p id, when it is not what the book has
    void save(const QString& id);
    /// @brief The state of the buttons: a book open read-only changes nothing
    void updateButtons();
    void updateIcons();

    /// @brief A row: the field and its value
    struct Row {
        QString id;
        QString name;
        bool series = false;
        QLineEdit* edit = nullptr;
    };

    core::ProjectManager& m_book;
    QFormLayout* m_rows = nullptr;
    QList<Row> m_fields;
    QLabel* m_empty = nullptr;
    QLabel* m_seriesNote = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_insert = nullptr;
    QToolButton* m_settings = nullptr;
    QString m_lastField;
    bool m_canInsert = false;
    bool m_saving = false;  ///< A value is being kept: the book's change is already here
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_BOOK_FIELDS_BOX_H
