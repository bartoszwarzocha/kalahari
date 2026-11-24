// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file tables_page.cpp
/// @brief Settings > Editor > Tables: the room new tables keep from the text, and where the
///        caption of a table goes

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QComboBox>
#include <QFormLayout>

namespace kalahari::gui {

EditorTablesPage::EditorTablesPage(QWidget* parent)
    : SettingsPage(parent)
{
    // The room a new table keeps from the text (Table Properties, Table)
    //: Settings > Editor > Tables. In Polish: Nowa tabela
    QFormLayout* table = addGroup(tr("New Table"));
    const auto space = [](const char* name) {
        auto* field = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_TABLE_SPACE);
        field->setObjectName(QLatin1String(name));
        return field;
    };
    //: The room between a new table (or its caption above it) and the text above it. In
    //: Polish: Odstęp &górny:
    addField(table, tr("Space abov&e:"), space("tableSpaceAbove"), TABLE_SPACE_ABOVE_KEY);
    //: In Polish: Odstęp do&lny:
    addField(table, tr("S&pace below:"), space("tableSpaceBelow"), TABLE_SPACE_BELOW_KEY);
    //: In Polish: Tekst nad tabelą i pod nią zachowuje ten odstęp, więc nie trzeba tam pustych
    //: akapitów. Tabela, która już jest w tekście, ma własne odstępy: Tabela > Właściwości
    //: tabeli..., karta „Tabela”.
    addNote(table, tr("The text above and below a table keeps this room from it, so no empty "
                      "paragraph is needed there. A table already in the text has its own room: "
                      "Table > Table Properties..., the Table tab."));

    // Where Insert Caption puts the caption of a table
    //: In Polish: Podpis tabeli
    QFormLayout* caption = addGroup(tr("Caption of a Table"));
    auto* position = new QComboBox();
    position->setObjectName(QStringLiteral("tableCaptionPosition"));
    //: In Polish: Nad tabelą
    position->addItem(tr("Above the table"), QStringLiteral("above"));
    //: In Polish: Pod tabelą
    position->addItem(tr("Below the table"), QStringLiteral("below"));
    //: In Polish: &Położenie:
    addField(caption, tr("P&osition:"), position, TABLE_CAPTION_POSITION_KEY);
    //: %1: the label of the captions of tables. In Polish: Tabela > Wstaw podpis... oraz Wstaw >
    //: Podpis... z kursorem w tabeli dodają do niej numerowany podpis z etykietą „%1”, tak jak do
    //: obrazów; w oknie podpisu można go przenieść na drugą stronę tabeli.
    addNote(caption, tr("Table > Insert Caption... and Insert > Caption... with the cursor in a "
                        "table give it a numbered caption with the label \"%1\", as images have; "
                        "the window of the caption can put it on the other side.")
                         .arg(defaultTableCaptionLabel()));
}

}  // namespace kalahari::gui
