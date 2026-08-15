#include <gtest/gtest.h>

#include "library/dao/smartplaylistdao.h"
#include "test/librarytest.h"

class SmartPlaylistDAOTest : public LibraryTest {};

TEST_F(SmartPlaylistDAOTest, CreateAndRead) {
    SmartPlaylistDAO& dao = internalCollection()->getSmartPlaylistDAO();
    EXPECT_EQ(dao.getSmartPlaylists().size(), 0);

    const int id = dao.createSmartPlaylist("Techno", "genre:Techno");
    ASSERT_NE(id, kInvalidSmartPlaylistId);
    EXPECT_TRUE(dao.smartPlaylistExists("Techno"));
    EXPECT_EQ(dao.getSmartPlaylistName(id), QStringLiteral("Techno"));
    EXPECT_EQ(dao.getSmartPlaylistQuery(id), QStringLiteral("genre:Techno"));
    EXPECT_EQ(dao.getSmartPlaylistIdFromName("Techno"), id);
    ASSERT_EQ(dao.getSmartPlaylists().size(), 1);
    EXPECT_EQ(dao.getSmartPlaylists().first().id, id);
    EXPECT_EQ(dao.getSmartPlaylists().first().query, QStringLiteral("genre:Techno"));

    // Creating a smart playlist with an existing name fails.
    EXPECT_EQ(dao.createSmartPlaylist("Techno", "foo"), kInvalidSmartPlaylistId);
    // Creating a smart playlist with a blank name fails.
    EXPECT_EQ(dao.createSmartPlaylist("  ", "foo"), kInvalidSmartPlaylistId);
}

TEST_F(SmartPlaylistDAOTest, Rename) {
    SmartPlaylistDAO& dao = internalCollection()->getSmartPlaylistDAO();
    const int id = dao.createSmartPlaylist("A", "");
    ASSERT_NE(id, kInvalidSmartPlaylistId);
    ASSERT_NE(dao.createSmartPlaylist("B", ""), kInvalidSmartPlaylistId);

    EXPECT_TRUE(dao.renameSmartPlaylist(id, "C"));
    EXPECT_EQ(dao.getSmartPlaylistName(id), QStringLiteral("C"));

    // Renaming to an existing name fails and leaves the name unchanged.
    EXPECT_FALSE(dao.renameSmartPlaylist(id, "B"));
    EXPECT_EQ(dao.getSmartPlaylistName(id), QStringLiteral("C"));

    // Renaming to a blank name fails.
    EXPECT_FALSE(dao.renameSmartPlaylist(id, "  "));
    EXPECT_EQ(dao.getSmartPlaylistName(id), QStringLiteral("C"));
}

TEST_F(SmartPlaylistDAOTest, UpdateQuery) {
    SmartPlaylistDAO& dao = internalCollection()->getSmartPlaylistDAO();
    const int id = dao.createSmartPlaylist("A", "old");
    ASSERT_NE(id, kInvalidSmartPlaylistId);

    EXPECT_TRUE(dao.updateSmartPlaylistQuery(id, "bpm:124..128"));
    EXPECT_EQ(dao.getSmartPlaylistQuery(id), QStringLiteral("bpm:124..128"));
}

TEST_F(SmartPlaylistDAOTest, Delete) {
    SmartPlaylistDAO& dao = internalCollection()->getSmartPlaylistDAO();
    const int id = dao.createSmartPlaylist("A", "");
    ASSERT_NE(id, kInvalidSmartPlaylistId);

    dao.deleteSmartPlaylist(id);
    EXPECT_FALSE(dao.smartPlaylistExists("A"));
    EXPECT_EQ(dao.getSmartPlaylists().size(), 0);
}

TEST_F(SmartPlaylistDAOTest, SortedByName) {
    SmartPlaylistDAO& dao = internalCollection()->getSmartPlaylistDAO();
    ASSERT_NE(dao.createSmartPlaylist("Bravo", ""), kInvalidSmartPlaylistId);
    const int alphaId = dao.createSmartPlaylist("alpha", "");
    ASSERT_NE(alphaId, kInvalidSmartPlaylistId);
    ASSERT_NE(dao.createSmartPlaylist("Charlie", ""), kInvalidSmartPlaylistId);

    const QList<SmartPlaylistDAO::SmartPlaylist> playlists = dao.getSmartPlaylists();
    ASSERT_EQ(playlists.size(), 3);
    EXPECT_EQ(playlists[0].id, alphaId); // alpha < Bravo < Charlie, case-insensitive
    EXPECT_EQ(playlists[1].name, QStringLiteral("Bravo"));
    EXPECT_EQ(playlists[2].name, QStringLiteral("Charlie"));
}
