/// @file new_element_dialog.cpp
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/core/settings_manager.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

NewElementDialog::NewElementDialog(NewElementKind kind, const QList<NewElementChoice>& choices,
                                   int current, const QString& groupTitle, QWidget* parent)
    : KalahariDialog(parent)
    , m_choices(choices)
    , m_current(choices.isEmpty() ? -1 : qBound(0, current, static_cast<int>(choices.size()) - 1))
{
    switch (kind) {
    case NewElementKind::Chapter:
        setHeading(tr("Add Chapter"),
                   groupTitle.isEmpty()
                       ? tr("The chapter is added as the last one in the body of the book.")
                       : tr("The chapter is added as the last one in the part \"%1\".")
                             .arg(groupTitle));
        setHeadingIcon(QStringLiteral("template.chapter"));
        break;
    case NewElementKind::Part:
        setHeading(tr("Add Part"), tr("The part is added as the last one in the book."));
        setHeadingIcon(QStringLiteral("structure.part"));
        break;
    case NewElementKind::FrontMatterItem:
        setHeading(tr("Add Front Matter Item"),
                   tr("The item is added as the last one in the front matter."));
        setHeadingIcon(QStringLiteral("structure.frontmatter"));
        break;
    case NewElementKind::BackMatterItem:
        setHeading(tr("Add Back Matter Item"),
                   tr("The item is added as the last one in the back matter."));
        setHeadingIcon(QStringLiteral("structure.backmatter"));
        break;
    }

    // With more than one kind the writer chooses it; kinds are named in the program's language
    if (m_choices.size() > 1) {
        const QString language =
            QString::fromStdString(core::SettingsManager::getInstance().getLanguage());
        m_kindBox = new QComboBox(this);
        for (const NewElementChoice& choice : std::as_const(m_choices)) {
            m_kindBox->addItem(choice.kind ? choice.kind.kind->name.text(language) : QString());
        }
        m_kindBox->setCurrentIndex(m_current);
        addField(tr("Kind"), m_kindBox);
        connect(m_kindBox, &QComboBox::currentIndexChanged, this, &NewElementDialog::onKindChanged);
    }

    m_titleEdit = new QLineEdit(m_current >= 0 ? m_choices.at(m_current).title : QString(), this);
    m_titleEdit->selectAll();
    addField(tr("Title"), m_titleEdit);
    contentLayout()->addStretch(1);
    connect(m_titleEdit, &QLineEdit::textEdited, this, [this]() { m_titleChanged = true; });
    connect(m_titleEdit, &QLineEdit::textChanged, this, &NewElementDialog::updateAcceptButton);

    setAcceptText(tr("Add"));
    updateAcceptButton();

    // The writer starts with what the element is: its kind, or straight away its title
    if (m_kindBox) {
        m_kindBox->setFocus();
    } else {
        m_titleEdit->setFocus();
    }
}

core::KindRef NewElementDialog::kind() const
{
    return m_current >= 0 ? m_choices.at(m_current).kind : core::KindRef{};
}

QString NewElementDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

void NewElementDialog::onKindChanged()
{
    m_current = m_kindBox->currentIndex();
    if (!m_titleChanged && m_current >= 0) {
        m_titleEdit->setText(m_choices.at(m_current).title);
    }
}

void NewElementDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!title().isEmpty() && kind());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
