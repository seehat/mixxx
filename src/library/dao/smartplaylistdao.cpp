#include "library/dao/smartplaylistdao.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>

#include <algorithm>
#include <utility>

#include "moc_smartplaylistdao.cpp"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("SmartPlaylistDAO");

const QString kFileName = QStringLiteral("smartplaylists.json");

const QString kVersionKey = QStringLiteral("version");
const QString kPlaylistsKey = QStringLiteral("playlists");
const QString kIdKey = QStringLiteral("id");
const QString kNameKey = QStringLiteral("name");
const QString kQueryKey = QStringLiteral("query");
const QString kDateCreatedKey = QStringLiteral("date_created");
const QString kDateModifiedKey = QStringLiteral("date_modified");

constexpr int kFileVersion = 1;

QString currentTimestamp() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // anonymous namespace

SmartPlaylistDAO::SmartPlaylistDAO(UserSettingsPointer pConfig)
        : m_pConfig(std::move(pConfig)) {
}

QString SmartPlaylistDAO::filePath() const {
    return QDir(m_pConfig->getSettingsPath()).absoluteFilePath(kFileName);
}

QList<SmartPlaylistDAO::PlaylistRecord> SmartPlaylistDAO::loadRecords() const {
    QList<PlaylistRecord> records;

    QFile file(filePath());
    if (!file.exists()) {
        return records;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        kLogger.warning()
                << "Failed to open" << file.fileName()
                << file.errorString();
        return records;
    }

    QJsonParseError parseError;
    const QJsonDocument doc =
            QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        kLogger.warning()
                << "Failed to parse" << file.fileName()
                << parseError.errorString();
        return records;
    }

    const QJsonArray playlists = doc.object().value(kPlaylistsKey).toArray();
    records.reserve(playlists.size());
    for (const QJsonValue& value : playlists) {
        const QJsonObject object = value.toObject();
        PlaylistRecord record;
        record.id = object.value(kIdKey).toInt(kInvalidSmartPlaylistId);
        record.name = object.value(kNameKey).toString();
        record.query = object.value(kQueryKey).toString();
        record.dateCreated = object.value(kDateCreatedKey).toString();
        record.dateModified = object.value(kDateModifiedKey).toString();
        if (record.id > 0 && !record.name.isEmpty()) {
            records.append(record);
        }
    }
    return records;
}

bool SmartPlaylistDAO::saveRecords(
        const QList<PlaylistRecord>& records) const {
    QJsonArray playlists;
    for (const PlaylistRecord& record : records) {
        QJsonObject object;
        object.insert(kIdKey, record.id);
        object.insert(kNameKey, record.name);
        object.insert(kQueryKey, record.query);
        object.insert(kDateCreatedKey, record.dateCreated);
        object.insert(kDateModifiedKey, record.dateModified);
        playlists.append(object);
    }

    QJsonObject root;
    root.insert(kVersionKey, kFileVersion);
    root.insert(kPlaylistsKey, playlists);

    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kLogger.warning()
                << "Failed to open" << file.fileName()
                << "for writing:" << file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        kLogger.warning()
                << "Failed to write" << file.fileName()
                << file.errorString();
        return false;
    }
    if (!file.commit()) {
        kLogger.warning()
                << "Failed to commit" << file.fileName()
                << file.errorString();
        return false;
    }
    return true;
}

int SmartPlaylistDAO::createSmartPlaylist(
        const QString& name,
        const QString& query) {
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return kInvalidSmartPlaylistId;
    }

    QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.name == trimmedName) {
            return kInvalidSmartPlaylistId;
        }
    }

    int maxId = 0;
    for (const PlaylistRecord& record : records) {
        maxId = std::max(maxId, record.id);
    }

    PlaylistRecord record;
    record.id = maxId + 1;
    record.name = trimmedName;
    record.query = query;
    record.dateCreated = currentTimestamp();
    record.dateModified = record.dateCreated;
    records.append(record);

    if (!saveRecords(records)) {
        return kInvalidSmartPlaylistId;
    }
    emit added(record.id);
    return record.id;
}

void SmartPlaylistDAO::deleteSmartPlaylist(const int smartPlaylistId) {
    QList<PlaylistRecord> records = loadRecords();
    const auto it = std::find_if(records.begin(),
            records.end(),
            [smartPlaylistId](const PlaylistRecord& record) {
                return record.id == smartPlaylistId;
            });
    if (it == records.end()) {
        return;
    }
    records.erase(it);
    if (!saveRecords(records)) {
        return;
    }
    emit deleted(smartPlaylistId);
}

bool SmartPlaylistDAO::renameSmartPlaylist(
        const int smartPlaylistId,
        const QString& newName) {
    const QString trimmedName = newName.trimmed();
    if (trimmedName.isEmpty()) {
        return false;
    }

    QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.name == trimmedName) {
            return false;
        }
    }

    const auto it = std::find_if(records.begin(),
            records.end(),
            [smartPlaylistId](const PlaylistRecord& record) {
                return record.id == smartPlaylistId;
            });
    if (it == records.end()) {
        return false;
    }
    it->name = trimmedName;
    it->dateModified = currentTimestamp();

    if (!saveRecords(records)) {
        return false;
    }
    emit renamed(smartPlaylistId, trimmedName);
    return true;
}

bool SmartPlaylistDAO::updateSmartPlaylistQuery(
        const int smartPlaylistId,
        const QString& query) {
    QList<PlaylistRecord> records = loadRecords();
    const auto it = std::find_if(records.begin(),
            records.end(),
            [smartPlaylistId](const PlaylistRecord& record) {
                return record.id == smartPlaylistId;
            });
    if (it == records.end()) {
        return false;
    }
    it->query = query;
    it->dateModified = currentTimestamp();

    if (!saveRecords(records)) {
        return false;
    }
    emit queryChanged(smartPlaylistId);
    return true;
}

bool SmartPlaylistDAO::smartPlaylistExists(const QString& name) const {
    const QString trimmedName = name.trimmed();
    const QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.name == trimmedName) {
            return true;
        }
    }
    return false;
}

int SmartPlaylistDAO::getSmartPlaylistIdFromName(const QString& name) const {
    const QString trimmedName = name.trimmed();
    const QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.name == trimmedName) {
            return record.id;
        }
    }
    return kInvalidSmartPlaylistId;
}

QString SmartPlaylistDAO::getSmartPlaylistName(
        const int smartPlaylistId) const {
    const QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.id == smartPlaylistId) {
            return record.name;
        }
    }
    return QString();
}

QString SmartPlaylistDAO::getSmartPlaylistQuery(
        const int smartPlaylistId) const {
    const QList<PlaylistRecord> records = loadRecords();
    for (const PlaylistRecord& record : records) {
        if (record.id == smartPlaylistId) {
            return record.query;
        }
    }
    return QString();
}

QList<SmartPlaylistDAO::SmartPlaylist> SmartPlaylistDAO::getSmartPlaylists() const {
    QList<SmartPlaylist> result;
    const QList<PlaylistRecord> records = loadRecords();
    result.reserve(records.size());
    for (const PlaylistRecord& record : records) {
        SmartPlaylist smartPlaylist;
        smartPlaylist.id = record.id;
        smartPlaylist.name = record.name;
        smartPlaylist.query = record.query;
        result.append(smartPlaylist);
    }
    std::sort(result.begin(),
            result.end(),
            [](const SmartPlaylist& lhs, const SmartPlaylist& rhs) {
                return QString::compare(
                               lhs.name, rhs.name, Qt::CaseInsensitive) < 0;
            });
    return result;
}
