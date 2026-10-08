/// @file rename_element_dialog.cpp
/// @brief Dialog for a new name of an element of the book in the Navigator

#include "kalahari/gui/dialogs/rename_element_dialog.h"

#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

RenameElementDialog::RenameElementDialog(const QString& currentName, const QString& iconId,
                                         QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(tr("Rename"), tr("Current name: %1").arg(currentName));
    setHeadingIcon(iconId);

    m_nameEdit = new QLineEdit(currentName, this);
    m_nameEdit->selectAll();
    addField(tr("New name"), m_nameEdit);
    contentLayout()->addStretch(1);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &RenameElementDialog::updateAcceptButton);

    setAcceptText(tr("Rename"));
    updateAcceptButton();
    m_nameEdit->setFocus();
}

QString RenameElementDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

void RenameElementDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!name().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
