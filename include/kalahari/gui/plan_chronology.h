// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology.h
/// @brief The chronology of the Book Plan: what the program knows of the view that the module
/// of the timelines gives it (IPlanChronologyProvider)

#ifndef KALAHARI_GUI_PLAN_CHRONOLOGY_H
#define KALAHARI_GUI_PLAN_CHRONOLOGY_H

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

#include <memory>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief A card of the chronology: a scene of a text, or a text without scene breaks, as the
/// board shows it
struct PlanChronologyCard {
    QString id;           ///< Its id on the board (PlotCard::id)
    QString number;       ///< Its place in the order of reading: "3" a text, "3.2" a scene
    QString title;        ///< Its title; of a scene without one "Scene 2"
    QString description;  ///< Its short description
    QString pov;          ///< Point of view; empty: not said
    QString place;        ///< Place of the action; empty: not said
    QString time;         ///< Time of the action as the writer typed it; empty: not said
    QString textTitle;    ///< The title of its text
    QColor color;         ///< The color of its card on the board; invalid: none
    bool planned = false; ///< In the plan: no text yet
    bool textTime = false;  ///< A scene without a time of its own: time is that of its text

    bool operator==(const PlanChronologyCard&) const = default;
};

/// @brief What the writer changed of a card in the chronology (F2)
struct PlanChronologyEdit {
    QString title;
    QString description;
    QString pov;
    QString place;
    QString time;

    bool operator==(const PlanChronologyEdit&) const = default;
};

/// @brief The chronology of the plan of the book: the scenes in the order of the story, with
/// the events outside the text
///
/// The board gives it the cards in the order of reading (setCards()) and the plan of the time
/// of its book (setPlan()): the events outside the text, day one, the writer's periods and how
/// the chronology shows them, as its .ktl file keeps them. A change of a card goes back to the
/// board (cardEdited()); a change of the plan comes as a new plan (planEdited()), and the board
/// keeps both on its Undo.
class PlanChronology : public QWidget {
    Q_OBJECT

public:
    using QWidget::QWidget;

    /// @brief The cards of the book, in the order of reading
    virtual void setCards(const QList<PlanChronologyCard>& cards) = 0;

    /// @brief The plan of the time of the book, as its file keeps it; empty: none yet
    virtual void setPlan(const QJsonObject& plan) = 0;

    /// @brief No changes: the book is open to read
    virtual void setReadOnly(bool readOnly) = 0;

    /// @brief Whether Undo and Redo of the board have something to do
    virtual void setUndoState(bool canUndo, bool canRedo) = 0;

    /// @brief The card chosen: its id on the board, or "event:" and the id of an event
    /// outside the text; empty: none
    [[nodiscard]] virtual QString currentCard() const = 0;

    /// @brief Choose card @p id
    /// @return Whether the chronology has it
    virtual bool setCurrentCard(const QString& id) = 0;

    /// @brief What the line under the chronology says of the card chosen
    [[nodiscard]] virtual QString hint() const = 0;

    /// @brief The keys go to the cards
    virtual void focusCards() = 0;

    /// @brief The prefix of the ids of the events outside the text
    static constexpr const char* EVENT_PREFIX = "event:";

signals:
    /// @brief Another card is chosen
    void currentChanged();

    /// @brief Open the text of card @p cardId (Enter)
    void openRequested(const QString& cardId);

    /// @brief The writer changed card @p cardId of the book
    void cardEdited(const QString& cardId, const kalahari::gui::PlanChronologyEdit& edit);

    /// @brief The writer changed the plan of the time: an event outside the text, day one, a
    /// period; @p plan is the new one, @p undoText what Undo undoes, @p message what the bar
    /// under the chronology says
    void planEdited(const QJsonObject& plan, const QString& undoText, const QString& message);

    /// @brief The writer changed how the chronology shows the plan (its scale, its bands): kept
    /// in its file, but no step of Undo
    void planViewChanged(const QJsonObject& plan);

    /// @brief Ctrl+Z and Ctrl+Y in the chronology
    void undoRequested();
    void redoRequested();

    /// @brief Say @p message in the bar under the chronology
    void told(const QString& message);
};

/// @brief The time of a text of the book that a timeline of the Workshop says: an event of the
/// timeline told in the text ("In the book")
struct PlanTextTime {
    QString elementId;  ///< The text
    QString time;       ///< As the plan writes it: "Dzień 3", "14 kwietnia 1931"
    QString pov;        ///< The first character of the event, by name; empty: none
    QString place;      ///< Its place, by name; empty: none
    QString timeline;   ///< The title of the timeline

    bool operator==(const PlanTextTime&) const = default;
};

/// @brief What the timelines of the Workshop say of the texts of the book
struct PlanTimelineTimes {
    QList<PlanTextTime> texts;  ///< One time of each text, the earliest of its events
    QJsonObject plan;           ///< A new plan with day one of the timeline of the first of them
};

/// @brief The module that gives the Book Plan its chronology
class IPlanChronologyProvider {
public:
    IPlanChronologyProvider() = default;
    virtual ~IPlanChronologyProvider() = default;
    IPlanChronologyProvider(const IPlanChronologyProvider&) = delete;
    IPlanChronologyProvider& operator=(const IPlanChronologyProvider&) = delete;
    IPlanChronologyProvider(IPlanChronologyProvider&&) = delete;
    IPlanChronologyProvider& operator=(IPlanChronologyProvider&&) = delete;

    /// @brief A new chronology in @p parent
    [[nodiscard]] virtual PlanChronology* createChronology(QWidget* parent) = 0;

    /// @brief The plan of the time a new book starts with: day one today
    [[nodiscard]] virtual QJsonObject newPlan() const = 0;

    /// @brief The times that the timelines of the Workshop of @p book give its texts: the
    /// events told in them; the Book Plan offers them to the cards without a time once
    [[nodiscard]] virtual PlanTimelineTimes timelineTimes(
        const core::ProjectManager& book) const = 0;
};

/// @brief The module of the chronology the program has; none without one
[[nodiscard]] std::shared_ptr<IPlanChronologyProvider> planChronologyProvider();

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLAN_CHRONOLOGY_H
