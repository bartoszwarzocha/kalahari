/// @file new_element_dialog.h
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, or an item of
/// the front or back section

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/section_words.h"

#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QWidget;

namespace kalahari {
namespace gui {

class ElementPlacePicker;

namespace dialogs {

/// @brief What a new element of the book is, which decides where it goes
enum class NewElementKind {
    Chapter,          ///< A chapter, at the end of a part or of the body of the book
    Part,             ///< A part, at the end of the book's body
    FrontMatterItem,  ///< An item at the end of the front section (title page, dedication...)
    BackMatterItem,   ///< An item at the end of the back section (afterword, glossary...)
};

/// @brief A kind the new element can have, with the title it starts with and its place
struct NewElementChoice {
    core::KindRef kind;          ///< Kind of the element
    QString title;               ///< Title of an element of the kind, e.g. "Chapter 3"
    core::ElementPlace place{};  ///< Place an element of the kind takes when the writer does
                                 ///< not choose it (ProjectManager::newPlaceOf())
};

/// @brief Dialog for a new element of the book in the Navigator
///
/// Asks for the title of the new element and, when it can have more than one kind (a
/// prologue or a chapter, a dedication or a preface), for its kind. Kinds are named in the
/// language of the program. The title starts as the title of the chosen kind, selected,
/// so typing replaces it; it follows the kind until the writer changes it. The dialog
/// cannot be accepted with an empty title.
///
/// With the part of the book it goes to (setSection()), the dialog says where the element
/// goes, and the writer chooses the place in a list of the part when there is a choice to
/// make:
/// - a prologue goes at the start of the part, first in its first group or anywhere else;
///   an epilogue at the end of the part, last in its last group or anywhere else;
/// - a chapter of the body goes before the epilogue at the end of the last part, or at the
///   end of the body;
/// - an element whose place makes another one stop opening or closing the part (a chapter in
///   a part after the epilogue) shows that place and the warning.
///
/// A new part shows the body as it will be, and takes inside it the elements that close the
/// body at the end of the parts before it (the epilogue), unless the writer leaves them
/// where they are.
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

    /// @brief The part of the book the element goes to, so that the dialog shows where it goes
    /// and lets the writer choose
    /// @param elements Elements of the part, with the elements inside its groups
    /// @param name Name of the part in the Navigator, in the language of the book: "Sekcja
    ///        główna"
    /// @param words How the dialog's sentences name the part
    /// @param registry Kinds of the elements: their icons, positions and the groups a kind
    ///        can be in; it must outlive the dialog
    /// @param groupId Group the dialog was opened on; empty: the part
    void setSection(const QList<core::ProjectElement>& elements, const QString& name,
                    const SectionWords& words, const core::BookTypeRegistry& registry,
                    const QString& groupId = QString());

    /// @brief The kind of the new element (one of the choices)
    core::KindRef kind() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

    /// @brief The place of the new element: the one the writer chose, or the place of its kind
    core::ElementPlace place() const;

    /// @brief Ids of the elements a new part takes inside it: those that close the body at
    /// the end of the parts before it, unless the writer leaves them there; empty for other
    /// elements
    QStringList takeInside() const;

private:
    /// @brief A new kind: the title follows it unless the writer has changed it, and the places
    /// are those of the kind
    void onKindChanged();

    /// @brief The dialog can be accepted only with a title
    void updateAcceptButton();

    /// @brief The places of the chosen kind, the options of the list and the heading
    void updatePlaces();

    /// @brief A new part: the elements it can take inside it, and the body as it will be
    void updatePart();

    /// @brief The sentence of the heading: where an element of the chosen kind goes
    void updateDescription();

    /// @brief The words of an option for @p place of an element with no position: "At the end
    /// of the main section, after "Part One"", "Before "Epilogue", at the end of "Part One""
    QString placeText(const core::ElementPlace& place) const;

    /// @brief Group @p id of the part, at any depth; nullptr when it has none
    const core::ProjectElement* groupOf(const QString& id) const;

    /// @brief The place of the chosen kind when the writer does not choose it
    core::ElementPlace defaultPlace() const;

    /// @brief Show what the dialog notes about the place: @p text with the icon @p iconId;
    /// an empty text hides the note
    void showNotice(const QString& iconId, const QString& text);

    NewElementKind m_dialogKind;
    QString m_groupTitle;
    QList<NewElementChoice> m_choices;
    int m_current = 0;                ///< Index of the chosen kind
    QComboBox* m_kindBox = nullptr;   ///< Kind of the element; none with a single choice
    QLineEdit* m_titleEdit = nullptr;
    bool m_titleChanged = false;      ///< The writer changed the title: it stays as it is

    // The part of the book the element goes to
    core::BookPlace m_part = core::BookPlace::Main;
    QList<core::ProjectElement> m_elements;              ///< Its elements
    QString m_sectionName;                               ///< Its name in the Navigator
    SectionWords m_words;                                ///< Its name in sentences
    const core::BookTypeRegistry* m_registry = nullptr;
    QString m_openedOn;                                  ///< Group the dialog was opened on

    // The place
    ElementPlacePicker* m_picker = nullptr;
    int m_pickerStretch = -1;          ///< Index of the stretch under the content
    bool m_choosing = false;           ///< The writer chooses the place in the list
    bool m_beforeClosing = false;      ///< A chapter goes before the epilogue in a group

    // A new part: the elements that close the body, which it takes inside it
    QList<core::ProjectElement> m_closing;  ///< At the end of the groups before the part
    bool m_canTake = false;                 ///< The part can have all of them inside it
    QCheckBox* m_takeBox = nullptr;
    QLabel* m_previewNote = nullptr;

    // The note about the place: information or a warning
    QWidget* m_noticeBox = nullptr;
    QLabel* m_noticeIcon = nullptr;
    QLabel* m_noticeText = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
