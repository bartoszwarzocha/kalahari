/// @file message_dialog.cpp
/// @brief The program's own windows for messages, questions and a typed text

#include "kalahari/gui/dialogs/message_dialog.h"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int ALTERNATIVE_RESULT = 2;      ///< Result code of the alternative button
constexpr int COPIED_NOTICE_MS = 2000;     ///< How long Copy says "Copied"
constexpr int DETAILS_LINES = 8;           ///< Height of the details, in lines

/// @brief ArtProvider id of the icon of a kind of message
QString iconId(MessageDialog::Kind kind)
{
    switch (kind) {
        case MessageDialog::Kind::Information: return QStringLiteral("help.about");
        case MessageDialog::Kind::Warning: return QStringLiteral("common.warning");
        case MessageDialog::Kind::Error: return QStringLiteral("common.error");
        case MessageDialog::Kind::Question: return QStringLiteral("help.help");
    }
    return QString();
}

/// @brief Show a message with an OK button
void showMessage(QWidget* parent, MessageDialog::Kind kind, const QString& title,
                 const QString& text, const QString& details)
{
    MessageDialog dialog(kind, title, text, parent);
    dialog.setDetails(details);
    dialog.exec();
}

} // anonymous namespace

// =============================================================================
// MessageDialog
// =============================================================================

MessageDialog::MessageDialog(Kind kind, const QString& title, const QString& text,
                             QWidget* parent)
    : KalahariDialog(parent)
    , m_title(title)
    , m_text(text)
{
    setHeading(title);
    setHeadingIcon(iconId(kind));

    // The message can be selected and copied in part, with the mouse or the keyboard
    m_textLabel = new QLabel(text, this);
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setWordWrap(true);
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                         Qt::TextSelectableByKeyboard);
    contentLayout()->addWidget(m_textLabel);

    m_detailsEdit = new QPlainTextEdit(this);
    m_detailsEdit->setReadOnly(true);
    m_detailsEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_detailsEdit->setFixedHeight(m_detailsEdit->fontMetrics().lineSpacing() * DETAILS_LINES);
    m_detailsEdit->setAccessibleName(tr("Details"));
    m_detailsEdit->hide();
    contentLayout()->addWidget(m_detailsEdit, 1);

    m_copyButton = addSideButton(tr("&Copy"));
    m_copyButton->setToolTip(tr("Copy the message to the clipboard"));
    // As wide as its longest text, so "Copied" does not move the other buttons
    const int copyWidth = m_copyButton->sizeHint().width();
    m_copyButton->setText(tr("Copied"));
    m_copyButton->setMinimumWidth(std::max(copyWidth, m_copyButton->sizeHint().width()));
    m_copyButton->setText(tr("&Copy"));
    connect(m_copyButton, &QPushButton::clicked, this, &MessageDialog::copyToClipboard);

    m_detailsButton = addSideButton(tr("Show &Details"));
    connect(m_detailsButton, &QPushButton::clicked, this, &MessageDialog::toggleDetails);

    m_alternativeButton = buttonBox()->addButton(QString(), QDialogButtonBox::DestructiveRole);
    connect(m_alternativeButton, &QPushButton::clicked, this, [this]() {
        m_answer = Answer::Alternative;
        done(ALTERNATIVE_RESULT);
    });

    // A message has only OK and Copy: Esc closes it as well. Hidden after the alternative
    // is added, as adding a button lays out the box again and shows the buttons in it
    m_detailsButton->hide();
    m_alternativeButton->hide();
    applyButton()->hide();
    cancelButton()->hide();
    connect(acceptButton(), &QPushButton::clicked, this, [this]() { m_answer = Answer::Accept; });
    acceptButton()->setFocus();
}

void MessageDialog::setDetails(const QString& details)
{
    m_detailsEdit->setPlainText(details);
    m_detailsButton->setVisible(!details.isEmpty());
    if (details.isEmpty()) {
        m_detailsEdit->hide();
    }
}

void MessageDialog::setQuestionButtons(const QString& acceptText, const QString& cancelText)
{
    setAcceptText(acceptText);
    if (!cancelText.isEmpty()) {
        cancelButton()->setText(cancelText);
    }
    cancelButton()->show();
}

void MessageDialog::setAlternativeText(const QString& text)
{
    m_alternativeButton->setText(text);
    m_alternativeButton->show();
}

void MessageDialog::setDefaultButton(DefaultButton button)
{
    QPushButton* defaultButton =
        button == DefaultButton::Cancel ? cancelButton() : acceptButton();
    acceptButton()->setDefault(false);
    cancelButton()->setDefault(false);
    defaultButton->setDefault(true);
    defaultButton->setFocus();
}

QString MessageDialog::clipboardText() const
{
    QStringList parts{m_title, m_text};
    const QString details = m_detailsEdit->toPlainText();
    if (!details.isEmpty()) {
        parts.append(details);
    }
    parts.removeAll(QString());
    return parts.join(QStringLiteral("\n\n"));
}

void MessageDialog::copyToClipboard()
{
    QApplication::clipboard()->setText(clipboardText());

    // Say it is done, then give the button its name back; the button is the timer's
    // context, so the timer does not fire after the button is gone
    const QString text = m_copyButton->text();
    m_copyButton->setText(tr("Copied"));
    QTimer::singleShot(COPIED_NOTICE_MS, m_copyButton,
                       [button = m_copyButton, text]() { button->setText(text); });
}

void MessageDialog::setMonospaced(bool monospaced)
{
    m_textLabel->setFont(monospaced ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                                    : font());
    // Columns keep their lines; the window gets as wide as they are, up to the screen
    m_textLabel->setWordWrap(!monospaced);
}

void MessageDialog::toggleDetails()
{
    const bool show = m_detailsEdit->isHidden();
    m_detailsEdit->setVisible(show);
    m_detailsButton->setText(show ? tr("Hide &Details") : tr("Show &Details"));
    // Once the layout has taken in the shown or hidden details
    QTimer::singleShot(0, this, [this]() {
        if (isVisible()) {
            fitToScreen(sizeHint());
        }
    });
}

void MessageDialog::showEvent(QShowEvent* event)
{
    KalahariDialog::showEvent(event);
    QAccessibleEvent alert(this, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);
}

void MessageDialog::information(QWidget* parent, const QString& title, const QString& text,
                                const QString& details)
{
    showMessage(parent, Kind::Information, title, text, details);
}

void MessageDialog::warning(QWidget* parent, const QString& title, const QString& text,
                            const QString& details)
{
    showMessage(parent, Kind::Warning, title, text, details);
}

void MessageDialog::error(QWidget* parent, const QString& title, const QString& text,
                          const QString& details)
{
    showMessage(parent, Kind::Error, title, text, details);
}

bool MessageDialog::confirm(QWidget* parent, const QString& title, const QString& text,
                            const QString& acceptText, Kind kind, DefaultButton defaultButton)
{
    MessageDialog dialog(kind, title, text, parent);
    dialog.setQuestionButtons(acceptText);
    dialog.setDefaultButton(defaultButton);
    dialog.exec();
    return dialog.answer() == Answer::Accept;
}

MessageDialog::Answer MessageDialog::ask(QWidget* parent, const QString& title,
                                         const QString& text, const QString& acceptText,
                                         const QString& alternativeText, Kind kind)
{
    MessageDialog dialog(kind, title, text, parent);
    dialog.setQuestionButtons(acceptText);
    dialog.setAlternativeText(alternativeText);
    dialog.exec();
    return dialog.answer();
}

// =============================================================================
// TextInputDialog
// =============================================================================

TextInputDialog::TextInputDialog(const QString& title, const QString& label,
                                 const QString& text, QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(title);
    setCompactHeading(true);

    m_field = new QLineEdit(text, this);
    m_field->selectAll();
    addField(label, m_field);
    contentLayout()->addStretch(1);
    connect(m_field, &QLineEdit::textChanged, this, &TextInputDialog::updateAcceptButton);

    updateAcceptButton();
    m_field->setFocus();
}

QString TextInputDialog::text() const
{
    return m_field->text().trimmed();
}

std::optional<QString> TextInputDialog::getText(QWidget* parent, const QString& title,
                                                const QString& label, const QString& text,
                                                const QString& acceptText)
{
    TextInputDialog dialog(title, label, text, parent);
    if (!acceptText.isEmpty()) {
        dialog.setAcceptText(acceptText);
    }
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.text();
}

void TextInputDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!text().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
