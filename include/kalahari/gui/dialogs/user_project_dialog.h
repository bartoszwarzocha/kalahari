/// @file user_project_dialog.h
/// @brief The second step of a new user project: the kinds of elements of the book

#pragma once

#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"

#include <QString>

#include <optional>

class QLabel;
class QPushButton;

namespace kalahari {
namespace gui {

class KindPalette;

namespace dialogs {

/// @brief "New Book – User Project": the kinds of elements of a new book without a type
///
/// +----------------------------------------------------------------------------+
/// | [icon] Kinds of Elements of the Book "Desert"                               |
/// |        Check the kinds the book is to have. ...                             |
/// +----------------------------------------------------------------------------+
/// | Checked kinds: 20               Check All   Only "Base"   Uncheck All       |
/// | +------------------------------------------------------------------------+ |
/// | | Front Section 8                                                         | |
/// | | [tiles]                                                                 | |
/// | +------------------------------------------------------------------------+ |
/// | (i) Tools of the Types                                                      |
/// |     No type has tools yet. ...                                              |
/// | [< Back] [Save as Type...]               [Skip] [Create Book] [Cancel]     |
/// +----------------------------------------------------------------------------+
///
/// The palette (KindPalette) shows the kinds of all book types, under the names of the
/// sections of the book. At first the kinds of the base package are checked, with a chapter
/// created with the book. Create Book makes the book with the checked kinds and the elements
/// of "Create now"; Skip makes it as the palette was at first, whatever the writer changed;
/// Back returns to the New Book window, which keeps what the writer typed; Cancel creates
/// nothing. When a book is open, Create Book and Skip first ask whether the new book takes
/// its place, and Cancel of the question returns to this window. Save as Type... saves the
/// checked kinds and the elements of "Create now" as a type of the writer's own
/// (SaveTypeDialog), which the New Book window offers after Back.
class UserProjectDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The result of exec() after Back
    static constexpr int BACK = 2;

    /// @brief The window with the kinds of the packages of @p registry
    /// @param registry Packages of the kinds; they must stay loaded while the window is there
    /// @param parent Parent widget
    explicit UserProjectDialog(const core::BookTypeRegistry& registry, QWidget* parent = nullptr);

    /// @brief Show the book the kinds are for: its title in the heading, and the names of its
    /// sections, in the language of the book, over the groups of the palette
    /// @param title Title of the book
    /// @param language Language code of the book (e.g., "pl")
    /// @param sections Whether the book shows its sections, and their names; none: no
    ///        sections
    void setBook(const QString& title, const QString& language,
                 const std::optional<core::BookSections>& sections);

    /// @brief Ask @p question about the open book before the book is created; without a
    /// question the new book opens
    void setOpeningQuestion(NewBookQuestion question);

    /// @brief The kinds of the book and its elements: the palette's after Create Book, the
    /// first ones after Skip
    /// @note Only valid after the window is accepted
    core::PaletteChoice choice() const { return m_choice; }

    /// @brief Whether the new book opens: false when the writer chose to create it without
    /// opening, and the open book stays open
    /// @note Only valid after the window is accepted
    bool opens() const { return m_opens; }

    /// @brief The choice the palette starts with, which Skip makes: the kinds of the base
    /// package, with an element of its main text kind in the main section
    core::PaletteChoice firstChoice() const { return m_firstChoice; }

    /// @brief The palette
    KindPalette* palette() const { return m_palette; }

    /// @brief The button that creates the book as the palette was at first
    QPushButton* skipButton() const { return m_skipButton; }

    /// @brief The button that returns to the New Book window
    QPushButton* backButton() const { return m_backButton; }

    /// @brief The button that saves the checked kinds as a type of the writer's own
    QPushButton* saveTypeButton() const { return m_saveTypeButton; }

    /// @brief Save as Type... saves to @p folder instead of the folder of the writer's own
    /// types, for tests
    void setTypesFolder(const QString& folder);

public slots:
    /// @brief Create Book or Skip: asks about the open book, then closes the window; Cancel
    /// of the question keeps it open
    void accept() override;

private:
    /// @brief Show the number of the checked kinds; Create Book needs at least one
    void updateCount();

    const core::BookTypeRegistry& m_registry;
    KindPalette* m_palette = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_skipButton = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_saveTypeButton = nullptr;
    QString m_typesFolder;              ///< Where Save as Type... saves
    NewBookQuestion m_openingQuestion;
    QString m_title;                    ///< Title of the book, for the question
    core::PaletteChoice m_firstChoice;  ///< What the palette starts with
    core::PaletteChoice m_choice;       ///< What the accepted window creates
    bool m_skipped = false;             ///< The button that accepts the window is Skip
    bool m_opens = true;                ///< The accepted window's book opens
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
