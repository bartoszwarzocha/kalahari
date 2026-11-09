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
    if (!writing_activity_store::createTable(m_db)) {
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

}  // namespace kalahari::core
