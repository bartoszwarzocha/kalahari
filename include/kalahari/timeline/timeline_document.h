// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_document.h
/// @brief A timeline open on its tab: its file, its changes and their undoing, the event
/// selected

#ifndef KALAHARI_TIMELINE_TIMELINE_DOCUMENT_H
#define KALAHARI_TIMELINE_TIMELINE_DOCUMENT_H

#include <kalahari/timeline/timeline.h>

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

namespace kalahari::timeline {

/// @brief A timeline open on its tab: each change is saved to its file at once and can be
/// undone
///
/// The views (the axis, the list, the time and the book) change the timeline only through
/// change(), which keeps the timeline as it was for Undo, saves the file and tells the views.
/// The selected event is the same in all of them.
class TimelineDocument : public QObject {
    Q_OBJECT

public:
    /// @brief The timeline of file @p path, empty until load()
    explicit TimelineDocument(const QString& path, QObject* parent = nullptr);

    /// @brief Read the file; a file that is missing or cannot be read leaves an empty timeline
    /// @return Whether the file was read
    bool load(QString* error = nullptr);

    [[nodiscard]] const Timeline& timeline() const { return m_timeline; }
    [[nodiscard]] const QString& path() const { return m_path; }

    /// @brief Change the timeline with @p change, as one step of Undo called @p what
    ///
    /// The timeline is kept as it was when @p change returns false, or changes nothing. Saves
    /// the file and emits changed().
    /// @param what What Undo undoes: "moving “Kompas”"
    /// @return Whether the timeline changed
    bool change(const QString& what, const std::function<bool(Timeline&)>& change);

    /// @brief Change event @p id with @p change, as one step of Undo called @p what
    bool changeEvent(const QString& id, const QString& what,
                     const std::function<void(TimelineEvent&)>& change);

    /// @brief Change the view of the timeline (its scale, lanes, zoom...): saved, but not a
    /// step of Undo
    void changeView(const std::function<void(TimelineView&)>& change);

    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }
    /// @brief What Undo undoes; empty: nothing
    [[nodiscard]] QString undoText() const;
    /// @brief What Redo does again
    [[nodiscard]] QString redoText() const;
    void undo();
    void redo();

    /// @brief The event selected; empty: none
    [[nodiscard]] const QString& selected() const { return m_selected; }
    /// @brief The lane of the event selected, where it is in several; empty: its first
    [[nodiscard]] const QString& selectedLane() const { return m_selectedLane; }
    /// @brief Select event @p id (empty: none) in lane @p lane
    void select(const QString& id, const QString& lane = QString());

    /// @brief Whether the last save failed
    [[nodiscard]] bool saveFailed() const { return m_saveFailed; }

    /// @brief The most steps Undo keeps
    static constexpr qsizetype MOST_STEPS = 200;

signals:
    /// @brief The timeline changed: a change, Undo or Redo
    void changed();
    /// @brief The view of the timeline changed (changeView())
    void viewChanged();
    /// @brief Another event is selected
    void selectionChanged();
    /// @brief The file could not be written; @p error says why
    void saveError(const QString& error);

private:
    /// A step of Undo or Redo: the timeline before it, and the event selected then
    struct Step {
        QJsonObject timeline;
        QString selected;
        QString what;
    };

    void save();
    void restore(const Step& step);

    QString m_path;
    Timeline m_timeline;
    QList<Step> m_undo;
    QList<Step> m_redo;
    QString m_selected;
    QString m_selectedLane;
    bool m_saveFailed = false;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_DOCUMENT_H
