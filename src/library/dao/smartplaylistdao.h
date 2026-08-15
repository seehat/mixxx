#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>

#include "library/dao/dao.h"
#include "util/class.h"

class QSqlDatabase;

constexpr int kInvalidSmartPlaylistId = -1;

// A smart playlist is a named search query which filters the whole library
// when activated in the sidebar.
class SmartPlaylistDAO : public QObject, public virtual DAO {
    Q_OBJECT
  public:
    struct SmartPlaylist {
        int id = kInvalidSmartPlaylistId;
        QString name;
        QString query;

        bool isValid() const {
            return id > 0;
        }
    };

    SmartPlaylistDAO();
    ~SmartPlaylistDAO() override = default;

    void initialize(const QSqlDatabase& database) override;

    // Creates a new smart playlist. Returns the new id or
    // kInvalidSmartPlaylistId if the name is empty or already in use or the
    // creation failed.
    int createSmartPlaylist(const QString& name, const QString& query);
    // Deletes the smart playlist with the given id.
    void deleteSmartPlaylist(const int smartPlaylistId);
    // Renames the smart playlist. Returns false if the name is empty or
    // already in use.
    bool renameSmartPlaylist(const int smartPlaylistId, const QString& newName);
    // Replaces the search query of the smart playlist.
    bool updateSmartPlaylistQuery(const int smartPlaylistId, const QString& query);

    // Check if a smart playlist with the given name exists.
    bool smartPlaylistExists(const QString& name) const;
    // Get the id of the smart playlist by its name or kInvalidSmartPlaylistId.
    int getSmartPlaylistIdFromName(const QString& name) const;
    // Get the name of the smart playlist with the given id.
    QString getSmartPlaylistName(const int smartPlaylistId) const;
    // Get the search query of the smart playlist with the given id.
    QString getSmartPlaylistQuery(const int smartPlaylistId) const;
    // All smart playlists ordered by name (case-insensitive).
    QList<SmartPlaylist> getSmartPlaylists() const;

  signals:
    void added(int smartPlaylistId);
    void renamed(int smartPlaylistId, const QString& newName);
    void queryChanged(int smartPlaylistId);
    void deleted(int smartPlaylistId);

  private:
    DISALLOW_COPY_AND_ASSIGN(SmartPlaylistDAO);
};
