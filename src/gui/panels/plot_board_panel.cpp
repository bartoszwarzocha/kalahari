// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel.cpp
/// @brief The tab of the plot board in the middle of the window: the board of the cards or the
/// outline, the switches of their look, and what was just done

#include "kalahari/gui/panels/plot_board_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/info_bar.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QStackedWidget>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

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

/// @brief The width of a column of the outline in average characters
int charactersOf(int column) {
    switch (column) {
    case PlotOutlineModel::NumberColumn:
        return 5;
    case PlotOutlineModel::TitleColumn:
        return 24;
    case PlotOutlineModel::PovColumn:
    case PlotOutlineModel::TimeColumn:
    case PlotOutlineModel::StatusColumn:
        return 14;
    case PlotOutlineModel::PlaceColumn:
        return 18;
    case PlotOutlineModel::WordsColumn:
        return 9;
    default:
        break;
    }
    return 40;
}

}  // namespace

PlotBoardPanel::PlotBoardPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("plotBoardPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("plotBoardHeader"));
    buildHeader(header);
    layout->addWidget(header);

    m_stack = new QStackedWidget(this);
    m_board = new PlotBoardView(m_stack);
    m_outline = new QTableView(m_stack);
    m_outline->setObjectName(QStringLiteral("plotOutline"));
    //: The outline of the book, for the screen reader. In Polish: Konspekt
    m_outline->setAccessibleName(tr("Outline"));
    m_model = new PlotOutlineModel(this);
    m_delegate = new PlotOutlineDelegate(this);
    m_outline->setModel(m_model);
    m_outline->setItemDelegate(m_delegate);
    m_outline->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_outline->setSelectionMode(QAbstractItemView::SingleSelection);
    m_outline->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::EditKeyPressed |
                               QAbstractItemView::AnyKeyPressed);
    m_outline->setTabKeyNavigation(true);
    m_outline->setWordWrap(false);
    m_outline->verticalHeader()->hide();
    m_outline->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_outline->horizontalHeader()->setStretchLastSection(false);
    m_outline->horizontalHeader()->setSectionResizeMode(PlotOutlineModel::DescriptionColumn,
                                                        QHeaderView::Stretch);
    m_outline->setContextMenuPolicy(Qt::CustomContextMenu);
    m_outline->installEventFilter(this);
    m_stack->addWidget(m_board);
    m_stack->addWidget(m_outline);
    layout->addWidget(m_stack, 1);

    m_infoBar = new InfoBar(this);
    m_infoBar->setObjectName(QStringLiteral("plotBoardInfoBar"));
    m_infoBar->hide();
    layout->addWidget(m_infoBar);

    m_menu = new QMenu(this);

    // The board
    connect(m_board, &PlotBoardView::currentCardChanged, this, [this](const QString& id) {
        const int row = m_model->rowOf(id);
        if (row >= 0 && m_stack->currentWidget() == m_board) {
            m_outline->setCurrentIndex(m_model->index(row, PlotOutlineModel::TitleColumn));
        }
        emit currentCardChanged(id);
    });
    connect(m_board, &PlotBoardView::openRequested, this, &PlotBoardPanel::openRequested);
    connect(m_board, &PlotBoardView::editRequested, this, &PlotBoardPanel::editCard);
    connect(m_board, &PlotBoardView::editFinished, this, &PlotBoardPanel::editFinished);
    connect(m_board, &PlotBoardView::moveRequested, this, &PlotBoardPanel::moveRequested);
    connect(m_board, &PlotBoardView::moveToTextRequested, this,
            &PlotBoardPanel::moveToTextRequested);
    connect(m_board, &PlotBoardView::dropBesideRequested, this,
            &PlotBoardPanel::dropBesideRequested);
    connect(m_board, &PlotBoardView::dropInPartRequested, this,
            &PlotBoardPanel::dropInPartRequested);
    connect(m_board, &PlotBoardView::addRequested, this, &PlotBoardPanel::addRequested);
    connect(m_board, &PlotBoardView::addSceneRequested, this, &PlotBoardPanel::addSceneRequested);
    connect(m_board, &PlotBoardView::deleteRequested, this, &PlotBoardPanel::deleteRequested);
    connect(m_board, &PlotBoardView::cutRequested, this, &PlotBoardPanel::cutRequested);
    connect(m_board, &PlotBoardView::pasteRequested, this, &PlotBoardPanel::pasteRequested);
    connect(m_board, &PlotBoardView::cutCancelled, this, &PlotBoardPanel::cutCancelled);
    connect(m_board, &PlotBoardView::undoRequested, this, &PlotBoardPanel::undoRequested);
    connect(m_board, &PlotBoardView::redoRequested, this, &PlotBoardPanel::redoRequested);
    connect(m_board, &PlotBoardView::propertiesRequested, this,
            &PlotBoardPanel::propertiesRequested);
    connect(m_board, &PlotBoardView::menuRequested, this, &PlotBoardPanel::showMenu);

    // The outline
    connect(m_model, &PlotOutlineModel::cellEdited, this, &PlotBoardPanel::onOutlineEdited);
    connect(m_outline->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) {
                const QString id = m_model->cardIdAt(current.row());
                if (!id.isEmpty() && m_stack->currentWidget() == m_outline) {
                    m_board->setCurrentCard(id);
                }
            });
    connect(m_outline, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        // A double-click edits the cells that can be edited, and opens the text on the others
        if (!(m_model->flags(index) & Qt::ItemIsEditable)) {
            const QString id = m_model->cardIdAt(index.row());
            if (!id.isEmpty()) {
                emit openRequested(id);
            }
        }
    });
    connect(m_outline, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_outline->indexAt(pos);
        if (index.isValid()) {
            m_outline->setCurrentIndex(index);
        }
        showMenu(m_model->cardIdAt(index.row()), m_outline->viewport()->mapToGlobal(pos));
    });

    connect(m_infoBar, &InfoBar::actionClicked, this, &PlotBoardPanel::undoRequested);
    connect(m_infoBar, &InfoBar::dismissed, m_infoBar, &QWidget::hide);

    // The look follows the settings, also when another window changes them
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [self = QPointer<PlotBoardPanel>(this)](const std::string& key) {
            if (self != nullptr && key.rfind(SETTINGS_PREFIX, 0) == 0) {
                QMetaObject::invokeMethod(
                    self.data(),
                    [self]() {
                        if (self != nullptr) {
                            self->applySettings();
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &PlotBoardPanel::applySettings);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            &PlotBoardPanel::applySettings);

    applySettings();
    for (int column = 0; column < PlotOutlineModel::ColumnCount; ++column) {
        if (column != PlotOutlineModel::DescriptionColumn) {
            m_outline->setColumnWidth(column,
                                      m_outline->fontMetrics().averageCharWidth() *
                                          charactersOf(column));
        }
    }
    showView(setting(PlotBoard::VIEW_SETTING));
}

PlotBoardPanel::~PlotBoardPanel() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void PlotBoardPanel::buildHeader(QWidget* header) {
    auto* flow = new FlowLayout(header, 6, 4);
    flow->setContentsMargins(8, 6, 8, 6);

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

    m_totals = new QLabel(header);
    m_totals->setObjectName(QStringLiteral("plotBoardTotals"));
    m_totals->setForegroundRole(QPalette::PlaceholderText);
    flow->addWidget(m_totals);
}

void PlotBoardPanel::setBands(const QList<PlotBand>& bands) {
    m_bands = bands;
    const QString current = currentCard();
    m_board->setBands(bands);
    m_model->setBands(bands);
    const int row = m_model->rowOf(m_board->currentCard().isEmpty() ? current
                                                                    : m_board->currentCard());
    if (row >= 0 && !m_outline->currentIndex().isValid()) {
        m_outline->setCurrentIndex(m_model->index(row, PlotOutlineModel::TitleColumn));
    }
    updateTotals();
}

void PlotBoardPanel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    m_board->setReadOnly(readOnly);
    m_model->setReadOnly(readOnly);
}

void PlotBoardPanel::setSuggestions(const QStringList& povs, const QStringList& places) {
    m_povs = povs;
    m_places = places;
    m_delegate->setSuggestions(povs, places);
}

void PlotBoardPanel::showView(const QString& view) {
    const bool outline = view == QLatin1String(PlotBoard::VIEW_OUTLINE);
    const QString current = currentCard();
    m_stack->setCurrentWidget(outline ? static_cast<QWidget*>(m_outline)
                                      : static_cast<QWidget*>(m_board));
    if (!current.isEmpty()) {
        setCurrentCard(current);
    }
    updateSwitches();
}

QString PlotBoardPanel::view() const {
    return QString::fromLatin1(m_stack->currentWidget() == m_outline ? PlotBoard::VIEW_OUTLINE
                                                                     : PlotBoard::VIEW_BOARD);
}

void PlotBoardPanel::focusView() {
    m_stack->currentWidget()->setFocus(Qt::OtherFocusReason);
}

QString PlotBoardPanel::currentCard() const {
    if (m_stack != nullptr && m_stack->currentWidget() == m_outline) {
        const QString id = m_model->cardIdAt(m_outline->currentIndex().row());
        if (!id.isEmpty()) {
            return id;
        }
    }
    return m_board != nullptr ? m_board->currentCard() : QString();
}

bool PlotBoardPanel::setCurrentCard(const QString& elementId) {
    const bool found = m_board->setCurrentCard(elementId);
    const int row = m_model->rowOf(elementId);
    if (row >= 0) {
        const int column = m_outline->currentIndex().isValid()
                               ? m_outline->currentIndex().column()
                               : int(PlotOutlineModel::TitleColumn);
        m_outline->setCurrentIndex(m_model->index(row, column));
    }
    return found;
}

void PlotBoardPanel::setMarkedCard(const QString& elementId) {
    m_board->setMarkedCard(elementId);
}

bool PlotBoardPanel::editCard(const QString& elementId) {
    if (m_stack->currentWidget() == m_board) {
        return m_board->editCard(elementId, m_povs, m_places);
    }
    const int row = m_model->rowOf(elementId);
    const QModelIndex title = m_model->index(row, PlotOutlineModel::TitleColumn);
    if (row < 0 || !(m_model->flags(title) & Qt::ItemIsEditable)) {
        return false;
    }
    m_outline->setCurrentIndex(title);
    m_outline->edit(title);
    return m_outline->indexWidget(title) != nullptr;
}

void PlotBoardPanel::showMessage(const QString& message, bool canUndo) {
    m_infoBar->setMessage(message);
    //: The button of the bar under the plot board that undoes what it says. In Polish: &Cofnij
    m_infoBar->setActionText(canUndo ? tr("&Undo") : QString());
    m_infoBar->setActionToolTip(canUndo ? QStringLiteral("Ctrl+Z") : QString());
    m_infoBar->setVisible(!message.isEmpty());
}

QString PlotBoardPanel::message() const {
    return m_infoBar->isHidden() ? QString() : m_infoBar->message();
}

void PlotBoardPanel::applySettings() {
    const QString layout = setting(PlotBoard::LAYOUT_SETTING);
    m_board->setArrangement(layout == QLatin1String(PlotBoard::LAYOUT_GRID)
                                ? PlotBoardView::Arrangement::Grid
                                : PlotBoardView::Arrangement::Columns);
    const QString size = setting(PlotBoard::SIZE_SETTING);
    m_board->setCardSize(size == QLatin1String(PlotBoard::SIZE_SMALL)
                             ? PlotBoardView::CardSize::Small
                         : size == QLatin1String(PlotBoard::SIZE_LARGE)
                             ? PlotBoardView::CardSize::Large
                             : PlotBoardView::CardSize::Medium);
    m_board->setColoring(setting(PlotBoard::COLOR_SETTING), PlotColors::current());
    const bool scenes =
        core::SettingsManager::getInstance().get<bool>(PlotBoard::SCENES_SETTING, true);
    m_board->setShowScenes(scenes);
    m_model->setShowScenes(scenes);
    updateSwitches();
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
    const bool grid = m_board->arrangement() == PlotBoardView::Arrangement::Grid;
    m_columnsButton->setChecked(!grid);
    m_gridButton->setChecked(grid);
    m_smallButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Small);
    m_mediumButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Medium);
    m_largeButton->setChecked(m_board->cardSize() == PlotBoardView::CardSize::Large);
    const int color = m_colorBy->findData(setting(PlotBoard::COLOR_SETTING));
    m_colorBy->setCurrentIndex(std::max(0, color));
}

void PlotBoardPanel::updateTotals() {
    const QList<PlotCard> cards = PlotBoard::cardsOf(m_bands);
    int words = 0;
    for (const PlotCard& card : cards) {
        words += card.words;
    }
    //: The texts and the words of the book over the plot board; %1 the texts ("7 texts"), %2
    //: the words. In Polish: %1 · %2
    m_totals->setText(tr("%1 · %2").arg(PlotBoard::textsText(int(cards.size())),
                                        PlotBoard::wordsText(words)));
}

std::optional<PlotCard> PlotBoardPanel::card(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return std::nullopt;
    }
    for (const PlotCard& each : PlotBoard::allCardsOf(m_bands)) {
        if (each.id == elementId) {
            return each;
        }
    }
    return std::nullopt;
}

void PlotBoardPanel::onOutlineEdited(const QString& elementId, int column,
                                     const QString& value) {
    const std::optional<PlotCard> found = card(elementId);
    if (!found) {
        return;
    }
    if (column == PlotOutlineModel::StatusColumn) {
        emit statusRequested(elementId, value);
        return;
    }
    PlotCardEdit edit{found->title, found->description, found->plot};
    switch (column) {
    case PlotOutlineModel::TitleColumn:
        edit.title = value;
        break;
    case PlotOutlineModel::DescriptionColumn:
        edit.description = value;
        break;
    case PlotOutlineModel::PovColumn:
        edit.plot.pov = value;
        break;
    case PlotOutlineModel::PlaceColumn:
        edit.plot.place = value;
        break;
    case PlotOutlineModel::TimeColumn:
        edit.plot.time = value;
        break;
    default:
        return;
    }
    emit editFinished(elementId, edit);
}

QMenu* PlotBoardPanel::cardMenu(const QString& elementId) {
    m_menu->clear();
    const std::optional<PlotCard> found = card(elementId);
    const bool shown = found.has_value();
    const bool changing = shown && !m_readOnly;
    const bool scene = shown && found->isScene();
    const auto add = [this](const QString& text, const QString& keys, bool enabled,
                            const std::function<void()>& run) {
        QAction* action = m_menu->addAction(text);
        if (!keys.isEmpty()) {
            action->setShortcut(QKeySequence(keys));
            action->setShortcutVisibleInContextMenu(true);
        }
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, run);
        return action;
    };
    //: The menu of a card of the plot board. In Polish: &Otwórz
    add(tr("&Open"), QStringLiteral("Return"), shown, [this, elementId]() {
        emit openRequested(elementId);
    });
    //: In Polish: &Edytuj kartę
    add(tr("&Edit Card"), QStringLiteral("F2"), changing,
        [this, elementId]() { editCard(elementId); });
    add(scene
            //: A new scene right after the card of a scene. In Polish: &Nowa scena za nią
            ? tr("&New Scene After It")
            //: A new text of the same kind right after the card. In Polish: &Nowa karta za nią
            : tr("&New Card After It"),
        QStringLiteral("Ins"), !m_readOnly, [this, elementId]() { emit addRequested(elementId); });
    //: A new scene at the end of the text of the card. In Polish: Nowa scena n&a końcu tekstu
    add(tr("New Scene &at the End of the Text"), QStringLiteral("Alt+Ins"), changing,
        [this, elementId]() { emit addSceneRequested(elementId); });
    m_menu->addSeparator();
    //: In Polish: Wy&tnij
    add(tr("Cu&t"), QStringLiteral("Ctrl+X"), changing, [this, elementId]() {
        emit cutRequested(elementId);
    });
    //: In Polish: Wklej &za nią
    add(tr("&Paste After It"), QStringLiteral("Ctrl+V"),
        changing && !m_board->markedCard().isEmpty() && m_board->markedCard() != elementId,
        [this, elementId]() { emit pasteRequested(elementId); });
    m_menu->addSeparator();

    //: In Polish: &Stan
    QMenu* states = m_menu->addMenu(tr("&State"));
    states->setEnabled(changing);
    auto* stateGroup = new QActionGroup(states);
    stateGroup->setExclusive(true);
    for (const char* status : {"draft", "revision", "final"}) {
        const QString code = QString::fromLatin1(status);
        QAction* action = states->addAction(PlotBoard::statusName(code));
        action->setCheckable(true);
        action->setChecked(shown && found->status == code);
        stateGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, elementId, code]() {
            emit statusRequested(elementId, code);
        });
    }
    //: In Polish: &Kolor karty
    QMenu* colors = m_menu->addMenu(tr("Card &Color"));
    colors->setEnabled(changing);
    auto* colorGroup = new QActionGroup(colors);
    colorGroup->setExclusive(true);
    QStringList names{QString()};
    names << PlotColors::ownNames();
    const PlotColors palette = PlotColors::current();
    for (const QString& name : names) {
        //: A card without an own color. In Polish: Bez koloru
        QAction* action = colors->addAction(name.isEmpty() ? tr("No Color")
                                                           : PlotColors::ownTitle(name));
        if (!name.isEmpty()) {
            QPixmap swatch(12, 12);
            swatch.fill(palette.ofOwn(name));
            action->setIcon(QIcon(swatch));
        }
        action->setCheckable(true);
        action->setChecked(shown && found->plot.color == name);
        colorGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, elementId, name]() {
            emit colorRequested(elementId, name);
        });
    }
    m_menu->addSeparator();
    add(scene
            //: The menu of the card of a scene. In Polish: &Usuń scenę…
            ? tr("&Delete Scene...")
            //: In Polish: &Usuń z książki…
            : tr("&Delete from Book..."),
        QStringLiteral("Del"), changing, [this, elementId]() { emit deleteRequested(elementId); });
    //: In Polish: &Właściwości
    add(tr("P&roperties"), QStringLiteral("Alt+Return"), shown, [this, elementId]() {
        emit propertiesRequested(elementId);
    });
    return m_menu;
}

void PlotBoardPanel::showMenu(const QString& elementId, const QPoint& globalPos) {
    cardMenu(elementId)->popup(globalPos);
}

bool PlotBoardPanel::eventFilter(QObject* watched, QEvent* event) {
    // While a cell is edited, a key its editor does not take (Ctrl+Up) does nothing
    if (watched == m_outline &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) &&
        m_outline->indexWidget(m_outline->currentIndex()) == nullptr) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::ShortcutOverride) {
            // The keys of the outline go to it, not to the commands of the window
            if (outlineKey(key, false)) {
                key->accept();
                return true;
            }
        } else if (outlineKey(key, true)) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool PlotBoardPanel::outlineKey(const QKeyEvent* key, bool run) {
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    const bool plain = modifiers == Qt::NoModifier;
    const bool control = modifiers == Qt::ControlModifier;
    const QString id = m_model->cardIdAt(m_outline->currentIndex().row());
    const bool changing = !m_readOnly && !id.isEmpty();
    switch (key->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (plain || modifiers == Qt::AltModifier) {
            if (run && !id.isEmpty()) {
                if (plain) {
                    emit openRequested(id);
                } else {
                    emit propertiesRequested(id);
                }
            }
            return true;
        }
        return false;
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (control && run && changing) {
            emit moveRequested(id, key->key() == Qt::Key_Up ? -1 : 1);
        }
        return control;
    case Qt::Key_Insert:
        if (plain && run && !m_readOnly) {
            emit addRequested(id);
        } else if (modifiers == Qt::AltModifier && run && changing) {
            emit addSceneRequested(id);
        }
        return plain || modifiers == Qt::AltModifier;
    case Qt::Key_Delete:
        if (plain && run && changing) {
            emit deleteRequested(id);
        }
        return plain;
    case Qt::Key_X:
        if (control && run && changing) {
            emit cutRequested(id);
        }
        return control;
    case Qt::Key_V:
        if (control && run && !m_readOnly && !m_board->markedCard().isEmpty()) {
            emit pasteRequested(id);
        }
        return control;
    case Qt::Key_Z:
    case Qt::Key_Y: {
        const bool undo = key->key() == Qt::Key_Z && control;
        const bool redo = (key->key() == Qt::Key_Y && control) ||
                          (key->key() == Qt::Key_Z &&
                           modifiers == (Qt::ControlModifier | Qt::ShiftModifier));
        if (run && undo) {
            emit undoRequested();
        } else if (run && redo) {
            emit redoRequested();
        }
        return undo || redo;
    }
    case Qt::Key_Escape:
        if (plain && !m_board->markedCard().isEmpty()) {
            if (run) {
                emit cutCancelled();
            }
            return true;
        }
        return false;
    case Qt::Key_Menu:
    case Qt::Key_F10:
        if ((key->key() == Qt::Key_Menu && plain) ||
            (key->key() == Qt::Key_F10 && modifiers == Qt::ShiftModifier)) {
            if (run) {
                const QRect cell = m_outline->visualRect(m_outline->currentIndex());
                showMenu(id, m_outline->viewport()->mapToGlobal(cell.isValid() ? cell.center()
                                                                               : QPoint()));
            }
            return true;
        }
        return false;
    default:
        break;
    }
    return false;
}

void PlotBoardPanel::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        updateTotals();
    }
}

}  // namespace kalahari::gui
