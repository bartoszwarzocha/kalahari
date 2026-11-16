/// @file open_book_dialog.cpp
/// @brief The question "Opening a Book": where a book goes when another one is open in the
/// window

#include "kalahari/gui/dialogs/open_book_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

OpenBookDialog::OpenBookDialog(const QString& openBook, const QString& otherBook,
                               QWidget* parent)
    : KalahariDialog(parent)
{
    //: The question when a book is opened while another one is open in the window. In Polish:
    //: Otwieranie książki
    const QString title = tr("Opening a Book");
    //: %1: the title of the open book, %2: the name of the book chosen. In Polish: W tym oknie
    //: jest otwarta książka „%1”. Gdzie otworzyć książkę „%2”?
    const QString question = tr("This window has the book \"%1\" open. Where do you want to "
                                "open the book \"%2\"?");
    setHeading(title, question.arg(openBook, otherBook));
    setHeadingIcon(QStringLiteral("window.new"));
    //: Settings > General has the option of where a book opens. In Polish: Ustawienia okien
    addSettingsButton(QString::fromLatin1(SettingsDialog::GENERAL_PAGE), tr("Window Settings"));

    //: In Polish: &Zapamiętaj wybór i nie pytaj więcej (zmienisz go w Ustawieniach › Ogólne)
    m_rememberBox = new QCheckBox(
        tr("&Remember my choice and do not ask again (you can change it in Settings > General)"),
        this);
    m_rememberBox->setObjectName(QStringLiteral("rememberChoice"));
    contentLayout()->addWidget(m_rememberBox);
    contentLayout()->addStretch();

    //: The book opens in a new window; the open one stays. In Polish: W &nowym oknie
    setAcceptText(tr("In a &New Window"));
    //: The open book closes, and the book chosen opens in its place. In Polish: W &tym oknie
    m_thisWindowButton =
        buttonBox()->addButton(tr("In &This Window"), QDialogButtonBox::DestructiveRole);
    m_thisWindowButton->setObjectName(QStringLiteral("inThisWindow"));
    // Adding a button lays out the box again and shows the buttons in it, Apply too
    setApplyButtonVisible(false);
    connect(acceptButton(), &QPushButton::clicked, this,
            [this]() { m_choice = Choice::NewWindow; });
    connect(m_thisWindowButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::ThisWindow;
        accept();
    });
    acceptButton()->setFocus();
}

bool OpenBookDialog::remembersChoice() const
{
    return m_rememberBox->isChecked();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
