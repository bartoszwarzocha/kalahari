// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file materials_page.cpp
/// @brief Settings page: Materials
///
/// How a material opens: a picture on a preview tab of the program or in the program the
/// system gives its file; a page or a picture on the web in the browser or on its card; a PDF
/// file on a preview tab of the program or in the system program (always there without the
/// preview of PDF files). The Navigator reads them each time a material opens.
///
/// The preview of PDF files: the layout of the pages, their look in a dark theme, the zoom and
/// the side panel it opens with, and whether it opens at the page shown last. The previews open
/// follow the layout and the look at once; the zoom and the side panel apply to the next one.
///
/// The sources: the style of the bibliography of a book without its own style (a book may
/// have its own in the Properties of the book) and the form of a reference to a source in the
/// text, which the card of a quote shows. The cards of the open books follow a change at once.

#include "kalahari/gui/settings/settings_pages.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_catalogs.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/source_names.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>

#include <string>
#include <vector>

namespace kalahari::gui {

namespace {

/// @brief The values of NavigatorPanel::OPEN_MATERIALS_SETTING and OPEN_PAGES_SETTING
constexpr const char* CARD = "card";
constexpr const char* SYSTEM = "system";
constexpr const char* BROWSER = "browser";

/// @brief The zoom of the pages at their size, in percent
constexpr int ACTUAL_PERCENT = 100;

/// @brief The fewest letters a choice of a combo box shows: a longer one is shortened in a
/// narrow window, and its list shows it whole
constexpr int MIN_CHOICE_CHARACTERS = 14;

/// @brief A combo box whose longest choice does not widen the page in a narrow window
QComboBox* narrowComboBox() {
    auto* box = new QComboBox();
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    return box;
}

/// @brief Half a line between two settings of a group, more than between the choices of one
void addGap(QFormLayout* form) {
    const int line = form->parentWidget()->fontMetrics().height();
    form->addItem(new QSpacerItem(0, line / 2, QSizePolicy::Minimum, QSizePolicy::Fixed));
}

}  // namespace

MaterialsPage::MaterialsPage(QWidget* parent) : SettingsPage(parent) {
    addOpening();
    addPdfPreview();
    addSources();
    pageLayout()->addStretch();
}

QComboBox* MaterialsPage::addChoices(QFormLayout* form, const QString& label, const char* key,
                                     const std::vector<Choice>& choices) {
    QComboBox* list = addChoice(form, label, choices, std::string());
    // Its longest choice does not widen the page in a narrow window
    list->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    list->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    bindChoice(list, key, choices.front().value.toString().toStdString());
    return list;
}

void MaterialsPage::addOpening() {
    //: The group of the page Materials: how the materials open. In Polish: Otwieranie
    QFormLayout* opening = addGroup(tr("Opening"));

    //: In Polish: Obrazy:
    addChoices(opening, tr("Pictures:"), NavigatorPanel::OPEN_MATERIALS_SETTING,
               //: In Polish: Na karcie w programie
               {{tr("On a tab of the program"), QString::fromLatin1(CARD),
                 //: In Polish: Obraz, także kopia obrazu z sieci, otwiera się na karcie
                 //: podglądu, jak rozdział. Plik, którego program nie pokazuje, otwiera się
                 //: w programie systemowym, a materiał bez pliku i adresu – na swojej karcie.
                 tr("A picture, also the copy of one from the web, opens on a preview tab, as a "
                    "chapter does. A file the program does not show opens in the system "
                    "program, and a material without a file and an address on its card.")},
                //: In Polish: W programie systemowym
                {tr("In the system program"), QString::fromLatin1(SYSTEM),
                 //: In Polish: Kartę podglądu otwiera wtedy przycisk „Podgląd” we
                 //: Właściwościach.
                 tr("The Preview button in the Properties panel then opens the preview tab.")}})
        ->setObjectName(QStringLiteral("materialsOpenPictures"));

    //: In Polish: Strony i obrazy z sieci:
    addChoices(opening, tr("Pages and pictures on the web:"), NavigatorPanel::OPEN_PAGES_SETTING,
               //: In Polish: W przeglądarce
               {{tr("In the browser"), QString::fromLatin1(BROWSER),
                 //: In Polish: Dwuklik i Enter otwierają adres w przeglądarce systemowej. Kartę
                 //: materiału na środku okna otwiera polecenie „Otwórz kartę” w menu węzła.
                 tr("Double-click and Enter open the address in the system's browser. Open Card "
                    "in the menu of the node opens the card of the material in the middle of the "
                    "window.")},
                //: In Polish: Na karcie materiału
                {tr("On the card of the material"), QString::fromLatin1(CARD),
                 //: In Polish: Dwuklik i Enter otwierają kartę materiału na środku okna, a adres
                 //: – przycisk „Otwórz w przeglądarce” na tej karcie.
                 tr("Double-click and Enter open the card of the material in the middle of the "
                    "window; its Open in Browser button opens the address.")}})
        ->setObjectName(QStringLiteral("materialsOpenPages"));

    const bool preview = PdfFiles::previewAvailable();
    //: In Polish: Strony, zakładki, spis treści i szukanie w pliku; tekst zaznaczony na
    //: stronie daje cytat z numerem strony.
    QString previewNote = tr("The pages, the bookmarks, the contents and the search of the file; "
                             "text selected on a page makes a quote with its page.");
    //: In Polish: Przycisk „Pokaż stronę” na kartach źródeł i cytatów nadal otwiera podgląd na
    //: tej stronie.
    QString systemNote = tr("Show Page on the cards of the sources and the quotes still opens "
                            "the preview at the page.");
    if (!preview) {
        //: In Polish: Ta wersja programu nie ma podglądu plików PDF: otwierają się
        //: w programie systemowym.
        previewNote = tr("This version of the program has no preview of PDF files: they open "
                         "in the system program.");
        systemNote.clear();
    }
    //: In Polish: Pliki PDF:
    m_openPdf = addChoices(opening, tr("PDF files:"), PdfFiles::OPEN_SETTING,
                           //: In Polish: Na karcie podglądu w programie
                           {{tr("On a preview tab of the program"),
                             QString::fromLatin1(PdfFiles::PREVIEW), previewNote},
                            {tr("In the system program"), QString::fromLatin1(PdfFiles::SYSTEM),
                             systemNote}});
    m_openPdf->setObjectName(QStringLiteral("materialsOpenPdf"));
    // Without the preview of PDF files they open in the system program, whatever is chosen
    if (auto* items = qobject_cast<QStandardItemModel*>(m_openPdf->model()); !preview && items) {
        items->item(0)->setEnabled(false);
    }
}

void MaterialsPage::addPdfPreview() {
    //: The group of the page Materials. In Polish: Podgląd PDF
    QFormLayout* pdf = addGroup(tr("PDF Preview"));
    // Without the preview of PDF files its settings wait
    pdf->parentWidget()->setEnabled(PdfFiles::previewAvailable());

    //: In Polish: Układ stron:
    addChoices(pdf, tr("Page layout:"), PdfFiles::LAYOUT_SETTING,
               //: In Polish: Jedna pod drugą
               {{tr("One under another"), QString::fromLatin1(PdfFiles::CONTINUOUS), {}},
                //: In Polish: Po jednej stronie
                {tr("One page at a time"), QString::fromLatin1(PdfFiles::SINGLE), {}},
                //: In Polish: Dwie obok siebie
                {tr("Two side by side"), QString::fromLatin1(PdfFiles::TWO), {}}})
        ->setObjectName(QStringLiteral("materialsPdfLayout"));
    //: In Polish: Przyciski na pasku podglądu zmieniają to samo ustawienie.
    addFieldNote(pdf, tr("The buttons on the bar of the preview change the same setting."));

    //: In Polish: W ciemnym motywie:
    addChoices(pdf, tr("In a dark theme:"), PdfFiles::DARK_SETTING,
               //: In Polish: Strony jak w pliku
               {{tr("Pages as in the file"), QString::fromLatin1(PdfFiles::AS_FILE), {}},
                //: In Polish: Strony przyciemnione
                {tr("Pages dimmed"), QString::fromLatin1(PdfFiles::DIMMED), {}},
                //: In Polish: Odwrócone kolory
                {tr("Inverted colors"), QString::fromLatin1(PdfFiles::INVERTED),
                 //: Under the list In a dark theme while Inverted colors is chosen. In Polish:
                 //: Jasny tekst na ciemnym tle. Zmienia się tylko wygląd na ekranie; plik
                 //: zostaje bez zmian.
                 tr("Light text on a dark page. Only the look on the screen changes; the file "
                    "stays as it is.")}})
        ->setObjectName(QStringLiteral("materialsPdfDark"));

    m_pdfZoom = narrowComboBox();
    m_pdfZoom->setObjectName(QStringLiteral("materialsPdfZoom"));
    //: The zoom the preview opens with. In Polish: Szerokość strony (zalecana)
    m_pdfZoom->addItem(tr("Page width (recommended)"), QString::fromLatin1(PdfFiles::PAGE_WIDTH));
    //: In Polish: Cała strona
    m_pdfZoom->addItem(tr("Whole page"), QString::fromLatin1(PdfFiles::WHOLE_PAGE));
    m_pdfZoom->addItem(tr("%1%").arg(ACTUAL_PERCENT), QString::fromLatin1(PdfFiles::ACTUAL));
    //: In Polish: Jak ostatnio
    m_pdfZoom->addItem(tr("As last time"), QString::fromLatin1(PdfFiles::LAST));
    //: In Polish: Powiększenie:
    addField(pdf, tr("Zoom:"), m_pdfZoom, PdfFiles::ZOOM_SETTING);

    m_pdfSide = narrowComboBox();
    m_pdfSide->setObjectName(QStringLiteral("materialsPdfSide"));
    //: The side panel the preview opens with. In Polish: Zakładki, a bez nich spis treści
    //: (zalecane)
    m_pdfSide->addItem(tr("Bookmarks, else the contents (recommended)"),
                       QString::fromLatin1(PdfFiles::BOOKMARKS));
    //: In Polish: Spis treści
    m_pdfSide->addItem(tr("Contents"), QString::fromLatin1(PdfFiles::CONTENTS));
    //: In Polish: Miniatury
    m_pdfSide->addItem(tr("Thumbnails"), QString::fromLatin1(PdfFiles::THUMBNAILS));
    //: In Polish: Ukryty
    m_pdfSide->addItem(tr("Hidden"), QString::fromLatin1(PdfFiles::HIDDEN));
    //: In Polish: Panel boczny:
    addField(pdf, tr("Side panel:"), m_pdfSide, PdfFiles::SIDE_SETTING);

    //: In Polish: Otwieraj na stronie oglądanej ostatnio
    QCheckBox* lastPage =
        addCheckBox(pdf, tr("Open at the page shown last"), PdfFiles::LAST_PAGE_SETTING);
    lastPage->setObjectName(QStringLiteral("materialsPdfLastPage"));
    //: In Polish: Zakreślenia na stronach, zapisane w książce
    QCheckBox* highlights = addCheckBox(pdf, tr("Highlights on the pages, kept in the book"),
                                        PdfFiles::HIGHLIGHTS_SETTING);
    highlights->setObjectName(QStringLiteral("materialsPdfHighlights"));
    //: In Polish: Polecenie „Zakreśl” w menu i na pasku zaznaczenia oznacza tekst kolorem, a
    //: panel boczny pokazuje zakreślenia z notatkami obok zakładek. Plik PDF zostaje bez zmian.
    addNote(pdf, tr("Highlight in the menu and on the bar of a selection marks the text with a "
                    "color, and the side panel lists the highlights with their notes next to the "
                    "bookmarks. The PDF file stays as it is."));

    // The colors of the preview are those of the editor, on another page. The row is a widget
    // of its own, so that the form gives the wrapped text the height it needs
    addGap(pdf);
    auto* colorsRow = new QWidget();
    auto* colors = new QHBoxLayout(colorsRow);
    colors->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Kolory tła wokół stron, zaznaczenia i wyników szukania są na stronie
    //: Edytor > Kolory, osobno w każdym motywie.
    auto* colorsText = new QLabel(tr("The colors around the pages, of the selection and of the "
                                     "search hits are on the page Editor > Colors, for each "
                                     "theme apart."));
    colorsText->setWordWrap(true);
    colors->addWidget(colorsText, 1);
    //: Shows the page Editor > Colors. In Polish: Po&każ
    auto* showColors = new QPushButton(tr("Sh&ow"));
    showColors->setObjectName(QStringLiteral("materialsShowEditorColors"));
    colors->addWidget(showColors);
    pdf->addRow(colorsRow);
    connect(showColors, &QPushButton::clicked, this, &MaterialsPage::colorsPageAsked);
}

void MaterialsPage::addSources() {
    using core::Bibliography;
    //: The group of the page Materials. In Polish: Źródła
    QFormLayout* sources = addGroup(tr("Sources"));

    m_style = narrowComboBox();
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

    m_reference = narrowComboBox();
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
    //: In Polish: Odnośnik pokazuje karta cytatu, a „Kopiuj z odnośnikiem” w podglądzie PDF
    //: dodaje go do skopiowanego tekstu.
    addNote(sources, tr("The card of a quote shows the reference, and Copy with Reference in "
                        "the preview of a PDF file adds it to the text copied."));
    addGap(sources);
    addLookup(sources);
}

void MaterialsPage::addLookup(QFormLayout* sources) {
    using core::SourceCatalogs;
    //: In Polish: Opisy z sieci na podstawie ISBN i DOI
    QCheckBox* lookup = addCheckBox(sources, tr("Descriptions from the web by ISBN and DOI"),
                                    SourceCatalogs::LOOKUP_SETTING);
    lookup->setObjectName(QStringLiteral("materialsSourcesLookup"));

    auto* catalog = narrowComboBox();
    catalog->setObjectName(QStringLiteral("materialsSourcesIsbnCatalog"));
    for (const QString& each : SourceCatalogs::isbnCatalogs()) {
        const QString name = SourceCatalogs::catalogName(each);
        //: %1: the name of a catalog. In Polish: %1 (zalecany)
        const QString recommended = tr("%1 (recommended)", "catalog").arg(name);
        catalog->addItem(each == QLatin1String(SourceCatalogs::GOOGLE_BOOKS) ? recommended : name,
                         each);
    }
    //: The catalog on the web an ISBN goes to. In Polish: Katalog dla ISBN:
    addField(sources, tr("ISBN catalog:"), catalog, SourceCatalogs::ISBN_CATALOG_SETTING);
    //: In Polish: Wysyła tylko numer i tylko na Twoje polecenie: ISBN do wybranego katalogu,
    //: DOI do doi.org. Bez tej opcji karta źródła nie ma przycisku „Opis z sieci (ISBN, DOI)...”.
    addNote(sources, tr("Sends only the number, and only when you ask: an ISBN to the catalog "
                        "chosen, a DOI to doi.org. Without this option the card of a source has "
                        "no From the Web by ISBN or DOI... button."));
    // The catalog matters only with the lookup
    catalog->setEnabled(lookup->isChecked());
    connect(lookup, &QCheckBox::toggled, catalog, &QWidget::setEnabled);
}

}  // namespace kalahari::gui
