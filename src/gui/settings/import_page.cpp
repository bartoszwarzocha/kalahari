// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file import_page.cpp
/// @brief Settings page: Files > Import
///
/// The choices the window of File > Import > DOCX, Markdown, Plain Text and Scrivener Project
/// starts with: the texts of more files, the images of Internet addresses, the formatting, the
/// comments and the tracked changes of Word, what of a project of Scrivener goes to the book,
/// and the headings, the dialogue and the cleanups of each format. The window keeps the writer's
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
constexpr std::array<const char*, 4> FORMAT_KEYS{"docx", "markdown", "text", "scrivener"};

/// The format whose texts are the documents of its binder: no heading is their title
constexpr const char* PROJECT_KEY = "scrivener";

}  // namespace

ImportPage::ImportPage(QWidget* parent)
    : SettingsPage(parent)
{
    //: A group of Settings > Files > Import. In Polish: Okno importu
    QFormLayout* window = addGroup(tr("Window of the Import"));
    //: In Polish: Polecenia Plik › Importuj › Dokument DOCX, Markdown, Zwykły tekst i Projekt
    //: Scrivenera zaczynają od tych ustawień. To, co wybierzesz w oknie importu, zostaje tutaj
    //: na następny import.
    addNote(window, tr("File › Import › DOCX Document, Markdown, Plain Text and Scrivener "
                       "Project start with these choices. What you choose in the window of the "
                       "import stays here for the next import."));
    //: In Polish: &Kilka plików: każdy plik jako jeden tekst
    QCheckBox* whole =
        addCheckBox(window, tr("&More files: each file as one text"), "import.wholeFiles");
    whole->setObjectName(QStringLiteral("importWholeFiles"));
    //: In Polish: Pobierz obrazy z adresów &internetowych
    QCheckBox* web = addCheckBox(window, tr("Download the &images of Internet addresses"),
                                 "import.webImages");
    web->setObjectName(QStringLiteral("importWebImages"));
    //: In Polish: Obrazy, które pliki Markdown i dokumenty Worda wskazują adresem
    //: internetowym; program łączy się z tymi adresami dopiero wtedy, gdy import się zacznie.
    //: Bez tej opcji w miejscu obrazów zostaje ich opis.
    addNote(window, tr("The images that Markdown files and Word documents give by Internet "
                       "addresses; the program connects to the addresses only when the import "
                       "starts. Without it, the descriptions of the images stay in their places."));

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

    addProject();
    addFormats();
    pageLayout()->addStretch();
}

void ImportPage::addProject()
{
    //: A group of Settings > Files > Import. In Polish: Projekty Scrivenera
    QFormLayout* project = addGroup(tr("Scrivener Projects"));
    const auto named = [](QWidget* widget, const char* option) {
        widget->setObjectName(QStringLiteral("import_scrivener_%1").arg(QLatin1String(option)));
    };
    //: In Polish: Tylko to, co Scrivener &bierze do kompilacji
    named(addCheckBox(project, tr("&Only what Scrivener compiles"), "import.scrivener.compiledOnly"),
          "compiledOnly");
    auto* scenes = new QComboBox();
    named(scenes, "scenes");
    //: What divides the scenes of a text. In Polish: podział sceny
    scenes->addItem(tr("a scene break"), QStringLiteral("breaks"));
    //: In Polish: tytuł sceny jako nagłówek
    scenes->addItem(tr("the title of the scene as a heading"), QStringLiteral("titles"));
    //: In Polish: nic (tekst ciągły)
    scenes->addItem(tr("nothing (one text)"), QStringLiteral("none"));
    //: In Polish: Sceny tekstu oddziel&a:
    addField(project, tr("Scenes divided b&y:"), scenes, "import.scrivener.scenes");
    auto* formatting = new QComboBox();
    named(formatting, "formatting");
    //: How much of the formatting of the documents the texts keep. In Polish: całe formatowanie
    formatting->addItem(tr("all the formatting"), QStringLiteral("all"));
    //: In Polish: wyróżnienia i wyrównanie
    formatting->addItem(tr("emphasis and alignment"), QStringLiteral("emphasis"));
    //: In Polish: tylko style
    formatting->addItem(tr("only the styles"), QStringLiteral("styles"));
    //: In Polish: Formatowanie z pro&jektu:
    addField(project, tr("Formatting of the pro&ject:"), formatting, "import.scrivener.formatting");
    //: In Polish: Streszczenia na p&oczątku notatek
    named(addCheckBox(project, tr("Syno&pses at the start of the notes"), "import.scrivener.synopses"),
          "synopses");
    //: In Polish: Notatki doku&mentów jako notatki
    named(addCheckBox(project, tr("&Notes of the documents as notes"), "import.scrivener.notes"),
          "notes");
    //: In Polish: Komentarze i adnotacje jako uwa&gi
    named(addCheckBox(project, tr("Comments and inline &annotations as annotations"),
                      "import.scrivener.comments"),
          "comments");
    //: In Polish: Migawki jako &wersje tekstów
    named(addCheckBox(project, tr("Snapshots as &versions of the texts"),
                      "import.scrivener.snapshots"),
          "snapshots");
    //: In Polish: Folder „&Research” do Warsztatu
    named(addCheckBox(project, tr("The folder \"Research\" to the &Workshop"),
                      "import.scrivener.research"),
          "research");
    //: Under the options of a project of Scrivener. In Polish: Stany Scrivenera stają się
    //: stanami tekstów, a etykiety i słowa kluczowe trafiają do ich notatek.
    addNote(project, tr("The statuses of Scrivener become the statuses of the texts, and the "
                        "labels and the keywords go to their notes."));
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
    //: In Polish: projekty Scrivenera
    format->addItem(tr("Scrivener projects"));
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
        // The texts of a project are the documents of its binder, with their titles
        if (std::string(key) != PROJECT_KEY) {
            //: As the tab Division of the window of the import. In Polish: &Nagłówek jako
            //: tytuł, nie w tekście
            named(addCheckBox(form, tr("&Heading as the title, not in the text"),
                              prefix + "headingAsTitle"),
                  "headingAsTitle");
        }
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
