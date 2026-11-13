/// @file edit_dialogs.h
/// @brief The windows of the Edit menu: Paste Special... and Go To...

#pragma once

#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/text_places.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

#include <functional>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QMimeData;
class QPushButton;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari {
namespace gui {
namespace dialogs {

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
        editor::ClipboardSource source = editor::ClipboardSource::Nothing;
        std::vector<editor::PasteForm> forms;  ///< Those it pastes in; the others greyed out
        editor::PasteForm chosen = editor::PasteForm::Merge;  ///< Chosen first, among forms
        bool picture = false;  ///< The clipboard has a picture or files of pictures
        QString whyNoImage;    ///< Why Image is greyed out with a picture; empty: it is not
    };

    explicit PasteSpecialDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The form chosen
    [[nodiscard]] editor::PasteForm form() const;

    /// @brief What the window shows for MIME data pasted in a text
    [[nodiscard]] static Data dataFor(const editor::BookEditor& text, const QMimeData* data);

    /// @brief The window over @p parent for the clipboard, and the paste in the form chosen
    /// @return Whether it pasted
    static bool pasteInto(editor::BookEditor& text, QWidget* parent);

    /// @brief The name of a source: "Microsoft Word", "a web page or another program"
    [[nodiscard]] static QString sourceName(editor::ClipboardSource source);

private:
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

} // namespace dialogs
} // namespace gui
} // namespace kalahari
