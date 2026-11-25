// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plot_board_coordinator.h
/// @brief The plot board of the book on its tab in the middle of the window, and the changes
/// of the book it asks for

#ifndef KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
#define KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/gui/plot_board.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

class QStatusBar;
class QTabWidget;
class QTimer;
class QUndoStack;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class NavigatorCoordinator;
class PlotBoardPanel;
struct PlotCardEdit;

/// @brief Opens the plot board (PlotBoardPanel) on a tab of its own and makes the changes of
/// the book it asks for
///
/// Book > Plot Board and Book > Outline show the one tab of the board with the board of the
/// cards or with the outline. The board follows the book: a text added, moved, renamed or
/// deleted anywhere, its state, its words; a board behind another tab is filled again when it
/// comes to the front.
///
/// What the writer changes on the board is undone with Ctrl+Z there, and done again with
/// Ctrl+Y: a card moved, a card edited, its state and its color. A change of the book made
/// elsewhere (the Navigator, the Properties panel), a text added or deleted, forgets what can
/// be undone, since the places the board remembers may be no more. Opening a text, a text
/// added or deleted and the properties are those of the Navigator (NavigatorCoordinator).
///
/// The cards of the scenes of a text show what its scene breaks say, also in a text open with
/// changes not saved; what the board changes of a scene goes to the scene break in the editor
/// of an open text (to be saved with the text), else to its file, and of the first scene to
/// the .klh file (core::ProjectManager::setScenePlot()). Enter on a scene opens its text at the
/// scene; a scene changed in the editor since is not undone on the board.
///
/// A scene moves among the scenes of its text, a new one comes after a scene or at the end of
/// a text, and one is deleted with its text or with its break only: the text changes at once,
/// in its editor when it is open (one step of its Ctrl+Z), and one Ctrl+Z on the board undoes
/// it. A scene moves to another text with its notes and the annotations anchored in it
/// (core::ChapterDocument::withSceneMovedTo()): both texts change, and one Ctrl+Z undoes both.
///
/// The scenes in the plans of the texts (core::PlannedScene, PlotBoard::SCENES_VARIANT_SETTING)
/// are in the .klh file: they are added, edited, moved (also to another text) and deleted
/// there, each one Ctrl+Z on the board. Enter on a scene in the plan beside the scenes of its
/// text writes it: a scene break with what it says comes at its place in the text, and the text
/// opens there.
///
/// A move that leaves the numbers in the titles the program gave out of the order of the book
/// ("Chapter 3"; PlotBoard::renumbered()) renumbers them as the settings say: the bar under the
/// board offers it (a step of its own), the move does it in its own step, or nothing happens.
/// The board also shows the texts of the front and back sections when the settings say so.
class PlotBoardCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Brings the Properties panel to the front; @p keys: with the keys in its first
    /// field (Alt+Enter)
    using PropertiesShower = std::function<void(bool keys)>;

    /// @brief Constructor
    /// @param book The book of its window
    /// @param centralTabs The tabs in the middle of the window
    /// @param navigator The commands of the elements of the book
    /// @param statusBar Where the board says what needs a book
    /// @param showProperties Brings the Properties panel to the front
    /// @param parent Parent QObject
    PlotBoardCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                         NavigatorCoordinator* navigator, QStatusBar* statusBar,
                         PropertiesShower showProperties, QObject* parent = nullptr);
    ~PlotBoardCoordinator() override;

    PlotBoardCoordinator(const PlotBoardCoordinator&) = delete;
    PlotBoardCoordinator& operator=(const PlotBoardCoordinator&) = delete;
    PlotBoardCoordinator(PlotBoardCoordinator&&) = delete;
    PlotBoardCoordinator& operator=(PlotBoardCoordinator&&) = delete;

    /// @brief Show the board on its tab, in front
    /// @param view PlotBoard::VIEW_BOARD or VIEW_OUTLINE, kept as the one shown last; empty:
    ///        the one shown last
    /// @param keys The board or the outline gets the keys
    /// @return The board; none without an open book
    PlotBoardPanel* openBoard(const QString& view, bool keys);

    /// @brief The board open on its tab; none
    [[nodiscard]] PlotBoardPanel* board() const;

    /// @brief Show element @p elementId of the book on the board, with the keys there (Show on
    /// the Plot Board in the Navigator): a text with its card chosen, a part with the card of
    /// its first text; empty: the board as it was
    void showOnBoard(const QString& elementId);

    /// @brief Give text in the plan @p textId its chapter file and open it (Create on its card,
    /// Create the Chapter File in the Navigator); Ctrl+Z on the board puts it back into the
    /// plan, its file to the Trash
    void createFile(const QString& textId);

    /// @brief What can be undone and done again on the board
    [[nodiscard]] QUndoStack* undoStack() const { return m_undo; }

    /// @brief The file of the plan of the time of the book in its folder: the events outside
    /// the text, day one and the periods of the chronology (a timeline, .ktl)
    static constexpr const char* PLAN_FILE = "book-plan.ktl";

    /// @brief Where the open book keeps the plan of its time (PLAN_FILE); empty: no book
    [[nodiscard]] QString planPath() const;

    /// @brief The card marked to move (Ctrl+X), a text or a scene; empty: none
    [[nodiscard]] QString markedCard() const { return m_marked; }

public slots:
    /// @brief Fill the board again from the open book: at once when it is in front, else when
    /// it comes to the front
    void refresh();

signals:
    /// @brief Open Settings > Plot Board (the gear over the board)
    void settingsRequested();

private:
    /// @brief What a card shows of its text that the writer can change: its title, its
    /// description and its plot
    struct CardText {
        QString title;
        QString description;
        core::ElementPlot plot;

        bool operator==(const CardText&) const = default;
    };

    /// @brief What the scenes of a text are: its text with its first scene, and its plan
    struct ScenesState {
        core::SceneText text;
        QList<core::PlannedScene> plan;

        bool operator==(const ScenesState&) const = default;
    };

    /// @brief A change of the scenes of text @p textId from @p before to @p wanted
    struct ScenesChange {
        QString textId;
        ScenesState before;
        ScenesState wanted;
    };

    void refreshNow();
    void connectBoard(PlotBoardPanel* panel);

    /// @name The plan of the time of the book in the chronology
    /// (plot_board_coordinator_chronology.cpp)
    /// @{
    void connectChronology(PlotBoardPanel* panel);
    /// @brief The chronology shows the plan of the open book, read from its file once
    void showPlan();
    /// @brief The plan in file @p path; a new one when there is none yet
    [[nodiscard]] static QJsonObject readPlan(const QString& path);
    /// @brief Keep @p plan in file @p path (planPath() of a volume)
    /// @return false when the book is open to read or the file could not be written
    bool writePlan(const QString& path, const QJsonObject& plan);
    /// @brief Keep @p plan in file @p path, with the view the chronology has now, and show
    /// it: Undo and Redo of a change of the plan
    bool changePlan(const QString& path, const QJsonObject& plan);
    /// @brief The chronology shown first for a book without a plan file: when the timelines
    /// of the Workshop say when its texts happen ("In the book"), offer to copy those times,
    /// points of view and places to the cards without a time, in one step of Undo; the
    /// timelines stay as they are
    void offerTimelineTimes();
    /// @}
    void updateTab();
    void onCurrentTabChanged();
    /// @brief The book was saved: a change made elsewhere forgets what can be undone
    void onProjectChanged();
    /// @brief Another book, or none: the board forgets what it remembers
    void forget();
    /// @brief Volume @p index of the series chosen in the list Volume: it becomes the current
    /// volume, whose texts the board shows
    void showVolume(int index);

    void open(const QString& elementId);
    void edit(const QString& elementId, const PlotCardEdit& edit);
    void setStatus(const QString& elementId, const QString& status);
    void setColor(const QString& elementId, const QString& color);
    void move(const QString& elementId, int steps);
    void moveToText(const QString& cardId, int direction);
    void dropBeside(const QString& elementId, const QString& anchorId, bool after);
    void dropInPart(const QString& elementId, const QString& partId);
    void add(const QString& afterId);
    /// @name The texts in the plan, and new texts (plot_board_coordinator_texts.cpp)
    /// @{
    /// @brief Add a text after text @p afterId (empty: at the end of the main section), in the
    /// plan without a file (@p planned), and edit its card; one Ctrl+Z takes it out again
    void addText(const QString& afterId, bool planned);
    /// @brief Delete text in the plan @p textId, which has nothing but its card; Ctrl+Z puts
    /// it back
    void removePlannedText(const QString& textId);
    /// @brief Add a scene written, a scene break: after scene @p cardId, else at the end of
    /// text @p cardId
    void newScene(const QString& cardId);
    /// @brief Take text @p elementId added on the board out of the book again: its file to the
    /// Trash; @p kml gets what its file had
    bool takeOutText(const QString& elementId, std::optional<QString>& kml);
    /// @brief Put @p element back at @p place, with its file of @p kml
    bool putBackText(const core::ProjectElement& element, const core::ElementPlace& place,
                     const std::optional<QString>& kml);
    /// @brief Why text @p elementId cannot leave the book or the plan now: changes not saved in
    /// its tab; empty: it can
    [[nodiscard]] QString openChangesOf(const QString& elementId) const;
    /// @}
    /// @brief Insert on the card of scene @p cardId: a scene after it, in the plan when the
    /// board shows the plans
    void addAfterScene(const QString& cardId);
    void addScene(const QString& cardId);
    void remove(const QString& elementId);
    void cut(const QString& elementId);
    void paste(const QString& afterId);
    void cancelCut();
    void undo();
    void redo();
    void showProperties(const QString& cardId, bool keys);
    /// @brief Move scene @p cardId dropped before or after scene @p anchorId, or first among
    /// the scenes of text @p anchorId
    void dropSceneBeside(const QString& cardId, const QString& anchorId, bool after);
    /// @brief Move the scene marked (@p marked) after card @p afterId: after a scene, or first
    /// among the scenes of a text
    void pasteScene(const QString& marked, const QString& afterId);

    /// @name The scenes of the texts
    /// @{
    /// @brief Give scene @p cardId (PlotBoard::sceneId()) @p plot and remember it to be undone;
    /// @p message says what was done
    void changeSceneWithUndo(const QString& cardId, const core::ScenePlot& plot,
                             const QString& message, const QString& undoText);
    /// @brief Give scene @p scene of text @p textId @p plot when it still says @p before: in
    /// the editor of the text when it is open, else in the book
    /// @return false when the scene says something else now (m_changedElsewhere), or the book
    ///         could not take the change
    bool changeScene(const QString& textId, int scene, const core::ScenePlot& before,
                     const core::ScenePlot& plot);
    /// @brief What scene @p scene of text @p textId says now, also in its open editor;
    /// nullopt: the text has no such scene
    [[nodiscard]] std::optional<core::ScenePlot> scenePlotOf(const QString& textId,
                                                             int scene) const;
    /// @brief Move the scene of text @p textId at @p slot to place @p to of the scenes the board
    /// shows (without it): a scene of the text moves in its text, and the scenes in the plan
    /// keep their places among the others
    void placeScene(const QString& textId, const PlotBoard::SceneSlot& slot, int to);
    /// @brief Add a scene at place @p at of the scenes of text @p textId and edit its card;
    /// @p message says where it is
    void addSceneAt(const QString& textId, int at, const QString& message);
    /// @brief Ask whether to delete scene @p scene of text @p textId with its text or its break
    /// only, and do it
    void removeScene(const QString& textId, int scene);
    /// @brief Delete scene @p scene of text @p textId (now @p text) with its text, or its
    /// break only (@p keepText)
    void deleteScene(const QString& textId, const core::SceneText& text, int scene,
                     bool keepText);
    /// @brief The card chosen after scene @p scene of the @p count scenes of text @p textId is
    /// deleted: the scene beside, or the text when it has one scene then
    [[nodiscard]] static QString cardAfterRemoving(const QString& textId, int scene, int count,
                                                   bool keepText);
    /// @brief Give text @p textId @p wanted, remember it to be undone and choose card
    /// @p cardId; @p message says what was done
    /// @return Whether the book took the change
    bool changeScenes(const QString& textId, const ScenesState& wanted, const QString& cardId,
                      const QString& message, const QString& undoText);
    /// @brief Make @p changes (of one text or two), remember them as one step to be undone and
    /// choose card @p cardId; @p message says what was done
    /// @return Whether the book took the changes
    bool changeScenes(QList<ScenesChange> changes, const QString& cardId, const QString& message,
                      const QString& undoText);
    /// @brief Make @p changes from what the texts are now, @p from (in the order of the
    /// changes), to what they want, or back (@p back, the last one first) to what they were
    /// before; @p now gets what each text is then
    /// @return false when a text is something else now, or the book could not take a change
    bool replaceAll(const QList<ScenesChange>& changes, const QList<ScenesState>& from, bool back,
                    QList<ScenesState>& now);
    /// @brief Forget the scene marked to move (Ctrl+X) when @p changes change its text
    void forgetMarkedOf(const QList<ScenesChange>& changes);
    /// @brief Give text @p textId @p wanted when what changes is still @p expected: its text in
    /// its editor when it is open, else in the book, its plan in the book; @p now gets what it
    /// is then
    /// @return false when it is something else now (m_changedElsewhere), or the book could not
    ///         take the change
    bool replaceScenes(const QString& textId, const ScenesState& expected,
                       const ScenesState& wanted, ScenesState& now);
    /// @brief Whether what changes from @p expected to @p wanted is still as @p expected in
    /// @p current: the text may have changed in its editor since a change of its plan
    [[nodiscard]] static bool isAsExpected(const ScenesState& current,
                                           const ScenesState& expected,
                                           const ScenesState& wanted);
    /// @brief Give text @p textId, now @p current, @p wanted: in its editor when it is open,
    /// else in the book
    bool writeSceneText(const QString& textId, const core::SceneText& current,
                        const core::SceneText& wanted);
    /// @brief Give text @p textId plan @p plan in the book
    bool writePlan(const QString& textId, const QList<core::PlannedScene>& plan);
    /// @brief Text @p textId and its first scene as they are now, also in its open editor
    [[nodiscard]] std::optional<core::SceneText> sceneTextOf(const QString& textId) const;
    /// @brief The scenes of text @p textId as they are now: its text and its plan
    [[nodiscard]] std::optional<ScenesState> scenesStateOf(const QString& textId) const;
    /// @brief The scenes of text @p state in the order of the board, as its variant shows them
    [[nodiscard]] static QList<PlotBoard::SceneSlot> orderOf(const ScenesState& state);
    /// @brief How many scenes text @p textId has now
    [[nodiscard]] int sceneCount(const QString& textId) const;
    /// @brief The KML of the open texts with changes not saved, by the ids of their elements
    [[nodiscard]] QHash<QString, QString> unsavedTexts() const;
    /// @brief The editor of text @p elementId open in a tab; nullptr: none
    [[nodiscard]] editor::BookEditor* editorOf(const QString& elementId) const;
    /// @}

    /// @name The scenes in the plans of the texts, and the scenes moved to other texts
    /// @{
    /// @brief Write scene @p index of the plan of text @p textId: a scene break with what it
    /// says at its place in the text; the text opens there
    void writePlanned(const QString& textId, int index);
    /// @brief Give scene @p index of the plan of text @p textId what card editor @p edit says
    void editPlanned(const QString& textId, int index, const PlotCardEdit& edit);
    /// @brief Give scene @p index of the plan of text @p textId its own color @p color
    void colorPlanned(const QString& textId, int index, const QString& color,
                      const QString& message, const QString& undoText);
    /// @brief Add a scene to the plan of text @p textId at place @p at of the scenes the board
    /// shows, and edit its card; @p message says where it is
    void addPlanned(const QString& textId, int at, const QString& message);
    /// @brief Delete scene @p index from the plan of text @p textId
    void removePlanned(const QString& textId, int index);
    /// @brief Move scene @p index of the plan of text @p textId to the plan of text @p targetId,
    /// at place @p at of the scenes the board shows there
    void movePlanned(const QString& textId, int index, const QString& targetId, int at);
    /// @brief Move scene @p scene of text @p textId to text @p targetId, at place @p at of the
    /// scenes the board shows there, with its text, what it says and its annotations; in an
    /// empty text in place of its empty scene
    void moveSceneToText(const QString& textId, int scene, const QString& targetId, int at);
    /// @brief Move the scene of text @p textId at @p slot to text @p targetId, at place @p at of
    /// the scenes the board shows there
    void moveSlotToText(const QString& textId, const PlotBoard::SceneSlot& slot,
                        const QString& targetId, int at);
    /// @brief Move the scene of text @p textId at @p slot to the text before (@p direction
    /// less than 0: at its end) or after (at its start)
    void moveToTextBeside(const QString& textId, const PlotBoard::SceneSlot& slot,
                          int direction);
    /// @brief The text of the board before (@p direction less than 0) or after text @p textId;
    /// empty: none
    [[nodiscard]] QString textBeside(const QString& textId, int direction) const;
    /// @}

    /// @brief Move text @p elementId to @p place (in its list without it) and remember it to
    /// be undone; the numbers in the titles as the settings say (PlotBoard::RENUMBER_SETTING)
    void moveWithUndo(const QString& elementId, const core::ElementPlace& place);

    /// @name The numbers in the titles the program gave ("Chapter 3")
    /// @{
    /// @brief Renumber the titles as the order of the book says, as one step to be undone (the
    /// button Renumber of the bar under the board)
    void renumberTitles();
    /// @brief Give the texts @p titles after a move, when the settings say so at once
    /// (PlotBoard::RENUMBER_AUTO); @p former gets the titles they had
    /// @return The titles given; none when the settings say otherwise, or the book could not
    ///         take them (the titles stay as they were)
    QList<PlotBoard::TitleChange> renumberAtOnce(const QList<PlotBoard::TitleChange>& titles,
                                                 QList<PlotBoard::TitleChange>& former);
    /// @brief Say @p moved under the board after a move: with what happened to the numbers in
    /// the titles, @p outOfOrder (they no longer follow the order) and @p renumbered (at once)
    void sayMoved(const QString& moved, bool outOfOrder, bool renumbered);
    /// @brief Give the texts the titles of @p titles
    /// @return false when the book could not take a title
    bool retitle(const QList<PlotBoard::TitleChange>& titles);
    /// @brief The titles the texts of @p titles have now
    [[nodiscard]] QList<PlotBoard::TitleChange> formerTitles(
        const QList<PlotBoard::TitleChange>& titles) const;
    /// @}
    /// @brief Give the card of text @p elementId @p text and remember it to be undone;
    /// @p message says what was done
    void changeWithUndo(const QString& elementId, const CardText& text, const QString& message,
                        const QString& undoText);
    /// @brief Remember a change made on the board, @p undoText after "Undone:": Ctrl+Z calls
    /// @p undoIt and Ctrl+Y @p redoIt, each false when the book could not take it; @p undoGuard
    /// says why Ctrl+Z cannot undo it now (empty: it can), and then nothing is undone
    void pushChange(const QString& undoText, std::function<bool()> undoIt,
                    std::function<bool()> redoIt, std::function<QString()> undoGuard = {});

    /// @name The changes of the book, done and undone
    /// @return false when the book could not take the change
    /// @{
    bool moveTo(const QString& elementId, const core::ElementPlace& place);
    bool changeCard(const QString& elementId, const CardText& text);
    bool changeStatus(const QString& elementId, const QString& status);
    /// @}

    /// @brief What a card of text @p elementId shows that the writer can change
    [[nodiscard]] CardText cardTextOf(const QString& elementId) const;
    /// @brief The title of text @p elementId; of a scene card, its title or "The Storm, Scene 2"
    [[nodiscard]] QString titleOf(const QString& elementId) const;
    /// @brief The title of text @p textId; empty: no such text
    [[nodiscard]] QString textTitleOf(const QString& textId) const;
    /// @brief Say @p message under the board; @p canUndo: with Undo
    void say(const QString& message, bool canUndo = false);
    /// @brief Tell the writer that the book could not take a change
    void notSaved();
    /// @brief Tell the writer that a scene undone or done again changed in the editor since,
    /// else that the book could not take the change
    void changedOrNotSaved();
    [[nodiscard]] QWidget* window() const;

    core::ProjectManager& m_book;  ///< The book of its window
    QTabWidget* m_centralTabs;
    NavigatorCoordinator* m_navigator;
    QStatusBar* m_statusBar;
    PropertiesShower m_showProperties;
    PlotBoardPanel* m_panel = nullptr;  ///< The board on its tab; none once it is deleted
    QUndoStack* m_undo = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QString m_marked;     ///< The card marked to move (Ctrl+X): a text or a scene
    bool m_stale = false;  ///< The board behind another tab missed a change of the book
    int m_ownChanges = 0;  ///< > 0 while the board changes the book itself
    QString m_planPath;    ///< The file of the plan shown (planPath()); empty: none read yet
    QJsonObject m_plan;    ///< The plan of the time of the book, as its file keeps it
    QSet<QString> m_timesOffered;  ///< The plan files offerTimelineTimes() asked about
    bool m_changedElsewhere = false;  ///< The scene undone last was changed in the editor
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
