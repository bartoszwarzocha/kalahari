// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel.h
/// @brief The tab of the plot board in the middle of the window: the board of the cards or the
/// outline, the switches of their look, and what was just done

#ifndef KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H
#define KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H

#include "kalahari/gui/edit_target.h"
#include "kalahari/gui/plan_chronology.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QComboBox;
class QKeyEvent;
class QLabel;
class QMenu;
class QStackedWidget;
class QTableView;
class QToolButton;
class QVBoxLayout;

namespace kalahari::gui {

class InfoBar;
class PlotOutlineDelegate;
class PlotOutlineModel;

/// @brief The plot board on its tab: the cards of the texts of the book (PlotBoardView) or
/// the outline, the same texts in a table (PlotOutlineModel)
///
/// At the top the bar of the commands of the card chosen (gui::CardBar): New Chapter, Chapter in
/// the Plan, New Scene, Scene in the Plan, Edit the Card, Open (on a card in the plan: Create
/// the Chapter File or Write the Scene), Card Color, State, Delete, Undo and Redo, with their
/// keys in their tips; the same commands are in the menu of the card. Under it Board | Outline |
/// Chronology (the scenes in the order of the story, from the module of the timelines: it has
/// its own options and commands, gui::PlanChronology),
/// Scenes (the scenes of the texts shown or not), the arrangement of the cards (Columns |
/// Grid), their size, what colors them, over the outline the columns it shows, and how many
/// texts and words the book has; the choices are kept in the settings
/// (PlotBoard::LAYOUT_SETTING...), with what the cards show and how they are colored (Settings
/// > Plot Board). Under them a line says what the card chosen is, and a bar what was just done,
/// with Undo.
///
/// The keys of the outline are those of the board where a table allows them: the arrows go
/// from cell to cell, F2 or a letter edits a cell, Tab goes to the next one, Enter keeps it
/// and Esc leaves it; Enter on a cell not edited opens the text, Ctrl+Up and Ctrl+Down move
/// its row. The panel asks for every change of the book: PlotBoardCoordinator makes it.
///
/// Edit > Undo, Redo, Cut, Paste and Delete work on it as its keys do (EditTarget), outside a
/// field being written in.
class PlotBoardPanel : public QWidget, public EditTarget {
    Q_OBJECT

public:
    /// @brief The commands of the bar over the board and the outline
    enum class Command : std::uint8_t {
        NewText,    ///< A text after the text of the card (Insert)
        PlanText,   ///< A text in the plan after it (Ctrl+Shift+Insert)
        NewScene,   ///< A scene written, a scene break (Alt+Shift+Insert)
        PlanScene,  ///< A scene in the plan: after a scene (Insert), at the end (Alt+Insert)
        Edit,       ///< Edit the card (F2)
        Open,       ///< Open it; create the file of a text in the plan, write a scene (Enter)
        Color,      ///< Its own color, in a menu
        State,      ///< Its state, in a menu
        Delete,     ///< Delete it (Delete)
        Undo,       ///< Ctrl+Z
        Redo,       ///< Ctrl+Y
        Count       ///< How many there are
    };

    /// @brief The tab without texts, its look from the settings
    explicit PlotBoardPanel(QWidget* parent = nullptr);
    ~PlotBoardPanel() override;

    PlotBoardPanel(const PlotBoardPanel&) = delete;
    PlotBoardPanel& operator=(const PlotBoardPanel&) = delete;
    PlotBoardPanel(PlotBoardPanel&&) = delete;
    PlotBoardPanel& operator=(PlotBoardPanel&&) = delete;

    /// @brief Show @p bands on the board and in the outline
    void setBands(const QList<PlotBand>& bands);
    /// @brief The bands shown
    [[nodiscard]] const QList<PlotBand>& bands() const { return m_bands; }

    /// @brief The texts of the front (@p front) and the back section (@p back) of the book: the
    /// board shows them as a preview when the settings say so (PlotBoard::SECTIONS_SETTING)
    void setSections(const PlotBand& front, const PlotBand& back);

    /// @brief The volumes of a series in the list Volume over the board and the outline, by
    /// their names (NavigatorPanel::volumeName()), volume @p current chosen; the list is
    /// hidden with fewer than two of them
    void setVolumes(const QStringList& names, int current);
    /// @brief The list Volume over the board and the outline
    [[nodiscard]] QComboBox* volumeBox() const { return m_volumeBox; }

    /// @brief Whether the book is open read-only: nothing can be changed
    void setReadOnly(bool readOnly);
    /// @brief Whether the book is open read-only
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

    /// @brief The points of view and the places the editors suggest
    void setSuggestions(const QStringList& povs, const QStringList& places);

    /// @brief Show the board (PlotBoard::VIEW_BOARD), the outline (VIEW_OUTLINE) or the
    /// chronology (VIEW_CHRONOLOGY; the board without it)
    void showView(const QString& view);

    /// @brief PlotBoard::VIEW_BOARD, VIEW_OUTLINE or VIEW_CHRONOLOGY, whichever is shown
    [[nodiscard]] QString view() const;

    /// @brief Give the keys to the board, the outline or the chronology, whichever is shown
    void focusView();

    /// @brief The card chosen in the view shown: a text or a scene (PlotBoard::sceneId()); in
    /// the chronology also an event outside the text (PlanChronology::EVENT_PREFIX); empty: none
    [[nodiscard]] QString currentCard() const;

    /// @brief Choose card @p elementId (a text or a scene) on the board, in the outline and in
    /// the chronology (a text with scenes: its first scene there)
    bool setCurrentCard(const QString& elementId);

    /// @brief Card @p elementId marked to move (Ctrl+X); empty: none
    void setMarkedCard(const QString& elementId);

    /// @brief Edit text @p elementId: its card on the board (F2), its title in the outline
    bool editCard(const QString& elementId);

    /// @brief Say @p message under the board; @p canUndo: with an Undo button
    void showMessage(const QString& message, bool canUndo);

    /// @brief Say @p message under the board with the button Renumber (renumberRequested()):
    /// the numbers in the titles no longer follow the order
    void offerRenumbering(const QString& message);

    /// @brief What the bar under the board says; empty when it is hidden
    [[nodiscard]] QString message() const;

    /// @brief What the line under the board says of the card chosen; empty when it is hidden
    /// (the bar under the board says something)
    [[nodiscard]] QString hint() const;

    /// @brief The action of command @p which of the bar over the board
    [[nodiscard]] QAction* command(Command which) const;

    /// @brief Read the look of the board from the settings again, with the theme's colors
    void applySettings();

    /// @brief The menu of card @p elementId (a text or a scene), as the menu key opens it
    [[nodiscard]] QMenu* cardMenu(const QString& elementId);

    /// @brief The card @p elementId shown: a text or a scene; nullopt: none
    [[nodiscard]] std::optional<PlotCard> card(const QString& elementId) const;

    /// @brief The board of the cards
    [[nodiscard]] PlotBoardView* board() const { return m_board; }
    /// @brief The chronology: the scenes in the order of the story; none without the module
    /// of the timelines
    [[nodiscard]] PlanChronology* chronology() const { return m_chronology; }
    /// @brief The cards of the chronology, from the texts and the scenes of the board, in the
    /// order of reading
    [[nodiscard]] QList<PlanChronologyCard> chronologyCards() const;
    /// @brief The outline: the table of the texts
    [[nodiscard]] QTableView* outline() const { return m_outline; }
    /// @brief The model of the outline
    [[nodiscard]] PlotOutlineModel* outlineModel() const { return m_model; }

    /// @brief Whether a change of the board can be undone (@p canUndo) and one undone done
    /// again (@p canRedo): Edit > Undo and Redo
    void setUndoState(bool canUndo, bool canRedo);

    /// @name Edit > Undo, Redo, Cut, Paste and Delete on the board and in the outline
    /// @{
    [[nodiscard]] bool takesEdit(EditCommand command) const override;
    [[nodiscard]] bool canEditHere(EditCommand command) const override;
    void editHere(EditCommand command) override;
    QMetaObject::Connection followEditStates(QObject* receiver,
                                             std::function<void()> changed) override;
    /// @}

signals:
    /// @brief The text chosen on the board or in the outline changed; empty: none
    void currentCardChanged(const QString& elementId);
    /// @brief Open the text in the editor
    void openRequested(const QString& elementId);
    /// @brief Keep what was written on a card (F2) or in a cell of the outline
    void editFinished(const QString& elementId, const kalahari::gui::PlotCardEdit& edit);
    /// @brief Give the text state @p status ("draft"...)
    void statusRequested(const QString& elementId, const QString& status);
    /// @brief Give the card its own color @p color (PlotColors::ownNames()); empty: none
    void colorRequested(const QString& elementId, const QString& color);
    /// @brief Move the text @p steps places back (less than 0) or forward; a scene among the
    /// scenes of its text
    void moveRequested(const QString& elementId, int steps);
    /// @brief Move scene @p cardId to the text before (@p direction less than 0) or after
    void moveToTextRequested(const QString& cardId, int direction);
    /// @brief Move the text before or after text @p anchorId
    void dropBesideRequested(const QString& elementId, const QString& anchorId, bool after);
    /// @brief Move the text to the start of part @p partId
    void dropInPartRequested(const QString& elementId, const QString& partId);
    /// @brief Add a text after text @p afterId, a scene after scene @p afterId; empty: a text at
    /// the end of the main section
    void addRequested(const QString& afterId);
    /// @brief Add a text in the plan after text @p afterId; empty: at the end of the main
    /// section
    void addPlannedRequested(const QString& afterId);
    /// @brief Add a scene at the end of the text of card @p cardId (Alt+Insert)
    void addSceneRequested(const QString& cardId);
    /// @brief Add a scene written after scene @p cardId, or at the end of text @p cardId
    /// (Alt+Shift+Insert)
    void newSceneRequested(const QString& cardId);
    /// @brief Delete the text from the book, or the scene from its text
    void deleteRequested(const QString& elementId);
    /// @brief Mark the text to move (Ctrl+X)
    void cutRequested(const QString& elementId);
    /// @brief Move the text marked after text @p afterId (Ctrl+V)
    void pasteRequested(const QString& afterId);
    /// @brief Forget the text marked (Esc)
    void cutCancelled();
    /// @brief Undo the last change of the board
    void undoRequested();
    /// @brief Redo the last change of the board undone
    void redoRequested();
    /// @brief Renumber the titles of the texts as the order of the book says (the button of
    /// offerRenumbering())
    void renumberRequested();
    /// @brief Show the text in the Properties panel with the keys there (Alt+Enter)
    void propertiesRequested(const QString& elementId);
    /// @brief The settings show other scenes (PlotBoard::SCENES_VARIANT_SETTING): the board is
    /// to be filled again
    void scenesVariantChanged();
    /// @brief What Edit > Undo, Redo, Cut, Paste and Delete can do may have changed
    void editStatesChanged();
    /// @brief Open Settings > Plot Board (the gear over the board and the outline)
    void settingsRequested();
    /// @brief The writer chose volume @p index of the series in the list Volume
    void volumeChosen(int index);
    /// @brief View @p view is shown (showView()): PlotBoard::VIEW_BOARD...
    void viewShown(const QString& view);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildHeader(QWidget* header);
    /// @brief The bar of the commands over the switches (plot_board_panel_commands.cpp)
    void buildCommands(QWidget* bar);
    /// @brief The commands follow the card chosen, what can be undone and the book
    void updateCommands();
    /// @brief The line under the board says what the card chosen is
    void updateHint();
    /// @brief What the line under the board says of card @p cardId
    [[nodiscard]] QString hintOf(const QString& cardId) const;
    /// @brief Ask for command @p which with the card chosen
    void runCommand(Command which);
    /// @brief The board and the outline in a stack, and the bar under them
    void buildBody(QVBoxLayout* layout);
    void connectBoard();
    void connectOutline();
    /// @brief The chronology asks the panel for what the board and the outline ask
    void connectChronology();
    /// @brief Keep what was written on card @p cardId of the chronology
    void onChronologyEdited(const QString& cardId, const PlanChronologyEdit& edit);
    /// @brief The look follows the settings and the theme
    void followSettings();
    QAction* addMenuAction(const QString& text, const QString& keys, bool enabled,
                           const std::function<void()>& run);
    /// @brief The states of the card in menu @p states
    void fillStateMenu(QMenu* states, const QString& elementId,
                       const std::optional<PlotCard>& found);
    /// @brief The own colors of the card in menu @p colors
    void fillColorMenu(QMenu* colors, const QString& elementId,
                       const std::optional<PlotCard>& found);
    /// @brief Whether the card marked can go after card @p elementId
    [[nodiscard]] bool canPasteAfter(const QString& elementId) const;
    void updateTotals();
    void updateSwitches();
    /// @brief The button over the outline with the menu of the columns it shows
    [[nodiscard]] QToolButton* buildColumnsButton(QWidget* header);
    /// @brief The gear over the board and the outline: settingsRequested()
    [[nodiscard]] QToolButton* buildSettingsButton(QWidget* header) const;
    /// @brief The columns of the outline as the settings say (PlotBoard::OUTLINE_SETTING)
    void applyOutlineColumns();
    void onOutlineEdited(const QString& elementId, int column, const QString& value);
    void showMenu(const QString& elementId, const QPoint& globalPos);
    /// @brief Whether a key of the outline is the board's (false: the table's own key), and
    /// with @p run, do what it does
    bool outlineKey(const QKeyEvent* key, bool run);
    /// @brief Whether the outline takes key @p action of the board
    [[nodiscard]] bool takesKey(PlotBoardView::Key action) const;
    void doOutlineKey(PlotBoardView::Key action);

    /// @brief Whether the chronology is in front
    [[nodiscard]] bool showsChronology() const;

    QList<PlotBand> m_bands;
    bool m_readOnly = false;
    bool m_canUndo = false;  ///< setUndoState()
    bool m_canRedo = false;  ///< setUndoState()
    bool m_offersRenumbering = false;  ///< The button of the bar renumbers: offerRenumbering()
    QStackedWidget* m_stack = nullptr;
    PlotBoardView* m_board = nullptr;
    QTableView* m_outline = nullptr;
    PlanChronology* m_chronology = nullptr;
    QWidget* m_commandBar = nullptr;  ///< Of the board and the outline
    PlotOutlineModel* m_model = nullptr;
    PlotOutlineDelegate* m_delegate = nullptr;
    InfoBar* m_infoBar = nullptr;
    QLabel* m_hint = nullptr;  ///< What the card chosen is, under the board
    std::array<QAction*, static_cast<std::size_t>(Command::Count)> m_commands{};
    QToolButton* m_openButton = nullptr;  ///< Named on a card in the plan
    QMenu* m_colorMenu = nullptr;         ///< Of the button Card Color
    QMenu* m_stateMenu = nullptr;         ///< Of the button State
    QLabel* m_volumeLabel = nullptr;
    QComboBox* m_volumeBox = nullptr;
    QStringList m_volumeNames;  ///< setVolumes()
    QToolButton* m_boardButton = nullptr;
    QToolButton* m_outlineButton = nullptr;
    QToolButton* m_chronologyButton = nullptr;
    QLabel* m_colorLabel = nullptr;
    QToolButton* m_scenesButton = nullptr;
    QLabel* m_layoutLabel = nullptr;
    QToolButton* m_columnsButton = nullptr;
    QToolButton* m_gridButton = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QToolButton* m_smallButton = nullptr;
    QToolButton* m_mediumButton = nullptr;
    QToolButton* m_largeButton = nullptr;
    QComboBox* m_colorBy = nullptr;
    QToolButton* m_outlineColumnsButton = nullptr;  ///< buildColumnsButton()
    QLabel* m_totals = nullptr;
    QMenu* m_menu = nullptr;
    QStringList m_povs;
    QStringList m_places;
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id
    /// Which scenes the settings showed last (PlotBoard::scenesVariant())
    QString m_scenesVariant;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H
