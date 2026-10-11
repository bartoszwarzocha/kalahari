/// @file add_to_book_dialog.h
/// @brief Add to Book: a file as a new text element, of the kind, with the title and at the
/// place the writer chooses

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Add to Book: a file as a new text element of the book
///
/// A file of "Unassigned Files" (the file of an element deleted from the book) goes back to
/// the book and stays where it is; a file from outside the book is copied or moved to the
/// book's folder, as the writer chooses. The writer chooses the kind of the element (the text
/// kinds the book has a place for, in the program's language), its title (the title in the
/// file, or the file's name) and its place in the list of the book; the element takes the
/// status and the notes of the file.
///
/// The dialog cannot be accepted without a title or a place.
class AddToBookDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The file to add
    struct File {
        QString path;         ///< Absolute path of the file
        QString title;        ///< Title of the text in the file; empty: none (a text file)
        QString status;       ///< Status in the file ("draft"...); empty: none
        bool inBook = false;  ///< In the book's folder: it stays there; else the writer chooses
                              ///< whether the book gets a copy of it or the file itself
    };

    /// @brief The kinds the element can have and the parts of the book it can go to
    struct Book {
        QList<NewElementChoice> choices;  ///< Text kinds, each with its place when the writer
                                          ///< does not choose another; not empty
        int current = 0;                  ///< Index of the kind chosen at the start
        QList<PickerSection> sections;    ///< The parts of the book, for the list of places
    };

    /// @brief The kinds and the parts of the book open in core::ProjectManager, for file
    /// @p path: the kind chosen at the start is the one its name says ("chapter_004.kchapter":
    /// a chapter), else the main text kind of the book
    static Book openBook(const QString& path);

    /// @brief Create the dialog
    /// @param file The file
    /// @param book The kinds the element can have and the parts of the book
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    AddToBookDialog(const File& file, const Book& book, const core::BookTypeRegistry& registry,
                    QWidget* parent = nullptr);

    /// @brief The kind of the new element (one of the choices)
    core::KindRef kind() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

    /// @brief The place of the new element
    core::ElementPlace place() const;

    /// @brief For a file from outside the book: whether the book gets a copy of it (true) or
    /// the file itself (false); true for a file of the book
    bool copy() const;

    /// @brief The kinds the element can have
    QComboBox* kindBox() const { return m_kindBox; }

    /// @brief The title of the element
    QLineEdit* titleEdit() const { return m_titleEdit; }

    /// @brief The place of the element in the book
    ElementPlacePicker* picker() const { return m_picker; }

    /// @brief "Copy to the book"; nullptr for a file of the book
    QRadioButton* copyButton() const { return m_copyButton; }

    /// @brief "Move to the book"; nullptr for a file of the book
    QRadioButton* moveButton() const { return m_moveButton; }

    /// @brief The status the element takes
    QLabel* statusLabel() const { return m_statusLabel; }

    /// @brief The name of status @p status in the program's language, as the Navigator names
    /// it: "Draft"; an unknown status as it is, capitalized
    static QString statusName(const QString& status);

private:
    /// @brief A new kind: the places are those of the kind, the element at its place
    void onKindChanged();

    /// @brief The dialog can be accepted only with a title and a place
    void updateAcceptButton();

    File m_file;
    Book m_book;
    int m_current = 0;
    QComboBox* m_kindBox = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QRadioButton* m_copyButton = nullptr;
    QRadioButton* m_moveButton = nullptr;
    ElementPlacePicker* m_picker = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
