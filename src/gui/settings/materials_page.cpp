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
#include <QRadioButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

using json = nlohmann::json;

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

void MaterialsPage::addChoices(QFormLayout* form, const QString& label, const char* key,
                               const QList<Choice>& choices) {
    auto* box = new QWidget();
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    for (const Choice& choice : choices) {
        layout->addWidget(choice.button);
        if (!choice.note.isEmpty()) {
            alignWithText(addNote(layout, choice.note), choice.button);
        }
    }
    auto* caption = new QLabel(label);
    caption->setBuddy(choices.front().button);
    form->addRow(caption, box);
    addGap(form);

    Binding binding;
    binding.key = [setting = std::string(key)]() { return setting; };
    binding.stored = [setting = std::string(key), fallback = std::string(choices.front().value)]() {
        return json(core::SettingsManager::getInstance().get<std::string>(setting, fallback));
    };
    binding.shown = [choices]() {
        for (const Choice& choice : choices) {
            if (choice.button->isChecked()) {
                return json(choice.value);
            }
        }
        return json(choices.front().value);
    };
    binding.show = [choices](const json& value) {
        QRadioButton* chosen = choices.front().button;
        for (const Choice& choice : choices) {
            if (value.is_string() && value.get<std::string>() == choice.value) {
                chosen = choice.button;
            }
        }
        chosen->setChecked(true);
    };
    binding.store = [setting = std::string(key)](const json& value) {
        core::SettingsManager::getInstance().set<std::string>(setting, value.get<std::string>());
    };
    bindCustom(std::move(binding));
}

void MaterialsPage::addOpening() {
    //: The group of the page Materials: how the materials open. In Polish: Otwieranie
    QFormLayout* opening = addGroup(tr("Opening"));

    //: In Polish: Na karcie w programie
    auto* pictureCard = new QRadioButton(tr("On a tab of the program"));
    pictureCard->setObjectName(QStringLiteral("materialsOpenPicturesCard"));
    //: In Polish: W programie systemowym
    auto* pictureSystem = new QRadioButton(tr("In the system program"));
    pictureSystem->setObjectName(QStringLiteral("materialsOpenPicturesSystem"));
    //: In Polish: Obraz, także kopia obrazu z sieci, otwiera się na karcie podglądu, jak
    //: rozdział. Plik, którego program nie pokazuje, otwiera się w programie systemowym,
    //: a materiał bez pliku i adresu – na swojej karcie.
    const QString pictureCardNote =
        tr("A picture, also the copy of one from the web, opens on a preview tab, as a chapter "
           "does. A file the program does not show opens in the system program, and a material "
           "without a file and an address on its card.");
    //: In Polish: Kartę podglądu otwiera wtedy przycisk „Podgląd” we Właściwościach.
    const QString pictureSystemNote =
        tr("The Preview button in the Properties panel then opens the preview tab.");
    //: In Polish: Obrazy:
    addChoices(opening, tr("Pictures:"), NavigatorPanel::OPEN_MATERIALS_SETTING,
               {{CARD, pictureCard, pictureCardNote}, {SYSTEM, pictureSystem, pictureSystemNote}});

    //: In Polish: W przeglądarce
    auto* pagesBrowser = new QRadioButton(tr("In the browser"));
    pagesBrowser->setObjectName(QStringLiteral("materialsOpenPagesBrowser"));
    //: In Polish: Na karcie materiału
    auto* pagesCard = new QRadioButton(tr("On the card of the material"));
    pagesCard->setObjectName(QStringLiteral("materialsOpenPagesCard"));
    //: In Polish: Dwuklik i Enter otwierają adres w przeglądarce systemowej. Kartę materiału
    //: na środku okna otwiera polecenie „Otwórz kartę” w menu węzła.
    const QString pagesBrowserNote =
        tr("Double-click and Enter open the address in the system's browser. Open Card in the "
           "menu of the node opens the card of the material in the middle of the window.");
    //: In Polish: Dwuklik i Enter otwierają kartę materiału na środku okna, a adres – przycisk
    //: „Otwórz w przeglądarce” na tej karcie.
    const QString pagesCardNote =
        tr("Double-click and Enter open the card of the material in the middle of the window; "
           "its Open in Browser button opens the address.");
    //: In Polish: Strony i obrazy z sieci:
    addChoices(opening, tr("Pages and pictures on the web:"), NavigatorPanel::OPEN_PAGES_SETTING,
               {{BROWSER, pagesBrowser, pagesBrowserNote}, {CARD, pagesCard, pagesCardNote}});

    const bool preview = PdfFiles::previewAvailable();
    //: In Polish: Na karcie podglądu w programie
    m_openPdfPreview = new QRadioButton(tr("On a preview tab of the program"));
    m_openPdfPreview->setObjectName(QStringLiteral("materialsOpenPdfPreview"));
    m_openPdfPreview->setEnabled(preview);
    auto* pdfSystem = new QRadioButton(tr("In the system program"));
    pdfSystem->setObjectName(QStringLiteral("materialsOpenPdfSystem"));
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
    addChoices(opening, tr("PDF files:"), PdfFiles::OPEN_SETTING,
               {{PdfFiles::PREVIEW, m_openPdfPreview, previewNote},
                {PdfFiles::SYSTEM, pdfSystem, systemNote}});
}

void MaterialsPage::addPdfPreview() {
    //: The group of the page Materials. In Polish: Podgląd PDF
    QFormLayout* pdf = addGroup(tr("PDF Preview"));
    // Without the preview of PDF files its settings wait
    pdf->parentWidget()->setEnabled(PdfFiles::previewAvailable());

    //: In Polish: Jedna pod drugą
    auto* continuous = new QRadioButton(tr("One under another"));
    continuous->setObjectName(QStringLiteral("materialsPdfContinuous"));
    //: In Polish: Po jednej stronie
    auto* single = new QRadioButton(tr("One page at a time"));
    single->setObjectName(QStringLiteral("materialsPdfSingle"));
    //: In Polish: Dwie obok siebie
    auto* two = new QRadioButton(tr("Two side by side"));
    two->setObjectName(QStringLiteral("materialsPdfTwo"));
    //: In Polish: Przyciski na pasku podglądu zmieniają to samo ustawienie.
    const QString layoutNote = tr("The buttons on the bar of the preview change the same setting.");
    //: In Polish: Układ stron:
    addChoices(pdf, tr("Page layout:"), PdfFiles::LAYOUT_SETTING,
               {{PdfFiles::CONTINUOUS, continuous, QString()},
                {PdfFiles::SINGLE, single, QString()},
                {PdfFiles::TWO, two, layoutNote}});

    //: In Polish: Strony jak w pliku
    auto* asFile = new QRadioButton(tr("Pages as in the file"));
    asFile->setObjectName(QStringLiteral("materialsPdfAsFile"));
    //: In Polish: Strony przyciemnione
    auto* dimmed = new QRadioButton(tr("Pages dimmed"));
    dimmed->setObjectName(QStringLiteral("materialsPdfDimmed"));
    //: In Polish: Odwrócone kolory
    auto* inverted = new QRadioButton(tr("Inverted colors"));
    inverted->setObjectName(QStringLiteral("materialsPdfInverted"));
    //: Under Inverted colors, the last choice of In a dark theme. In Polish: Jasny tekst na
    //: ciemnym tle. Zmienia się tylko wygląd na ekranie; plik zostaje bez zmian.
    const QString darkNote = tr("Light text on a dark page. Only the look on the screen changes; "
                                "the file stays as it is.");
    //: In Polish: W ciemnym motywie:
    addChoices(pdf, tr("In a dark theme:"), PdfFiles::DARK_SETTING,
               {{PdfFiles::AS_FILE, asFile, QString()},
                {PdfFiles::DIMMED, dimmed, QString()},
                {PdfFiles::INVERTED, inverted, darkNote}});

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
    addGap(pdf);

    //: In Polish: Otwieraj na stronie oglądanej ostatnio
    QCheckBox* lastPage =
        addCheckBox(pdf, tr("Open at the page shown last"), PdfFiles::LAST_PAGE_SETTING);
    lastPage->setObjectName(QStringLiteral("materialsPdfLastPage"));

    // The colors of the preview are those of the editor, on another page
    auto* colors = new QHBoxLayout();
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
    pdf->addRow(colors);
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
}

}  // namespace kalahari::gui
