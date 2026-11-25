// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file take_out_volume_dialog.h
/// @brief Take Out as a Book... of a volume: the volume copied out of the series as a book of
///        its own, in one window from the choice of its place to the result

#ifndef KALAHARI_GUI_DIALOGS_TAKE_OUT_VOLUME_DIALOG_H
#define KALAHARI_GUI_DIALOGS_TAKE_OUT_VOLUME_DIALOG_H

#include "kalahari/gui/dialogs/archive_dialogs.h"

#include <QString>
#include <QStringList>

#include <functional>

class QCheckBox;
class QLineEdit;

namespace kalahari::gui::dialogs {

/// @brief Take Out as a Book... in the menu of a volume of a series
///
/// The new book is made in a folder named after the title of the volume, in the folder the
/// writer chooses (at first the one that holds the folder of the series), by
/// core::ProjectManager::takeOutBook(): the series stays as it was. The volume goes from the
/// series only when the writer checks it (as Delete Volume..., its files to the Trash); the new
/// book opens in a new window when the window closes, unless the writer unchecks it. As the
/// windows of the archives, the window shows the progress of the work, its result with Show in
/// Folder, or what went wrong, with Copy.
class TakeOutVolumeDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief What the window says about the volume
    struct Volume {
        int number = 0;          ///< Its number in the series, from 1
        QString title;           ///< Its title, which names the folder of the new book
        bool canDelete = false;  ///< The series can lose it: the book is not read-only
    };

    /// @brief What the work did
    struct Outcome {
        QString manifest;            ///< The project file of the new book; empty: none written
        QStringList problems;        ///< How the work failed, as the log has it
        bool volumeDeleted = false;  ///< The series no longer has the volume
    };

    /// @brief Writes the new book in a folder named after the title of the volume in folder
    /// @p folder and, when @p deleteVolume is true, deletes the volume from the series
    using Taker = std::function<Outcome(const QString& folder, bool deleteVolume)>;

    /// @brief Create the window
    /// @param volume What the window says about the volume
    /// @param folder The folder in which the folder of the book is made, at the start
    /// @param taker Writes the book
    /// @param parent Parent widget
    TakeOutVolumeDialog(Volume volume, const QString& folder, Taker taker,
                        QWidget* parent = nullptr);

    /// @brief The folder in which the folder of the book is made
    [[nodiscard]] QString folder() const { return m_folder; }

    /// @brief Set the folder in which the folder of the book is made, as Browse... does
    void setFolder(const QString& folder);

    /// @brief The folder of the new book: the folder and the title of the volume; empty when
    /// the title gives no name
    [[nodiscard]] QString bookFolder() const;

    /// @brief The line under the folder: where the book goes, or why it cannot go there
    [[nodiscard]] QString folderNote() const { return noteText(m_folderNote); }

    /// @brief The field of the folder, which cannot be typed
    [[nodiscard]] QLineEdit* folderField() const { return m_folderField; }

    /// @brief The check box that deletes the volume from the series
    [[nodiscard]] QCheckBox* deleteBox() const { return m_deleteBox; }

    /// @brief The check box that opens the new book in a new window when the window closes
    [[nodiscard]] QCheckBox* openBox() const { return m_openBox; }

    /// @brief The project file of the new book; empty before the work, or when it failed
    [[nodiscard]] QString manifest() const { return m_manifest; }

protected:
    /// @brief Take the volume out
    void run() override;

private:
    /// @brief Choose the folder in the system window
    void browseFolder();

    /// @brief The line under the folder and the accept button
    void updateFields();

    Volume m_volume;
    Taker m_taker;
    QString m_folder;
    QString m_manifest;
    QLineEdit* m_folderField = nullptr;
    FieldNote m_folderNote;
    QCheckBox* m_deleteBox = nullptr;
    FieldNote m_deleteNote;
    QCheckBox* m_openBox = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_TAKE_OUT_VOLUME_DIALOG_H
