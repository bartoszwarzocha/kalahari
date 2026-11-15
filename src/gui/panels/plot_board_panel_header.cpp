// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel_header.cpp
/// @brief PlotBoardPanel: the switches over the board and the outline, and their look from
/// the settings (PlotBoard::LAYOUT_SETTING...)

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QPalette>
#include <QPointer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The beginning of the keys of the settings of the board
constexpr const char* SETTINGS_PREFIX = "plotBoard.";

/// @brief A setting of the board as text
QString setting(const char* key) {
    return QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(key, std::string()));
}

void setSetting(const char* key, const char* value) {
    core::SettingsManager::getInstance().set<std::string>(key, value);
}

/// @brief A switch of the header: a button that stays down while its choice is on
QToolButton* switchButton(const QString& text, const char* name, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setText(text);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    return button;
}

constexpr int HEADER_SPACING = 6;       ///< Between the switches over the board
constexpr int HEADER_LINE_SPACING = 4;  ///< Between their lines, when they wrap
constexpr int HEADER_MARGIN_X = 8;      ///< On the left and right of the switches
constexpr int HEADER_MARGIN_Y = 6;      ///< Over and under them

/// @brief The size of the cards a setting names (PlotBoard::SIZE_SMALL...)
PlotBoardView::CardSize cardSizeOf(const QString& size) {
    if (size == QLatin1String(PlotBoard::SIZE_SMALL)) {
        return PlotBoardView::CardSize::Small;
    }
    if (size == QLatin1String(PlotBoard::SIZE_LARGE)) {
        return PlotBoardView::CardSize::Large;
    }
    return PlotBoardView::CardSize::Medium;
}

/// @brief What is under the cards, as a setting names it (PlotBoard::BACKGROUND_THEME...)
PlotBoardView::Background backgroundOf(const QString& background) {
    if (background == QLatin1String(PlotBoard::BACKGROUND_CORK)) {
        return PlotBoardView::Background::Cork;
    }
    if (background == QLatin1String(PlotBoard::BACKGROUND_OWN)) {
        return PlotBoardView::Background::Own;
    }
    return PlotBoardView::Background::Theme;
}

using Fields = PlotBoardView::Fields;

/// @brief The fields of the cards by the names of their settings
constexpr std::array<std::pair<const char*, bool Fields::*>, 7> FIELDS{{
    {PlotBoard::FIELD_DESCRIPTION, &Fields::description},
    {PlotBoard::FIELD_POV, &Fields::pov},
    {PlotBoard::FIELD_PLACE, &Fields::place},
    {PlotBoard::FIELD_TIME, &Fields::time},
    {PlotBoard::FIELD_WORDS, &Fields::words},
    {PlotBoard::FIELD_STATUS, &Fields::status},
    {PlotBoard::FIELD_SCENES, &Fields::scenes},
}};

/// @brief What the cards show, as @p settings say (PlotBoard::FIELDS_SETTING)
Fields fieldsOf(const core::SettingsManager& settings) {
    Fields fields;
    for (const auto& [name, field] : FIELDS) {
        fields.*field = settings.get<bool>(PlotBoard::fieldSetting(name), fields.*field);
    }
    return fields;
}

}  // namespace

void PlotBoardPanel::followSettings() {
    // The look follows the settings, also when another window changes them
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [self = QPointer<PlotBoardPanel>(this)](const std::string& key) {
            if (self == nullptr || !key.starts_with(SETTINGS_PREFIX)) {
                return;
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self]() {
                    if (self != nullptr) {
                        self->applySettings();
                    }
                },
                Qt::QueuedConnection);
        });
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &PlotBoardPanel::applySettings);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            &PlotBoardPanel::applySettings);
}

void PlotBoardPanel::buildHeader(QWidget* header) {
    auto* flow = new FlowLayout(header, HEADER_SPACING, HEADER_LINE_SPACING);
    flow->setContentsMargins(HEADER_MARGIN_X, HEADER_MARGIN_Y, HEADER_MARGIN_X, HEADER_MARGIN_Y);

    // Board | Outline
    //: The switch of the tab of the plot board: the cards. In Polish: Tablica
    m_boardButton = switchButton(tr("Board"), "plotBoardShowBoard", header);
    //: The switch: the table of the texts. In Polish: Konspekt
    m_outlineButton = switchButton(tr("Outline"), "plotBoardShowOutline", header);
    auto* views = new QButtonGroup(header);
    views->addButton(m_boardButton);
    views->addButton(m_outlineButton);
    flow->addWidget(m_boardButton);
    flow->addWidget(m_outlineButton);
    connect(m_boardButton, &QToolButton::clicked, this, [this]() {
        showView(QString::fromLatin1(PlotBoard::VIEW_BOARD));
        setSetting(PlotBoard::VIEW_SETTING, PlotBoard::VIEW_BOARD);
    });
    connect(m_outlineButton, &QToolButton::clicked, this, [this]() {
        showView(QString::fromLatin1(PlotBoard::VIEW_OUTLINE));
        setSetting(PlotBoard::VIEW_SETTING, PlotBoard::VIEW_OUTLINE);
    });

    // The scenes of the texts, on the board and in the outline
    //: The switch over the plot board: the scenes of the texts shown or not. In Polish: Sceny
    m_scenesButton = switchButton(tr("Scenes"), "plotBoardScenes", header);
    //: In Polish: Sceny tekstów: pod ich kartami, w szufladach siatki i w wierszach konspektu
    m_scenesButton->setToolTip(tr("The scenes of the texts: under their cards, in the drawers "
                                  "of the grid and in rows of the outline"));
    flow->addWidget(m_scenesButton);
    connect(m_scenesButton, &QToolButton::clicked, this, [](bool checked) {
        core::SettingsManager::getInstance().set<bool>(PlotBoard::SCENES_SETTING, checked);
    });

    // Columns | Grid
    //: How the cards of the plot board are arranged. In Polish: Układ:
    m_layoutLabel = new QLabel(tr("Layout:"), header);
    //: The parts side by side, each over its texts. In Polish: Kolumny
    m_columnsButton = switchButton(tr("Columns"), "plotBoardColumns", header);
    //: The texts in rows under the titles of their parts. In Polish: Siatka
    m_gridButton = switchButton(tr("Grid"), "plotBoardGrid", header);
    auto* layouts = new QButtonGroup(header);
    layouts->addButton(m_columnsButton);
    layouts->addButton(m_gridButton);
    flow->addWidget(m_layoutLabel);
    flow->addWidget(m_columnsButton);
    flow->addWidget(m_gridButton);
    connect(m_columnsButton, &QToolButton::clicked, this,
            []() { setSetting(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_COLUMNS); });
    connect(m_gridButton, &QToolButton::clicked, this,
            []() { setSetting(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_GRID); });

    // Small | Medium | Large
    //: The size of the cards of the plot board. In Polish: Karty:
    m_sizeLabel = new QLabel(tr("Cards:"), header);
    //: In Polish: Małe
    m_smallButton = switchButton(tr("Small"), "plotBoardSmall", header);
    //: In Polish: Średnie
    m_mediumButton = switchButton(tr("Medium"), "plotBoardMedium", header);
    //: In Polish: Duże
    m_largeButton = switchButton(tr("Large"), "plotBoardLarge", header);
    auto* sizes = new QButtonGroup(header);
    sizes->addButton(m_smallButton);
    sizes->addButton(m_mediumButton);
    sizes->addButton(m_largeButton);
    flow->addWidget(m_sizeLabel);
    flow->addWidget(m_smallButton);
    flow->addWidget(m_mediumButton);
    flow->addWidget(m_largeButton);
    connect(m_smallButton, &QToolButton::clicked, this,
            []() { setSetting(PlotBoard::SIZE_SETTING, PlotBoard::SIZE_SMALL); });
    connect(m_mediumButton, &QToolButton::clicked, this,
            []() { setSetting(PlotBoard::SIZE_SETTING, PlotBoard::SIZE_MEDIUM); });
    connect(m_largeButton, &QToolButton::clicked, this,
            []() { setSetting(PlotBoard::SIZE_SETTING, PlotBoard::SIZE_LARGE); });

    // What colors the cards
    //: What colors the cards of the plot board. In Polish: &Kolory:
    auto* colorLabel = new QLabel(tr("&Colors:"), header);
    m_colorBy = new QComboBox(header);
    m_colorBy->setObjectName(QStringLiteral("plotBoardColorBy"));
    colorLabel->setBuddy(m_colorBy);
    //: In Polish: według stanu
    m_colorBy->addItem(tr("by state"), QString::fromLatin1(PlotBoard::COLOR_STATUS));
    //: In Polish: według punktu widzenia
    m_colorBy->addItem(tr("by point of view"), QString::fromLatin1(PlotBoard::COLOR_POV));
    //: In Polish: własne kolory kart
    m_colorBy->addItem(tr("own colors of the cards"), QString::fromLatin1(PlotBoard::COLOR_OWN));
    //: In Polish: bez kolorów
    m_colorBy->addItem(tr("none"), QString::fromLatin1(PlotBoard::COLOR_NONE));
    flow->addWidget(colorLabel);
    flow->addWidget(m_colorBy);
    connect(m_colorBy, &QComboBox::activated, this, [this](int index) {
        core::SettingsManager::getInstance().set<std::string>(
            PlotBoard::COLOR_SETTING, m_colorBy->itemData(index).toString().toStdString());
    });

    // The columns of the outline
    m_outlineColumnsButton = buildColumnsButton(header);
    flow->addWidget(m_outlineColumnsButton);

    m_totals = new QLabel(header);
    m_totals->setObjectName(QStringLiteral("plotBoardTotals"));
    m_totals->setForegroundRole(QPalette::PlaceholderText);
    flow->addWidget(m_totals);
}

void PlotBoardPanel::applySettings() {
    const QString layout = setting(PlotBoard::LAYOUT_SETTING);
    m_board->setArrangement(layout == QLatin1String(PlotBoard::LAYOUT_GRID)
                                ? PlotBoardView::Arrangement::Grid
                                : PlotBoardView::Arrangement::Columns);
    m_board->setCardSize(cardSizeOf(setting(PlotBoard::SIZE_SETTING)));
    m_board->setColoring(setting(PlotBoard::COLOR_SETTING), PlotColors::current());
    m_board->setTint(setting(PlotBoard::COLOR_PLACE_SETTING) ==
                     QLatin1String(PlotBoard::COLOR_TINT));
    m_board->setBackground(backgroundOf(setting(PlotBoard::BACKGROUND_SETTING)));
    m_board->setFields(fieldsOf(core::SettingsManager::getInstance()));
    const bool scenes =
        core::SettingsManager::getInstance().get<bool>(PlotBoard::SCENES_SETTING, true);
    m_board->setShowScenes(scenes);
    m_model->setShowScenes(scenes);
    m_board->setShowSections(
        core::SettingsManager::getInstance().get<bool>(PlotBoard::SECTIONS_SETTING, false));
    applyOutlineColumns();
    updateSwitches();
    // Other scenes are other cards
    if (const QString variant = PlotBoard::scenesVariant(); variant != m_scenesVariant) {
        const bool changed = !m_scenesVariant.isEmpty();
        m_scenesVariant = variant;
        if (changed) {
            emit scenesVariantChanged();
        }
    }
}

void PlotBoardPanel::updateSwitches() {
    const bool outline = m_stack->currentWidget() == m_outline;
    m_boardButton->setChecked(!outline);
    m_outlineButton->setChecked(outline);
    m_scenesButton->setChecked(m_board->showsScenes());
    // The look of the cards is the board's
    for (QWidget* widget : {static_cast<QWidget*>(m_layoutLabel),
                            static_cast<QWidget*>(m_columnsButton),
                            static_cast<QWidget*>(m_gridButton),
                            static_cast<QWidget*>(m_sizeLabel),
                            static_cast<QWidget*>(m_smallButton),
                            static_cast<QWidget*>(m_mediumButton),
                            static_cast<QWidget*>(m_largeButton)}) {
        widget->setVisible(!outline);
    }
    m_outlineColumnsButton->setVisible(outline);
    const bool grid = m_board->arrangement() == PlotBoardView::Arrangement::Grid;
    m_columnsButton->setChecked(!grid);
    m_gridButton->setChecked(grid);
    m_smallButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Small);
    m_mediumButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Medium);
    m_largeButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Large);
    const int color = m_colorBy->findData(setting(PlotBoard::COLOR_SETTING));
    m_colorBy->setCurrentIndex(std::max(0, color));
}

QToolButton* PlotBoardPanel::buildColumnsButton(QWidget* header) {
    auto* button = new QToolButton(header);
    button->setObjectName(QStringLiteral("plotBoardOutlineColumns"));
    //: The button over the outline with the columns it shows. In Polish: Kolumny
    button->setText(tr("Table Columns"));
    button->setAutoRaise(true);
    button->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(button);
    menu->setObjectName(QStringLiteral("plotBoardOutlineColumnsMenu"));
    for (int column = 0; column < PlotOutlineModel::COLUMN_COUNT; ++column) {
        const char* name = PlotOutlineModel::columnName(column);
        if (name == nullptr) {
            continue;  // the title, always shown
        }
        QAction* action =
            menu->addAction(PlotOutlineModel::columnTitle(column));
        action->setCheckable(true);
        action->setData(column);
        connect(action, &QAction::toggled, this, [name](bool shown) {
            core::SettingsManager::getInstance().set<bool>(PlotBoard::outlineSetting(name),
                                                           shown);
        });
    }
    button->setMenu(menu);
    return button;
}

void PlotBoardPanel::applyOutlineColumns() {
    const auto& settings = core::SettingsManager::getInstance();
    for (QAction* action : m_outlineColumnsButton->menu()->actions()) {
        const int column = action->data().toInt();
        const bool shown =
            settings.get<bool>(PlotBoard::outlineSetting(PlotOutlineModel::columnName(column)),
                               true);
        const QSignalBlocker quiet(action);
        action->setChecked(shown);
        m_outline->setColumnHidden(column, !shown);
    }
    // The description takes the room left; without it the last column
    m_outline->horizontalHeader()->setStretchLastSection(
        m_outline->isColumnHidden(PlotOutlineModel::DESCRIPTION_COLUMN));
    // The cell chosen in a column hidden: the title of its row
    if (const QModelIndex current = m_outline->currentIndex();
        current.isValid() && m_outline->isColumnHidden(current.column())) {
        m_outline->setCurrentIndex(m_model->index(current.row(), PlotOutlineModel::TITLE_COLUMN));
    }
}

void PlotBoardPanel::updateTotals() {
    const QList<PlotCard> cards = PlotBoard::cardsOf(m_bands);
    int words = 0;
    for (const PlotCard& card : cards) {
        words += card.words;
    }
    //: The texts and the words of the book over the plot board; %1 the texts ("7 texts"), %2
    //: the words. In Polish: %1 · %2
    m_totals->setText(tr("%1 · %2").arg(PlotBoard::textsText(static_cast<int>(cards.size())),
                                        PlotBoard::wordsText(words)));
}

}  // namespace kalahari::gui
