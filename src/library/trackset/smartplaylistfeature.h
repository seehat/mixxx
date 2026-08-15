#pragma once

#include <QAction>
#include <QModelIndex>
#include <QPointer>

#include "library/trackset/basetracksetfeature.h"
#include "util/parented_ptr.h"

class KeyboardEventFilter;
class Library;
class SmartPlaylistDAO;
class WLibrary;
class WLibrarySidebar;

// A smart playlist is a named search query which filters the whole library
// when activated in the sidebar.
class SmartPlaylistFeature : public BaseTrackSetFeature {
    Q_OBJECT
  public:
    SmartPlaylistFeature(
            Library* pLibrary,
            UserSettingsPointer pConfig);

    QVariant title() override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void bindSidebarWidget(WLibrarySidebar* pSidebarWidget) override;
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;
    void renameItem(const QModelIndex& index) override;
    void deleteItem(const QModelIndex& index) override;

  private:
    void initActions();
    void constructChildModel(int selectedId);
    int smartPlaylistIdFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromSmartPlaylistId(int smartPlaylistId) const;
    int currentSelectedSmartPlaylistId() const;
    QString formatRootViewHtml() const;
    void htmlLinkClicked(const QUrl& link);

    void slotCreateSmartPlaylist();
    void slotRenameSmartPlaylist();
    void slotEditQuery();
    void slotDeleteSmartPlaylist();
    void slotSaveCurrentSearch();
    void slotSmartPlaylistAdded(int smartPlaylistId);
    void slotSmartPlaylistChanged(int smartPlaylistId);
    void slotSmartPlaylistDeleted(int smartPlaylistId);

    SmartPlaylistDAO& m_smartPlaylistDao;
    QPointer<WLibrarySidebar> m_pSidebarWidget;
    QModelIndex m_lastClickedIndex;
    QModelIndex m_lastRightClickedIndex;
    parented_ptr<QAction> m_pCreateSmartPlaylistAction;
    parented_ptr<QAction> m_pRenameSmartPlaylistAction;
    parented_ptr<QAction> m_pEditQueryAction;
    parented_ptr<QAction> m_pDeleteSmartPlaylistAction;
    parented_ptr<QAction> m_pSaveCurrentSearchAction;
};
