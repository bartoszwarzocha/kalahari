// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_page.cpp
/// @brief Settings page: Plot Board
///
/// The view a new tab of the board opens on, the layout and the size of the cards, the scenes
/// shown and which scenes there are: the scenes of the texts and those in their plans (the
/// default), the scenes of the texts only, or those in the plans only
/// (PlotBoard::SCENES_VARIANT_SETTING); what the cards show; what colors them, as a strip at
/// their top or as their background, and what is under them; the columns of the outline; the
/// front and back sections shown as a preview; what happens to the numbers in the titles after
/// a move (PlotBoard::RENUMBER_SETTING). The board follows the settings at once. The colors
/// are those of the theme: kept for each theme, as the other colors of the editor.

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/settings_schema.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::gui {

namespace {

constexpr int CHOICE_SPACING = 12;  ///< Between the check boxes of a row
constexpr int CHOICE_LINES = 4;     ///< Between their lines, when they wrap
constexpr int MIN_CHOICE_CHARACTERS = 14;  ///< The fewest characters a list of choices shows

/// @brief A row of a form for check boxes, wrapping when it is narrow
FlowLayout* rowLayout(QWidget* row) {
    auto* flow = new FlowLayout(row, CHOICE_SPACING, CHOICE_LINES);
    flow->setContentsMargins(0, 0, 0, 0);
    return flow;
}

/// @brief Choose the item of @p list with value @p value of setting @p key; a value the
/// program does not know shows as the default of the setting
void choose(QComboBox* list, const std::string& value, const char* key) {
    int index = list->findData(QString::fromStdString(value));
    if (index < 0) {
        const nlohmann::json* fallback = core::settings_schema::defaultValue(key);
        index = fallback != nullptr && fallback->is_string()
                    ? list->findData(QString::fromStdString(fallback->get<std::string>()))
                    : 0;
    }
    list->setCurrentIndex(std::max(0, index));
}

/// @brief The name of what a card and the outline show, in a row of check boxes
QString fieldName(const char* field) {
    const std::string name(field);
    if (name == PlotBoard::FIELD_DESCRIPTION) {
        //: A field of the cards and a column of the outline in Settings > Plot Board. In
        //: Polish: opis
        return PlotBoardPage::tr("description");
    }
    if (name == PlotBoard::FIELD_POV) {
        //: In Polish: punkt widzenia
        return PlotBoardPage::tr("point of view");
    }
    if (name == PlotBoard::FIELD_PLACE) {
        //: In Polish: miejsce
        return PlotBoardPage::tr("place");
    }
    if (name == PlotBoard::FIELD_TIME) {
        //: In Polish: czas akcji
        return PlotBoardPage::tr("time of the action");
    }
    //: In Polish: stan
    return PlotBoardPage::tr("state");
}

}  // namespace

PlotBoardPage::PlotBoardPage(QWidget* parent) : SettingsPage(parent) {
    addViewGroup();
    addScenesGroup();
    addCardsGroup();
    addColorsGroup();
    addOutlineGroup();
    addNumbersGroup();
    pageLayout()->addStretch();
}

QComboBox* PlotBoardPage::addChoices(QFormLayout* form, const QString& label, const char* key,
                                     const std::vector<Choice>& choices) {
    QComboBox* list = addChoice(form, label, choices, std::string());
    list->setObjectName(QLatin1String(key));
    // Its longest choice does not widen the page in a narrow window
    list->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    list->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);

    auto& settings = core::SettingsManager::getInstance();
    Binding binding;
    binding.key = [key]() { return std::string(key); };
    binding.stored = [&settings, key]() {
        return nlohmann::json(settings.get<std::string>(key, std::string()));
    };
    binding.shown = [list]() {
        return nlohmann::json(list->currentData().toString().toStdString());
    };
    binding.show = [list, key](const nlohmann::json& value) {
        choose(list, value.is_string() ? value.get<std::string>() : std::string(), key);
    };
    binding.store = [&settings, key](const nlohmann::json& value) {
        settings.set<std::string>(key, value.get<std::string>());
    };
    bindCustom(std::move(binding));
    return list;
}

void PlotBoardPage::addChecks(QFormLayout* form, const QString& label,
                              const std::vector<std::pair<QString, std::string>>& checks) {
    auto* row = new QWidget();
    FlowLayout* flow = rowLayout(row);
    QCheckBox* first = nullptr;
    for (const auto& [text, key] : checks) {
        auto* check = new QCheckBox(text, row);
        check->setObjectName(QString::fromStdString(key));
        flow->addWidget(check);
        bind(check, key);
        if (first == nullptr) {
            first = check;
        }
    }
    auto* title = new QLabel(label);
    title->setBuddy(first);
    form->addRow(title, row);
}

void PlotBoardPage::addViewGroup() {
    //: A group of Settings > Plot Board. In Polish: Widok
    QFormLayout* view = addGroup(tr("View"));
    //: The view a new tab of the plot board opens on. In Polish: Otwieraj na:
    addChoices(view, tr("Open on:"), PlotBoard::START_VIEW_SETTING,
               {//: In Polish: tablicy
                {tr("the board"), QLatin1String(PlotBoard::VIEW_BOARD), {}},
                //: In Polish: konspekcie
                {tr("the outline"), QLatin1String(PlotBoard::VIEW_OUTLINE), {}},
                //: In Polish: ostatnio używanym
                {tr("the view used last"), QLatin1String(PlotBoard::START_LAST), {}}});
    //: The note under the view a tab opens on. In Polish: Książka > Tablica fabuły i Książka >
    //: Konspekt otwierają swój widok; tak otwiera się karta z polecenia „Pokaż na tablicy
    //: fabuły” w Nawigatorze.
    addFieldNote(view, tr("Book > Plot Board and Book > Outline open their own views; this is "
                          "how the tab opens from Show on the Plot Board in the Navigator."));
    //: In Polish: Układ tablicy:
    addChoices(view, tr("Layout of the board:"), PlotBoard::LAYOUT_SETTING,
               {//: The parts side by side. In Polish: kolumny
                {tr("columns"), QLatin1String(PlotBoard::LAYOUT_COLUMNS), {}},
                //: The cards in rows. In Polish: siatka
                {tr("grid"), QLatin1String(PlotBoard::LAYOUT_GRID), {}}});
    //: In Polish: Rozmiar kart:
    addChoices(view, tr("Size of the cards:"), PlotBoard::SIZE_SETTING,
               {//: In Polish: małe
                {tr("small"), QLatin1String(PlotBoard::SIZE_SMALL), {}},
                //: In Polish: średnie
                {tr("medium"), QLatin1String(PlotBoard::SIZE_MEDIUM), {}},
                //: In Polish: duże
                {tr("large"), QLatin1String(PlotBoard::SIZE_LARGE), {}}});
    //: What the plot board shows besides the main section. In Polish: Pokazuj na tablicy:
    addChecks(view, tr("Show on the board:"),
              {//: In Polish: sekcję początkową i końcową (tylko podgląd)
               {tr("the front and back sections (a preview only)"),
                PlotBoard::SECTIONS_SETTING}});
}

void PlotBoardPage::addScenesGroup() {
    //: A group of Settings > Plot Board. In Polish: Sceny
    QFormLayout* scenes = addGroup(tr("Scenes"));
    //: In Polish: Pokazuj sceny pod tekstami
    QCheckBox* shown =
        addCheckBox(scenes, tr("Show the scenes under the texts"), PlotBoard::SCENES_SETTING);
    shown->setObjectName(QStringLiteral("plotBoardShowScenes"));
    //: The note under Show the scenes under the texts. In Polish: W siatce – w otwartych
    //: szufladach pod rzędami kart.
    addOptionNote(scenes, shown,
                  tr("In the grid: in the drawers under the rows of the cards, open."));
    auto* variant = new QComboBox();
    variant->setObjectName(QStringLiteral("plotBoardScenes"));
    //: Which scenes the plot board shows. In Polish: tekstów i w ich planach
    variant->addItem(tr("of the texts and in their plans"), QLatin1String(PlotBoard::SCENES_BOTH));
    //: In Polish: tylko tekstów
    variant->addItem(tr("of the texts only"), QLatin1String(PlotBoard::SCENES_TEXT));
    //: In Polish: tylko w planach
    variant->addItem(tr("in the plans only"), QLatin1String(PlotBoard::SCENES_PLAN));
    //: In Polish: Sceny na tablicy:
    addField(scenes, tr("Scenes on the board:"), variant, PlotBoard::SCENES_VARIANT_SETTING);
    //: The note under the choice of the scenes. In Polish: Sceny tekstu to jego fragmenty między
    //: podziałami sceny (Wstaw > Podział sceny): przeniesienie karty przenosi tekst sceny. Sceny
    //: w planie są tylko w pliku książki, w przerywanej ramce: nie mają tekstu, słów ani stanu,
    //: a Enter na takiej karcie obok scen tekstu wstawia w jej miejscu podział sceny z jej
    //: opisem i otwiera tam tekst.
    addFieldNote(scenes,
                 tr("The scenes of a text are its parts between the scene breaks (Insert > "
                    "Scene Break): moving a card moves the text of the scene. The scenes in the "
                    "plan are only in the file of the book, in a dashed frame: they have no "
                    "text, no words and no state, and Enter on such a card beside the scenes of "
                    "the text puts a scene break with its description at its place and opens "
                    "the text there."));
}

void PlotBoardPage::addCardsGroup() {
    //: A group of Settings > Plot Board. In Polish: Karty
    QFormLayout* cards = addGroup(tr("Cards"));
    std::vector<std::pair<QString, std::string>> fields;
    for (const char* field : {PlotBoard::FIELD_DESCRIPTION, PlotBoard::FIELD_POV,
                              PlotBoard::FIELD_PLACE, PlotBoard::FIELD_TIME}) {
        fields.emplace_back(fieldName(field), PlotBoard::fieldSetting(field));
    }
    //: In Polish: liczbę słów
    fields.emplace_back(tr("number of words"), PlotBoard::fieldSetting(PlotBoard::FIELD_WORDS));
    fields.emplace_back(fieldName(PlotBoard::FIELD_STATUS),
                        PlotBoard::fieldSetting(PlotBoard::FIELD_STATUS));
    //: In Polish: liczbę scen
    fields.emplace_back(tr("number of scenes"),
                        PlotBoard::fieldSetting(PlotBoard::FIELD_SCENES));
    //: What the cards of the plot board show besides their titles. In Polish: Na kartach
    //: widać:
    addChecks(cards, tr("The cards show:"), fields);
    //: In Polish: Ile linii opisu mieści karta, zależy od jej rozmiaru. Liczba scen jest na
    //: karcie tekstu, którego scen nie widać pod nim; w siatce otwiera i zamyka ich szufladę
    //: (także Spacja).
    addFieldNote(cards, tr("How many lines of the description a card holds depends on its "
                           "size. The number of scenes is on the card of a text whose scenes "
                           "are not shown under it; in the grid it opens and closes their "
                           "drawer (also Space)."));
}

void PlotBoardPage::addColorsGroup() {
    //: A group of Settings > Plot Board. In Polish: Kolory
    QFormLayout* colors = addGroup(tr("Colors"));
    //: In Polish: Koloruj karty:
    addChoices(colors, tr("Color the cards:"), PlotBoard::COLOR_SETTING,
               {//: In Polish: według stanu
                {tr("by state"), QLatin1String(PlotBoard::COLOR_STATUS), {}},
                //: In Polish: według punktu widzenia
                {tr("by point of view"), QLatin1String(PlotBoard::COLOR_POV), {}},
                //: In Polish: własnym kolorem karty
                {tr("by the own color of the card"), QLatin1String(PlotBoard::COLOR_OWN), {}},
                //: In Polish: bez kolorów
                {tr("without colors"), QLatin1String(PlotBoard::COLOR_NONE), {}}});
    //: Where the color of a card is. In Polish: Kolor na karcie:
    addChoices(colors, tr("Color on the card:"), PlotBoard::COLOR_PLACE_SETTING,
               {//: In Polish: pasek u góry
                {tr("a strip at the top"), QLatin1String(PlotBoard::COLOR_STRIP), {}},
                //: In Polish: tło karty
                {tr("the background of the card"), QLatin1String(PlotBoard::COLOR_TINT), {}}});
    //: What is under the cards. In Polish: Tło tablicy:
    addChoices(colors, tr("Background of the board:"), PlotBoard::BACKGROUND_SETTING,
               {//: In Polish: kolor motywu
                {tr("the color of the theme"), QLatin1String(PlotBoard::BACKGROUND_THEME), {}},
                //: In Polish: korek
                {tr("cork"), QLatin1String(PlotBoard::BACKGROUND_CORK), {}},
                //: In Polish: własny kolor
                {tr("own color"), QLatin1String(PlotBoard::BACKGROUND_OWN), {}}});
    //: In Polish: Kolory stanów tekstów, scen w planie i własne kolory kart. Każdy motyw ma
    //: swoje kolory: to są kolory bieżącego motywu.
    addNote(colors, tr("The colors of the states of the texts, of the scenes in the plan and "
                       "the own colors of the cards. Each theme has its own colors: these are "
                       "the colors of the current theme."));
    const std::vector<std::string>& keys = PlotColors::keys();
    const QStringList titles{PlotBoard::statusName(QStringLiteral("draft")),
                             PlotBoard::statusName(QStringLiteral("revision")),
                             PlotBoard::statusName(QStringLiteral("final")),
                             PlotBoard::plannedName()};
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto own = static_cast<qsizetype>(i) - titles.size();
        addThemeColor(colors,
                      own < 0 ? titles.at(static_cast<qsizetype>(i))
                              : PlotColors::ownTitle(PlotColors::ownNames().value(own)),
                      keys.at(i));
    }
    //: The color of the cork under the cards. In Polish: Korek
    addThemeColor(colors, tr("Cork"), PlotColors::CORK_KEY);
    //: The writer's own background of the board. In Polish: Własne tło
    addThemeColor(colors, tr("Own Background"), PlotColors::BACKGROUND_KEY);
    addRestoreColorsButton(colors);
}

void PlotBoardPage::addOutlineGroup() {
    //: A group of Settings > Plot Board. In Polish: Konspekt
    QFormLayout* outline = addGroup(tr("Outline"));
    std::vector<std::pair<QString, std::string>> columns;
    for (int column = 0; column < PlotOutlineModel::COLUMN_COUNT; ++column) {
        const char* name = PlotOutlineModel::columnName(column);
        if (name == nullptr) {
            continue;  // the title, always shown
        }
        QString text;
        if (column == PlotOutlineModel::NUMBER_COLUMN) {
            //: A column of the outline: the number of the text. In Polish: numer
            text = tr("number");
        } else if (column == PlotOutlineModel::WORDS_COLUMN) {
            //: A column of the outline. In Polish: słowa
            text = tr("words");
        } else {
            text = fieldName(name);
        }
        columns.emplace_back(text, PlotBoard::outlineSetting(name));
    }
    //: The columns the outline shows. In Polish: Kolumny:
    addChecks(outline, tr("Columns:"), columns);
    //: In Polish: Tytuł widać zawsze. Kolumny wybiera też przycisk „Kolumny” nad konspektem.
    addFieldNote(outline, tr("The title is always shown. The button Table Columns over the "
                             "outline chooses the columns too."));
}

void PlotBoardPage::addNumbersGroup() {
    //: A group of Settings > Plot Board: the numbers in the titles of the texts ("Chapter 3").
    //: In Polish: Numery w tytułach
    QFormLayout* numbers = addGroup(tr("Numbers in the Titles"));
    //: In Polish: Po przestawieniu rozdziałów:
    addChoices(numbers, tr("After the chapters are rearranged:"), PlotBoard::RENUMBER_SETTING,
               {//: In Polish: zaproponuj na pasku
                {tr("offer it on the bar"), QLatin1String(PlotBoard::RENUMBER_ASK), {}},
                //: In Polish: ponumeruj od razu
                {tr("renumber at once"), QLatin1String(PlotBoard::RENUMBER_AUTO), {}},
                //: In Polish: nie zmieniaj
                {tr("do not change"), QLatin1String(PlotBoard::RENUMBER_NEVER), {}}});
    //: In Polish: Dotyczy tytułów nadanych przez program („Rozdział 3”, także z dalszym ciągiem,
    //: np. „Rozdział 3. Burza”); zmienia się tylko numer.
    addFieldNote(numbers, tr("For the titles the program gave (\"Chapter 3\", also with more, "
                             "as in \"Chapter 3. Storm\"); only the number changes."));
}

}  // namespace kalahari::gui
