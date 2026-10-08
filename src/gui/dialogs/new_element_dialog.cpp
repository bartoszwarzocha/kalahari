/// @file new_element_dialog.cpp
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/core/book_constants.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

NewElementDialog::NewElementDialog(NewElementKind kind, const QString& partTitle, QWidget* parent)
    : KalahariDialog(parent)
    , m_kind(kind)
{
    QString title;
    switch (kind) {
    case NewElementKind::Chapter:
        setHeading(tr("Add Chapter"),
                   tr("The chapter is added as the last one in the part \"%1\".").arg(partTitle));
        setHeadingIcon(QStringLiteral("template.chapter"));
        title = tr("New Chapter");
        break;
    case NewElementKind::Part:
        setHeading(tr("Add Part"), tr("The part is added as the last one in the book."));
        setHeadingIcon(QStringLiteral("structure.part"));
        title = tr("New Part");
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

    // An item has a type, and its title starts as the name of the type
    const QStringList types = elementTypes(kind);
    if (kind == NewElementKind::FrontMatterItem || kind == NewElementKind::BackMatterItem) {
        m_typeBox = new QComboBox(this);
        for (const QString& type : types) {
            m_typeBox->addItem(typeName(type), type);
        }
        addField(tr("Type"), m_typeBox);
        title = typeName(types.first());
        connect(m_typeBox, &QComboBox::currentIndexChanged, this, &NewElementDialog::onTypeChanged);
    }

    m_titleEdit = new QLineEdit(title, this);
    m_titleEdit->selectAll();
    addField(tr("Title"), m_titleEdit);
    contentLayout()->addStretch(1);
    connect(m_titleEdit, &QLineEdit::textEdited, this, [this]() { m_titleChanged = true; });
    connect(m_titleEdit, &QLineEdit::textChanged, this, &NewElementDialog::updateAcceptButton);

    setAcceptText(tr("Add"));
    updateAcceptButton();

    // The writer starts with what the element is: its type, or straight away its title
    if (m_typeBox) {
        m_typeBox->setFocus();
    } else {
        m_titleEdit->setFocus();
    }
}

QStringList NewElementDialog::elementTypes(NewElementKind kind)
{
    switch (kind) {
    case NewElementKind::Chapter:
        return {QString::fromLatin1(core::TYPE_CHAPTER)};
    case NewElementKind::Part:
        return {QStringLiteral("part")};
    case NewElementKind::FrontMatterItem:
        return {QString::fromLatin1(core::TYPE_TITLE_PAGE), QString::fromLatin1(core::TYPE_COPYRIGHT),
                QString::fromLatin1(core::TYPE_DEDICATION), QString::fromLatin1(core::TYPE_PREFACE)};
    case NewElementKind::BackMatterItem:
        return {QString::fromLatin1(core::TYPE_EPILOGUE), QString::fromLatin1(core::TYPE_GLOSSARY),
                QString::fromLatin1(core::TYPE_BIBLIOGRAPHY),
                QString::fromLatin1(core::TYPE_ABOUT_AUTHOR)};
    }
    return {};
}

QString NewElementDialog::elementType() const
{
    return m_typeBox ? m_typeBox->currentData().toString() : elementTypes(m_kind).first();
}

QString NewElementDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

QString NewElementDialog::typeName(const QString& type)
{
    if (type == QLatin1String(core::TYPE_TITLE_PAGE)) {
        return tr("Title Page");
    }
    if (type == QLatin1String(core::TYPE_COPYRIGHT)) {
        return tr("Copyright Page");
    }
    if (type == QLatin1String(core::TYPE_DEDICATION)) {
        return tr("Dedication");
    }
    if (type == QLatin1String(core::TYPE_PREFACE)) {
        return tr("Preface");
    }
    if (type == QLatin1String(core::TYPE_EPILOGUE)) {
        return tr("Epilogue");
    }
    if (type == QLatin1String(core::TYPE_GLOSSARY)) {
        return tr("Glossary");
    }
    if (type == QLatin1String(core::TYPE_BIBLIOGRAPHY)) {
        return tr("Bibliography");
    }
    if (type == QLatin1String(core::TYPE_ABOUT_AUTHOR)) {
        return tr("About the Author");
    }
    return type;
}

void NewElementDialog::onTypeChanged()
{
    if (!m_titleChanged) {
        m_titleEdit->setText(typeName(elementType()));
    }
}

void NewElementDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!title().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
