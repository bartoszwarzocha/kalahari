/// @file writing_journal.cpp
/// @brief The table of the record of writing, and the writer's journal of all the books

#include "kalahari/core/writing_journal.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/unicode_paths.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace kalahari::core {

namespace writing_activity_store {

namespace {

const QString DAY_FORMAT = QStringLiteral("yyyy-MM-dd");

}  // namespace

bool createTable(QSqlDatabase& db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS writing_activity ("
            " day TEXT NOT NULL,"
            " slot INTEGER NOT NULL,"
            " book_id TEXT NOT NULL,"
            " element_id TEXT NOT NULL DEFAULT '',"
            " written INTEGER NOT NULL DEFAULT 0,"
            " deleted INTEGER NOT NULL DEFAULT 0,"
            " pasted INTEGER NOT NULL DEFAULT 0,"
            " active_ms INTEGER NOT NULL DEFAULT 0,"
            " PRIMARY KEY (day, slot, book_id, element_id)"
            ") WITHOUT ROWID"))) {
        return false;
    }
    // A record kept before the words pasted were kept apart gets their column
    if (!query.exec(QStringLiteral("PRAGMA table_info(writing_activity)"))) {
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == QLatin1String("pasted")) {
            return true;
        }
    }
    return query.exec(QStringLiteral(
        "ALTER TABLE writing_activity ADD COLUMN pasted INTEGER NOT NULL DEFAULT 0"));
}

bool add(QSqlDatabase& db, const QList<WritingActivity>& activity)
{
    if (activity.isEmpty()) {
        return true;
    }
    // In a transaction of its own unless the caller has one
    const bool ownTransaction = db.transaction();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO writing_activity"
        " (day, slot, book_id, element_id, written, deleted, pasted, active_ms)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT (day, slot, book_id, element_id) DO UPDATE SET"
        " written = written + excluded.written,"
        " deleted = deleted + excluded.deleted,"
        " pasted = pasted + excluded.pasted,"
        " active_ms = active_ms + excluded.active_ms"));
    for (const WritingActivity& entry : activity) {
        if (entry.isEmpty() || !entry.date.isValid()) {
            continue;
        }
        query.addBindValue(entry.date.toString(DAY_FORMAT));
        query.addBindValue(entry.slot);
        query.addBindValue(entry.bookId);
        query.addBindValue(entry.elementId);
        query.addBindValue(entry.written);
        query.addBindValue(entry.deleted);
        query.addBindValue(entry.pasted);
        query.addBindValue(entry.activeMs);
        if (!query.exec()) {
            Logger::getInstance().error("Writing record: cannot write: {}",
                                        query.lastError().text().toStdString());
            if (ownTransaction) {
                db.rollback();
            }
            return false;
        }
    }
    return !ownTransaction || db.commit();
}

QList<WritingActivity> read(const QSqlDatabase& db, const QDate& from, const QDate& to,
                            const QString& bookId)
{
    QList<WritingActivity> result;
    QSqlQuery query(db);
    QString sql = QStringLiteral(
        "SELECT day, slot, book_id, element_id, written, deleted, pasted, active_ms"
        " FROM writing_activity WHERE day >= ? AND day <= ?");
    if (!bookId.isEmpty()) {
        sql += QStringLiteral(" AND book_id = ?");
    }
    sql += QStringLiteral(" ORDER BY day, slot");
    query.prepare(sql);
    query.addBindValue(from.toString(DAY_FORMAT));
    query.addBindValue(to.toString(DAY_FORMAT));
    if (!bookId.isEmpty()) {
        query.addBindValue(bookId);
    }
    if (!query.exec()) {
        Logger::getInstance().error("Writing record: cannot read: {}",
                                    query.lastError().text().toStdString());
        return result;
    }
    while (query.next()) {
        WritingActivity entry;
        entry.date = QDate::fromString(query.value(0).toString(), DAY_FORMAT);
        entry.slot = query.value(1).toInt();
        entry.bookId = query.value(2).toString();
        entry.elementId = query.value(3).toString();
        entry.written = query.value(4).toInt();
        entry.deleted = query.value(5).toInt();
        entry.pasted = query.value(6).toInt();
        entry.activeMs = query.value(7).toLongLong();
        if (entry.date.isValid() && entry.slot >= 0 && entry.slot < ACTIVITY_SLOTS_PER_DAY) {
            result.append(entry);
        }
    }
    return result;
}

}  // namespace writing_activity_store

// =============================================================================
// WritingJournal
// =============================================================================

WritingJournal::~WritingJournal()
{
    close();
}

QString WritingJournal::defaultPath()
{
    const std::filesystem::path settings = SettingsManager::getInstance().getSettingsFilePath();
    return QString::fromStdString(pathText(settings.parent_path() / "writing_journal.db"));
}

namespace {

const QString JOURNAL_DAY_FORMAT = QStringLiteral("yyyy-MM-dd");

/// The tables of the journal beside the record: the badges, the goals of the days, the books
/// finished and the challenges
bool createJournalTables(QSqlDatabase& db)
{
    QSqlQuery query(db);
    return query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS writing_badges ("
                                     " scope TEXT NOT NULL,"
                                     " badge TEXT NOT NULL,"
                                     " level INTEGER NOT NULL,"
                                     " earned TEXT NOT NULL,"
                                     " PRIMARY KEY (scope, badge, level)"
                                     ") WITHOUT ROWID")) &&
           query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS writing_day_goals ("
                                     " day TEXT PRIMARY KEY,"
                                     " daily INTEGER NOT NULL,"
                                     " weekly INTEGER NOT NULL"
                                     ") WITHOUT ROWID")) &&
           query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS writing_finished_books ("
                                     " book_id TEXT PRIMARY KEY,"
                                     " finished TEXT NOT NULL"
                                     ") WITHOUT ROWID")) &&
           query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS writing_challenges ("
                                     " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     " kind TEXT NOT NULL,"
                                     " target INTEGER NOT NULL,"
                                     " start_day TEXT NOT NULL,"
                                     " end_day TEXT NOT NULL,"
                                     " done TEXT NOT NULL DEFAULT ''"
                                     ")"));
}

/// The goals of the days in column @p column of writing_day_goals
QMap<QDate, int> dayGoals(const QSqlDatabase& db, const QString& column)
{
    QMap<QDate, int> goals;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT day, %1 FROM writing_day_goals").arg(column))) {
        return goals;
    }
    while (query.next()) {
        const QDate day = QDate::fromString(query.value(0).toString(), JOURNAL_DAY_FORMAT);
        if (day.isValid()) {
            goals.insert(day, query.value(1).toInt());
        }
    }
    return goals;
}

}  // namespace

bool WritingJournal::open(const QString& path)
{
    close();
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_connectionName = QStringLiteral("journal_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        Logger::getInstance().error("Writing journal: cannot open {}: {}", path.toStdString(),
                                    m_lastError.toStdString());
        close();
        return false;
    }
    QSqlQuery pragmas(m_db);
    pragmas.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragmas.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    if (!writing_activity_store::createTable(m_db) || !createJournalTables(m_db)) {
        m_lastError = m_db.lastError().text();
        close();
        return false;
    }
    return true;
}

void WritingJournal::close()
{
    if (m_connectionName.isEmpty()) {
        return;
    }
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
}

bool WritingJournal::isOpen() const
{
    return m_db.isOpen();
}

bool WritingJournal::add(const QList<WritingActivity>& activity)
{
    if (!isOpen()) {
        return false;
    }
    return writing_activity_store::add(m_db, activity);
}

QList<WritingActivity> WritingJournal::activity(const QDate& from, const QDate& to,
                                                const QString& bookId) const
{
    if (!isOpen()) {
        return {};
    }
    return writing_activity_store::read(m_db, from, to, bookId);
}

QList<EarnedBadge> WritingJournal::badges() const
{
    QList<EarnedBadge> badges;
    if (!isOpen()) {
        return badges;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT scope, badge, level, earned FROM writing_badges ORDER BY earned, scope"))) {
        return badges;
    }
    while (query.next()) {
        const std::optional<BadgeKind> kind = badgeKindOf(query.value(1).toString());
        const int level = query.value(2).toInt();
        if (!kind || level < 0 || level >= BADGE_LEVELS) {
            continue;
        }
        badges.append({*kind, level,
                       QDate::fromString(query.value(3).toString(), JOURNAL_DAY_FORMAT),
                       query.value(0).toString()});
    }
    return badges;
}

bool WritingJournal::addBadges(const QList<EarnedBadge>& badges)
{
    if (!isOpen()) {
        return false;
    }
    const bool ownTransaction = m_db.transaction();
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO writing_badges (scope, badge, level, earned)"
                                 " VALUES (?, ?, ?, ?)"));
    for (const EarnedBadge& badge : badges) {
        // An empty scope, not NULL, which the table does not take
        query.addBindValue(badge.scope.isNull() ? QStringLiteral("") : badge.scope);
        query.addBindValue(badgeIdOf(badge.kind));
        query.addBindValue(badge.level);
        query.addBindValue(badge.date.toString(JOURNAL_DAY_FORMAT));
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            if (ownTransaction) {
                m_db.rollback();
            }
            return false;
        }
    }
    return !ownTransaction || m_db.commit();
}

QMap<QDate, int> WritingJournal::dailyGoals() const
{
    return isOpen() ? dayGoals(m_db, QStringLiteral("daily")) : QMap<QDate, int>();
}

QMap<QDate, int> WritingJournal::weeklyGoals() const
{
    return isOpen() ? dayGoals(m_db, QStringLiteral("weekly")) : QMap<QDate, int>();
}

bool WritingJournal::setDayGoals(const QDate& day, int dailyGoal, int weeklyGoal)
{
    if (!isOpen() || !day.isValid()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO writing_day_goals (day, daily, weekly) VALUES (?, ?, ?)"
                                 " ON CONFLICT (day) DO UPDATE SET daily = excluded.daily,"
                                 " weekly = excluded.weekly"));
    query.addBindValue(day.toString(JOURNAL_DAY_FORMAT));
    query.addBindValue(dailyGoal);
    query.addBindValue(weeklyGoal);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QStringList WritingJournal::finishedBooks() const
{
    QStringList books;
    if (!isOpen()) {
        return books;
    }
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT book_id FROM writing_finished_books ORDER BY finished"))) {
        while (query.next()) {
            books.append(query.value(0).toString());
        }
    }
    return books;
}

bool WritingJournal::addFinishedBook(const QString& bookId, const QDate& day)
{
    if (!isOpen() || bookId.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO writing_finished_books (book_id, finished) VALUES (?, ?)"));
    query.addBindValue(bookId);
    query.addBindValue(day.toString(JOURNAL_DAY_FORMAT));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QList<WritingChallenge> WritingJournal::challenges() const
{
    QList<WritingChallenge> challenges;
    if (!isOpen()) {
        return challenges;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, kind, target, start_day, end_day, done FROM writing_challenges ORDER BY id"))) {
        return challenges;
    }
    while (query.next()) {
        WritingChallenge challenge;
        challenge.id = query.value(0).toInt();
        challenge.kind = challengeKindOf(query.value(1).toString());
        challenge.target = query.value(2).toInt();
        challenge.start = QDate::fromString(query.value(3).toString(), JOURNAL_DAY_FORMAT);
        challenge.end = QDate::fromString(query.value(4).toString(), JOURNAL_DAY_FORMAT);
        challenge.done = QDate::fromString(query.value(5).toString(), JOURNAL_DAY_FORMAT);
        if (challenge.start.isValid() && challenge.end.isValid()) {
            challenges.append(challenge);
        }
    }
    return challenges;
}

int WritingJournal::addChallenge(const WritingChallenge& challenge)
{
    if (!isOpen() || !challenge.start.isValid() || !challenge.end.isValid()) {
        return 0;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO writing_challenges (kind, target, start_day, end_day, done)"
        " VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(challengeIdOf(challenge.kind));
    query.addBindValue(challenge.target);
    query.addBindValue(challenge.start.toString(JOURNAL_DAY_FORMAT));
    query.addBindValue(challenge.end.toString(JOURNAL_DAY_FORMAT));
    query.addBindValue(challenge.done.isValid() ? challenge.done.toString(JOURNAL_DAY_FORMAT)
                                                : QStringLiteral(""));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool WritingJournal::setChallengeDone(int id, const QDate& day)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE writing_challenges SET done = ? WHERE id = ?"));
    query.addBindValue(day.toString(JOURNAL_DAY_FORMAT));
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool WritingJournal::removeChallenge(int id)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM writing_challenges WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

}  // namespace kalahari::core
