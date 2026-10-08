/// @file new_element_dialog.h
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>
#include <QStringList>

class QComboBox;
class QLineEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief What a new element of the book is, which decides where it goes
enum class NewElementKind {
    Chapter,          ///< A chapter, at the end of a part
    Part,             ///< A part, at the end of the book's body
    FrontMatterItem,  ///< An item at the end of the front matter (title page, dedication...)
    BackMatterItem,   ///< An item at the end of the back matter (epilogue, glossary...)
};

/// @brief Dialog for a new element of the book in the Navigator
///
/// Asks for the title of a new chapter or part, and for a front or back matter item also
/// for its type. The title starts as a name for the element ("New Chapter", or the
/// item's type), selected, so typing replaces it; for an item it follows the type until
/// the writer changes it. The dialog cannot be accepted with an empty title.
class NewElementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog for a new element
    /// @param kind What the element is
    /// @param partTitle For a chapter, the title of its part (named in the heading)
    /// @param parent Parent widget
    explicit NewElementDialog(NewElementKind kind, const QString& partTitle = QString(),
                              QWidget* parent = nullptr);

    /// @brief The types an element can have, in the order the dialog offers them
    /// @return "chapter" for a chapter, "part" for a part, the front or back matter types
    ///         (core::TYPE_TITLE_PAGE...) for an item
    static QStringList elementTypes(NewElementKind kind);

    /// @brief The type of the new element (one of elementTypes())
    QString elementType() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

private:
    /// @brief Name of an item type as the dialog shows it
    static QString typeName(const QString& type);

    /// @brief A new type: the title follows it unless the writer has changed it
    void onTypeChanged();

    /// @brief The dialog can be accepted only with a title
    void updateAcceptButton();

    NewElementKind m_kind;
    QComboBox* m_typeBox = nullptr;   ///< Type of an item; none for a chapter or part
    QLineEdit* m_titleEdit = nullptr;
    bool m_titleChanged = false;      ///< The writer changed the title: it stays as it is
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
