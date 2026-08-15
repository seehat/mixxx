#include "library/dao/smartplaylistdao.h"

#include <QSqlQuery>
#include <QSqlRecord>

#include "moc_smartplaylistdao.cpp"
#include "library/queryutil.h"
#include "util/db/dbconnection.h"

namespace {

const QString kSmartPlaylistsTableName = QStringLiteral("smart_playlists");

const QString kSmartPlaylistsIdColumn = QStringLiteral("id");
const QString kSmartPlaylistsNameColumn = QStringLiteral("name");
const QString kSmartPlaylistsQueryColumn = QStringLiteral("query");

} // anonymous namespace

SmartPlaylistDAO::SmartPlaylistDAO() {
}

void SmartPlaylistDAO::initialize(const QSqlDatabase& database) {
    DAO::initialize(database);
}

int SmartPlaylistDAO::createSmartPlaylist(
        const QString& name,
        const QString& query) {
    if (name.trimmed().isEmpty() || smartPlaylistExists(name)) {
        return kInvalidSmartPlaylistId;
    }
    QSqlQuery queryExec(m_database);
    queryExec.prepare(QStringLiteral(
            "INSERT INTO %1 (%2, %3, date_created, date_modified) "
            "VALUES (:name, :query, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)")
                              .arg(kSmartPlaylistsTableName,
                                      kSmartPlaylistsNameColumn,
                                      kSmartPlaylistsQueryColumn));
    queryExec.bindValue(":name", name);
    queryExec.bindValue(":query", query);
    if (!queryExec.exec()) {
        LOG_FAILED_QUERY(queryExec);
        return kInvalidSmartPlaylistId;
    }
    const int smartPlaylistId = queryExec.lastInsertId().toInt();
    emit added(smartPlaylistId);
    return smartPlaylistId;
}

void SmartPlaylistDAO::deleteSmartPlaylist(const int smartPlaylistId) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "DELETE FROM %1 WHERE %2 = :id")
                          .arg(kSmartPlaylistsTableName,
                                  kSmartPlaylistsIdColumn));
    query.bindValue(":id", smartPlaylistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }
    emit deleted(smartPlaylistId);
}

bool SmartPlaylistDAO::renameSmartPlaylist(
        const int smartPlaylistId,
        const QString& newName) {
    if (newName.trimmed().isEmpty() || smartPlaylistExists(newName)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "UPDATE %1 SET %2 = :name, date_modified = CURRENT_TIMESTAMP "
            "WHERE %3 = :id")
                          .arg(kSmartPlaylistsTableName,
                                  kSmartPlaylistsNameColumn,
                                  kSmartPlaylistsIdColumn));
    query.bindValue(":name", newName);
    query.bindValue(":id", smartPlaylistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    emit renamed(smartPlaylistId, newName);
    return true;
}

bool SmartPlaylistDAO::updateSmartPlaylistQuery(
        const int smartPlaylistId,
        const QString& query) {
    QSqlQuery queryExec(m_database);
    queryExec.prepare(QStringLiteral(
            "UPDATE %1 SET %2 = :query, date_modified = CURRENT_TIMESTAMP "
            "WHERE %3 = :id")
                              .arg(kSmartPlaylistsTableName,
                                      kSmartPlaylistsQueryColumn,
                                      kSmartPlaylistsIdColumn));
    queryExec.bindValue(":query", query);
    queryExec.bindValue(":id", smartPlaylistId);
    if (!queryExec.exec()) {
        LOG_FAILED_QUERY(queryExec);
        return false;
    }
    emit queryChanged(smartPlaylistId);
    return true;
}

bool SmartPlaylistDAO::smartPlaylistExists(const QString& name) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM %1 WHERE %2 = :name")
                          .arg(kSmartPlaylistsTableName,
                                  kSmartPlaylistsNameColumn));
    query.bindValue(":name", name);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    if (query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

int SmartPlaylistDAO::getSmartPlaylistIdFromName(const QString& name) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT %1 FROM %2 WHERE %3 = :name")
                          .arg(kSmartPlaylistsIdColumn,
                                  kSmartPlaylistsTableName,
                                  kSmartPlaylistsNameColumn));
    query.bindValue(":name", name);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return kInvalidSmartPlaylistId;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return kInvalidSmartPlaylistId;
}

QString SmartPlaylistDAO::getSmartPlaylistName(const int smartPlaylistId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT %1 FROM %2 WHERE %3 = :id")
                          .arg(kSmartPlaylistsNameColumn,
                                  kSmartPlaylistsTableName,
                                  kSmartPlaylistsIdColumn));
    query.bindValue(":id", smartPlaylistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return QString();
    }
    if (query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

QString SmartPlaylistDAO::getSmartPlaylistQuery(const int smartPlaylistId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT %1 FROM %2 WHERE %3 = :id")
                          .arg(kSmartPlaylistsQueryColumn,
                                  kSmartPlaylistsTableName,
                                  kSmartPlaylistsIdColumn));
    query.bindValue(":id", smartPlaylistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return QString();
    }
    if (query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

QList<SmartPlaylistDAO::SmartPlaylist> SmartPlaylistDAO::getSmartPlaylists() const {
    QList<SmartPlaylist> result;
    QSqlQuery query(m_database);
    QString queryString = QStringLiteral(
            "SELECT %1, %2, %3 FROM %4")
                                  .arg(kSmartPlaylistsIdColumn,
                                          kSmartPlaylistsNameColumn,
                                          kSmartPlaylistsQueryColumn,
                                          kSmartPlaylistsTableName);
    queryString.append(
            mixxx::DbConnection::collateLexicographically(
                    QStringLiteral(" ORDER BY LOWER(name)")));
    query.prepare(queryString);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return result;
    }
    const int idColumn = query.record().indexOf(kSmartPlaylistsIdColumn);
    const int nameColumn = query.record().indexOf(kSmartPlaylistsNameColumn);
    const int queryColumn = query.record().indexOf(kSmartPlaylistsQueryColumn);
    while (query.next()) {
        SmartPlaylist smartPlaylist;
        smartPlaylist.id = query.value(idColumn).toInt();
        smartPlaylist.name = query.value(nameColumn).toString();
        smartPlaylist.query = query.value(queryColumn).toString();
        result.append(smartPlaylist);
    }
    return result;
}
