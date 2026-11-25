// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file new_quote_dialog.cpp
/// @brief The window of New Quote from the text selected in the preview of a PDF file

#include "kalahari/gui/dialogs/new_quote_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari::gui::dialogs {

namespace {

/// @brief Characters of a page at most: "xii", "41–42"
constexpr int MAX_PAGE_LENGTH = 32;

/// @brief Lines of the words the field shows without a scroll bar
constexpr int TEXT_LINES = 5;

}  // namespace

NewQuoteDialog::NewQuoteDialog(const QString& text, const QString& page, const QString& source,
                               const QString& file, const QString& chapter, bool canInsert,
                               QWidget* parent)
    : KalahariDialog(parent), m_canInsert(!chapter.isEmpty() && canInsert) {
    setObjectName(QStringLiteral("newQuoteDialog"));
    //: %1: the title of the file. In Polish: Z zaznaczenia w pliku „%1”.
    QString from = tr("From the selection in “%1”.").arg(file);
    if (!page.isEmpty()) {
        //: %1: the page, %2: the title of the file. In Polish: Z zaznaczenia na stronie %1
        //: pliku „%2”.
        from = tr("From the selection on page %1 of “%2”.").arg(page, file);
    }
    //: In Polish: Nowy cytat
    setHeading(tr("New Quote"), from);
    setHeadingIcon(QStringLiteral("pdf.quote"));

    m_textEdit = new QPlainTextEdit(text, this);
    m_textEdit->setObjectName(QStringLiteral("newQuoteText"));
    m_textEdit->setTabChangesFocus(true);
    m_textEdit->setFixedHeight((m_textEdit->fontMetrics().lineSpacing() * TEXT_LINES) +
                               (2 * m_textEdit->frameWidth()) +
                               static_cast<int>(2 * m_textEdit->document()->documentMargin()));
    //: In Polish: &Cytat
    addField(tr("&Quote"), m_textEdit);

    auto* sourceLabel = new QLabel(source, this);
    sourceLabel->setObjectName(QStringLiteral("newQuoteSource"));
    sourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sourceLabel->setWordWrap(true);
    //: In Polish: Źródło
    addField(tr("Source"), sourceLabel);

    m_pageEdit = new QLineEdit(page, this);
    m_pageEdit->setObjectName(QStringLiteral("newQuotePage"));
    m_pageEdit->setMaxLength(MAX_PAGE_LENGTH);
    //: In Polish: &Strona
    addField(tr("&Page"), m_pageEdit);

    m_linkBox = new QComboBox(this);
    m_linkBox->setObjectName(QStringLiteral("newQuoteLink"));
    if (!chapter.isEmpty()) {
        //: The chapter open last. %1: its title. In Polish: %1 (otwarty)
        m_linkBox->addItem(tr("%1 (open)").arg(chapter), static_cast<int>(Link::Chapter));
    }
    //: In Polish: bez powiązania
    m_linkBox->addItem(tr("no link"), static_cast<int>(Link::None));
    //: In Polish: Inny element...
    m_linkBox->addItem(tr("Other Element..."), static_cast<int>(Link::Other));
    //: In Polish: &Powiąż z
    addField(tr("&Link with"), m_linkBox);

    QLabel* noteIcon = nullptr;
    QLabel* noteText = nullptr;
    contentLayout()->addWidget(newNote(this, &noteIcon, &noteText));
    setNoteIcon(noteIcon, QStringLiteral("help.about"));
    //: In Polish: Cytat trafia do Warsztatu ze źródłem i stroną. Zaznaczenie w pliku PDF
    //: zostaje bez zmian.
    noteText->setText(tr("The quote goes to the Workshop with its source and page. The "
                         "selection in the PDF file stays as it is."));
    contentLayout()->addStretch(1);

    //: In Polish: &Utwórz
    setAcceptText(tr("&Create"));
    //: In Polish: Utwórz i &wstaw do rozdziału
    m_insertButton = buttonBox()->addButton(tr("Create and &Insert into Chapter"),
                                            QDialogButtonBox::ActionRole);
    m_insertButton->setObjectName(QStringLiteral("newQuoteInsert"));
    m_insertButton->setAutoDefault(false);
    m_insertButton->setIcon(core::ArtProvider::getInstance().getIcon(
        QStringLiteral("edit.paste"), core::IconContext::Button));
    if (chapter.isEmpty()) {
        //: In Polish: Żaden rozdział nie jest otwarty
        m_insertButton->setToolTip(tr("No chapter is open"));
    } else if (!m_canInsert) {
        //: In Polish: Rozdział jest tylko do odczytu
        m_insertButton->setToolTip(tr("The chapter is read-only"));
    }
    connect(m_insertButton, &QPushButton::clicked, this, [this]() {
        m_insert = true;
        accept();
    });
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &NewQuoteDialog::updateButtons);
    connect(m_linkBox, &QComboBox::currentIndexChanged, this, &NewQuoteDialog::updateButtons);
    updateButtons();
    m_textEdit->setFocus(Qt::OtherFocusReason);
}

QString NewQuoteDialog::text() const {
    return m_textEdit->toPlainText().trimmed();
}

QString NewQuoteDialog::page() const {
    return m_pageEdit->text().trimmed();
}

NewQuoteDialog::Link NewQuoteDialog::link() const {
    return static_cast<Link>(m_linkBox->currentData().toInt());
}

void NewQuoteDialog::updateButtons() {
    const bool words = !text().isEmpty();
    acceptButton()->setEnabled(words);
    m_insertButton->setEnabled(words && m_canInsert);
}

}  // namespace kalahari::gui::dialogs
