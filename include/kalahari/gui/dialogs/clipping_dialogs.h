// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clipping_dialogs.h
/// @brief The windows of the clippings: the small window of Edit > Cut to Clippings, and the
///        window of Insert > Clipping with the list and the preview

#ifndef KALAHARI_GUI_DIALOGS_CLIPPING_DIALOGS_H
#define KALAHARI_GUI_DIALOGS_CLIPPING_DIALOGS_H

#include "kalahari/gui/clippings_coordinator.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QFrame>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

#include <functional>

class QCheckBox;
class QComboBox;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui::dialogs {

/// @brief The small window of Cut to Clippings (Settings > Editor > Clippings): the title of
///        the new clipping, a note, and whether the next cut goes without the window
///
/// The title starts with the first words of the fragment, selected; Enter cuts, Esc gives up.
class CutClippingDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param title The title the clipping gets without a change: its first words
    /// @param words The words of the fragment
    /// @param from The title of the text it is cut from
    /// @param parent Parent widget
    CutClippingDialog(const QString& title, int words, const QString& from,
                      QWidget* parent = nullptr);

    /// @brief The title, without spaces around it; empty: its first words
    [[nodiscard]] QString title() const;

    /// @brief The note, without spaces around it; empty: none
    [[nodiscard]] QString note() const;

    /// @brief Whether the next cuts go without the window
    [[nodiscard]] bool cutAtOnceNextTime() const;

private:
    QLineEdit* m_title = nullptr;
    QPlainTextEdit* m_note = nullptr;
    QCheckBox* m_atOnce = nullptr;
};

/// @brief The window of Insert > Clipping: the clippings, the last cut first, with the search,
///        where they come from and the preview of the one chosen
///
/// The keys stay in the search: the letters narrow the list, Up and Down choose, Enter inserts
/// the one chosen at the cursor, Alt+P restores it to the place it came from, Alt+O opens it,
/// Esc closes.
class InsertClippingDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window was closed with
    using Choice = ClippingChoice;

    /// @brief The text of a clipping (KML), for the preview
    using TextOf = std::function<QString(const QString& clippingId)>;

    /// @param clippings All the clippings, those inserted too, the last cut first
    /// @param textOf The text of a clipping
    /// @param text The text it goes into: its title for the heading, its look for the preview
    /// @param textTitle The title of that text
    /// @param textId Its element ("Where from: this text")
    /// @param keepsInserted The inserted clippings stay in the Workshop: the window can show
    ///        them too
    /// @param parent Parent widget
    InsertClippingDialog(QList<ClippingEntry> clippings, TextOf textOf,
                         const editor::BookEditor* text, const QString& textTitle,
                         const QString& textId, bool keepsInserted, QWidget* parent = nullptr);

    /// @brief What the window was closed with
    [[nodiscard]] Choice choice() const { return m_choice; }

    /// @brief The clipping chosen; empty: none
    [[nodiscard]] QString chosenId() const;

    /// @brief The clippings the list shows now, in its order
    [[nodiscard]] QStringList shownIds() const;

protected:
    /// @brief The keys that choose go from the search to the list: Up, Down, Page Up, Page Down
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Show the clippings the search, the place and the box let through
    void filter();

    /// @brief Whether the place chosen and the box with the inserted ones let @p clipping
    ///        through
    [[nodiscard]] bool letsThrough(const ClippingEntry& clipping) const;

    /// @brief Show the clipping chosen in the preview, and the buttons for it
    void showChosen();

    /// @brief Close with @p choice of the clipping chosen
    void finish(Choice choice);

    QList<ClippingEntry> m_clippings;
    TextOf m_textOf;
    QString m_textId;
    Choice m_choice = Choice::None;

    QLineEdit* m_search = nullptr;
    QComboBox* m_from = nullptr;
    QCheckBox* m_inserted = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_details = nullptr;
    editor::BookEditor* m_preview = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_restore = nullptr;
};

/// @brief Insert > Clipping as a list at the cursor (Settings > Editor > Clippings): the
///        clippings with the search, without the preview
///
/// The clippings not inserted yet come first, the last cut first. The keys stay in the search:
/// the letters narrow the list, Up and Down choose, Enter or a click inserts the one chosen at
/// the cursor, Alt+P restores it to the place it came from, Alt+O opens it, Esc closes. It is
/// a small window of its own, which closes when another window gets the keys.
class ClippingListPopup : public QFrame {
    Q_OBJECT

public:
    /// @param clippings All the clippings, those inserted too, the last cut first
    /// @param parent Parent widget (the main window)
    explicit ClippingListPopup(QList<ClippingEntry> clippings, QWidget* parent = nullptr);

    /// @brief Show the list under the cursor of the text, or over it where the screen ends
    ///        below, within the screen
    /// @param cursor The cursor, in the coordinates of the screen
    void showAt(const QRect& cursor);

    /// @brief The clipping chosen; empty: none
    [[nodiscard]] QString chosenId() const;

    /// @brief The clippings the list shows now, in its order
    [[nodiscard]] QStringList shownIds() const;

    /// @brief Search for a text, as typed in the field
    void setSearchText(const QString& text);

    /// @brief Close with @p choice of the clipping chosen (none: nothing happens)
    void finish(kalahari::gui::ClippingChoice choice);

signals:
    /// @brief The writer chose what to do with a clipping; the list closes
    void chosen(kalahari::gui::ClippingChoice choice, const QString& clippingId);

protected:
    /// @brief The keys go to the search
    void showEvent(QShowEvent* event) override;
    /// @brief The keys of the search (keyPressed())
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Show the clippings the search lets through
    void filter();

    /// @brief @p key in the search: Up and Down choose, Enter inserts, Esc closes
    /// @return true when the list took it
    bool keyPressed(const QKeyEvent& key);

    /// @brief @p key with Alt in the search: Alt+P restores, Alt+O opens
    /// @return true when the list took it
    bool altKeyPressed(const QKeyEvent& key);

    QList<ClippingEntry> m_clippings;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_CLIPPING_DIALOGS_H
