// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_plan.cpp
/// @brief The schedule of a book: when each of its texts is written and when it is revised

#include "kalahari/core/writing_plan.h"

#include "kalahari/core/writing_goals.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

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

}  // namespace kalahari::core
