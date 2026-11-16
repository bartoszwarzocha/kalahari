// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file import_page.cpp
/// @brief Settings page: Files > Import
///
/// The choices the window of File > Import > DOCX, Markdown and Plain Text starts with: the
/// texts of more files, the formatting, the comments and the tracked changes of Word, and the
/// headings, the dialogue and the cleanups of each format. The window keeps the writer's
/// choices in the same settings.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QWidget>

#include <nlohmann/json.hpp>

#include <array>
#include <string>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// The formats of the import, as the settings name them
constexpr std::array<const char*, 3> FORMAT_KEYS{"docx", "markdown", "text"};

}  // namespace

ImportPage::ImportPage(QWidget* parent)
    : SettingsPage(parent)
{
    //: A group of Settings > Files > Import. In Polish: Okno importu
    QFormLayout* window = addGroup(tr("Window of the Import"));
    //: In Polish: Polecenia Plik › Importuj › Dokument DOCX, Markdown i Zwykły tekst zaczynają
    //: od tych ustawień. To, co wybierzesz w oknie importu, zostaje tutaj na następny import.
    addNote(window, tr("File › Import › DOCX Document, Markdown and Plain Text start with these "
                       "choices. What you choose in the window of the import stays here for the "
                       "next import."));
    //: In Polish: &Kilka plików: każdy plik jako jeden tekst
    QCheckBox* whole =
        addCheckBox(window, tr("&More files: each file as one text"), "import.wholeFiles");
    whole->setObjectName(QStringLiteral("importWholeFiles"));

    //: A group of Settings > Files > Import. In Polish: Dokumenty Worda
    QFormLayout* word = addGroup(tr("Word Documents"));
    auto* formatting = new QComboBox();
    formatting->setObjectName(QStringLiteral("importFormatting"));
    //: How much of the formatting of the file the texts keep. In Polish: całe formatowanie
    formatting->addItem(tr("all the formatting"), QStringLiteral("all"));
    //: In Polish: wyróżnienia i wyrównanie
    formatting->addItem(tr("emphasis and alignment"), QStringLiteral("emphasis"));
    //: In Polish: tylko style
    formatting->addItem(tr("only the styles"), QStringLiteral("styles"));
    //: In Polish: Formatowanie z &pliku:
    addField(word, tr("Fo&rmatting of the file:"), formatting, "import.docx.formatting");
    //: In Polish: Wyróżnienia to kursywa, pogrubienie, podkreślenie i przekreślenie; resztę
    //: wyglądu dają style książki.
    addNote(word, tr("The emphasis is italics, bold, underline and strikethrough; the styles of "
                     "the book give the rest of the look."));
    //: In Polish: Komentarze Worda jako &uwagi
    QCheckBox* comments =
        addCheckBox(word, tr("Word &comments as annotations"), "import.docx.comments");
    comments->setObjectName(QStringLiteral("importComments"));

    // The tracked changes: a choice of two words for a flag
    auto* changes = new QComboBox();
    changes->setObjectName(QStringLiteral("importChanges"));
    //: The tracked changes of Word. In Polish: przyjmij wszystkie
    changes->addItem(tr("accept all"), false);
    //: In Polish: odrzuć wszystkie
    changes->addItem(tr("reject all"), true);
    //: In Polish: &Zmiany śledzone:
    addField(word, tr("&Tracked changes:"), changes, "import.docx.rejectChanges");
    Binding& binding = lastBinding();
    binding.shown = [changes]() { return json(changes->currentData().toBool()); };
    binding.show = [changes](const json& value) {
        changes->setCurrentIndex(value.is_boolean() && value.get<bool>() ? 1 : 0);
    };

    addFormats();
    pageLayout()->addStretch();
}

void ImportPage::addFormats()
{
    //: A group of Settings > Files > Import, as the tabs of the window of the import. In
    //: Polish: Podział, style i porządki
    QFormLayout* group = addGroup(tr("Division, Styles and Cleanups"));

    // One format at a time: the same options of each, so the page stays narrow
    auto* format = new QComboBox();
    format->setObjectName(QStringLiteral("importFormat"));
    //: The formats of the import. In Polish: dokumenty Worda
    format->addItem(tr("Word documents"));
    //: In Polish: Markdown
    format->addItem(tr("Markdown"));
    //: In Polish: zwykły tekst
    format->addItem(tr("plain text"));
    //: The format whose options the page shows. In Polish: &Format:
    auto* formatLabel = new QLabel(tr("&Format:"));
    formatLabel->setBuddy(format);
    group->addRow(formatLabel, format);

    auto* stack = new QStackedWidget();
    stack->setObjectName(QStringLiteral("importFormats"));
    group->addRow(stack);
    connect(format, &QComboBox::currentIndexChanged, stack, &QStackedWidget::setCurrentIndex);

    for (const char* key : FORMAT_KEYS) {
        auto* options = new QWidget();
        auto* form = new QFormLayout(options);
        form->setContentsMargins(0, 0, 0, 0);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        stack->addWidget(options);

        const std::string prefix = std::string("import.") + key + ".";
        const auto named = [key](QWidget* widget, const char* option) {
            widget->setObjectName(
                QStringLiteral("import_%1_%2").arg(QLatin1String(key), QLatin1String(option)));
        };
        //: As the tab Division of the window of the import. In Polish: &Nagłówek jako tytuł,
        //: nie w tekście
        named(addCheckBox(form, tr("&Heading as the title, not in the text"),
                          prefix + "headingAsTitle"),
              "headingAsTitle");
        //: As the tab Styles of the window of the import; when the book has the style. In
        //: Polish: Akapity zaczynające się pauzą w stylu Dia&log
        named(addCheckBox(form, tr("Paragraphs with a dash in the style Dia&logue"),
                          prefix + "dialogueStyle"),
              "dialogueStyle");
        //: The cleanups. In Polish: Typograficzne &cudzysłowy i pauzy
        named(addCheckBox(form, tr("Typographic &quotation marks and dashes"),
                          prefix + "typography"),
              "typography");
        //: In Polish: Po&dwójne spacje jako jedna
        named(addCheckBox(form, tr("&Double spaces as one"), prefix + "doubleSpaces"),
              "doubleSpaces");
        //: Spaces and tabs. In Polish: Bez spacji na począ&tku akapitu
        named(addCheckBox(form, tr("No &spaces at the start of a paragraph"),
                          prefix + "leadingSpaces"),
              "leadingSpaces");
        //: In Polish: „* * *” i podobne jako podział &sceny
        named(addCheckBox(form, tr("\"* * *\" and the like as scene &breaks"),
                          prefix + "sceneMarkers"),
              "sceneMarkers");

        auto* empty = new QComboBox();
        named(empty, "emptyParagraphs");
        //: What the empty paragraphs become. In Polish: usuń
        empty->addItem(tr("remove"), QStringLiteral("remove"));
        //: In Polish: zostaw
        empty->addItem(tr("keep"), QStringLiteral("keep"));
        //: In Polish: podział sceny
        empty->addItem(tr("scene break"), QStringLiteral("sceneBreak"));
        //: In Polish: Pust&e akapity:
        addField(form, tr("&Empty paragraphs:"), empty, prefix + "emptyParagraphs");
    }

    //: Under the cleanups. In Polish: Cudzysłowy, apostrofy, pauzy i wielokropki zależą od
    //: języka książki i Ustawień › Edytor › Autokorekta. Podziałem sceny stają się akapity
    //: „* * *”, „***”, „#”, „⁂” i „❦”. Styl Dialog działa tylko w książce, która go ma.
    //: Porządki nie zmieniają akapitów w stylu Kod.
    addNote(group, tr("The quotation marks, apostrophes, dashes and ellipses depend on the "
                      "language of the book and Settings › Editor › Auto-correct. The paragraphs "
                      "\"* * *\", \"***\", \"#\", \"⁂\" and \"❦\" become scene breaks. The style "
                      "Dialogue is used only in a book that has it. The cleanups do not change "
                      "the paragraphs in the style Code."));
}

}  // namespace gui
}  // namespace kalahari
