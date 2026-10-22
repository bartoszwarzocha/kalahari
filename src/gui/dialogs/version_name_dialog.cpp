/// @file version_name_dialog.cpp
/// @brief The small window of Save Version, and of Change Description of a version

#include "kalahari/gui/dialogs/version_name_dialog.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// @brief Characters of a description at most: a line of the list of the versions
constexpr int MAX_NAME_LENGTH = 200;

}  // namespace

VersionNameDialog::VersionNameDialog(const QString& title, const core::KindWords& kind,
                                     QWidget* parent)
    : KalahariDialog(parent)
{
    //: The window of File > Save Version... In Polish: Zapisz wersję
    const QString heading = tr("Save Version");
    //: %1: the title of the text. In Polish: Wersja zachowa tekst {kind:genitive} „%1” taki, jak
    //: teraz w edytorze: z formatowaniem i uwagami, także niezapisane zmiany.
    setHeading(heading,
               core::fillWords(tr("The version keeps the text of the {kind} \"%1\" as it is "
                                  "in the editor now: with its formatting and annotations, "
                                  "also the changes not saved yet."),
                               {{QStringLiteral("kind"), kind}})
                   .arg(title));
    setHeadingIcon(QStringLiteral("file.saveVersion"));
    //: In Polish: Bez opisu wersja ma tylko datę i godzinę. Opis można zmienić później (F2 w
    //: oknie „Historia wersji”).
    addNameField(QString(), tr("Without a description the version has only its date and time. "
                               "The description can be changed later (F2 in the window "
                               "\"Version History\")."));
    setAcceptText(heading);
}

VersionNameDialog::VersionNameDialog(const QString& when, const QString& origin,
                                     const QString& name, QWidget* parent)
    : KalahariDialog(parent)
{
    //: The window of Change Description... in the window of the versions. In Polish: Zmień
    //: opis wersji
    const QString heading = tr("Change Description of the Version");
    //: %1: when the version was kept (today, 14:52), %2: where it comes from (Saved by you).
    //: In Polish: Wersja z %1. %2.
    const QString description = tr("The version of %1. %2.").arg(when, origin);
    setHeading(heading, description);
    setHeadingIcon(QStringLiteral("edit.rename"));
    //: In Polish: Bez opisu wersja ma tylko datę i godzinę.
    addNameField(name, tr("Without a description the version has only its date and time."));
    //: In Polish: Zmień opis
    setAcceptText(tr("Change Description"));
}

QString VersionNameDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

void VersionNameDialog::addNameField(const QString& name, const QString& hintText)
{
    m_nameEdit = new QLineEdit(name, this);
    m_nameEdit->setObjectName(QStringLiteral("versionNameEdit"));
    m_nameEdit->setMaxLength(MAX_NAME_LENGTH);
    //: In Polish: np. Przed zmianą zakończenia
    m_nameEdit->setPlaceholderText(tr("e.g. Before the ending was changed"));
    m_nameEdit->selectAll();
    //: In Polish: Opis (nieobowiązkowy)
    addField(tr("Description (optional)"), m_nameEdit);

    auto* hint = new QLabel(hintText, this);
    hint->setObjectName(QStringLiteral("versionNameHint"));
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    contentLayout()->addWidget(hint);
    contentLayout()->addStretch(1);
    m_nameEdit->setFocus();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
