// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_document.cpp
/// @brief A timeline open on its tab: its file, its changes and their undoing

#include <kalahari/timeline/timeline_document.h>

#include <kalahari/core/logger.h>

#include <QFileInfo>

namespace kalahari::timeline {

TimelineDocument::TimelineDocument(const QString& path, QObject* parent)
    : QObject(parent), m_path(path) {}

bool TimelineDocument::load(QString* error) {
    Timeline loaded;
    if (!QFileInfo::exists(m_path) || !loaded.load(m_path, error)) {
        core::Logger::getInstance().warn(
            "TimelineDocument: {} cannot be read; the timeline is empty", m_path.toStdString());
        return false;
    }
    m_timeline = loaded;
    m_selected.clear();
    m_selectedLane.clear();
    m_undo.clear();
    m_redo.clear();
    emit changed();
    return true;
}

bool TimelineDocument::change(const QString& what, const std::function<bool(Timeline&)>& change) {
    const Step before{m_timeline.toJson(), m_selected, what};
    if (!change(m_timeline)) {
        m_timeline.fromJson(before.timeline);
        return false;
    }
    if (m_timeline.toJson() == before.timeline) {
        return false;
    }
    m_undo.append(before);
    if (m_undo.size() > MOST_STEPS) {
        m_undo.removeFirst();
    }
    m_redo.clear();
    if (!m_selected.isEmpty() && m_timeline.event(m_selected) == nullptr) {
        m_selected.clear();
        m_selectedLane.clear();
    }
    save();
    emit changed();
    return true;
}

bool TimelineDocument::changeEvent(const QString& id, const QString& what,
                                   const std::function<void(TimelineEvent&)>& change) {
    return this->change(what, [&id, &change](Timeline& timeline) {
        const TimelineEvent* event = timeline.event(id);
        if (event == nullptr) {
            return false;
        }
        TimelineEvent changed = *event;
        change(changed);
        return timeline.setEvent(changed);
    });
}

void TimelineDocument::changeView(const std::function<void(TimelineView&)>& change) {
    TimelineView view = m_timeline.view();
    change(view);
    if (view != m_timeline.view()) {
        m_timeline.setView(view);
        save();
        emit viewChanged();
    }
}

QString TimelineDocument::undoText() const {
    return m_undo.isEmpty() ? QString() : m_undo.last().what;
}

QString TimelineDocument::redoText() const {
    return m_redo.isEmpty() ? QString() : m_redo.last().what;
}

void TimelineDocument::restore(const Step& step) {
    // The view of the timeline (its scale, lanes, zoom) stays as it is now
    const TimelineView view = m_timeline.view();
    m_timeline.fromJson(step.timeline);
    m_timeline.setView(view);
    m_selected = m_timeline.event(step.selected) != nullptr ? step.selected : QString();
    m_selectedLane.clear();
    save();
    emit changed();
    emit selectionChanged();
}

void TimelineDocument::undo() {
    if (m_undo.isEmpty()) {
        return;
    }
    const Step step = m_undo.takeLast();
    m_redo.append(Step{m_timeline.toJson(), m_selected, step.what});
    restore(step);
}

void TimelineDocument::redo() {
    if (m_redo.isEmpty()) {
        return;
    }
    const Step step = m_redo.takeLast();
    m_undo.append(Step{m_timeline.toJson(), m_selected, step.what});
    restore(step);
}

void TimelineDocument::select(const QString& id, const QString& lane) {
    const QString wanted = m_timeline.event(id) != nullptr ? id : QString();
    const QString wantedLane = wanted.isEmpty() ? QString() : lane;
    if (wanted == m_selected && wantedLane == m_selectedLane) {
        return;
    }
    m_selected = wanted;
    m_selectedLane = wantedLane;
    emit selectionChanged();
}

void TimelineDocument::save() {
    QString error;
    m_saveFailed = !m_timeline.save(m_path, &error);
    if (m_saveFailed) {
        core::Logger::getInstance().error("TimelineDocument: {} cannot be written: {}",
                                          m_path.toStdString(), error.toStdString());
        emit saveError(error);
    }
}

}  // namespace kalahari::timeline
