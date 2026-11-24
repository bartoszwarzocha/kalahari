// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel_commands.cpp
/// @brief PlotBoardPanel: the bar of the commands over the board and the outline, and the line
/// under them that says what the card chosen is

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/card_bar.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QAction>
#include <QHBoxLayout>
#include <QKeyCombination>
#include <QLabel>
#include <QLatin1String>
#include <QMenu>
#include <QString>
#include <QToolButton>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <optional>

namespace kalahari::gui {

namespace {

constexpr int BAR_MARGIN = 6;      ///< Around the commands
constexpr int BAR_SPACING = 6;     ///< Between the buttons of a group
constexpr int GROUP_SPACING = 14;  ///< Between the groups

using Command = PlotBoardPanel::Command;

/// @brief A group of buttons of the bar, side by side; @p row gets its layout
QWidget* group(QWidget* parent, QHBoxLayout** row) {
    auto* box = new QWidget(parent);
    *row = new QHBoxLayout(box);
    (*row)->setContentsMargins(0, 0, 0, 0);
    (*row)->setSpacing(BAR_SPACING / 2);
    return box;
}

/// @brief Keys @p pressed with @p modifiers
QKeyCombination keys(Qt::Key pressed, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    return QKeyCombination(modifiers, pressed);
}

/// @brief Give @p action the icon @p iconId of the program, which follows the theme
void setIconId(QAction* action, const QString& iconId) {
    QVariantMap data = action->data().toMap();
    if (data.value(QStringLiteral("cmdId")).toString() == iconId) {
        return;
    }
    data.insert(QStringLiteral("cmdId"), iconId);
    action->setData(data);
    action->setIcon(core::ArtProvider::getInstance().getIcon(iconId));
}

/// @brief Whether the plan of the texts shows the scenes written (@p written) or the scenes in
/// the plans
bool shows(bool written) {
    const QString variant = PlotBoard::scenesVariant();
    return variant == QLatin1String(PlotBoard::SCENES_BOTH) ||
           variant == QLatin1String(written ? PlotBoard::SCENES_TEXT : PlotBoard::SCENES_PLAN);
}

}  // namespace

QAction* PlotBoardPanel::command(Command which) const {
    return m_commands.at(static_cast<std::size_t>(which));
}

void PlotBoardPanel::buildCommands(QWidget* bar) {
    auto* flow = new FlowLayout(bar, GROUP_SPACING, BAR_SPACING / 2);
    flow->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, 0);
    const auto add = [this](Command which, const char* iconId, const QString& name,
                            QKeyCombination combination) {
        QAction* action =
            CardBar::command(QString::fromLatin1(iconId), name, combination, this);
        m_commands.at(static_cast<std::size_t>(which)) = action;
        connect(action, &QAction::triggered, this, [this, which]() { runCommand(which); });
    };
    //: A command of the bar over the plan of the book: a text after the text of the card. In
    //: Polish: Nowy rozdział
    add(Command::NewText, "common.add", tr("New Chapter"), keys(Qt::Key_Insert));
    //: A text in the plan, without a file. In Polish: Rozdział w planie
    add(Command::PlanText, "plotBoard.planText", tr("Chapter in the Plan"),
        keys(Qt::Key_Insert, Qt::ControlModifier | Qt::ShiftModifier));
    //: A scene written: a scene break in the text. In Polish: Nowa scena
    add(Command::NewScene, "book.newScene", tr("New Scene"),
        keys(Qt::Key_Insert, Qt::AltModifier | Qt::ShiftModifier));
    //: A scene in the plan of the text. In Polish: Scena w planie
    add(Command::PlanScene, "plotBoard.planScene", tr("Scene in the Plan"),
        keys(Qt::Key_Insert, Qt::AltModifier));
    //: In Polish: Edytuj kartę
    add(Command::Edit, "edit.rename", tr("Edit the Card"), keys(Qt::Key_F2));
    //: Open the text of the card. In Polish: Otwórz
    add(Command::Open, "template.chapter", tr("Open"), keys(Qt::Key_Return));
    //: The own color of the card, in a menu. In Polish: Kolor karty
    add(Command::Color, "common.palette", tr("Card Color"), keys(Qt::Key_unknown));
    //: The state of the text, in a menu. In Polish: Stan
    add(Command::State, "plotBoard.state", tr("State"), keys(Qt::Key_unknown));
    //: In Polish: Usuń
    add(Command::Delete, "common.delete", tr("Delete"), keys(Qt::Key_Delete));
    //: The command of the bar over the plan of the book. In Polish: Cofnij
    add(Command::Undo, "edit.undo", tr("Undo"), keys(Qt::Key_Z, Qt::ControlModifier));
    //: The command of the bar over the plan of the book. In Polish: Ponów
    add(Command::Redo, "edit.redo", tr("Redo"), keys(Qt::Key_Y, Qt::ControlModifier));

    QHBoxLayout* line = nullptr;
    QWidget* texts = group(bar, &line);
    line->addWidget(CardBar::button(command(Command::NewText), "plotBoardNewText", true, texts));
    line->addWidget(CardBar::button(command(Command::PlanText), "plotBoardPlanText", true, texts));
    flow->addWidget(texts);
    QWidget* scenes = group(bar, &line);
    line->addWidget(CardBar::button(command(Command::NewScene), "plotBoardNewScene", true, scenes));
    line->addWidget(
        CardBar::button(command(Command::PlanScene), "plotBoardPlanScene", true, scenes));
    flow->addWidget(scenes);
    QWidget* cardGroup = group(bar, &line);
    line->addWidget(CardBar::button(command(Command::Edit), "plotBoardEditCard", false, cardGroup));
    m_openButton = CardBar::button(command(Command::Open), "plotBoardOpen", false, cardGroup);
    line->addWidget(m_openButton);
    const auto menuButton = [this, cardGroup, line](Command which, const char* name, QMenu** menu) {
        QToolButton* button = CardBar::button(command(which), name, false, cardGroup);
        *menu = new QMenu(button);
        button->setMenu(*menu);
        button->setPopupMode(QToolButton::InstantPopup);
        line->addWidget(button);
    };
    menuButton(Command::Color, "plotBoardColor", &m_colorMenu);
    menuButton(Command::State, "plotBoardState", &m_stateMenu);
    flow->addWidget(cardGroup);
    flow->addWidget(CardBar::button(command(Command::Delete), "plotBoardDelete", false, bar));
    QWidget* steps = group(bar, &line);
    line->addWidget(CardBar::button(command(Command::Undo), "plotBoardUndo", false, steps));
    line->addWidget(CardBar::button(command(Command::Redo), "plotBoardRedo", false, steps));
    flow->addWidget(steps);

    // The menus of the card chosen, when they open
    connect(m_colorMenu, &QMenu::aboutToShow, this, [this]() {
        const QString id = currentCard();
        fillColorMenu(m_colorMenu, id, card(id));
    });
    connect(m_stateMenu, &QMenu::aboutToShow, this, [this]() {
        const QString id = currentCard();
        fillStateMenu(m_stateMenu, id, card(id));
    });
}

void PlotBoardPanel::runCommand(Command which) {
    const QString id = currentCard();
    const QString textId = PlotBoard::textOf(id);
    const bool scene = !id.isEmpty() && textId != id;
    switch (which) {
    case Command::NewText:
        emit addRequested(textId);
        break;
    case Command::PlanText:
        emit addPlannedRequested(textId);
        break;
    case Command::NewScene:
        emit newSceneRequested(id);
        break;
    case Command::PlanScene:
        // After a scene, else at the end of the text
        if (scene) {
            emit addRequested(id);
        } else {
            emit addSceneRequested(id);
        }
        break;
    case Command::Edit:
        editCard(id);
        break;
    case Command::Open:
        emit openRequested(id);
        break;
    case Command::Delete:
        emit deleteRequested(id);
        break;
    case Command::Undo:
        emit undoRequested();
        break;
    case Command::Redo:
        emit redoRequested();
        break;
    case Command::Color:
    case Command::State:
    case Command::Count:
        break;
    }
    focusView();
}

void PlotBoardPanel::updateCommands() {
    if (command(Command::Open) == nullptr || m_stack == nullptr) {
        return;
    }
    const QString id = currentCard();
    const std::optional<PlotCard> found = card(id);
    const bool writable = !m_readOnly;
    const bool chosen = found.has_value();
    const bool scene = chosen && found->isScene();
    const std::optional<PlotCard> text = scene ? card(PlotBoard::textOf(id)) : found;
    const bool textPlanned = text.has_value() && text->planned;
    const bool planned = chosen && found->planned;

    command(Command::NewText)->setEnabled(writable);
    command(Command::PlanText)->setEnabled(writable);
    // A scene written needs a text with a file
    command(Command::NewScene)->setEnabled(writable && chosen && !textPlanned && shows(true));
    command(Command::PlanScene)->setEnabled(writable && chosen && shows(false));
    command(Command::PlanScene)
        ->setToolTip(CardBar::withKeys(tr("Scene in the Plan"),
                                       scene ? keys(Qt::Key_Insert)
                                             : keys(Qt::Key_Insert, Qt::AltModifier)));
    command(Command::Edit)->setEnabled(writable && chosen);

    // Open; a text in the plan gets its file, a scene in the plan is written
    QAction* open = command(Command::Open);
    QString openName = tr("Open");
    bool named = false;
    bool openEnabled = chosen;
    if (planned && !scene) {
        //: The command of the bar on a text in the plan, without a file. In Polish: Utwórz
        //: plik rozdziału
        openName = tr("Create the Chapter File");
        setIconId(open, QStringLiteral("plotBoard.createFile"));
        named = true;
        openEnabled = writable;
    } else if (planned && PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_BOTH)) {
        //: The command of the bar on a scene in the plan. In Polish: Napisz scenę
        openName = tr("Write the Scene");
        setIconId(open, QStringLiteral("plotBoard.writeScene"));
        named = true;
        openEnabled = writable && !textPlanned;
    } else {
        setIconId(open, QStringLiteral("template.chapter"));
    }
    open->setText(openName);
    open->setToolTip(CardBar::withKeys(openName, keys(Qt::Key_Return)));
    open->setEnabled(openEnabled);
    m_openButton->setToolButtonStyle(named ? Qt::ToolButtonTextBesideIcon
                                           : Qt::ToolButtonIconOnly);

    command(Command::Color)->setEnabled(writable && chosen);
    // A text or a scene in the plan gets a state once it is written
    command(Command::State)->setEnabled(writable && chosen && !planned);

    QString remove = tr("Delete");
    if (planned) {
        //: The command of the bar on a card in the plan. In Polish: Usuń z planu
        remove = tr("Delete from the Plan");
    } else if (scene) {
        //: The command of the bar on the card of a scene. In Polish: Usuń scenę…
        remove = tr("Delete the Scene…");
    } else if (chosen) {
        //: The command of the bar on the card of a text. In Polish: Usuń z książki…
        remove = tr("Delete from the Book…");
    }
    command(Command::Delete)->setText(remove);
    command(Command::Delete)->setToolTip(CardBar::withKeys(remove, keys(Qt::Key_Delete)));
    command(Command::Delete)->setEnabled(writable && chosen);
    command(Command::Undo)->setEnabled(writable && m_canUndo);
    command(Command::Redo)->setEnabled(writable && m_canRedo);
    updateHint();
}

QString PlotBoardPanel::hint() const {
    return m_hint->isHidden() ? QString() : m_hint->text();
}

void PlotBoardPanel::updateHint() {
    if (m_hint == nullptr) {
        return;
    }
    m_hint->setText(showsChronology() ? m_chronology->hint() : hintOf(currentCard()));
    // The bar under the board says what was just done in place of the line
    m_hint->setVisible(m_infoBar->isHidden());
}

QString PlotBoardPanel::hintOf(const QString& cardId) const {
    const std::optional<PlotCard> found = card(cardId);
    if (!found) {
        //: The line under the plan of the book without a card chosen. In Polish: Strzałki
        //: wybierają kartę, Insert dodaje, F2 albo ołówek edytuje, Enter otwiera.
        return tr("The arrows choose a card, Insert adds one, F2 or the pencil edits it, Enter "
                  "opens it.");
    }
    const std::optional<PlotCard> text =
        found->isScene() ? card(PlotBoard::textOf(cardId)) : found;
    const QString title = text ? text->title : QString();
    if (!found->isScene()) {
        if (found->planned) {
            //: The line under the plan of the book on a text in the plan; %1 its title. In
            //: Polish: %1: w planie, jeszcze bez pliku. „Utwórz plik rozdziału” utworzy go w
            //: folderze książki.
            return tr("%1: in the plan, without a file yet. Create the Chapter File makes it in "
                      "the folder of the book.")
                .arg(title);
        }
        const auto scenes = static_cast<int>(found->scenes.size());
        if (scenes == 0) {
            //: The line under the plan of the book on a text without scene breaks; %1 its
            //: title. In Polish: %1: jedna scena, bo tekst nie ma podziałów sceny. „Nowa
            //: scena” doda pierwszy podział.
            return tr("%1: one scene, as the text has no scene breaks. New Scene adds the first "
                      "break.")
                .arg(title);
        }
        const auto inPlan = static_cast<int>(std::ranges::count_if(
            found->scenes, [](const PlotCard& scene) { return scene.planned; }));
        if (inPlan == 0) {
            //: The line under the plan of the book on a text with scenes; %1 its title, %2
            //: "3 scenes". In Polish: %1: %2.
            return tr("%1: %2.").arg(title, PlotBoard::scenesText(scenes));
        }
        //: The line under the plan of the book on a text with scenes; %1 its title, %2
        //: "3 scenes", %3 how many of them are in the plan. In Polish: %1: %2, w tym %3 w
        //: planie.
        return tr("%1: %2, %3 of them in the plan.")
            .arg(title, PlotBoard::scenesText(scenes))
            .arg(inPlan);
    }
    if (found->planned) {
        if (text && text->planned) {
            //: The line under the plan of the book on a scene in the plan of a text in the
            //: plan. In Polish: Scena w planie, jeszcze bez tekstu. Najpierw trzeba utworzyć
            //: plik rozdziału: „Utwórz plik rozdziału” na karcie tekstu.
            return tr("A scene in the plan, without text yet. The chapter file comes first: "
                      "Create the Chapter File on the card of the text.");
        }
        if (PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_BOTH)) {
            //: The line under the plan of the book on a scene in the plan. In Polish: Scena w
            //: planie, jeszcze bez tekstu. „Napisz scenę” wstawi do tekstu podział sceny z jej
            //: tytułem i opisem.
            return tr("A scene in the plan, without text yet. Write the Scene puts a scene break "
                      "with its title and description in the text.");
        }
        //: The line under the plan of the book on a scene in the plan; %1 the title of its
        //: text. In Polish: Scena w planie tekstu „%1”.
        return tr("A scene in the plan of \"%1\".").arg(title);
    }
    if (found->scene == 0) {
        //: The line under the plan of the book on the first scene of a text; %1 the title of
        //: the text. In Polish: Scena 1 tekstu „%1”: tekst od początku do pierwszego podziału
        //: sceny.
        return tr("Scene 1 of \"%1\": the text from its start to the first scene break.")
            .arg(title);
    }
    //: The line under the plan of the book on a scene of a text; %1 its number, %2 the title of
    //: the text, %3 the number of the scene break it starts at. In Polish: Scena %1 tekstu
    //: „%2”: tekst od %3. podziału sceny do następnego (Wstaw > Podział sceny).
    return tr("Scene %1 of \"%2\": the text from scene break %3 to the next one (Insert > Scene "
              "Break).")
        .arg(found->scene + 1)
        .arg(title)
        .arg(found->scene);
}

}  // namespace kalahari::gui
