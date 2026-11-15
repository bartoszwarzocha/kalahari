// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file clippings_page.cpp
/// @brief Settings page: Editor > Clippings
///
/// Cut to Clippings and Insert > Clipping from the Workshop read these settings each time
/// (ClippingsCoordinator); the counting of the words of a cut is on Statistics and Goals >
/// Counting.

#include "kalahari/core/clipping.h"
#include "kalahari/gui/clippings_coordinator.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#include <vector>

namespace kalahari::gui {

namespace {

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

    // Cutting: at once or with the small window; without a selection
    //: A group of Settings > Editor > Clippings. In Polish: Wycinanie
    QFormLayout* cutting = addGroup(tr("Cutting"));
    std::vector<QRadioButton*> cut = addChoices(
        cutting, QString(), Clippings::CUT_SETTING,
        //: In Polish: &Wycinaj od razu / Tytuł to pierwsze słowa fragmentu; można go zmienić
        //: później, jak tytuł każdego tekstu (F2 w Nawigatorze).
        {{tr("Cut at &once"), Clippings::CUT_AT_ONCE,
          tr("The title is the first words of the fragment; it can be changed later, as that "
             "of any text (F2 in the Navigator).")},
         //: In Polish: Najpierw &małe okno / Z tytułem i notatką wycinka.
         {tr("First a s&mall window"), Clippings::CUT_WITH_WINDOW,
          tr("With the title and a note of the clipping.")}});
    cut.front()->setObjectName(QStringLiteral("clippingsCutAtOnce"));
    cut.back()->setObjectName(QStringLiteral("clippingsCutWithWindow"));
    std::vector<QRadioButton*> noSelection = addChoices(
        cutting,
        //: Followed by what the command does. In Polish: Bez zaznaczenia polecenie:
        tr("Without a selection the command:"), Clippings::NO_SELECTION_SETTING,
        //: In Polish: wycina &akapit z kursorem
        {{tr("cuts the &paragraph with the cursor"), Clippings::NO_SELECTION_PARAGRAPH, {}},
         //: In Polish: jest &nieaktywne
         {tr("is &greyed out"), Clippings::NO_SELECTION_NOTHING, {}}});
    noSelection.front()->setObjectName(QStringLiteral("clippingsNoSelectionParagraph"));
    noSelection.back()->setObjectName(QStringLiteral("clippingsNoSelectionNothing"));

    // Inserting: the window or the list; the clipping inserted
    //: A group of Settings > Editor > Clippings. In Polish: Wstawianie
    QFormLayout* inserting = addGroup(tr("Inserting"));
    std::vector<QRadioButton*> insert = addChoices(
        inserting,
        //: Followed by what it opens. In Polish: Polecenie otwiera:
        tr("The command opens:"), Clippings::INSERT_SETTING,
        //: In Polish: &okno z listą i podglądem
        {{tr("a &window with the list and the preview"), Clippings::INSERT_WINDOW, {}},
         //: In Polish: &listę przy kursorze / Jak szybkie wstawianie, bez podglądu.
         {tr("a &list at the cursor"), Clippings::INSERT_LIST,
          tr("As quick insert does, without the preview.")}});
    insert.front()->setObjectName(QStringLiteral("clippingsInsertWindow"));
    insert.back()->setObjectName(QStringLiteral("clippingsInsertList"));
    std::vector<QRadioButton*> afterInsert = addChoices(
        inserting,
        //: Followed by what happens to it. In Polish: Wstawiony wycinek:
        tr("A clipping inserted:"), Clippings::AFTER_INSERT_SETTING,
        //: In Polish: &zostaje w Warsztacie / Oznaczony jako wstawiony.
        {{tr("&stays in the Workshop"), Clippings::AFTER_INSERT_KEEP,
          tr("Marked as inserted.")},
         //: In Polish: zni&ka z Warsztatu / Cofnięcie usuwa wstawiony tekst, a wycinek wraca
         //: do Warsztatu.
         {tr("goes from the Wor&kshop"), Clippings::AFTER_INSERT_REMOVE,
          tr("Undo takes the insertion back, and the clipping is in the Workshop again.")}});
    afterInsert.front()->setObjectName(QStringLiteral("clippingsAfterInsertKeep"));
    afterInsert.back()->setObjectName(QStringLiteral("clippingsAfterInsertRemove"));

    //: In Polish: Czy wycięcie zmniejsza słowa dnia, ustawia się w Statystyki i cele > Liczenie.
    addNote(pageLayout(), tr("Whether a cut lowers the words of the day is set in Statistics "
                             "and Goals > Counting."));
    pageLayout()->addStretch();
}

}  // namespace kalahari::gui
