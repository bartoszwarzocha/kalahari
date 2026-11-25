// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file new_volume_dialog.h
/// @brief Book > New Volume...: a new book of a series, with the same Workshop

#ifndef KALAHARI_GUI_DIALOGS_NEW_VOLUME_DIALOG_H
#define KALAHARI_GUI_DIALOGS_NEW_VOLUME_DIALOG_H

#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;

namespace kalahari::gui::dialogs {

/// @brief Book > New Volume...: a new book of the series of the open project
///
/// The volume gets its title, its author (at first that of the current volume), the language
/// of its text and its place among the volumes (at first after the current one), and starts
/// either with the empty sections of a new book (its first chapter) or with the pages of the
/// front and back sections of the volume before it (core::NewBook::pagesOfPrevious). In a
/// project with one book the window also asks for the name of the series, at first the title
/// of the book, which becomes volume 1 (volume 2 with the new volume before it). The window can
/// be accepted only with a title.
class NewVolumeDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window for the project open in @p book
    explicit NewVolumeDialog(const core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief The new volume, as core::ProjectManager::addBook() takes it
    [[nodiscard]] core::NewBook request() const;

    /// @brief The name of the series typed in the window, without the spaces at its ends;
    /// empty in a series, whose window does not ask for it
    [[nodiscard]] QString seriesName() const;

    /// @brief The field of the name of the series; nullptr in a series
    [[nodiscard]] QLineEdit* seriesEdit() const { return m_seriesEdit; }

    /// @brief The field of the title of the volume
    [[nodiscard]] QLineEdit* titleEdit() const { return m_titleEdit; }

    /// @brief The field of the author of the volume
    [[nodiscard]] QLineEdit* authorEdit() const { return m_authorEdit; }

    /// @brief The list of the languages of the text
    [[nodiscard]] QComboBox* languageBox() const { return m_languageBox; }

    /// @brief The list of the places among the volumes
    [[nodiscard]] QComboBox* placeBox() const { return m_placeBox; }

    /// @brief The start with the empty sections of a new book
    [[nodiscard]] QRadioButton* emptyButton() const { return m_emptyButton; }

    /// @brief The start with the pages of the volume before it
    [[nodiscard]] QRadioButton* pagesButton() const { return m_pagesButton; }

private:
    /// @brief The group of the series, in a project with one book
    QWidget* seriesGroup(const QString& title);

    /// @brief The group of the volume: its title, author, language and place
    QWidget* volumeGroup(const core::ProjectManager& book);

    /// @brief The group of the start of the volume
    QWidget* startGroup();

    /// @brief The note of the series says the number the book gets, and the start with pages
    /// names the volume before the place chosen
    void updatePlace();

    /// @brief The window can be accepted only with a title
    void updateAcceptButton();

    QStringList m_titles;  ///< The titles of the volumes, in their order
    QLineEdit* m_seriesEdit = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QComboBox* m_languageBox = nullptr;
    QComboBox* m_placeBox = nullptr;
    QRadioButton* m_emptyButton = nullptr;
    QRadioButton* m_pagesButton = nullptr;
    QLabel* m_seriesNote = nullptr;  ///< What becomes of the book; nullptr in a series
    QLabel* m_pagesNote = nullptr;   ///< What the start with pages gives
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_NEW_VOLUME_DIALOG_H
