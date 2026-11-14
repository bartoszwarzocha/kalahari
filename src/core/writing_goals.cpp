// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_goals.cpp
/// @brief The goals of a book and its deadlines, and the reckoning of the words they need

#include "kalahari/core/writing_goals.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>

namespace kalahari::core {

namespace {

const QString DAY_FORMAT = QStringLiteral("yyyy-MM-dd");

/// @brief The writing days of the week; every day when the writer has none
int writingDayMask(const WritingRules& rules)
{
    const int mask = rules.writingDays & 0x7f;
    return mask != 0 ? mask : 0x7f;
}

bool isWritingDay(int mask, const QDate& day)
{
    return (mask & (1 << (day.dayOfWeek() - 1))) != 0;
}

}  // namespace

// =============================================================================
// BookGoals
// =============================================================================

bool BookGoals::isEmpty() const
{
    return targetWords <= 0 && !deadline.isValid() && deadlines.isEmpty();
}

void BookGoals::sortDeadlines()
{
    std::stable_sort(deadlines.begin(), deadlines.end(),
                     [](const WritingDeadline& a, const WritingDeadline& b) {
                         if (a.date.isValid() != b.date.isValid()) {
                             return a.date.isValid();
                         }
                         return a.date < b.date;
                     });
}

bool BookGoals::hasDeadlineAhead(const QDate& today) const
{
    return targetWords > 0 && deadline.isValid() && today.isValid() && deadline >= today;
}

int BookGoals::nextDeadline(const QDate& today) const
{
    int next = -1;
    for (int i = 0; i < deadlines.size(); ++i) {
        const QDate& date = deadlines[i].date;
        if (date.isValid() && date >= today && (next < 0 || date < deadlines[next].date)) {
            next = i;
        }
    }
    return next;
}

QString BookGoals::toJson() const
{
    QJsonObject object;
    if (targetWords > 0) {
        object.insert(QStringLiteral("targetWords"), targetWords);
    }
    if (deadline.isValid()) {
        object.insert(QStringLiteral("deadline"), deadline.toString(DAY_FORMAT));
    }
    if (dailyGoalFromDeadline) {
        object.insert(QStringLiteral("dailyGoalFromDeadline"), true);
    }
    QJsonArray list;
    for (const WritingDeadline& item : deadlines) {
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), item.name);
        if (item.date.isValid()) {
            entry.insert(QStringLiteral("date"), item.date.toString(DAY_FORMAT));
        }
        if (!item.elementId.isEmpty()) {
            entry.insert(QStringLiteral("elementId"), item.elementId);
        }
        if (item.targetWords > 0) {
            entry.insert(QStringLiteral("targetWords"), item.targetWords);
        }
        list.append(entry);
    }
    if (!list.isEmpty()) {
        object.insert(QStringLiteral("deadlines"), list);
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

BookGoals BookGoals::fromJson(const QString& text)
{
    BookGoals goals;
    const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
    goals.targetWords = std::max(0, object.value(QStringLiteral("targetWords")).toInt());
    goals.deadline = QDate::fromString(object.value(QStringLiteral("deadline")).toString(), DAY_FORMAT);
    goals.dailyGoalFromDeadline = object.value(QStringLiteral("dailyGoalFromDeadline")).toBool();
    const QJsonArray list = object.value(QStringLiteral("deadlines")).toArray();
    for (qsizetype i = 0; i < list.size(); ++i) {
        const QJsonObject entry = list.at(i).toObject();
        WritingDeadline item;
        item.name = entry.value(QStringLiteral("name")).toString();
        item.date = QDate::fromString(entry.value(QStringLiteral("date")).toString(), DAY_FORMAT);
        item.elementId = entry.value(QStringLiteral("elementId")).toString();
        item.targetWords = std::max(0, entry.value(QStringLiteral("targetWords")).toInt());
        goals.deadlines.append(item);
    }
    goals.sortDeadlines();
    return goals;
}

// =============================================================================
// The reckoning
// =============================================================================

int writingDaysBetween(const WritingRules& rules, const QDate& from, const QDate& to)
{
    if (!from.isValid() || !to.isValid() || to < from) {
        return 0;
    }
    const int mask = writingDayMask(rules);
    const qint64 days = from.daysTo(to) + 1;
    qint64 count = (days / 7) * std::popcount(unsigned(mask));
    for (QDate day = from.addDays((days / 7) * 7); day <= to; day = day.addDays(1)) {
        if (isWritingDay(mask, day)) {
            ++count;
        }
    }
    return int(std::min<qint64>(count, INT_MAX));
}

int wordsPerWritingDay(int wordsLeft, int writingDays)
{
    if (wordsLeft <= 0) {
        return 0;
    }
    if (writingDays <= 0) {
        return wordsLeft;
    }
    return int((qint64(wordsLeft) + writingDays - 1) / writingDays);
}

QDate forecastDate(const WritingRules& rules, const QDate& today, int wordsLeft,
                   double averagePerDay)
{
    if (!today.isValid()) {
        return {};
    }
    if (wordsLeft <= 0) {
        return today;
    }
    if (!(averagePerDay > 0.0)) {
        return {};
    }
    const double needed = std::ceil(wordsLeft / averagePerDay);
    const int mask = writingDayMask(rules);
    const int perWeek = std::popcount(unsigned(mask));
    if (needed / perWeek * 7.0 > 100 * 366.0) {
        return {};
    }
    // Whole weeks first, then day by day
    qint64 days = qint64(needed);
    const qint64 weeks = (days - 1) / perWeek;
    QDate day = today.addDays(weeks * 7);
    days -= weeks * perWeek;
    while (days > 0) {
        day = day.addDays(1);
        if (isWritingDay(mask, day)) {
            --days;
        }
    }
    return day;
}

int dailyGoalForDeadline(const WritingRules& rules, const BookGoals& goals,
                         int wordsAtStartOfToday, const QDate& today)
{
    if (!goals.hasDeadlineAhead(today)) {
        return 0;
    }
    return wordsPerWritingDay(goals.targetWords - wordsAtStartOfToday,
                              writingDaysBetween(rules, today, goals.deadline));
}

}  // namespace kalahari::core
