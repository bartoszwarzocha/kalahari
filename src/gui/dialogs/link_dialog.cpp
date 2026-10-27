/// @file link_dialog.cpp
/// @brief Insert > Link... (Ctrl+K): the text of a link and its address

#include "kalahari/gui/dialogs/link_dialog.h"

#include "kalahari/editor/text_link.h"
#include "kalahari/gui/settings_dialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

LinkDialog::LinkDialog(QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(tr("Link"), tr("The text of the link and the address of the web page."));
    setHeadingIcon(QStringLiteral("insert.link"));
    setAcceptText(tr("OK"));
    // The color of links is chosen with the colors of the editor
    addSettingsButton(QString::fromLatin1(SettingsDialog::EDITOR_COLORS_PAGE),
                      tr("Link Color Settings"));

    m_text = new QLineEdit(this);
    addField(tr("&Text to display"), m_text);
    m_address = new QLineEdit(this);
    m_address->setPlaceholderText(QStringLiteral("https://"));
    addField(tr("&Address"), m_address);

    //: %1 is the key held for the click: "Ctrl+" (on macOS "⌘")
    auto* hint = new QLabel(
        tr("%1click on a link in the text opens its address in the browser.")
            .arg(editor::linkClickKeys()),
        this);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(hint);
    contentLayout()->addStretch(1);

    m_remove = addSideButton(tr("&Remove Link"));
    m_remove->hide();
    connect(m_remove, &QPushButton::clicked, this, [this]() {
        m_removeAsked = true;
        accept();
    });
    connect(m_address, &QLineEdit::textChanged, this, &LinkDialog::updateAcceptButton);
    updateAcceptButton();

    m_address->setFocus();
}

void LinkDialog::setLink(const QString& text, const QString& address, bool existing,
                         bool textEditable)
{
    m_text->setText(text);
    m_text->setReadOnly(!textEditable);
    m_address->setText(address);
    m_remove->setVisible(existing);
    // The address first, as in Word: the text is often the selected one already
    m_address->setFocus();
    m_address->selectAll();
}

QString LinkDialog::text() const
{
    return m_text->text();
}

QString LinkDialog::address() const
{
    return m_address->text();
}

void LinkDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!m_address->text().trimmed().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
