// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_lookup_dialog.cpp
/// @brief The window of From the Web by ISBN or DOI...: the number of a source, and the
/// description a catalog on the web gives for it

#include "kalahari/gui/dialogs/source_lookup_dialog.h"

#include "kalahari/core/source_catalogs.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/source_lookup.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari::gui::dialogs {

namespace {

/// @brief Characters of a number at most: a DOI can be long
constexpr int MAX_NUMBER_LENGTH = 300;

}  // namespace

SourceLookupDialog::SourceLookupDialog(const QString& number, QWidget* parent)
    : KalahariDialog(parent),
      m_numberEdit(new QLineEdit(number, this)),
      m_statusLabel(new QLabel(this)),
      m_lookup(new SourceLookup(this)) {
    setObjectName(QStringLiteral("sourceLookupDialog"));
    //: In Polish: Opis z sieci (ISBN, DOI)
    setHeading(tr("From the Web by ISBN or DOI"),
               //: In Polish: Opis źródła z katalogu w sieci.
               tr("The description of the source from a catalog on the web."));
    setHeadingIcon(QStringLiteral("material.page"));

    m_numberEdit->setObjectName(QStringLiteral("sourceLookupNumber"));
    m_numberEdit->setMaxLength(MAX_NUMBER_LENGTH);
    //: In Polish: 978-83-… albo 10.…/…
    m_numberEdit->setPlaceholderText(tr("978-83-… or 10.…/…"));
    //: In Polish: &ISBN albo DOI
    addField(tr("&ISBN or DOI"), m_numberEdit);

    m_statusLabel->setObjectName(QStringLiteral("sourceLookupStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->hide();
    contentLayout()->addWidget(m_statusLabel);

    QLabel* noteIcon = nullptr;
    QLabel* noteText = nullptr;
    contentLayout()->addWidget(newNote(this, &noteIcon, &noteText));
    setNoteIcon(noteIcon, QStringLiteral("help.about"));
    //: %1: the catalog of an ISBN, Google Books or Open Library. In Polish: Kalahari wysyła
    //: tylko ten numer: ISBN do %1, DOI do doi.org. Pola z opisu katalogu zastępują te na
    //: karcie; pozostałe zostają bez zmian.
    noteText->setText(tr("Kalahari sends only this number: an ISBN to %1, a DOI to doi.org. "
                         "The description of the catalog takes the place of the fields it has; "
                         "the others stay.")
                          .arg(core::SourceCatalogs::catalogName(
                              core::SourceCatalogs::settingsIsbnCatalog())));
    contentLayout()->addStretch(1);

    //: The settings of the sources. In Polish: Ustawienia źródeł
    addSettingsButton(QString::fromLatin1(SettingsDialog::MATERIALS_PAGE), tr("Source Settings"));
    //: In Polish: &Szukaj
    setAcceptText(tr("&Look Up"));
    connect(m_lookup, &SourceLookup::finished, this, &SourceLookupDialog::take);
    connect(m_numberEdit, &QLineEdit::textEdited, this, [this]() { m_statusLabel->hide(); });
    m_numberEdit->setFocus(Qt::OtherFocusReason);
    m_numberEdit->selectAll();
}

void SourceLookupDialog::accept() {
    if (m_lookup->isRunning()) {
        return;
    }
    const QString catalog = SourceLookup::catalogOf(m_numberEdit->text());
    if (catalog.isEmpty()) {
        //: In Polish: To nie jest ISBN ani DOI. ISBN ma 10 albo 13 cyfr (ostatnia może być
        //: X), a DOI zaczyna się od „10.” i zawiera ukośnik.
        showStatus(tr("This is neither an ISBN nor a DOI. An ISBN has 10 or 13 digits (the "
                      "last can be X), a DOI begins with \"10.\" and has a slash."));
        m_numberEdit->setFocus(Qt::OtherFocusReason);
        return;
    }
    //: %1: the catalog, Google Books, Open Library or doi.org. In Polish: Łączenie z %1…
    showStatus(tr("Asking %1…").arg(core::SourceCatalogs::catalogName(catalog)));
    setWaiting(true);
    m_lookup->start(m_numberEdit->text());
}

void SourceLookupDialog::reject() {
    m_lookup->cancel();
    QDialog::reject();
}

void SourceLookupDialog::take(const SourceLookup::Result& result) {
    setWaiting(false);
    if (result.source) {
        m_found = result.source;
        m_catalog = result.catalog;
        QDialog::accept();
        return;
    }
    showStatus(result.error);
    m_numberEdit->setFocus(Qt::OtherFocusReason);
    m_numberEdit->selectAll();
}

void SourceLookupDialog::showStatus(const QString& text) {
    m_statusLabel->setText(text);
    m_statusLabel->show();
    // The note under it stays in view
    growForContent();
}

void SourceLookupDialog::setWaiting(bool waiting) {
    m_numberEdit->setReadOnly(waiting);
    acceptButton()->setEnabled(!waiting);
}

}  // namespace kalahari::gui::dialogs
