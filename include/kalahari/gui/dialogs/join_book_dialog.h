// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file join_book_dialog.h
/// @brief Book > Join Book as Volume...: a Kalahari book copied into the series as a new
///        volume, with its Workshop joined to the series', in one window from the choice of
///        the book to the result

#ifndef KALAHARI_GUI_DIALOGS_JOIN_BOOK_DIALOG_H
#define KALAHARI_GUI_DIALOGS_JOIN_BOOK_DIALOG_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/volume_copies.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QTreeWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

class RowList;

/// @brief Book > Join Book as Volume...: a book of one volume, of the type of the open project,
/// joins it as a new volume
///
/// The window reads the book chosen (its .klh file) without opening it, and says why it cannot
/// join: a series, another type, or a book the series has already (core::JoinProblem). The
/// volume goes to the place chosen among the volumes, at first the last one. The cards of the
/// book with the kind and the title of a card of the series are listed, each checked at first:
/// a checked one is the card of the series, which keeps what it has and gains the links of the
/// book's card. As the windows of the archives, the window shows the progress of the work, its
/// result, or what went wrong, with Copy. The book stays as it is.
class JoinBookDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief What the work did
    struct Outcome {
        QString bookId;        ///< The id of the new volume; empty: the book did not join
        QStringList problems;  ///< How the work failed, as the log has it
    };

    /// @brief Joins the book of the .klh file @p manifest as the volume at @p index, the cards
    /// of @p sameCards being those of the series (core::ProjectManager::joinBook())
    using Joiner = std::function<Outcome(const QString& manifest, qsizetype index,
                                         const QList<core::CardPair>& sameCards)>;

    /// @brief Create the window without a book
    /// @param book The open project, which the book joins
    /// @param joiner Joins the book
    /// @param parent Parent widget
    JoinBookDialog(const core::ProjectManager& book, Joiner joiner, QWidget* parent = nullptr);

    /// @brief The .klh file of the book; empty before it is chosen
    [[nodiscard]] QString manifest() const { return m_manifest; }

    /// @brief Read the book of the .klh file @p path and show it, as Browse... does
    void setManifest(const QString& path);

    /// @brief The line under the file: why the book cannot be read or cannot join; empty when
    /// it can join
    [[nodiscard]] QString fileNote() const { return noteText(m_fileNote); }

    /// @brief The book read, as the window shows it: "Title: value" a line
    [[nodiscard]] QString bookText() const;

    /// @brief The list of the places among the volumes
    [[nodiscard]] QComboBox* placeBox() const { return m_placeBox; }

    /// @brief The list of the cards of the book that have the kind and the title of a card of
    /// the series; hidden when there are none
    [[nodiscard]] QTreeWidget* cardList() const;

    /// @brief The pairs of cards checked in the list
    [[nodiscard]] QList<core::CardPair> sameCards() const;

    /// @brief The id of the new volume; empty before the work, or when it failed
    [[nodiscard]] QString bookId() const { return m_bookId; }

protected:
    /// @brief Join the book
    void run() override;

private:
    /// @brief Choose the book in the system window
    void browse();

    /// @brief The rows of the book, the list of the cards and the line under the file
    void showBook();

    /// @brief The accept button: a book that can join
    void updateFields();

    /// @brief Why the book cannot join, as the window says it; empty when it can
    [[nodiscard]] QString problemText() const;

    const core::ProjectManager& m_book;
    Joiner m_joiner;
    QString m_manifest;
    std::optional<core::BookProject> m_project;  ///< The book read
    QStringList m_readProblems;                  ///< Why it cannot be read, as the log has it
    QList<core::CardPair> m_pairs;               ///< The cards of the list, in its order
    QString m_bookId;
    QLineEdit* m_fileField = nullptr;
    FieldNote m_fileNote;
    QLabel* m_titleValue = nullptr;
    QLabel* m_authorValue = nullptr;
    QComboBox* m_placeBox = nullptr;
    QGroupBox* m_cardsGroup = nullptr;
    RowList* m_cardList = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_JOIN_BOOK_DIALOG_H
