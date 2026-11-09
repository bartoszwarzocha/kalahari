/// @file writing_awards_tracker.cpp
/// @brief The badges and challenges of the writer as the writing goes on

#include "kalahari/gui/writing_awards_tracker.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_goals.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QTimer>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The setting that turns the badges on
constexpr const char* BADGES_SETTING = "stats.badges";
/// The setting that turns the challenges on
constexpr const char* CHALLENGES_SETTING = "stats.challenges";

}  // namespace

WritingAwardsTracker::WritingAwardsTracker(WritingStatsModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &WritingAwardsTracker::evaluate);
    connect(m_model, &WritingStatsModel::changed, this, &WritingAwardsTracker::schedule);
    // Another book, or none: its badges
    connect(m_model, &WritingStatsModel::goalsChanged, this, [this]() {
        if (m_model->bookId() != m_bookId) {
            readKept();
        }
        schedule();
    });
    readKept();
    // How far the badges are, at once; they are won and kept a little later
    if (m_model->journal().isOpen()) {
        m_progress = core::badgeProgress(input());
    }
    schedule();
}

void WritingAwardsTracker::setBookSource(BookSource source)
{
    m_source = std::move(source);
    schedule();
}

void WritingAwardsTracker::schedule()
{
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

QString WritingAwardsTracker::deadlineKey(const core::WritingDeadline& deadline)
{
    return QStringList{deadline.name, deadline.date.toString(Qt::ISODate), deadline.elementId,
                       QString::number(deadline.targetWords)}
        .join(QChar(0x1f));
}

void WritingAwardsTracker::readKept()
{
    core::WritingJournal& journal = m_model->journal();
    m_journalBadges = journal.badges();
    m_challenges = journal.challenges();
    m_finishedBooks = journal.finishedBooks();
    m_bookId = m_model->bookId();
    m_bookBadges = {};
    if (core::ProjectDatabase* database = m_model->database(); database != nullptr && database->isOpen()) {
        m_bookBadges = core::BookBadges::fromJson(
            database->getSetting(QString::fromLatin1(core::BookBadges::SETTING_KEY)).toString());
    }
}

core::AwardsInput WritingAwardsTracker::input() const
{
    core::AwardsInput input;
    input.today = m_model->today();
    input.journal = m_model->journalStatistics();
    input.goals = m_model->goalStatistics();
    input.dailyGoals = m_model->journal().dailyGoals();
    input.weeklyGoals = m_model->journal().weeklyGoals();
    input.finishedBooks = int(m_finishedBooks.size());
    if (m_model->hasBook()) {
        input.hasBook = true;
        input.book = m_model->bookStatistics();
        input.bookSince = m_bookBadges.since;
        input.bookWords = m_model->bookWords();
        input.bookTarget = m_model->goals().targetWords;
        if (m_source.chapters) {
            std::tie(input.chaptersDone, input.chapters) = m_source.chapters();
        }
        input.deadlinesKept = int(m_bookBadges.keptDeadlines.size());
    }
    return input;
}

void WritingAwardsTracker::keepDayGoals(const core::AwardsInput& input)
{
    // The goals as they are today, so that a goal changed later does not change the past
    const std::pair<int, int> goals{m_model->dailyGoalToday(), input.goals.rules().weeklyGoalWords()};
    if (m_keptGoalsDay == input.today && m_keptGoals == goals) {
        return;
    }
    if (m_model->journal().setDayGoals(input.today, goals.first, goals.second)) {
        m_keptGoalsDay = input.today;
        m_keptGoals = goals;
    }
}

void WritingAwardsTracker::keepBookFacts()
{
    if (!m_model->hasBook()) {
        return;
    }
    const core::BookGoals& goals = m_model->goals();
    const int words = m_model->bookWords();
    const QDate today = m_model->today();
    if (goals.targetWords > 0 && words >= goals.targetWords && !m_bookId.isEmpty() &&
        !m_finishedBooks.contains(m_bookId)) {
        if (m_model->journal().addFinishedBook(m_bookId, today)) {
            m_finishedBooks.append(m_bookId);
            core::Logger::getInstance().info("WritingAwardsTracker: The book reached its length");
        }
    }
    // A deadline is kept when its part has its words by its day
    bool kept = false;
    for (const core::WritingDeadline& deadline : goals.deadlines) {
        if (deadline.targetWords <= 0 || !deadline.date.isValid() || today > deadline.date) {
            continue;
        }
        const QString key = deadlineKey(deadline);
        if (m_bookBadges.keptDeadlines.contains(key)) {
            continue;
        }
        const int done = deadline.elementId.isEmpty()
                             ? words
                             : (m_source.words ? m_source.words(deadline.elementId) : 0);
        if (done >= deadline.targetWords) {
            m_bookBadges.keptDeadlines.append(key);
            kept = true;
        }
    }
    if (kept && m_model->database() != nullptr) {
        m_model->database()->setSetting(QString::fromLatin1(core::BookBadges::SETTING_KEY),
                                        m_bookBadges.toJson());
    }
}

void WritingAwardsTracker::evaluate()
{
    m_timer->stop();
    if (!m_model->journal().isOpen()) {
        return;
    }
    // What is kept, once the journal is open, and the badges of another book
    if (!m_counted || m_model->bookId() != m_bookId) {
        readKept();
    }
    auto& settings = core::SettingsManager::getInstance();
    core::AwardsInput data = input();
    keepDayGoals(data);
    keepBookFacts();
    data = input();
    m_progress = core::badgeProgress(data);
    const QDate today = data.today;

    QList<core::EarnedBadge> won;
    if (settings.get<bool>(BADGES_SETTING, true)) {
        for (const core::BadgeProgress& progress : std::as_const(m_progress)) {
            const core::BadgeGroup group = core::badgeGroupOf(progress.kind);
            // The month's share of days with the goal is given when the month is over
            if (progress.kind == core::BadgeKind::MonthGoalDays ||
                (group == core::BadgeGroup::Book && !data.hasBook)) {
                continue;
            }
            const QString scope = group == core::BadgeGroup::Month  ? core::monthScope(today)
                                  : group == core::BadgeGroup::Book ? m_bookId
                                                                    : QString();
            for (int level = 0; level < progress.level(); ++level) {
                if (!earned(progress.kind, level)) {
                    won.append({progress.kind, level, today, scope});
                }
            }
        }
        // The month before, once it is over: its badges, on its last day
        const QDate thisMonth(today.year(), today.month(), 1);
        const QDate previous = thisMonth.addMonths(-1);
        const QString previousScope = core::monthScope(previous);
        for (const core::BadgeProgress& progress : core::monthBadgeProgress(data, previous, thisMonth)) {
            for (int level = 0; level < progress.level(); ++level) {
                const bool kept = std::any_of(
                    m_journalBadges.cbegin(), m_journalBadges.cend(),
                    [&](const core::EarnedBadge& badge) {
                        return badge.kind == progress.kind && badge.level == level &&
                               badge.scope == previousScope;
                    });
                if (!kept) {
                    won.append({progress.kind, level, thisMonth.addDays(-1), previousScope});
                }
            }
        }
    }
    if (!won.isEmpty()) {
        QList<core::EarnedBadge> ofJournal;
        bool ofBook = false;
        for (const core::EarnedBadge& badge : std::as_const(won)) {
            if (core::badgeGroupOf(badge.kind) == core::BadgeGroup::Book) {
                core::EarnedBadge kept = badge;
                kept.scope.clear();
                m_bookBadges.earned.append(kept);
                ofBook = true;
            } else {
                ofJournal.append(badge);
            }
        }
        if (!ofJournal.isEmpty() && m_model->journal().addBadges(ofJournal)) {
            m_journalBadges.append(ofJournal);
        }
        if (ofBook && m_model->database() != nullptr) {
            m_model->database()->setSetting(QString::fromLatin1(core::BookBadges::SETTING_KEY),
                                            m_bookBadges.toJson());
        }
        core::Logger::getInstance().info("WritingAwardsTracker: {} new badge(s)", won.size());
        emit badgesWon(won);
    }

    // The challenges done
    if (settings.get<bool>(CHALLENGES_SETTING, false)) {
        for (core::WritingChallenge& challenge : m_challenges) {
            if (challenge.done.isValid()) {
                continue;
            }
            const core::ChallengeProgress progress = challengeProgress(challenge);
            if (progress.done && m_model->journal().setChallengeDone(challenge.id, today)) {
                challenge.done = today;
                emit challengeDone(challenge, progress.value);
            }
        }
    }

    checkGoals();
    m_counted = true;
    emit changed();
}

void WritingAwardsTracker::countIfNeeded()
{
    if (!m_counted) {
        evaluate();
    }
}

void WritingAwardsTracker::checkGoals()
{
    const QDate today = m_model->today();
    const core::WritingStatistics statistics = m_model->goalStatistics();
    const core::WritingRules& rules = m_model->rules();
    const bool firstCheck = !m_goalsDay.isValid();
    if (m_goalsDay != today) {
        m_goalsDay = today;
        m_dailyReached = false;
        m_weeklyReached = false;
        m_deadlinesSaid.clear();
    }

    // The goals reached just now; at the start of the program only what changes from now
    const int words = statistics.day(today).counted(rules.counting);
    const int goal = m_model->dailyGoalToday();
    const bool daily = goal > 0 && words >= goal;
    if (daily && !m_dailyReached && !firstCheck) {
        emit dailyGoalReached(words, goal);
    }
    m_dailyReached = daily;
    const QDate monday = today.addDays(1 - today.dayOfWeek());
    const int weekWords = statistics.period(monday, today).counted(rules.counting);
    const int weekGoal = rules.weeklyGoalWords();
    const bool weekly = weekGoal > 0 && weekWords >= weekGoal;
    if (weekly && !m_weeklyReached && !firstCheck) {
        emit weeklyGoalReached(weekWords, weekGoal);
    }
    m_weeklyReached = weekly;

    // The deadlines coming, once a day
    if (!m_model->hasBook()) {
        return;
    }
    const int ahead = std::max(0, core::SettingsManager::getInstance().get<int>(
                                      "stats.notify.deadlineDays", 3));
    const core::BookGoals& goals = m_model->goals();
    QList<core::WritingDeadline> deadlines = goals.deadlines;
    if (goals.deadline.isValid()) {
        deadlines.prepend({QString(), goals.deadline, QString(), goals.targetWords});
    }
    for (const core::WritingDeadline& deadline : std::as_const(deadlines)) {
        if (!deadline.date.isValid()) {
            continue;
        }
        const auto days = today.daysTo(deadline.date);
        const QString key = deadlineKey(deadline);
        if (days < 0 || days > ahead || m_deadlinesSaid.contains(key)) {
            continue;
        }
        m_deadlinesSaid.insert(key);
        const int done = deadline.elementId.isEmpty()
                             ? m_model->bookWords()
                             : (m_source.words ? m_source.words(deadline.elementId) : 0);
        emit deadlineComing(deadline.name, deadline.date, int(days), done, deadline.targetWords);
    }
}

core::BadgeProgress WritingAwardsTracker::progressOf(core::BadgeKind kind) const
{
    for (const core::BadgeProgress& progress : m_progress) {
        if (progress.kind == kind) {
            return progress;
        }
    }
    core::BadgeProgress none;
    none.kind = kind;
    return none;
}

std::optional<core::EarnedBadge> WritingAwardsTracker::earned(core::BadgeKind kind, int level) const
{
    const core::BadgeGroup group = core::badgeGroupOf(kind);
    if (group == core::BadgeGroup::Book) {
        for (const core::EarnedBadge& badge : m_bookBadges.earned) {
            if (badge.kind == kind && badge.level == level) {
                core::EarnedBadge found = badge;
                found.scope = m_bookId;
                return found;
            }
        }
        return std::nullopt;
    }
    const QString scope = group == core::BadgeGroup::Month ? core::monthScope(m_model->today())
                                                           : QString();
    for (const core::EarnedBadge& badge : m_journalBadges) {
        if (badge.kind == kind && badge.level == level && badge.scope == scope) {
            return badge;
        }
    }
    return std::nullopt;
}

QList<core::EarnedBadge> WritingAwardsTracker::shownBadges() const
{
    QList<core::EarnedBadge> shown;
    for (const core::BadgeKind kind : core::BADGE_KINDS) {
        for (int level = 0; level < core::BADGE_LEVELS; ++level) {
            if (const auto badge = earned(kind, level)) {
                shown.append(*badge);
            }
        }
    }
    return shown;
}

std::optional<core::EarnedBadge> WritingAwardsTracker::lastBadge() const
{
    std::optional<core::EarnedBadge> last;
    auto consider = [&last](const core::EarnedBadge& badge) {
        if (!last || badge.date >= last->date) {
            last = badge;
        }
    };
    for (const core::EarnedBadge& badge : m_journalBadges) {
        consider(badge);
    }
    for (core::EarnedBadge badge : m_bookBadges.earned) {
        badge.scope = m_bookId;
        consider(badge);
    }
    return last;
}

int WritingAwardsTracker::monthsWon(core::BadgeKind kind, int level) const
{
    return int(std::count_if(m_journalBadges.cbegin(), m_journalBadges.cend(),
                             [kind, level](const core::EarnedBadge& badge) {
                                 return badge.kind == kind && badge.level == level &&
                                        !badge.scope.isEmpty();
                             }));
}

void WritingAwardsTracker::startBookOver()
{
    if (m_model->database() == nullptr) {
        return;
    }
    m_bookBadges.history.append(m_bookBadges.earned);
    m_bookBadges.earned.clear();
    m_bookBadges.keptDeadlines.clear();
    m_bookBadges.since = m_model->today();
    m_model->database()->setSetting(QString::fromLatin1(core::BookBadges::SETTING_KEY),
                                    m_bookBadges.toJson());
    core::Logger::getInstance().info("WritingAwardsTracker: The badges of the book start over");
    evaluate();
}

core::ChallengeProgress WritingAwardsTracker::challengeProgress(
    const core::WritingChallenge& challenge) const
{
    return core::challengeProgress(challenge, m_model->goalStatistics(), m_model->today());
}

int WritingAwardsTracker::addChallenge(const core::WritingChallenge& challenge)
{
    const int id = m_model->journal().addChallenge(challenge);
    if (id <= 0) {
        return 0;
    }
    core::WritingChallenge kept = challenge;
    kept.id = id;
    m_challenges.append(kept);
    core::SettingsManager::getInstance().set<bool>(CHALLENGES_SETTING, true);
    core::Logger::getInstance().info("WritingAwardsTracker: Challenge added: {} {}",
                                     core::challengeIdOf(challenge.kind).toStdString(),
                                     challenge.target);
    evaluate();
    return id;
}

bool WritingAwardsTracker::removeChallenge(int id)
{
    if (!m_model->journal().removeChallenge(id)) {
        return false;
    }
    m_challenges.removeIf([id](const core::WritingChallenge& challenge) { return challenge.id == id; });
    core::Logger::getInstance().info("WritingAwardsTracker: Challenge removed");
    emit changed();
    return true;
}

}  // namespace kalahari::gui
