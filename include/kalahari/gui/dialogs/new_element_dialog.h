/// @file new_element_dialog.h
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#pragma once

#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QString>

class QComboBox;
class QLineEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief What a new element of the book is, which decides where it goes
enum class NewElementKind {
    Chapter,          ///< A chapter, at the end of a part or of the body of the book
    Part,             ///< A part, at the end of the book's body
    FrontMatterItem,  ///< An item at the end of the front matter (title page, dedication...)
    BackMatterItem,   ///< An item at the end of the back matter (afterword, glossary...)
};

/// @brief A kind the new element can have, with the title it starts with
struct NewElementChoice {
    core::KindRef kind;  ///< Kind of the element
    QString title;       ///< Title of an element of the kind, e.g. "Chapter 3"
};

/// @brief Dialog for a new element of the book in the Navigator
///
/// Asks for the title of the new element and, when it can have more than one kind (a
/// prologue or a chapter, a dedication or a preface), for its kind. Kinds are named in the
/// language of the program. The title starts as the title of the chosen kind, selected,
/// so typing replaces it; it follows the kind until the writer changes it. The dialog
/// cannot be accepted with an empty title.
class NewElementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog for a new element
    /// @param kind What the element is
    /// @param choices Kinds the element can have, in the order the dialog offers them; not
    ///        empty
    /// @param current Index of the kind chosen at the start
    /// @param groupTitle For a chapter in a part, the title of the part (named in the heading);
    ///        empty: the chapter goes to the body of the book
    /// @param parent Parent widget
    NewElementDialog(NewElementKind kind, const QList<NewElementChoice>& choices,
                     int current = 0, const QString& groupTitle = QString(),
                     QWidget* parent = nullptr);

    /// @brief The kind of the new element (one of the choices)
    core::KindRef kind() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

private:
    /// @brief A new kind: the title follows it unless the writer has changed it
    void onKindChanged();

    /// @brief The dialog can be accepted only with a title
    void updateAcceptButton();

    QList<NewElementChoice> m_choices;
    int m_current = 0;                ///< Index of the chosen kind
    QComboBox* m_kindBox = nullptr;   ///< Kind of the element; none with a single choice
    QLineEdit* m_titleEdit = nullptr;
    bool m_titleChanged = false;      ///< The writer changed the title: it stays as it is
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
