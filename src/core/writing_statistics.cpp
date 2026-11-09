/// @file writing_statistics.cpp
/// @brief The statistics of writing counted from the record of writing

#include "kalahari/core/writing_statistics.h"

#include "kalahari/core/settings_manager.h"

#include <algorithm>
#include <bit>
#include <string>
#include <utility>

namespace kalahari::core {

namespace {

constexpr int MINUTES_PER_DAY = 24 * 60;

/// @brief The time of the writing day, in minutes from its start, that a slot starts at
int minutesInWritingDay(const QDate& writingDay, const WritingActivity& activity,
                        int dayStartMinutes)
{
    return int(writingDay.daysTo(activity.date)) * MINUTES_PER_DAY +
           activity.slot * ACTIVITY_SLOT_MINUTES - dayStartMinutes;
}

/// @brief The number of days from @p from to @p to (both included) that @p counts
template<typename Counts>
int countDays(const QDate& from, const QDate& to, Counts counts)
{
    int days = 0;
    for (QDate day = from; day <= to; day = day.addDays(1)) {
        if (counts(day)) {
            ++days;
        }
    }
    return days;
}

}  // namespace

// =============================================================================
// WritingRules
// =============================================================================

bool WritingRules::isWritingDay(const QDate& day) const
{
    return day.isValid() && (writingDays & (1 << (day.dayOfWeek() - 1))) != 0;
}

int WritingRules::dailyGoalOn(const QDate& day) const
{
    return isWritingDay(day) ? std::max(0, dailyGoal) : 0;
}

int WritingRules::weeklyGoalWords() const
{
    if (weeklyGoal > 0) {
        return weeklyGoal;
    }
    return std::max(0, dailyGoal) * std::popcount(unsigned(writingDays & 0x7f));
}

WritingRules WritingRules::fromSettings()
{
    const auto& settings = SettingsManager::getInstance();
    WritingRules rules;
    rules.dayStartMinutes = std::clamp(settings.get<int>("stats.dayStart"), 0, MINUTES_PER_DAY - 1);
    rules.idleMinutes = std::max(1, settings.get<int>("stats.idleMinutes"));
    rules.largeChangeWords = settings.get<bool>("stats.skipLargeChanges")
                                 ? std::max(1, settings.get<int>("stats.largeChangeWords"))
                                 : 0;
    rules.counting = settings.get<std::string>("stats.goalCounting") == "written"
                         ? GoalCounting::Written
                         : GoalCounting::Net;
    rules.allBooks = settings.get<bool>("stats.dailyGoalAllBooks");
    rules.writingDays = settings.get<int>("stats.writingDays") & 0x7f;
    rules.dailyGoal = std::max(0, settings.get<int>("stats.dailyGoal"));
    rules.weeklyGoal = std::max(0, settings.get<int>("stats.weeklyGoal"));
    rules.streakRule = settings.get<std::string>("stats.streakRule") == "dailyGoal"
                           ? StreakRule::DailyGoal
                           : StreakRule::MinimumWords;
    rules.streakMinimumWords = std::max(1, settings.get<int>("stats.streakMinimumWords"));
    return rules;
}

// =============================================================================
// WritingTotals
// =============================================================================

double WritingTotals::wordsPerHour(GoalCounting counting) const
{
    if (activeMs <= 0) {
        return 0.0;
    }
    return counted(counting) * 3600000.0 / double(activeMs);
}

void WritingTotals::add(const WritingActivity& activity)
{
    written += activity.written;
    deleted += activity.deleted;
    activeMs += activity.activeMs;
}

WritingTotals& WritingTotals::operator+=(const WritingTotals& other)
{
    written += other.written;
    deleted += other.deleted;
    activeMs += other.activeMs;
    return *this;
}

// =============================================================================
// WritingHourMap
// =============================================================================

int WritingHourMap::bestDayOfWeek() const
{
    int best = 0;
    double bestWords = 0.0;
    for (int day = 0; day < 7; ++day) {
        double sum = 0.0;
        for (double hour : words[day]) {
            sum += hour;
        }
        if (sum > bestWords) {
            bestWords = sum;
            best = day + 1;
        }
    }
    return best;
}

int WritingHourMap::bestHours(int width) const
{
    width = std::clamp(width, 1, 24);
    int best = -1;
    double bestWords = 0.0;
    for (int first = 0; first < 24; ++first) {
        double sum = 0.0;
        for (const auto& day : words) {
            for (int i = 0; i < width; ++i) {
                sum += day[(first + i) % 24];
            }
        }
        if (sum > bestWords) {
            bestWords = sum;
            best = first;
        }
    }
    return best;
}

std::pair<int, int> WritingHourMap::bestTime(int width) const
{
    width = std::clamp(width, 1, 24);
    std::pair<int, int> best{0, -1};
    double bestWords = 0.0;
    for (int day = 0; day < 7; ++day) {
        for (int first = 0; first < 24; ++first) {
            double sum = 0.0;
            for (int i = 0; i < width; ++i) {
                sum += words[day][(first + i) % 24];
            }
            if (sum > bestWords) {
                bestWords = sum;
                best = {day + 1, first};
            }
        }
    }
    return best;
}

double WritingHourMap::shareOfHours(int fromHour, int toHour) const
{
    fromHour = std::clamp(fromHour, 0, 24) % 24;
    toHour = std::clamp(toHour, 0, 24);
    double all = 0.0;
    double part = 0.0;
    for (const auto& day : words) {
        for (int hour = 0; hour < 24; ++hour) {
            all += day[hour];
            const bool inside = fromHour <= toHour ? hour >= fromHour && hour < toHour
                                                   : hour >= fromHour || hour < toHour;
            if (inside) {
                part += day[hour];
            }
        }
    }
    return all > 0.0 ? part / all : 0.0;
}

double WritingHourMap::shareOfDays(int fromDay, int toDay) const
{
    fromDay = std::clamp(fromDay, 1, 7);
    toDay = std::clamp(toDay, 1, 7);
    double all = 0.0;
    double part = 0.0;
    for (int day = 1; day <= 7; ++day) {
        double sum = 0.0;
        for (double hour : words[day - 1]) {
            sum += hour;
        }
        all += sum;
        const bool inside = fromDay <= toDay ? day >= fromDay && day <= toDay
                                             : day >= fromDay || day <= toDay;
        if (inside) {
            part += sum;
        }
    }
    return all > 0.0 ? part / all : 0.0;
}

// =============================================================================
// WritingStatistics
// =============================================================================

WritingStatistics::WritingStatistics(const WritingRules& rules)
{
    setRules(rules);
}

void WritingStatistics::setRules(const WritingRules& rules)
{
    const int oldDayStart = m_rules.dayStartMinutes;
    m_rules = rules;
    const int dayStart = std::clamp(rules.dayStartMinutes, 0, MINUTES_PER_DAY - 1);
    m_rules.dayStartMinutes = dayStart - dayStart % ACTIVITY_SLOT_MINUTES;
    if (m_rules.dayStartMinutes != oldDayStart) {
        regroup();
    }
}

void WritingStatistics::setActivity(const QList<WritingActivity>& activity)
{
    m_byDay.clear();
    m_days.clear();
    addActivity(activity);
}

void WritingStatistics::addActivity(const QList<WritingActivity>& activity)
{
    for (const WritingActivity& entry : activity) {
        if (!entry.isEmpty() && entry.date.isValid()) {
            addToDays(entry);
        }
    }
}

QDate WritingStatistics::writingDayOf(const QDate& date, int slot) const
{
    if (slot * ACTIVITY_SLOT_MINUTES < m_rules.dayStartMinutes) {
        return date.addDays(-1);
    }
    return date;
}

QDate WritingStatistics::writingDayOf(const QDateTime& when) const
{
    return writingDayOf(when.date(), activitySlotOf(when.time()));
}

QDate WritingStatistics::firstDay() const
{
    return m_days.isEmpty() ? QDate() : m_days.firstKey();
}

WritingTotals WritingStatistics::day(const QDate& writingDay) const
{
    return m_days.value(writingDay);
}

WritingTotals WritingStatistics::period(const QDate& from, const QDate& to) const
{
    WritingTotals totals;
    for (auto it = m_days.lowerBound(from); it != m_days.end() && it.key() <= to; ++it) {
        totals += it.value();
    }
    return totals;
}

QList<int> WritingStatistics::dayGraph(const QDate& writingDay, int minutes) const
{
    // A piece is whole slots and the day is whole pieces: 5, 10, 15, 20, 30, 60 minutes...
    int piece = std::clamp(minutes, ACTIVITY_SLOT_MINUTES, MINUTES_PER_DAY);
    piece -= piece % ACTIVITY_SLOT_MINUTES;
    while (MINUTES_PER_DAY % piece != 0) {
        piece -= ACTIVITY_SLOT_MINUTES;
    }

    QList<int> graph(MINUTES_PER_DAY / piece, 0);
    for (const WritingActivity& entry : m_byDay.value(writingDay)) {
        const int at = minutesInWritingDay(writingDay, entry, m_rules.dayStartMinutes);
        if (at >= 0 && at < MINUTES_PER_DAY) {
            graph[at / piece] += m_rules.counting == GoalCounting::Net
                                     ? entry.written - entry.deleted
                                     : entry.written;
        }
    }
    return graph;
}

QHash<QString, WritingTotals> WritingStatistics::texts(const QDate& from, const QDate& to) const
{
    QHash<QString, WritingTotals> result;
    for (auto it = m_byDay.lowerBound(from); it != m_byDay.end() && it.key() <= to; ++it) {
        for (const WritingActivity& entry : it.value()) {
            result[entry.elementId].add(entry);
        }
    }
    return result;
}

int WritingStatistics::streakWords() const
{
    const int minimum = std::max(1, m_rules.streakMinimumWords);
    if (m_rules.streakRule == StreakRule::DailyGoal && m_rules.dailyGoal > 0) {
        return m_rules.dailyGoal;
    }
    return minimum;
}

bool WritingStatistics::countsToStreak(const QDate& writingDay) const
{
    return day(writingDay).counted(m_rules.counting) >= streakWords();
}

int WritingStatistics::streak(const QDate& today) const
{
    const QDate first = firstDay();
    if (!today.isValid() || !first.isValid()) {
        return 0;
    }
    int days = countsToStreak(today) ? 1 : 0;
    // Before the first day of the record nothing keeps the days going, and no days are lost
    for (QDate day = today.addDays(-1); day >= first; day = day.addDays(-1)) {
        if (countsToStreak(day)) {
            ++days;
        } else if (m_rules.isWritingDay(day)) {
            break;
        }
    }
    return days;
}

double WritingStatistics::averagePerWritingDay(const QDate& from, const QDate& to) const
{
    const QDate first = firstDay();
    if (!first.isValid() || !from.isValid() || !to.isValid()) {
        return 0.0;
    }
    const QDate start = std::max(from, first);
    const int days = countDays(start, to, [this](const QDate& day) {
        return m_rules.isWritingDay(day);
    });
    if (days == 0) {
        return 0.0;
    }
    return double(period(start, to).counted(m_rules.counting)) / days;
}

WritingHourMap WritingStatistics::hourMap(const QDate& from, const QDate& to) const
{
    WritingHourMap map;
    const QDate first = firstDay();
    if (!first.isValid() || !from.isValid() || !to.isValid()) {
        return map;
    }
    const QDate start = std::max(from, first);
    for (auto it = m_byDay.lowerBound(start); it != m_byDay.end() && it.key() <= to; ++it) {
        const int dayOfWeek = it.key().dayOfWeek() - 1;
        for (const WritingActivity& entry : it.value()) {
            const int hour = entry.slot * ACTIVITY_SLOT_MINUTES / 60;
            map.words[dayOfWeek][hour] += entry.written;
        }
    }
    // The average of each day of the week: the sum over the number of such days
    for (int dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek) {
        const int days = countDays(start, to, [dayOfWeek](const QDate& day) {
            return day.dayOfWeek() == dayOfWeek;
        });
        if (days > 1) {
            for (double& hour : map.words[dayOfWeek - 1]) {
                hour /= days;
            }
        }
    }
    return map;
}

WritingTemperature WritingStatistics::temperature(const QDateTime& now, int windowMinutes,
                                                  int historyDays) const
{
    if (!now.isValid()) {
        return WritingTemperature::Unknown;
    }
    const QDate today = writingDayOf(now);
    const WritingTotals usual = period(today.addDays(-std::max(1, historyDays)), today.addDays(-1));
    if (usual.activeMs < qint64(TEMPERATURE_HISTORY_MINUTES) * 60 * 1000) {
        return WritingTemperature::Unknown;
    }

    // The slots of the window: the slot of now and the ones before it
    const int windowSlots = std::max(1, (windowMinutes + ACTIVITY_SLOT_MINUTES - 1) / ACTIVITY_SLOT_MINUTES);
    const QDateTime last = activitySlotStart(now.date(), activitySlotOf(now.time()));
    const QDateTime first = last.addSecs(-qint64(windowSlots - 1) * ACTIVITY_SLOT_MINUTES * 60);
    WritingTotals recent;
    for (auto it = m_byDay.lowerBound(writingDayOf(first)); it != m_byDay.end() && it.key() <= today;
         ++it) {
        for (const WritingActivity& entry : it.value()) {
            const QDateTime start = activitySlotStart(entry.date, entry.slot);
            if (start >= first && start <= last) {
                recent.add(entry);
            }
        }
    }
    if (recent.written == 0 && recent.deleted == 0) {
        return WritingTemperature::Idle;
    }

    // Much: more words that count than usually in the same time of writing
    const double usualMinutes = usual.activeMs / 60000.0;
    const double usualWords = usual.counted(m_rules.counting) / usualMinutes * windowSlots *
                              ACTIVITY_SLOT_MINUTES;
    const bool much = recent.counted(m_rules.counting) > usualWords;
    // Fast: more words written and deleted in a minute of writing than usually
    const double usualSpeed = (usual.written + usual.deleted) / usualMinutes;
    const bool fast = recent.activeMs > 0 &&
                      (recent.written + recent.deleted) / (recent.activeMs / 60000.0) > usualSpeed;

    if (much) {
        return fast ? WritingTemperature::MuchFast : WritingTemperature::MuchSlow;
    }
    return fast ? WritingTemperature::LittleFast : WritingTemperature::LittleSlow;
}

void WritingStatistics::regroup()
{
    QList<WritingActivity> all;
    for (const QList<WritingActivity>& day : std::as_const(m_byDay)) {
        all.append(day);
    }
    setActivity(all);
}

void WritingStatistics::addToDays(const WritingActivity& activity)
{
    const QDate writingDay = writingDayOf(activity.date, activity.slot);
    m_byDay[writingDay].append(activity);
    m_days[writingDay].add(activity);
}

}  // namespace kalahari::core
