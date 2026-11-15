// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file materials_page.cpp
/// @brief Settings page: Materials
///
/// The sources: the style of the bibliography of a book without its own style (a book may
/// have its own in the Properties of the book) and the form of a reference to a source in the
/// text, which the card of a quote shows. The cards of the open books follow a change at once.

#include "kalahari/gui/settings/settings_pages.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/gui/source_names.h"

#include <QComboBox>
#include <QFormLayout>

namespace kalahari::gui {

MaterialsPage::MaterialsPage(QWidget* parent) : SettingsPage(parent) {
    using core::Bibliography;
    //: The group of the page Materials. In Polish: Źródła
    QFormLayout* sources = addGroup(tr("Sources"));

    m_style = new QComboBox();
    m_style->setObjectName(QStringLiteral("materialsSourcesStyle"));
    for (const QString& style : Bibliography::styles()) {
        const QString name = Bibliography::styleName(style);
        //: The style the program has at first: %1 its name. In Polish: %1 (zalecana)
        m_style->addItem(style == QLatin1String(Bibliography::PN_ISO_690)
                             ? tr("%1 (recommended)").arg(name)
                             : name,
                         style);
    }
    //: The style of the bibliography and of the entries of the sources. In Polish: Norma
    //: zapisu:
    addField(sources, tr("Style:"), m_style, Bibliography::STYLE_SETTING);
    //: In Polish: Norma nowych książek i książek bez własnej; książka może mieć własną we
    //: Właściwościach książki.
    addNote(sources, tr("The style of new books and of the books without their own; a book may "
                        "have its own in the Properties of the book."));

    m_reference = new QComboBox();
    m_reference->setObjectName(QStringLiteral("materialsSourcesReference"));
    for (const QString& form : Bibliography::referenceForms()) {
        const QString example = SourceNames::referenceExample(form);
        m_reference->addItem(form == QLatin1String(Bibliography::AUTHOR_YEAR)
                                 ? tr("%1 (recommended)").arg(example)
                                 : example,
                             form);
    }
    //: How the text refers to a source: (Wiśniewska 2011, s. 45). In Polish: Odnośnik:
    addField(sources, tr("Reference:"), m_reference, Bibliography::REFERENCE_SETTING);
    //: In Polish: Odnośnik pokazuje karta cytatu.
    addNote(sources, tr("The card of a quote shows the reference."));

    pageLayout()->addStretch();
}

}  // namespace kalahari::gui
