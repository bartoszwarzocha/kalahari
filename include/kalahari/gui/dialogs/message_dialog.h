/// @file message_dialog.h
/// @brief The program's own windows for messages, questions and a typed text

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

#include <optional>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A message or a question in the look of the program's own dialogs
///
/// It takes the place of the system message boxes: the heading shows the kind's icon and
/// the title, the content the message (it can be selected), and optional details under
/// Show Details. Copy puts the title, the message and the details on the clipboard.
///
/// The buttons: the one that does the job (OK for a message, named after the action for
/// a question), an optional alternative (e.g. "Don't Save") and Cancel. Enter presses the
/// default button, Esc closes the dialog as Cancel, and every button has a mnemonic.
///
/// The static functions show the dialog in one call, in place of QMessageBox's:
/// @code
/// MessageDialog::error(this, tr("Import Failed"), tr("The archive could not be read."), details);
/// if (MessageDialog::confirm(this, tr("Delete Toolbar"), text, tr("Delete"))) { ... }
/// switch (MessageDialog::ask(this, tr("Unsaved Changes"), text, tr("Save"), tr("Don't Save"))) { ... }
/// @endcode
class MessageDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the message is about; it chooses the icon
    enum class Kind { Information, Warning, Error, Question };

    /// @brief The button that closed the dialog
    enum class Answer {
        Accept,       ///< The button that does the job (OK, Save, Delete...)
        Alternative,  ///< The alternative button (e.g. "Don't Save")
        Cancel        ///< Cancel, Esc or the window's close button
    };

    /// @brief Which button Enter presses
    enum class DefaultButton {
        Accept,  ///< The button that does the job
        Cancel   ///< Cancel: for a step that cannot be undone
    };

    /// @brief Create a message with an OK button only
    /// @param kind What the message is about
    /// @param title Title of the dialog, also its window title
    /// @param text The message, plain text
    /// @param parent Parent widget
    MessageDialog(Kind kind, const QString& title, const QString& text, QWidget* parent = nullptr);

    /// @brief Set the details shown under Show Details (e.g. the error of the system)
    /// @param details Plain text; empty: no details and no Show Details button
    void setDetails(const QString& details);

    /// @brief Show the text in a fixed-width font, without wrapping, for text laid out in
    ///        columns (e.g. the command line help)
    void setMonospaced(bool monospaced);

    /// @brief Turn the message into a question: show Cancel and name the accept button
    /// @param acceptText Text of the button that does the job (e.g. "&Delete")
    /// @param cancelText Text of Cancel; empty: "Cancel"
    void setQuestionButtons(const QString& acceptText, const QString& cancelText = QString());

    /// @brief Show the alternative button, between the accept button and Cancel
    /// @param text Its text (e.g. "Do&n't Save")
    void setAlternativeText(const QString& text);

    /// @brief Choose the button Enter presses (by default the accept button)
    void setDefaultButton(DefaultButton button);

    /// @brief The button that closed the dialog; Cancel while it is open
    Answer answer() const { return m_answer; }

    /// @brief The text Copy puts on the clipboard: the title, the message and the details
    QString clipboardText() const;

    /// @brief The alternative button (hidden unless setAlternativeText() shows it)
    QPushButton* alternativeButton() const { return m_alternativeButton; }

    /// @brief The button that copies the message to the clipboard
    QPushButton* copyButton() const { return m_copyButton; }

    /// @brief The button that shows and hides the details (hidden without details)
    QPushButton* detailsButton() const { return m_detailsButton; }

    /// @brief Show a message with an OK button
    /// @param parent Parent widget
    /// @param title Title of the dialog
    /// @param text The message
    /// @param details Details under Show Details; empty: none
    static void information(QWidget* parent, const QString& title, const QString& text,
                            const QString& details = QString());

    /// @copydoc information
    static void warning(QWidget* parent, const QString& title, const QString& text,
                        const QString& details = QString());

    /// @copydoc information
    static void error(QWidget* parent, const QString& title, const QString& text,
                      const QString& details = QString());

    /// @brief Ask whether to do something: the action's button and Cancel
    /// @param parent Parent widget
    /// @param title Title of the dialog
    /// @param text The question
    /// @param acceptText Text of the action's button (e.g. "&Delete")
    /// @param kind Question, or Warning for a step that cannot be undone
    /// @param defaultButton The button Enter presses
    /// @return true if the action's button was pressed
    static bool confirm(QWidget* parent, const QString& title, const QString& text,
                        const QString& acceptText, Kind kind = Kind::Question,
                        DefaultButton defaultButton = DefaultButton::Accept);

    /// @brief Ask a question with three answers: the action, its alternative and Cancel
    /// @param parent Parent widget
    /// @param title Title of the dialog
    /// @param text The question
    /// @param acceptText Text of the action's button (e.g. "&Save")
    /// @param alternativeText Text of the alternative (e.g. "Do&n't Save")
    /// @param kind What the question is about
    /// @return The button that closed the dialog
    static Answer ask(QWidget* parent, const QString& title, const QString& text,
                      const QString& acceptText, const QString& alternativeText,
                      Kind kind = Kind::Question);

protected:
    /// @brief Tell screen readers about the message when it appears
    void showEvent(QShowEvent* event) override;

private:
    /// @brief Put the text of clipboardText() on the clipboard and say so on Copy
    void copyToClipboard();

    /// @brief Show or hide the details
    void toggleDetails();

    QString m_title;
    QString m_text;
    QLabel* m_textLabel = nullptr;
    QPlainTextEdit* m_detailsEdit = nullptr;
    QPushButton* m_alternativeButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_detailsButton = nullptr;
    Answer m_answer = Answer::Cancel;
};

/// @brief A text to type, e.g. the name of a new toolbar, in place of QInputDialog
///
/// The accept button is disabled while the field is empty; text() is trimmed.
class TextInputDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog
    /// @param title Title of the dialog, also its window title
    /// @param label Label of the field, without a colon
    /// @param text Text in the field at the start (it is selected)
    /// @param parent Parent widget
    TextInputDialog(const QString& title, const QString& label, const QString& text = QString(),
                    QWidget* parent = nullptr);

    /// @brief The typed text, without spaces at its ends
    QString text() const;

    /// @brief The field
    QLineEdit* field() const { return m_field; }

    /// @brief Ask for a text in one call
    /// @param parent Parent widget
    /// @param title Title of the dialog
    /// @param label Label of the field, without a colon
    /// @param text Text in the field at the start
    /// @param acceptText Text of the accept button; empty: "OK"
    /// @return The typed text (not empty), or nothing if the dialog was cancelled
    static std::optional<QString> getText(QWidget* parent, const QString& title,
                                          const QString& label, const QString& text = QString(),
                                          const QString& acceptText = QString());

private:
    /// @brief Enable the accept button only for a text that is not empty
    void updateAcceptButton();

    QLineEdit* m_field = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
