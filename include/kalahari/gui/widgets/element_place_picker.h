/// @file element_place_picker.h
/// @brief The place of an element in the book: options, and the list of the book with the
/// element in its place

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace kalahari {
namespace gui {

/// @brief A part of the book (front, main or back) in the list of ElementPlacePicker
struct PickerSection {
    core::BookPlace place = core::BookPlace::Main;  ///< Which part of the book it is
    QString name;    ///< Name of its row, as in the Navigator; empty: no row, its elements are
                     ///< at the top of the list
    QString inPart;  ///< The part in the picker's sentences, with its preposition: "in the main
                     ///< section" (SectionWords::inPart)
    QList<core::ProjectElement> elements;  ///< Its elements, with the elements inside its groups
};

/// @brief A place that ElementPlacePicker offers as an option above its list
struct PlaceOption {
    QString text;              ///< Words of the option: "At the end of the main section"
    core::ElementPlace place;  ///< The place
};

/// @brief The place of an element in the book
///
/// The list shows the parts of the book with their elements, also those inside groups, and
/// the element in its place, in bold. The writer moves it with the buttons next to the list,
/// with Up and Down in the list, or by clicking the element it is to go before; clicking the
/// row of a part of the book puts it first there. The places are those the kind of the
/// element can have, in reading order: in a part of the book that allows the kind, and in
/// its groups that can have it.
///
/// Above the list the picker can offer places as options, with "Elsewhere" for any other
/// place. Under it the picker says which elements stop opening or closing their part of the
/// book at the chosen place (a chapter after the epilogue), and whether the element itself
/// is not where its kind goes (a prologue after a chapter).
///
/// A preview only shows the book with the element in its place: no options, no buttons, no
/// clicks, and the elements the element takes inside it marked.
class ElementPlacePicker : public QWidget {
    Q_OBJECT

public:
    /// @brief Create an empty picker; setBook() and setElement() fill it
    explicit ElementPlacePicker(QWidget* parent = nullptr);

    /// @brief The parts of the book the list shows
    /// @param sections The parts, in reading order
    /// @param registry Kinds of the elements: their icons, forms and positions, and the groups
    ///        a kind can be in; it must outlive the picker
    void setBook(const QList<PickerSection>& sections, const core::BookTypeRegistry& registry);

    /// @brief The element to place: a new element of @p kind with @p title, with @p inside
    /// inside it (a new part that takes the epilogue)
    ///
    /// The places are those of the kind; the element is at the first of them until setPlace().
    void setElement(const core::KindRef& kind, const QString& title,
                    const QList<core::ProjectElement>& inside = {});

    /// @brief The title of the element in the list
    void setTitle(const QString& title);

    /// @brief The words above the list; by default "Place"
    void setLabel(const QString& text);

    /// @brief Places offered as options above the list, then "Elsewhere"; none: no options
    void setOptions(const QList<PlaceOption>& options);

    /// @brief Put the element at @p place, if its kind can be there
    void setPlace(const core::ElementPlace& place);

    /// @brief Whether the element can be at @p place
    bool canBeAt(const core::ElementPlace& place) const;

    /// @brief The place of the element; the default ElementPlace when it can be nowhere
    core::ElementPlace place() const;

    /// @brief The places the element can have, in reading order
    const QList<core::ElementPlace>& places() const { return m_places; }

    /// @brief What the chosen place changes, one sentence each: the elements that stop
    /// opening or closing their part of the book, and the element itself when it is not
    /// where its kind goes
    QStringList warnings() const;

    /// @brief Show only the book with the element in its place
    void setPreview(bool preview);

    /// @brief Titles in quotes, joined as in a sentence: "A", "B" and "C"
    static QString quoted(const QStringList& titles);

signals:
    /// @brief The writer or the program put the element at another place
    void placeChanged();

protected:
    /// @brief Up and Down in the list move the element
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The places of the kind in @p elements of group @p groupId in @p part of the
    /// book, in reading order: those in the list itself when @p here, and those in its groups
    void addPlacesIn(core::BookPlace part, const QList<core::ProjectElement>& elements,
                     const QString& groupId, bool here);

    /// @brief Whether the element can be inside @p element, a group
    bool canBeInside(const core::ProjectElement& element) const;

    /// @brief Show the book with the element at its place, the place's option, the buttons
    /// and the warnings
    void showPlace();

    /// @brief Rows of @p elements of group @p groupId in @p part of the book, and the row of
    /// the element at its place
    void addItems(QTreeWidgetItem* parent, core::BookPlace part,
                  const QList<core::ProjectElement>& elements, const QString& groupId);

    /// @brief The row of the element, with the rows of the elements inside it
    void addElementItem(QTreeWidgetItem* parent);

    /// @brief The element goes to place @p index of places()
    void moveTo(qsizetype index);

    /// @brief The writer clicked a row: the element goes before it
    void onItemClicked(QTreeWidgetItem* item);

    QList<PickerSection> m_sections;
    const core::BookTypeRegistry* m_registry = nullptr;
    core::KindRef m_kind;
    QString m_title;
    QList<core::ProjectElement> m_inside;  ///< Elements inside the element
    QList<core::ElementPlace> m_places;    ///< Places of the kind, in reading order
    qsizetype m_place = -1;                ///< Index of the chosen place
    QList<PlaceOption> m_options;
    bool m_preview = false;

    QLabel* m_label = nullptr;
    QVBoxLayout* m_optionLayout = nullptr;
    QList<QRadioButton*> m_optionButtons;
    QRadioButton* m_elsewhereButton = nullptr;
    QTreeWidget* m_list = nullptr;
    QTreeWidgetItem* m_elementItem = nullptr;  ///< The row of the element
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
    QLabel* m_hint = nullptr;
    QWidget* m_warningBox = nullptr;
    QLabel* m_warningIcon = nullptr;
    QLabel* m_warningText = nullptr;
};

} // namespace gui
} // namespace kalahari
