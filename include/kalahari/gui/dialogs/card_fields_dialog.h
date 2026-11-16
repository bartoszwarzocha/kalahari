// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_fields_dialog.h
/// @brief Card Fields: the fields of the cards of characters, locations and items, in the
/// order of the card and where each is shown; the fields the writer adds, and the values of
/// their lists

#ifndef KALAHARI_GUI_DIALOGS_CARD_FIELDS_DIALOG_H
#define KALAHARI_GUI_DIALOGS_CARD_FIELDS_DIALOG_H

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_field_set.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QTableWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

/// @brief Card Fields: the fields of the cards of each kind, in a table
///
/// - The kind of the cards above the table: character, location, item (the kinds of cards of
///   the book, or of the installed book types for the fields of new books).
/// - The table: the name and In Short first, which every card has; then the fields in the
///   order of the card, those of the book type and those of the writer ("· writer's" after
///   the name), each with its kind and where it is shown: on the full card, on the short card
///   of the Properties panel, as a column of the list of cards. A hidden field keeps the
///   values of the cards. Space ticks a box of the current cell.
/// - The buttons: Add Field... (Insert), Remove (Delete, a field of the writer), Move Up, Move
///   Down, List Values... (a field of the writer with a list), Restore Type Fields (the fields
///   of the book type, where it shows them; the cards keep the values). F2 or a double-click
///   changes a field of the writer; the menu of the table has the commands too.
/// - The fields of a book: "Save as defaults for new books"; the fields of new books (the
///   Settings) have no such box.
///
/// The changes are kept in the window until OK (editCardFields(), editDefaultCardFields()); a
/// book open read-only shows the fields and changes nothing.
class CardFieldsDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The fields of the cards of the book open in @p book
    /// @param book The book of its window
    /// @param kind The kind shown first ("kalahari.base:character"); empty: the first one
    /// @param parent Parent widget
    CardFieldsDialog(const core::ProjectManager& book, const QString& kind,
                     QWidget* parent = nullptr);

    /// @brief The fields that new books start with
    /// @param defaults The sets of the Settings (core::card_fields::defaults())
    /// @param parent Parent widget
    explicit CardFieldsDialog(const core::CardFieldSets& defaults, QWidget* parent = nullptr);

    /// @brief The kinds of the cards it shows, in the order of the list of kinds
    [[nodiscard]] QList<core::KindRef> kinds() const;

    /// @brief The fields of kind @p kind ("kalahari.base:character") as the window has them,
    /// each field of the kind (core::card_fields::complete())
    [[nodiscard]] QList<core::BookCardField> fieldsOf(const QString& kind) const;

    /// @brief Whether the fields are to become those of new books too
    [[nodiscard]] bool savesAsDefaults() const;

    /// @brief Show the fields of kind @p kind ("kalahari.base:character")
    void showKind(const QString& kind);

    /// @brief Make the row of field @p fieldId the current one
    void setCurrentField(const QString& fieldId);

    /// @brief The kind of the cards
    [[nodiscard]] QComboBox* kindCombo() const { return m_kindCombo; }
    /// @brief The fields: name, kind of field, on the card, in the Properties, in the list
    [[nodiscard]] QTableWidget* fieldTable() const { return m_table; }
    /// @brief Add Field...
    [[nodiscard]] QPushButton* addButton() const { return m_addButton; }
    /// @brief Remove: the current field of the writer
    [[nodiscard]] QPushButton* removeButton() const { return m_removeButton; }
    /// @brief Move Up
    [[nodiscard]] QPushButton* upButton() const { return m_upButton; }
    /// @brief Move Down
    [[nodiscard]] QPushButton* downButton() const { return m_downButton; }
    /// @brief List Values...: of the current field of the writer with a list
    [[nodiscard]] QPushButton* choicesButton() const { return m_choicesButton; }
    /// @brief Restore Type Fields
    [[nodiscard]] QPushButton* restoreButton() const { return m_restoreButton; }
    /// @brief Save as defaults for new books; nullptr for the fields of new books
    [[nodiscard]] QCheckBox* defaultsCheck() const { return m_defaultsCheck; }

    /// @brief The columns of the table
    enum Column { NAME_COLUMN, TYPE_COLUMN, CARD_COLUMN, PROPERTIES_COLUMN, LIST_COLUMN };

    /// @brief The rows of the name and In Short, before the fields
    static constexpr int FIXED_ROWS = 2;

public slots:
    /// @brief Add Field...: a new field of the writer after the current one
    void addField();
    /// @brief Change the current field of the writer: its name, its kind, the cards it names
    void changeField();
    /// @brief The current field of the writer goes; the cards keep its values
    void removeField();
    /// @brief The current field one row up
    void moveUp();
    /// @brief The current field one row down
    void moveDown();
    /// @brief List Values... of the current field of the writer
    void editChoices();
    /// @brief Restore Type Fields: the fields of the book type, where it shows them
    void restoreTypeFields();

protected:
    /// @brief The keys of the table: Insert, Delete, F2
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The fields of one kind as the window has them
    struct KindFields {
        core::KindRef kind;
        QList<core::BookCardField> fields;  ///< Every field of the kind (complete())
        QStringList taken;  ///< The ids that new fields of the writer do not take
    };

    /// @brief What both windows have: the kind, the table, the buttons
    void createContent(bool forBook, const QString& bookTitle);

    /// @brief The row of the buttons under the table
    void createButtons();

    /// @brief Under the buttons: Save as defaults for new books, beside the title of the book
    /// @p bookTitle, and beside the buttons that close the window, where the own fields of a
    /// card go; for the fields of new books, what they are
    void createNotes(bool forBook, const QString& bookTitle);

    /// @brief Fill the list of the kinds with @p kinds and show @p current
    void fillKinds(const QList<KindFields>& kinds, const QString& current);

    /// @brief The rows of the table: the name, In Short and the fields of the current kind
    void fillTable();

    /// @brief Fill row @p row with field @p field
    void fillRow(int row, const core::BookCardField& field);

    /// @brief The kind of field @p field as the table names it: "text", "list: elf, dwarf"
    [[nodiscard]] QString typeText(const core::BookCardField& field) const;

    /// @brief The name of field @p field in the program's language
    [[nodiscard]] QString nameOf(const core::BookCardField& field) const;

    /// @brief A box of the table was ticked or cleared
    void takeCheck(int row, int column);

    /// @brief The buttons follow the current field and a book open read-only
    void updateButtons();

    /// @brief The heading names the kind shown
    void updateHeading();

    /// @brief The menu of the table at @p pos
    void showMenu(const QPoint& pos);

    /// @brief The fields of the current kind; nullptr when there is none
    [[nodiscard]] KindFields* current();
    [[nodiscard]] const KindFields* current() const;

    /// @brief The index of the current field among the fields; -1 for none (or a row of the
    /// name or In Short)
    [[nodiscard]] int currentIndex() const;

    /// @brief The names of the fields of the current kind, but field @p exceptIndex
    [[nodiscard]] QStringList namesBut(int exceptIndex) const;

    /// @brief Move the current field by @p by rows
    void moveBy(int by);

    QList<KindFields> m_kinds;
    int m_kindIndex = -1;
    bool m_readOnly = false;
    bool m_filling = false;
    QComboBox* m_kindCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
    QPushButton* m_choicesButton = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QCheckBox* m_defaultsCheck = nullptr;
    QMenu* m_menu = nullptr;
};

/// @brief Card Field: a field of the writer of the cards of a kind, new or changed
///
/// Its name, which no other field of the kind has, and its kind: a text, a longer text, a list
/// of values to choose from or a card (a character, a location or an item), which names a
/// card by a relation named after the field; "The other way" names the relation the named
/// card has ("Pupil" for "Mentor"), empty: the same both ways. A changed field keeps its kind,
/// but a text can become a longer text and back.
class CardFieldDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param kind The kind of the cards
    /// @param field The field it changes; a new one has no id
    /// @param names The names of the other fields of the kind, which it does not take
    /// @param parent Parent widget
    CardFieldDialog(const core::KindRef& kind, const core::BookCardField& field,
                    const QStringList& names, QWidget* parent = nullptr);

    /// @brief The field as the writer set it
    [[nodiscard]] core::BookCardField field() const;

    /// @brief The name of the field
    [[nodiscard]] QLineEdit* nameEdit() const { return m_name; }
    /// @brief Its kind: text, longer text, list, card
    [[nodiscard]] QComboBox* typeCombo() const { return m_type; }
    /// @brief A card: of which kind
    [[nodiscard]] QComboBox* cardKindCombo() const { return m_cardKind; }
    /// @brief A card: what the card is to the card it names
    [[nodiscard]] QLineEdit* inverseEdit() const { return m_inverse; }
    /// @brief Why it cannot be saved, or what the kind of field does
    [[nodiscard]] QLabel* noteLabel() const { return m_note; }

private:
    /// @brief The rows of a card show for a card; the note and the button follow the name
    void update();

    core::BookCardField m_field;
    QStringList m_names;
    QLineEdit* m_name = nullptr;
    QComboBox* m_type = nullptr;
    QLabel* m_cardKindLabel = nullptr;
    QComboBox* m_cardKind = nullptr;
    QLabel* m_inverseLabel = nullptr;
    QLineEdit* m_inverse = nullptr;
    QLabel* m_note = nullptr;
};

/// @brief List Values: the values of a list of a field of the writer, in the order of the list
///
/// Add (Insert) adds one with the keys in its name, F2 or a double-click renames one, Remove
/// (Delete) takes it out: a card with that value keeps it as text. A value keeps its id when
/// it is renamed or moved, and a new one does not take the id of one that went.
class CardChoicesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param fieldName The name of the field
    /// @param choices Its values
    /// @param parent Parent widget
    CardChoicesDialog(const QString& fieldName, const QList<core::CardFieldChoice>& choices,
                      QWidget* parent = nullptr);

    /// @brief The values, without those left without a name
    [[nodiscard]] QList<core::CardFieldChoice> choices() const;

    /// @brief The values
    [[nodiscard]] QListWidget* list() const { return m_list; }
    /// @brief Add
    [[nodiscard]] QPushButton* addButton() const { return m_addButton; }
    /// @brief Remove
    [[nodiscard]] QPushButton* removeButton() const { return m_removeButton; }
    /// @brief Move Up
    [[nodiscard]] QPushButton* upButton() const { return m_upButton; }
    /// @brief Move Down
    [[nodiscard]] QPushButton* downButton() const { return m_downButton; }

public slots:
    /// @brief A new value at the end, with the keys in its name
    void addChoice();
    /// @brief The current value goes
    void removeChoice();

protected:
    /// @brief The keys of the list: Insert, Delete
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The current value by @p by rows
    void moveBy(int by);

    /// @brief The buttons follow the current value
    void updateButtons();

    QList<core::CardFieldChoice> m_all;  ///< Every value it had, also those that went
    QListWidget* m_list = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
};

/// @brief Open Card Fields for the book open in @p book at kind @p kind, and save what the
/// writer changed (core::ProjectManager::setCardFields()); with "Save as defaults for new
/// books", the fields of new books too
/// @return Whether anything was saved
bool editCardFields(core::ProjectManager& book, const QString& kind, QWidget* parent);

/// @brief Open Card Fields for the fields that new books start with, @p defaults
/// @return The sets as the writer changed them, with the sets of the kinds it does not show;
///         nullopt when the window was closed
[[nodiscard]] std::optional<core::CardFieldSets>
editDefaultCardFields(const core::CardFieldSets& defaults, QWidget* parent);

/// @brief @p sets as new books start with them: a kind with the fields of its package has no
/// set, a field of the writer that names a card no relation of a book; the sets of
/// @p defaults of the kinds of @p kinds that it does not know (a newer package) are kept
[[nodiscard]] core::CardFieldSets defaultsOf(const QList<core::KindRef>& kinds,
                                             const core::CardFieldSets& sets,
                                             const core::CardFieldSets& defaults);

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_CARD_FIELDS_DIALOG_H
