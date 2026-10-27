/// @file quick_insert_item_dialog.cpp
/// @brief The window of a text fragment or a typographic character of quick insert

#include "kalahari/gui/dialogs/quick_insert_item_dialog.h"
#include "kalahari/gui/quick_insert_items.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// @brief The lines of the text field: a short letter or a scene heading fits
constexpr int TEXT_LINES = 5;

}  // namespace

QuickInsertItemDialog::QuickInsertItemDialog(const editor::QuickInsertItem& item, QWidget* parent)
    : KalahariDialog(parent)
{
    const bool added = item.name.isEmpty();
    //: The window of Add... on the page Settings > Editor > Quick Insert. In Polish: Dodaj do
    //: szybkiego wstawiania
    setHeading(added ? tr("Add to Quick Insert")
                     //: The window of Edit... on the page Settings > Editor > Quick Insert. In
                     //: Polish: Zmień element szybkiego wstawiania
                     : tr("Edit Quick Insert Item"),
               tr("Type # and the first letters of the name at the start of a word: the list "
                  "at the cursor offers the text."));
    setHeadingIcon(QStringLiteral("insert.quickInsert"));

    m_name = new QLineEdit(item.name, this);
    m_name->setObjectName(QStringLiteral("quickInsertItemName"));
    //: In Polish: np. scena
    m_name->setPlaceholderText(tr("e.g. scene"));
    m_name->selectAll();
    addField(tr("Name"), m_name);

    m_text = new QPlainTextEdit(this);
    m_text->setObjectName(QStringLiteral("quickInsertItemText"));
    m_text->setPlainText(item.text);
    // Tab goes to the next field, as everywhere in the window; Enter starts a paragraph of the
    // text
    m_text->setTabChangesFocus(true);
    m_text->setFixedHeight(m_text->fontMetrics().lineSpacing() * TEXT_LINES +
                           2 * m_text->frameWidth() +
                           static_cast<int>(2 * m_text->document()->documentMargin()));
    addField(tr("Inserted text"), m_text);

    m_kind = new QComboBox(this);
    m_kind->setObjectName(QStringLiteral("quickInsertItemKind"));
    m_kind->addItem(tr("Text fragment"), static_cast<int>(editor::QuickInsertSource::Fragment));
    m_kind->addItem(tr("Typographic character"),
                    static_cast<int>(editor::QuickInsertSource::Character));
    m_kind->setCurrentIndex(item.source == editor::QuickInsertSource::Character ? 1 : 0);
    addField(tr("Group in the list"), m_kind);

    auto* hint = new QLabel(tr("A text in several lines inserts them as paragraphs. A pair of "
                               "marks, such as „” or (), leaves the cursor between them."),
                            this);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    contentLayout()->addWidget(hint);
    contentLayout()->addStretch(1);

    connect(m_name, &QLineEdit::textChanged, this, &QuickInsertItemDialog::updateAcceptButton);
    connect(m_text, &QPlainTextEdit::textChanged, this,
            &QuickInsertItemDialog::updateAcceptButton);
    //: In Polish: Dodaj / Zapisz
    setAcceptText(added ? tr("Add") : tr("Save"));
    updateAcceptButton();
    m_name->setFocus();
}

editor::QuickInsertItem QuickInsertItemDialog::item() const
{
    const auto source = static_cast<editor::QuickInsertSource>(m_kind->currentData().toInt());
    return fragmentItem(source, m_name->text().trimmed(), m_text->toPlainText());
}

void QuickInsertItemDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!m_name->text().trimmed().isEmpty() &&
                               !m_text->toPlainText().isEmpty());
}

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
