// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_activity.cpp
/// @brief The record of writing in 5-minute slots, and the recorder that collects it

#include "kalahari/core/writing_activity.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace kalahari::core {

int activitySlotOf(const QTime& time)
{
    if (!time.isValid()) {
        return 0;
    }
    return (time.hour() * 60 + time.minute()) / ACTIVITY_SLOT_MINUTES;
}

QDateTime activitySlotStart(const QDate& date, int slot)
{
    const int days = slot / ACTIVITY_SLOTS_PER_DAY;
    const int inDay = slot % ACTIVITY_SLOTS_PER_DAY;
    return QDateTime(date.addDays(days), QTime(0, 0).addSecs(inDay * ACTIVITY_SLOT_MINUTES * 60));
}

// =============================================================================
// WritingRecorder
// =============================================================================

void WritingRecorder::setPlace(const QString& bookId, const QString& elementId)
{
    m_bookId = bookId;
    m_elementId = elementId;
}

void WritingRecorder::setIdleSeconds(int seconds)
{
    m_idleMs = std::max(0, seconds) * qint64(1000);
}

void WritingRecorder::setLargeChangeWords(int words)
{
    m_largeChangeWords = std::max(0, words);
}

void WritingRecorder::noteActivity(const QDateTime& when)
{
    if (!when.isValid()) {
        return;
    }
    if (m_lastActivity.isValid() && when > m_lastActivity &&
        m_lastActivity.msecsTo(when) <= m_idleMs) {
        addTime(m_lastActivity, when);
    }
    // A clock set back starts a new stretch from the new time
    m_lastActivity = when;
}

bool WritingRecorder::recordWords(int delta, const QDateTime& when)
{
    if (delta == 0 || !when.isValid()) {
        return true;
    }
    if (m_largeChangeWords > 0 && std::abs(delta) > m_largeChangeWords) {
        return false;
    }
    WritingActivity& entry = entryAt(when);
    if (delta > 0) {
        entry.written += delta;
        m_written += delta;
    } else {
        entry.deleted -= delta;
        m_deleted -= delta;
    }
    return true;
}

void WritingRecorder::recordPasted(int delta, const QDateTime& when)
{
    if (delta == 0 || !when.isValid()) {
        return;
    }
    entryAt(when).pasted += delta;
    m_pasted += delta;
}

void WritingRecorder::pause()
{
    m_lastActivity = QDateTime();
}

QList<WritingActivity> WritingRecorder::takePending()
{
    return std::exchange(m_pending, {});
}

void WritingRecorder::resetTotals()
{
    m_written = 0;
    m_deleted = 0;
    m_pasted = 0;
    m_activeMs = 0;
}

WritingActivity& WritingRecorder::entryAt(const QDateTime& when)
{
    WritingActivity place;
    place.date = when.date();
    place.slot = activitySlotOf(when.time());
    place.bookId = m_bookId;
    place.elementId = m_elementId;
    // The entry of the slot is almost always the last one
    for (auto it = m_pending.rbegin(); it != m_pending.rend(); ++it) {
        if (it->samePlace(place)) {
            return *it;
        }
    }
    m_pending.append(place);
    return m_pending.last();
}

void WritingRecorder::addTime(const QDateTime& from, const QDateTime& to)
{
    QDateTime current = from;
    while (current < to) {
        QDateTime end = activitySlotStart(current.date(), activitySlotOf(current.time()) + 1);
        // A change of the clock (daylight saving time) may give no later end
        if (end <= current || end > to) {
            end = to;
        }
        const qint64 ms = current.msecsTo(end);
        entryAt(current).activeMs += ms;
        m_activeMs += ms;
        current = end;
    }
}

}  // namespace kalahari::core
