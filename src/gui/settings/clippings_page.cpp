// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clippings_page.cpp
/// @brief Settings page: Editor > Clippings
///
/// Cut to Clippings and Insert > Clipping from the Workshop read these settings each time
/// (ClippingsCoordinator), the scissors beside the paragraphs follow theirs at once
/// (ParagraphMarksCoordinator); the counting of the words of a cut is on Statistics and Goals >
/// Counting.

#include "kalahari/core/clipping.h"
#include "kalahari/gui/clippings_coordinator.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace kalahari::gui {

namespace {

/// @brief The fewest characters a list of choices shows
constexpr int MIN_CHOICE_CHARACTERS = 14;

/// @brief The keys of command @p commandId as the writer sees them; empty: none
QString keysOf(const CommandRegistry& commands, const char* commandId) {
    const Command* command = commands.getCommand(commandId);
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QString();
}

/// @brief The place of command @p commandId in the menu, with its keys when it has them
QString withKeys(const QString& menu, const CommandRegistry& commands, const char* commandId) {
    const QString keys = keysOf(commands, commandId);
    return keys.isEmpty() ? menu : menu + QStringLiteral(", ") + keys;
}

}  // namespace

EditorClippingsPage::EditorClippingsPage(CommandRegistry& commands, QWidget* parent)
    : SettingsPage(parent)
    , m_commands(commands)
{
    using core::Clippings;

    // What they are, with the keys the commands have now (the user may change them)
    QLabel* intro = addNote(pageLayout(), QString());
    intro->setObjectName(QStringLiteral("clippingsIntro"));
    const auto showIntro = [this, intro]() {
        //: The top of Settings > Editor > Clippings. %1, %2: the commands in the menus, with
        //: their keys ("Edit > Cut to Clippings, Ctrl+Shift+X"). In Polish: Wycinki to
        //: fragmenty wycięte z tekstu do Warsztatu (%1). Można do nich wrócić: wstawić w
        //: dowolne miejsce (%2) albo przywrócić tam, skąd pochodzą.
        intro->setText(tr("Clippings are fragments cut from the text to the Workshop (%1). They "
                          "can come back: inserted anywhere (%2) or restored where they came "
                          "from.")
                           .arg(withKeys(
                                    //: In Polish: Edycja > Wytnij do wycinków
                                    tr("Edit > Cut to Clippings"), m_commands,
                                    ClippingsCoordinator::CUT_COMMAND),
                                withKeys(
                                    //: In Polish: Wstaw > Wycinek z Warsztatu
                                    tr("Insert > Clipping from the Workshop"), m_commands,
                                    ClippingsCoordinator::INSERT_COMMAND)));
    };
    connect(&m_commands, &CommandRegistry::shortcutsChanged, intro, showIntro);
    whenLoaded(showIntro);

    // A list of the choices of setting @p key, bound to it (the first one when it has none of
    // them), narrow enough for the narrowest window
    const auto addList = [this](QFormLayout* form, const QString& label, const char* key,
                                const std::vector<Choice>& choices, const char* name) {
        QComboBox* list = addChoice(form, label, choices, std::string());
        list->setObjectName(QLatin1String(name));
        list->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        list->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
        bindChoice(list, key, choices.front().value.toString().toStdString());
    };

    // Cutting: at once or with the small window; without a selection
    //: A group of Settings > Editor > Clippings. In Polish: Wycinanie
    QFormLayout* cutting = addGroup(tr("Cutting"));
    addList(cutting,
            //: Followed by what Edit > Cut to Clippings does. In Polish: &Polecenie:
            tr("&The command:"), Clippings::CUT_SETTING,
            //: In Polish: wycina od razu / Tytuł to pierwsze słowa fragmentu; można go zmienić
            //: później, jak tytuł każdego tekstu (F2 w Nawigatorze).
            {{tr("cuts at once"), QLatin1String(Clippings::CUT_AT_ONCE),
              tr("The title is the first words of the fragment; it can be changed later, as "
                 "that of any text (F2 in the Navigator).")},
             //: In Polish: najpierw otwiera małe okno / Z tytułem i notatką wycinka.
             {tr("first opens a small window"), QLatin1String(Clippings::CUT_WITH_WINDOW),
              tr("With the title and a note of the clipping.")}},
            "clippingsCut");
    addList(cutting,
            //: Followed by what the command does. In Polish: &Bez zaznaczenia polecenie:
            tr("&Without a selection the command:"), Clippings::NO_SELECTION_SETTING,
            //: In Polish: wycina akapit z kursorem
            {{tr("cuts the paragraph with the cursor"),
              QLatin1String(Clippings::NO_SELECTION_PARAGRAPH), {}},
             //: In Polish: jest nieaktywne
             {tr("is greyed out"), QLatin1String(Clippings::NO_SELECTION_NOTHING), {}}},
            "clippingsNoSelection");

    // Inserting: the window or the list; the clipping inserted
    //: A group of Settings > Editor > Clippings. In Polish: Wstawianie
    QFormLayout* inserting = addGroup(tr("Inserting"));
    addList(inserting,
            //: Followed by what Insert > Clipping from the Workshop opens. In Polish: Polecenie
            //: &otwiera:
            tr("The command &opens:"), Clippings::INSERT_SETTING,
            //: In Polish: okno z listą i podglądem
            {{tr("a window with the list and the preview"),
              QLatin1String(Clippings::INSERT_WINDOW), {}},
             //: In Polish: listę przy kursorze / Jak szybkie wstawianie, bez podglądu.
             {tr("a list at the cursor"), QLatin1String(Clippings::INSERT_LIST),
              tr("As quick insert does, without the preview.")}},
            "clippingsInsert");
    addList(inserting,
            //: Followed by what happens to it. In Polish: Wstawiony &wycinek:
            tr("A clipping &inserted:"), Clippings::AFTER_INSERT_SETTING,
            //: In Polish: zostaje w Warsztacie / Oznaczony jako wstawiony.
            {{tr("stays in the Workshop"), QLatin1String(Clippings::AFTER_INSERT_KEEP),
              tr("Marked as inserted.")},
             //: In Polish: znika z Warsztatu / Cofnięcie usuwa wstawiony tekst, a wycinek wraca
             //: do Warsztatu.
             {tr("goes from the Workshop"), QLatin1String(Clippings::AFTER_INSERT_REMOVE),
              tr("Undo takes the insertion back, and the clipping is in the Workshop again.")}},
            "clippingsAfterInsert");

    // In the text: the scissors beside the paragraphs clippings were cut from
    //: A group of Settings > Editor > Clippings. In Polish: W tekście
    QFormLayout* inText = addGroup(tr("In the Text"));
    //: In Polish: &Rysuj nożyczki w miejscach wycięć
    QCheckBox* marks = addCheckBox(
        inText, tr("&Draw scissors where a fragment was cut"), Clippings::MARKS_SETTING);
    marks->setObjectName(QStringLiteral("clippingsMarks"));
    //: Under "Draw scissors". In Polish: Jak spinacze materiałów: kliknięcie
    //: albo menu akapitu pokazuje wycinek albo przywraca go w tym miejscu. Rysowane w kolorze
    //: tekstu.
    addOptionNote(inText, marks,
                  tr("As the paperclips of the materials: a click or the menu of the paragraph "
                     "shows the clipping or restores it there. Drawn in the color of the text."));

    //: In Polish: Czy wycięcie zmniejsza słowa dnia, ustawia się w Statystyki i cele > Liczenie.
    addNote(pageLayout(), tr("Whether a cut lowers the words of the day is set in Statistics "
                             "and Goals > Counting."));
    pageLayout()->addStretch();
}

}  // namespace kalahari::gui
