/// @file project_properties_dialog.h
/// @brief Project Properties: the type of the open book and the kinds of elements it can have

#pragma once

#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QHash>
#include <QList>
#include <QString>

class QLabel;
class QPushButton;

namespace kalahari {
namespace gui {

class KindPalette;

namespace dialogs {

/// @brief "Project Properties": the type of the open book, which cannot change, and the kinds
/// of elements of all book types that can be added to it
///
/// +----------------------------------------------------------------------------+
/// | [icon] Project Properties "Desert"                                          |
/// |        The book type and the kinds of elements that can be added to it.    |
/// +----------------------------------------------------------------------------+
/// | [icon] Book type: Novel                                                     |
/// |        The type cannot be changed. ...                                      |
/// | Kinds of the type: 14 · from other types: 1    Uncheck Kinds of Other Types |
/// | +------------------------------------------------------------------------+ |
/// | | Front Section 8                                                         | |
/// | | [tiles]                                                                 | |
/// | +------------------------------------------------------------------------+ |
/// | (!) Unchecked kinds have elements in the book: ...                         |
/// | (i) Tools of the Types                                                      |
/// | [Save as Type...]                                            [OK] [Cancel] |
/// +----------------------------------------------------------------------------+
///
/// The palette (KindPalette) has no "Create now": its tiles say how many elements of their
/// kinds the book has. In a book of a type the kinds of the type are checked for good, and the
/// writer checks the kinds of other types to add them to the book
/// (core::ProjectManager::setProjectKinds()); in a user project every kind can be checked and
/// unchecked, one at least. Unchecking a kind deletes nothing: its elements stay and work, but
/// no new ones can be added, which the note under the palette says. A book open read-only has
/// the palette greyed, and a sentence says why. Save as Type... saves the type of the book and
/// the kinds checked now as a type of the writer's own (SaveTypeDialog).
class ProjectPropertiesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window of the book open in core::ProjectManager
    /// @param parent Parent widget
    explicit ProjectPropertiesDialog(QWidget* parent = nullptr);

    /// @brief The palette of the kinds
    KindPalette* palette() const { return m_palette; }

    /// @brief The button that saves the kinds as a type of the writer's own
    QPushButton* saveTypeButton() const { return m_saveTypeButton; }

    /// @brief The type of the book, over the palette
    QLabel* typeLabel() const { return m_typeLabel; }

    /// @brief The number of the checked kinds, over the palette
    QLabel* countLabel() const { return m_countLabel; }

    /// @brief What happens to the elements of the unchecked kinds; hidden while there are none
    QLabel* noteLabel() const { return m_noteText; }

    /// @brief The kinds checked in the palette, in its order, the kinds of the type too
    QList<core::KindRef> chosenKinds() const;

    /// @brief Whether OK gave the book new kinds
    /// @note Only valid after the window is accepted
    bool changedKinds() const { return m_changedKinds; }

    /// @brief Save as Type... saves to @p folder instead of the folder of the writer's own
    /// types, for tests
    void setTypesFolder(const QString& folder);

public slots:
    /// @brief OK: the book gets the checked kinds, and the window closes; kinds that cannot be
    /// saved keep it open
    void accept() override;

private:
    /// @brief The number of the checked kinds, the note of the unchecked kinds with
    /// elements, and OK, which a user project without a kind cannot press
    void updateState();

    /// @brief Save as Type...: the type of the book and the kinds checked now
    void saveAsType();

    const core::BookTypeRegistry& m_registry;
    KindPalette* m_palette = nullptr;
    QLabel* m_typeLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QWidget* m_note = nullptr;
    QLabel* m_noteText = nullptr;
    QPushButton* m_saveTypeButton = nullptr;
    QString m_typesFolder;
    QString m_typeId;                                ///< The book's type; empty: none
    int m_typeKinds = 0;                             ///< How many kinds the type has
    QHash<const core::ElementKind*, int> m_counts;   ///< Elements of each kind in the book
    bool m_readOnly = false;
    bool m_changedKinds = false;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
