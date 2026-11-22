// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_plan.cpp
/// @brief The schedule of a book: when each of its texts is written and when it is revised

#include "kalahari/core/writing_plan.h"

#include "kalahari/core/writing_goals.h"
#include "kalahari/core/writing_statistics.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <limits>
#include <numeric>

namespace kalahari::core {

namespace {

const QString DAY_FORMAT = QStringLiteral("yyyy-MM-dd");

QDate earlier(const QDate& a, const QDate& b)
{
    if (!a.isValid()) {
        return b;
    }
    if (!b.isValid()) {
        return a;
    }
    return std::min(a, b);
}

QDate later(const QDate& a, const QDate& b)
{
    if (!a.isValid()) {
        return b;
    }
    if (!b.isValid()) {
        return a;
    }
    return std::max(a, b);
}

void insertDay(QJsonObject& object, const QString& name, const QDate& day)
{
    if (day.isValid()) {
        object.insert(name, day.toString(DAY_FORMAT));
    }
}

QDate dayOf(const QJsonObject& object, const QString& name)
{
    return QDate::fromString(object.value(name).toString(), DAY_FORMAT);
}

/// @brief The days of a plan are spread over at most this many days (ten years)
constexpr qint64 MAX_SPREAD_DAYS = 3660;

}  // namespace

// =============================================================================
// TextPlan
// =============================================================================

bool TextPlan::hasWriting() const
{
    return start.isValid() && end.isValid() && start <= end;
}

bool TextPlan::hasRevision() const
{
    return revisionStart.isValid() && revisionEnd.isValid() && revisionStart <= revisionEnd;
}

bool TextPlan::isEmpty() const
{
    return !hasWriting() && !hasRevision() && targetWords <= 0;
}

QDate TextPlan::firstDay() const
{
    QDate day;
    if (hasWriting()) {
        day = start;
    }
    if (hasRevision()) {
        day = earlier(day, revisionStart);
    }
    return day;
}

QDate TextPlan::lastDay() const
{
    QDate day;
    if (hasWriting()) {
        day = end;
    }
    if (hasRevision()) {
        day = later(day, revisionEnd);
    }
    return day;
}

void TextPlan::shift(int days)
{
    for (QDate* day : {&start, &end, &revisionStart, &revisionEnd}) {
        if (day->isValid()) {
            *day = day->addDays(days);
        }
    }
}

// =============================================================================
// WritingPlan
// =============================================================================

QString WritingPlan::settingKeyOf(const QString& goalsKey)
{
    const QString goals = QString::fromLatin1(BookGoals::SETTING_KEY);
    QString plan = QString::fromLatin1(SETTING_KEY);
    if (goalsKey.startsWith(goals)) {
        plan += goalsKey.mid(goals.size());
    }
    return plan;
}

const TextPlan* WritingPlan::find(const QString& elementId) const
{
    const auto it = std::find_if(texts.cbegin(), texts.cend(), [&elementId](const TextPlan& plan) {
        return plan.elementId == elementId;
    });
    return it != texts.cend() ? &*it : nullptr;
}

void WritingPlan::set(const TextPlan& plan)
{
    if (plan.elementId.isEmpty()) {
        return;
    }
    if (plan.isEmpty()) {
        remove(plan.elementId);
        return;
    }
    const auto it = std::find_if(texts.begin(), texts.end(), [&plan](const TextPlan& text) {
        return text.elementId == plan.elementId;
    });
    if (it != texts.end()) {
        *it = plan;
    } else {
        texts.append(plan);
    }
}

bool WritingPlan::remove(const QString& elementId)
{
    return texts.removeIf([&elementId](const TextPlan& plan) {
               return plan.elementId == elementId;
           }) > 0;
}

QDate WritingPlan::firstDay() const
{
    QDate day;
    for (const TextPlan& plan : texts) {
        day = earlier(day, plan.firstDay());
    }
    return day;
}

QDate WritingPlan::lastDay() const
{
    QDate day;
    for (const TextPlan& plan : texts) {
        day = later(day, plan.lastDay());
    }
    return day;
}

QString WritingPlan::toJson() const
{
    QJsonArray list;
    for (const TextPlan& plan : texts) {
        if (plan.elementId.isEmpty() || plan.isEmpty()) {
            continue;
        }
        QJsonObject entry;
        entry.insert(QStringLiteral("elementId"), plan.elementId);
        if (plan.hasWriting()) {
            insertDay(entry, QStringLiteral("start"), plan.start);
            insertDay(entry, QStringLiteral("end"), plan.end);
        }
        if (plan.hasRevision()) {
            insertDay(entry, QStringLiteral("revisionStart"), plan.revisionStart);
            insertDay(entry, QStringLiteral("revisionEnd"), plan.revisionEnd);
        }
        if (plan.targetWords > 0) {
            entry.insert(QStringLiteral("targetWords"), plan.targetWords);
        }
        list.append(entry);
    }
    QJsonObject object;
    if (!list.isEmpty()) {
        object.insert(QStringLiteral("texts"), list);
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

WritingPlan WritingPlan::fromJson(const QString& text)
{
    WritingPlan plan;
    const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
    const QJsonArray list = object.value(QStringLiteral("texts")).toArray();
    for (qsizetype i = 0; i < list.size(); ++i) {
        const QJsonObject entry = list.at(i).toObject();
        TextPlan item;
        item.elementId = entry.value(QStringLiteral("elementId")).toString();
        item.start = dayOf(entry, QStringLiteral("start"));
        item.end = dayOf(entry, QStringLiteral("end"));
        item.revisionStart = dayOf(entry, QStringLiteral("revisionStart"));
        item.revisionEnd = dayOf(entry, QStringLiteral("revisionEnd"));
        item.targetWords = std::max(0, entry.value(QStringLiteral("targetWords")).toInt());
        if (!item.hasWriting()) {
            item.start = QDate();
            item.end = QDate();
        }
        if (!item.hasRevision()) {
            item.revisionStart = QDate();
            item.revisionEnd = QDate();
        }
        if (!item.elementId.isEmpty() && !item.isEmpty() && plan.find(item.elementId) == nullptr) {
            plan.texts.append(item);
        }
    }
    return plan;
}

int dailyGoalForPlan(const WritingRules& rules, const WritingPlan& plan,
                     const QHash<QString, PlannedText>& texts, const QDate& today,
                     QStringList* counted)
{
    if (counted != nullptr) {
        counted->clear();
    }
    if (!today.isValid() || writingDaysBetween(rules, today, today) == 0) {
        return 0;
    }
    qint64 goal = 0;
    for (const TextPlan& text : plan.texts) {
        const auto found = texts.constFind(text.elementId);
        if (found == texts.constEnd() || !found->beingWritten || !text.hasWriting()
            || text.start > today || text.targetWords <= found->words) {
            continue;
        }
        const int lacking = text.targetWords - found->words;
        // Its bar ended: all it lacks is for today
        const int days = text.end >= today ? writingDaysBetween(rules, today, text.end) : 0;
        goal += wordsPerWritingDay(lacking, days);
        if (counted != nullptr) {
            counted->append(text.elementId);
        }
    }
    return int(std::min<qint64>(goal, std::numeric_limits<int>::max()));
}

QList<std::pair<QDate, QDate>> spreadOverWritingDays(const WritingRules& rules,
                                                    const QList<int>& wordsLeft,
                                                    const QDate& from, const QDate& to)
{
    QList<std::pair<QDate, QDate>> bars;
    if (wordsLeft.isEmpty() || !from.isValid() || !to.isValid() || to < from) {
        return bars;
    }
    QList<QDate> days;
    const QDate last = from.daysTo(to) < MAX_SPREAD_DAYS ? to : from.addDays(MAX_SPREAD_DAYS - 1);
    for (QDate day = from; day <= last; day = day.addDays(1)) {
        if (writingDaysBetween(rules, day, day) > 0) {
            days.append(day);
        }
    }
    if (days.isEmpty()) {
        return bars;
    }
    // Each text takes its words on a line of all the words; the days cut that line in equal
    // pieces, and a text has the days its words fall in
    QList<qint64> weights;
    for (const int words : wordsLeft) {
        weights.append(std::max(0, words));
    }
    qint64 total = std::accumulate(weights.cbegin(), weights.cend(), qint64(0));
    if (total == 0) {
        std::fill(weights.begin(), weights.end(), qint64(1));
        total = weights.size();
    }
    const qint64 count = days.size();
    qint64 before = 0;
    for (const qint64 weight : weights) {
        const qint64 after = before + weight;
        const qint64 first = std::min(count - 1, before * count / total);
        const qint64 end = std::clamp((after * count + total - 1) / total - 1, first, count - 1);
        bars.append({days.at(first), days.at(end)});
        before = after;
    }
    return bars;
}

}  // namespace kalahari::core
