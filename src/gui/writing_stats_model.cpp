/// @file writing_stats_model.cpp
/// @brief The statistics of writing of the writer and of the open book

#include "kalahari/gui/writing_stats_model.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/statistics_collector.h"

#include <QTimer>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// @brief The days the record is read from and to: all of it
const QDate FIRST_DAY(2000, 1, 1);
const QDate LAST_DAY(2999, 12, 31);

}  // namespace

WritingStatsModel::WritingStatsModel(QObject* parent)
    : QObject(parent)
    , m_rules(core::WritingRules::fromSettings())
    , m_journalRecord(m_rules)
    , m_bookRecord(m_rules)
    , m_changeTimer(new QTimer(this))
    , m_minuteTimer(new QTimer(this))
{
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(CHANGE_INTERVAL_MS);
    connect(m_changeTimer, &QTimer::timeout, this, &WritingStatsModel::changed);

    m_minuteTimer->setInterval(60 * 1000);
    connect(m_minuteTimer, &QTimer::timeout, this, &WritingStatsModel::changed);
    m_minuteTimer->start();

    // The settings may change on another thread: the rules change on this one
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.rfind("stats.", 0) == 0) {
                QMetaObject::invokeMethod(this, "reloadRules", Qt::QueuedConnection);
            }
        });
}

WritingStatsModel::~WritingStatsModel()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

bool WritingStatsModel::openJournal(const QString& path)
{
    if (!m_journal.open(path)) {
        core::Logger::getInstance().warn("WritingStatsModel: the writer's journal cannot be "
                                         "opened: {}",
                                         m_journal.lastError().toStdString());
        return false;
    }
    m_journalRecord.setActivity(m_journal.activity(FIRST_DAY, LAST_DAY));
    if (m_collector) {
        m_collector->setJournal(&m_journal);
    }
    schedule();
    return true;
}

void WritingStatsModel::setCollector(editor::StatisticsCollector* collector)
{
    if (m_collector == collector) {
        return;
    }
    if (m_collector) {
        disconnect(m_collector, nullptr, this, nullptr);
    }
    m_collector = collector;
    if (m_collector) {
        m_collector->setJournal(m_journal.isOpen() ? &m_journal : nullptr);
        m_collector->setRules(m_rules);
        m_collector->setBookId(m_bookId);
        connect(m_collector, &QObject::destroyed, this, [this]() { m_collector = nullptr; });
        connect(m_collector, &editor::StatisticsCollector::writingRecorded, this,
                &WritingStatsModel::schedule);
        // What it saved is in the book and in the journal now
        connect(m_collector, &editor::StatisticsCollector::activitySaved, this,
                [this](const QList<core::WritingActivity>& activity) {
                    if (m_journal.isOpen()) {
                        m_journalRecord.addActivity(activity);
                    }
                    if (m_database != nullptr) {
                        m_bookRecord.addActivity(activity);
                    }
                    schedule();
                });
    }
    schedule();
}

editor::StatisticsCollector* WritingStatsModel::collector() const
{
    return m_collector;
}

void WritingStatsModel::setBook(core::ProjectDatabase* database, const QString& bookId,
                                bool readOnly)
{
    m_database = database;
    m_bookId = database != nullptr ? bookId : QString();
    m_bookReadOnly = database != nullptr && readOnly;
    if (m_collector) {
        m_collector->setBookId(m_bookId);
    }
    readBook();
    emit goalsChanged();
    schedule();
}

void WritingStatsModel::readBook()
{
    m_goals = {};
    m_bookRecord.setActivity({});
    if (m_database == nullptr || !m_database->isOpen()) {
        return;
    }
    m_bookRecord.setActivity(
        m_database->writingActivity(FIRST_DAY, LAST_DAY));
    m_goals = core::BookGoals::fromJson(
        m_database->getSetting(QString::fromLatin1(core::BookGoals::SETTING_KEY)).toString());
}

void WritingStatsModel::setBookWordsSource(std::function<int()> source)
{
    m_bookWords = std::move(source);
    schedule();
}

int WritingStatsModel::bookWords() const
{
    return m_database != nullptr && m_bookWords ? m_bookWords() : 0;
}

int WritingStatsModel::bookWordsAtStartOfToday() const
{
    return bookWords() - bookStatistics().textGrowth(today());
}

bool WritingStatsModel::setGoals(const core::BookGoals& goals)
{
    if (m_database == nullptr || !m_database->isOpen() || m_bookReadOnly) {
        return false;
    }
    core::BookGoals sorted = goals;
    sorted.sortDeadlines();
    if (sorted == m_goals) {
        return true;
    }
    m_database->setSetting(QString::fromLatin1(core::BookGoals::SETTING_KEY), sorted.toJson());
    m_goals = sorted;
    emit goalsChanged();
    schedule();
    return true;
}

void WritingStatsModel::reloadRules()
{
    const core::WritingRules rules = core::WritingRules::fromSettings();
    m_rules = rules;
    m_journalRecord.setRules(rules);
    m_bookRecord.setRules(rules);
    if (m_collector) {
        m_collector->setRules(rules);
    }
    schedule();
}

core::WritingStatistics WritingStatsModel::goalStatistics() const
{
    return m_rules.allBooks ? journalStatistics() : bookStatistics();
}

core::WritingStatistics WritingStatsModel::bookStatistics() const
{
    core::WritingStatistics statistics = m_bookRecord;
    if (m_database != nullptr && m_collector) {
        statistics.addActivity(m_collector->pendingActivity());
    }
    return statistics;
}

core::WritingStatistics WritingStatsModel::journalStatistics() const
{
    core::WritingStatistics statistics = m_journalRecord;
    if (m_collector) {
        statistics.addActivity(m_collector->pendingActivity());
    }
    return statistics;
}

QDateTime WritingStatsModel::now() const
{
    return m_clock ? m_clock() : QDateTime::currentDateTime();
}

void WritingStatsModel::setClock(std::function<QDateTime()> clock)
{
    m_clock = std::move(clock);
    schedule();
}

QDate WritingStatsModel::today() const
{
    return m_journalRecord.writingDayOf(now());
}

QList<DayPiece> WritingStatsModel::dayPieces(const core::WritingStatistics& statistics,
                                             const QDate& day, int minutes)
{
    // The pieces of the graph of the day, as WritingStatistics::dayGraph() takes them
    constexpr int DAY_MINUTES = 24 * 60;
    int piece = std::clamp(minutes - minutes % core::ACTIVITY_SLOT_MINUTES,
                           core::ACTIVITY_SLOT_MINUTES, DAY_MINUTES);
    while (DAY_MINUTES % piece != 0) {
        piece -= core::ACTIVITY_SLOT_MINUTES;
    }
    QList<DayPiece> pieces(DAY_MINUTES / piece);
    const auto& record = statistics.record();
    const auto found = record.constFind(day);
    if (found == record.constEnd()) {
        return pieces;
    }
    const int dayStart = statistics.rules().dayStartMinutes;
    for (const core::WritingActivity& kept : found.value()) {
        const qint64 offset = day.daysTo(kept.date) * DAY_MINUTES +
                              qint64(kept.slot) * core::ACTIVITY_SLOT_MINUTES - dayStart;
        if (offset < 0 || offset >= DAY_MINUTES) {
            continue;
        }
        // The words pasted as the rules count them
        const core::WritingActivity entry = statistics.counted(kept);
        DayPiece& target = pieces[int(offset / piece)];
        target.written += entry.written;
        target.deleted += entry.deleted;
        target.activeMs += entry.activeMs;
    }
    return pieces;
}

std::pair<int, int> WritingStatsModel::shownMinutes(const QList<DayPiece>& pieces,
                                                    int pieceMinutes, int dayStart,
                                                    int fromHour, int toHour, int nowMinute)
{
    constexpr int DAY_MINUTES = 24 * 60;
    // The minute of the day at or before a minute where an hour of the clock starts
    const auto hourBefore = [dayStart](int minute) {
        const int clock = ((minute + dayStart) % 60 + 60) % 60;
        return minute - clock;
    };
    const auto ofDay = [dayStart](int clockMinute) {
        return ((clockMinute - dayStart) % DAY_MINUTES + DAY_MINUTES) % DAY_MINUTES;
    };
    int from = ofDay(fromHour * 60);
    int to = ofDay(toHour * 60);
    if (to == 0) {
        to = DAY_MINUTES;
    }
    // Hours across the start of the writing day (0:00 to 24:00 with a day that starts at
    // 4:00, or a night from 22:00 to 6:00) are two parts of it: the whole day shows them
    if (to <= from) {
        from = 0;
        to = DAY_MINUTES;
    }
    for (int i = 0; i < pieces.size(); ++i) {
        if (pieces[i].written != 0 || pieces[i].deleted != 0) {
            from = std::min(from, i * pieceMinutes);
            to = std::max(to, (i + 1) * pieceMinutes);
        }
    }
    from = std::min(from, nowMinute);
    to = std::max(to, nowMinute + 1);
    from = std::max(0, hourBefore(from));
    const int before = hourBefore(to);
    to = std::min(DAY_MINUTES, before == to ? to : before + 60);
    return {from, to};
}

qint64 WritingStatsModel::minutesIntoDay(const QDate& day, const QDateTime& when) const
{
    // In minutes of the clock, as the slots of the record count them: a change of the clock
    // adds or takes no hour, and the day starts on its slot, as the record rounds it
    constexpr qint64 DAY_MINUTES = 24 * 60;
    const int dayStart = m_journalRecord.rules().dayStartMinutes;
    return day.daysTo(when.date()) * DAY_MINUTES + when.time().msecsSinceStartOfDay() / 60000 -
           dayStart;
}

int WritingStatsModel::dailyGoalToday() const
{
    const QDate day = today();
    // Without a length or with the deadline passed, the writer's own daily goal again, as the
    // tab of the goals shows it
    if (m_goals.dailyGoalFromDeadline && m_database != nullptr && m_goals.hasDeadlineAhead(day)) {
        const int goal =
            core::dailyGoalForDeadline(m_rules, m_goals, bookWordsAtStartOfToday(), day);
        return m_rules.isWritingDay(day) ? goal : 0;
    }
    return m_rules.dailyGoalOn(day);
}

void WritingStatsModel::schedule()
{
    if (!m_changeTimer->isActive()) {
        m_changeTimer->start();
    }
}

}  // namespace kalahari::gui
