/// @file new_element_dialog.h
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QString>

#include <optional>

class QComboBox;
class QEvent;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

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
    QString before{};    ///< Title of the element it goes before, when it does not go last
                         ///< (a chapter goes before the epilogue); empty: it goes last
};

/// @brief A place in the body of the book: a list and a place in it
struct NewElementPlace {
    QString groupId;      ///< Group whose list it is; empty: the body itself
    qsizetype index = 0;  ///< Place in the list

    bool operator==(const NewElementPlace&) const = default;
};

/// @brief Dialog for a new element of the book in the Navigator
///
/// Asks for the title of the new element and, when it can have more than one kind (a
/// prologue or a chapter, a dedication or a preface), for its kind. Kinds are named in the
/// language of the program. The title starts as the title of the chosen kind, selected,
/// so typing replaces it; it follows the kind until the writer changes it. The dialog
/// cannot be accepted with an empty title.
///
/// With the body of the book (setBody()), the writer also chooses where an element of a kind
/// with a position goes: a prologue at the start of the body, first in its first part or
/// anywhere else; an epilogue at the end of the body, last in its last part or anywhere else.
/// A list of the body's elements shows the new element in its place, and the writer moves it
/// with the buttons next to the list or by clicking the element it is to go before.
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

    /// @brief Let the writer choose the place of an element of a kind with a position
    /// @param elements Elements of the body of the book, with the elements inside its groups
    /// @param registry Kinds of the elements: their icons and the groups a kind can be in;
    ///        it must outlive the dialog
    /// @param groupId Group the dialog was opened on, whose start (or end) it offers first;
    ///        empty: the body
    void setBody(const QList<core::ProjectElement>& elements,
                 const core::BookTypeRegistry& registry, const QString& groupId = QString());

    /// @brief The kind of the new element (one of the choices)
    core::KindRef kind() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

    /// @brief The place the writer chose for the element; nullopt when the dialog offers no
    /// places for its kind, so it goes where its kind goes
    std::optional<NewElementPlace> place() const;

protected:
    /// @brief Up and Down in the list of the body move the new element
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A new kind: the title follows it unless the writer has changed it, and the places
    /// are those of the kind
    void onKindChanged();

    /// @brief The dialog can be accepted only with a title
    void updateAcceptButton();

    /// @brief The sentence of the heading: where an element of the chosen kind goes
    void updateDescription();

    /// @brief The places of the chosen kind, or none when it has no position
    void updatePlaces();

    /// @brief Show the new element in the list at its place, and the place's option
    void showPlace();

    /// @brief The new element goes to place @p index of m_places
    void moveTo(qsizetype index);

    /// @brief The writer clicked an element of the list: the new element goes before it
    void onElementClicked(QTreeWidgetItem* item);

    /// @brief Whether an element of the chosen kind can be inside @p element
    bool canBeInside(const core::ProjectElement& element) const;

    NewElementKind m_dialogKind;
    QString m_groupTitle;
    QList<NewElementChoice> m_choices;
    int m_current = 0;                ///< Index of the chosen kind
    QComboBox* m_kindBox = nullptr;   ///< Kind of the element; none with a single choice
    QLineEdit* m_titleEdit = nullptr;
    bool m_titleChanged = false;      ///< The writer changed the title: it stays as it is

    // The place of an element of a kind with a position
    QList<core::ProjectElement> m_body;                 ///< Elements of the body of the book
    const core::BookTypeRegistry* m_registry = nullptr;
    QString m_openedOn;                                 ///< Group the dialog was opened on
    QList<NewElementPlace> m_places;                    ///< Places of the kind, in reading order
    qsizetype m_place = -1;                             ///< Index of the chosen place
    qsizetype m_firstOption = -1;                       ///< Place of the first option
    qsizetype m_groupOption = -1;                       ///< Place of the group option; -1: none
    QWidget* m_placeBox = nullptr;
    QRadioButton* m_firstButton = nullptr;     ///< Start (or end) of the body
    QRadioButton* m_groupButton = nullptr;     ///< First in the first group (last in the last)
    QRadioButton* m_elsewhereButton = nullptr; ///< The place shown in the list
    QTreeWidget* m_placeList = nullptr;
    QTreeWidgetItem* m_newItem = nullptr;      ///< The new element in the list
    int m_placeStretch = -1;                   ///< Index of the stretch under the content
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
