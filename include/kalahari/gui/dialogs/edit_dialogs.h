// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file edit_dialogs.h
/// @brief The windows of the Edit menu: Paste Special... and Go To...

#ifndef KALAHARI_GUI_DIALOGS_EDIT_DIALOGS_H
#define KALAHARI_GUI_DIALOGS_EDIT_DIALOGS_H

#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/text_places.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

#include <functional>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QMimeData;
class QPushButton;
class QVBoxLayout;

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui::dialogs {

/// @brief Edit > Paste Special...: the form the content of the clipboard goes into the text in
///
/// Where the content comes from (Kalahari, Word, LibreOffice, a web page...), the list of the
/// forms (editor::PasteForm), those the clipboard has not greyed out, saying why, and what
/// the one chosen keeps. The one Paste would give is chosen first: the Kalahari format of text
/// copied in Kalahari, else the paste mode of the setting, which the gear opens (Settings >
/// Editor > General, Paste Command).
class PasteSpecialDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows
    struct Data {
        editor::ClipboardSource source = editor::ClipboardSource::Nothing;  ///< Where it comes from
        std::vector<editor::PasteForm> forms;  ///< Those it pastes in; the others greyed out
        editor::PasteForm chosen = editor::PasteForm::Merge;  ///< Chosen first, among forms
        bool picture = false;  ///< The clipboard has a picture or files of pictures
        QString whyNoImage;    ///< Why Image is greyed out with a picture; empty: it is not
    };

    /// @brief The window showing @p shown
    explicit PasteSpecialDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The form chosen
    [[nodiscard]] editor::PasteForm form() const;

    /// @brief What the window shows for MIME data pasted in a text
    [[nodiscard]] static Data dataFor(const editor::BookEditor& text, const QMimeData* data);

    /// @brief The window over @p parent for the clipboard, and the paste in the form chosen
    /// @return Whether it pasted
    static bool pasteInto(editor::BookEditor& text, QWidget* parent);

    /// @brief The forms of the window as the items of @p menu, the submenu Paste Special of
    ///        the text's context menu: each pastes the clipboard in its form; those the
    ///        clipboard has not are greyed out, their tooltips saying why
    static void fillMenu(QMenu& menu, editor::BookEditor& text);

    /// @brief The name of a source: "Microsoft Word", "a web page or another program"
    [[nodiscard]] static QString sourceName(editor::ClipboardSource source);

private:
    /// @brief The list of the forms, the clipboard's own chosen, and the hint under it
    void addForms();

    /// @brief What the form chosen keeps, and what the clipboard has not
    void showHint();

    Data m_data;
    QListWidget* m_forms = nullptr;
    QLabel* m_hint = nullptr;
};

/// @brief Edit > Go To...: a page, a heading, a note, an image, a table or an annotation of the
///        text, by its number, on or back from the cursor, or the next and the previous one
///
/// The window stays open over the text, which it shows the place in: Go (Enter) goes to the
/// number ("+2", "-1" counting from the cursor), or without one to the next place, as Next
/// does; Previous goes back. The line under the hint says where the cursor went ("Heading 3
/// of 7") or why it did not ("This text has no tables"). The pages are those of the Page
/// Layout view: in the other views Page is greyed out, saying so.
class GoToDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The text the window goes in; nullptr: none
    using TextOf = std::function<editor::BookEditor*()>;

    /// @brief The window going in the text @p textOf gives
    explicit GoToDialog(TextOf textOf, QWidget* parent = nullptr);

    /// @brief The kind of place chosen
    [[nodiscard]] editor::PlaceKind kind() const;

    /// @brief Choose a kind (Page only where the text has pages)
    void setKind(editor::PlaceKind kind);

    /// @brief What the window says it did last ("Heading 3 of 7"); empty: nothing yet
    [[nodiscard]] QString result() const;

    /// @brief Follow the text: Page greyed out but in the Page Layout view, the buttons without
    ///        a text
    void updateTarget();

    /// @brief Go (Enter): the place of the number; without one the next place. The window
    ///        stays open.
    void accept() override;

    /// @brief Next and Previous
    void goAdjacent(bool next);

    /// @brief The sentence of an outcome: "Heading 3 of 7", "This text has no tables"
    [[nodiscard]] static QString outcomeText(editor::PlaceKind kind,
                                             const editor::GoToOutcome& outcome);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    /// @brief The list of the kinds of places with its label
    [[nodiscard]] QVBoxLayout* kindsColumn();

    /// @brief The number with its label, the hint of the kind and what Go To did
    [[nodiscard]] QVBoxLayout* numberColumn();

    /// @brief The label of the number and the hint of the kind chosen
    void showKind();

    TextOf m_textOf;
    QListWidget* m_kinds = nullptr;
    QLabel* m_numberLabel = nullptr;
    QLineEdit* m_number = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_result = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_EDIT_DIALOGS_H
