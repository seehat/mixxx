#include "library/trackset/smartplaylistfeature.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>
#include <vector>

#include "library/dao/smartplaylistdao.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "moc_smartplaylistfeature.cpp"
#include "util/defs.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wlibrarytextbrowser.h"

namespace {

// Dialog for creating or editing a smart playlist. Asks for a name and a
// search query that filters the whole library.
class SmartPlaylistDialog : public QDialog {
  public:
    SmartPlaylistDialog(
            QWidget* pParent,
            const QString& title,
            const QString& name,
            const QString& query,
            bool showNameField)
            : QDialog(pParent) {
        setWindowTitle(title);
        auto* pLayout = new QVBoxLayout(this);
        auto* pFormLayout = new QFormLayout();
        m_pNameEdit = new QLineEdit(name, this);
        m_pNameEdit->setPlaceholderText(QObject::tr("Name"));
        m_pQueryEdit = new QLineEdit(query, this);
        m_pQueryEdit->setPlaceholderText(QObject::tr(
                "Search query, e.g. genre:\"Techno\" bpm:124..128"));
        if (showNameField) {
            pFormLayout->addRow(QObject::tr("Name:"), m_pNameEdit);
        }
        pFormLayout->addRow(QObject::tr("Search query:"), m_pQueryEdit);
        pLayout->addLayout(pFormLayout);
        auto* pButtonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(pButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        pLayout->addWidget(pButtonBox);
    }

    QString name() const {
        return m_pNameEdit->text().trimmed();
    }

    QString query() const {
        return m_pQueryEdit->text().trimmed();
    }

  private:
    QLineEdit* m_pNameEdit;
    QLineEdit* m_pQueryEdit;
};

} // anonymous namespace

SmartPlaylistFeature::SmartPlaylistFeature(
        Library* pLibrary,
        UserSettingsPointer pConfig)
        : BaseTrackSetFeature(
                  pLibrary,
                  pConfig,
                  "SMARTPLAYLISTHOME",
                  QStringLiteral("smartplaylists")),
          m_smartPlaylistDao(pLibrary->trackCollectionManager()
                  ->internalCollection()
                  ->getSmartPlaylistDAO()),
          m_pLibraryTableModel(std::make_unique<LibraryTableModel>(
                  this,
                  pLibrary->trackCollectionManager(),
                  "mixxx.db.model.smartplaylists")),
          m_pSidebarWidget(nullptr) {
    initActions();

    // construct child model
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
    constructChildModel(kInvalidSmartPlaylistId);

    connect(&m_smartPlaylistDao,
            &SmartPlaylistDAO::added,
            this,
            &SmartPlaylistFeature::slotSmartPlaylistAdded);
    connect(&m_smartPlaylistDao,
            &SmartPlaylistDAO::renamed,
            this,
            &SmartPlaylistFeature::slotSmartPlaylistChanged);
    connect(&m_smartPlaylistDao,
            &SmartPlaylistDAO::queryChanged,
            this,
            &SmartPlaylistFeature::slotSmartPlaylistChanged);
    connect(&m_smartPlaylistDao,
            &SmartPlaylistDAO::deleted,
            this,
            &SmartPlaylistFeature::slotSmartPlaylistDeleted);
    connect(pLibrary->trackCollectionManager(),
            &TrackCollectionManager::libraryScanFinished,
            this,
            &SmartPlaylistFeature::slotLibraryScanFinished);
}

void SmartPlaylistFeature::initActions() {
    m_pCreateSmartPlaylistAction =
            make_parented<QAction>(tr("Create New Smart Playlist"), this);
    connect(m_pCreateSmartPlaylistAction.get(),
            &QAction::triggered,
            this,
            &SmartPlaylistFeature::slotCreateSmartPlaylist);

    m_pRenameSmartPlaylistAction =
            make_parented<QAction>(tr("Rename"), this);
    m_pRenameSmartPlaylistAction->setShortcut(kRenameSidebarItemShortcutKey);
    connect(m_pRenameSmartPlaylistAction.get(),
            &QAction::triggered,
            this,
            &SmartPlaylistFeature::slotRenameSmartPlaylist);

    m_pEditQueryAction =
            make_parented<QAction>(tr("Edit Search Query"), this);
    connect(m_pEditQueryAction.get(),
            &QAction::triggered,
            this,
            &SmartPlaylistFeature::slotEditQuery);

    m_pDeleteSmartPlaylistAction =
            make_parented<QAction>(tr("Remove"), this);
    const auto removeKeySequence =
            // TODO(XXX): Qt6 replace enum | with QKeyCombination
            QKeySequence(static_cast<int>(kHideRemoveShortcutModifier) |
                    kHideRemoveShortcutKey);
    m_pDeleteSmartPlaylistAction->setShortcut(removeKeySequence);
    connect(m_pDeleteSmartPlaylistAction.get(),
            &QAction::triggered,
            this,
            &SmartPlaylistFeature::slotDeleteSmartPlaylist);

    m_pSaveCurrentSearchAction =
            make_parented<QAction>(tr("Save Current Search"), this);
    connect(m_pSaveCurrentSearchAction.get(),
            &QAction::triggered,
            this,
            &SmartPlaylistFeature::slotSaveCurrentSearch);
}

QVariant SmartPlaylistFeature::title() {
    return tr("Smart Playlists");
}

void SmartPlaylistFeature::bindLibraryWidget(
        WLibrary* libraryWidget, KeyboardEventFilter* keyboard) {
    Q_UNUSED(keyboard);
    WLibraryTextBrowser* edit = new WLibraryTextBrowser(libraryWidget);
    edit->setHtml(formatRootViewHtml());
    edit->setOpenLinks(false);
    connect(edit,
            &WLibraryTextBrowser::anchorClicked,
            this,
            &SmartPlaylistFeature::htmlLinkClicked);
    libraryWidget->registerView(m_rootViewName, edit);
}

void SmartPlaylistFeature::bindSidebarWidget(WLibrarySidebar* pSidebarWidget) {
    // store the sidebar widget pointer for later use in onRightClickChild
    m_pSidebarWidget = pSidebarWidget;
}

TreeItemModel* SmartPlaylistFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void SmartPlaylistFeature::activate() {
    m_lastClickedIndex = QModelIndex();
    BaseTrackSetFeature::activate();
}

void SmartPlaylistFeature::activateChild(const QModelIndex& index) {
    const int smartPlaylistId = smartPlaylistIdFromIndex(index);
    VERIFY_OR_DEBUG_ASSERT(smartPlaylistId != kInvalidSmartPlaylistId) {
        return;
    }
    const QString query =
            m_smartPlaylistDao.getSmartPlaylistQuery(smartPlaylistId);
    m_lastClickedIndex = index;
    m_lastRightClickedIndex = QModelIndex();
    emit saveModelState();
    m_pLibrary->applySearchQuery(query);
    emit enableCoverArtDisplay(true);
}

void SmartPlaylistFeature::onRightClick(const QPoint& globalPos) {
    m_lastRightClickedIndex = QModelIndex();
    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pCreateSmartPlaylistAction.get());
    menu.exec(globalPos);
}

void SmartPlaylistFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    // Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;
    const int smartPlaylistId = smartPlaylistIdFromIndex(index);
    if (smartPlaylistId == kInvalidSmartPlaylistId) {
        return;
    }

    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pRenameSmartPlaylistAction.get());
    menu.addAction(m_pEditQueryAction.get());
    menu.addAction(m_pSaveCurrentSearchAction.get());
    menu.addSeparator();
    menu.addAction(m_pDeleteSmartPlaylistAction.get());
    menu.exec(globalPos);
}

void SmartPlaylistFeature::renameItem(const QModelIndex& index) {
    m_lastRightClickedIndex = index;
    slotRenameSmartPlaylist();
}

void SmartPlaylistFeature::deleteItem(const QModelIndex& index) {
    m_lastRightClickedIndex = index;
    slotDeleteSmartPlaylist();
}

int SmartPlaylistFeature::smartPlaylistIdFromIndex(
        const QModelIndex& index) const {
    if (!index.isValid()) {
        return kInvalidSmartPlaylistId;
    }
    TreeItem* pItem = m_pSidebarModel->getItem(index);
    VERIFY_OR_DEBUG_ASSERT(pItem != nullptr) {
        return kInvalidSmartPlaylistId;
    }
    const int smartPlaylistId = pItem->getData().toInt();
    return smartPlaylistId > 0 ? smartPlaylistId : kInvalidSmartPlaylistId;
}

QModelIndex SmartPlaylistFeature::indexFromSmartPlaylistId(
        int smartPlaylistId) const {
    if (smartPlaylistId <= 0) {
        return QModelIndex();
    }
    TreeItem* pRootItem = m_pSidebarModel->getRootItem();
    VERIFY_OR_DEBUG_ASSERT(pRootItem != nullptr) {
        return QModelIndex();
    }
    int row = 0;
    for (TreeItem* pTreeItem : pRootItem->children()) {
        if (pTreeItem->getData().toInt() == smartPlaylistId) {
            return m_pSidebarModel->index(row, 0);
        }
        ++row;
    }
    return QModelIndex();
}

int SmartPlaylistFeature::currentSelectedSmartPlaylistId() const {
    if (m_pSidebarWidget == nullptr || !m_lastClickedIndex.isValid() ||
            !m_pSidebarWidget->isChildIndexSelected(m_lastClickedIndex)) {
        return kInvalidSmartPlaylistId;
    }
    return smartPlaylistIdFromIndex(m_lastClickedIndex);
}

void SmartPlaylistFeature::constructChildModel(int selectedId) {
    TreeItem* pRootItem = m_pSidebarModel->getRootItem();
    VERIFY_OR_DEBUG_ASSERT(pRootItem != nullptr) {
        return;
    }
    m_pSidebarModel->removeRows(0, pRootItem->childRows());
    std::vector<std::unique_ptr<TreeItem>> childrenToAdd;
    const QList<SmartPlaylistDAO::SmartPlaylist> smartPlaylists =
            m_smartPlaylistDao.getSmartPlaylists();
    int selectedRow = -1;
    int row = 0;
    for (const auto& smartPlaylist : smartPlaylists) {
        if (selectedId == smartPlaylist.id) {
            selectedRow = row;
        }
        childrenToAdd.push_back(std::make_unique<TreeItem>(
                smartPlaylistLabel(smartPlaylist.name, smartPlaylist.query),
                smartPlaylist.id));
        ++row;
    }
    m_pSidebarModel->insertTreeItemRows(std::move(childrenToAdd), 0);
    m_lastClickedIndex = selectedRow == -1
            ? QModelIndex()
            : m_pSidebarModel->index(selectedRow, 0);
}

void SmartPlaylistFeature::slotCreateSmartPlaylist() {
    SmartPlaylistDialog dialog(
            m_pSidebarWidget,
            tr("Create New Smart Playlist"),
            QString(),
            QString(),
            true);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString name = dialog.name();
    if (name.isEmpty()) {
        QMessageBox::warning(nullptr,
                tr("Create New Smart Playlist"),
                tr("A smart playlist cannot have a blank name."));
        return;
    }
    if (m_smartPlaylistDao.smartPlaylistExists(name)) {
        QMessageBox::warning(nullptr,
                tr("Create New Smart Playlist"),
                tr("A smart playlist by that name already exists."));
        return;
    }
    const int smartPlaylistId =
            m_smartPlaylistDao.createSmartPlaylist(name, dialog.query());
    if (smartPlaylistId == kInvalidSmartPlaylistId) {
        return;
    }
    // The DAO emits added(), which rebuilds the sidebar. Select and activate
    // the new smart playlist.
    const QModelIndex index = indexFromSmartPlaylistId(smartPlaylistId);
    if (index.isValid() && m_pSidebarWidget) {
        m_pSidebarWidget->selectChildIndex(index, false);
        activateChild(index);
    }
}

void SmartPlaylistFeature::slotRenameSmartPlaylist() {
    const int smartPlaylistId = smartPlaylistIdFromIndex(m_lastRightClickedIndex);
    VERIFY_OR_DEBUG_ASSERT(smartPlaylistId != kInvalidSmartPlaylistId) {
        return;
    }
    const QString oldName = m_smartPlaylistDao.getSmartPlaylistName(smartPlaylistId);
    for (;;) {
        bool ok = false;
        const QString newName = QInputDialog::getText(nullptr,
                tr("Rename Smart Playlist"),
                tr("Enter new name for smart playlist:"),
                QLineEdit::Normal,
                oldName,
                &ok)
                                        .trimmed();
        if (!ok || newName.isEmpty()) {
            return;
        }
        if (m_smartPlaylistDao.smartPlaylistExists(newName)) {
            QMessageBox::warning(nullptr,
                    tr("Rename Smart Playlist"),
                    tr("A smart playlist by that name already exists."));
            continue;
        }
        m_smartPlaylistDao.renameSmartPlaylist(smartPlaylistId, newName);
        return;
    }
}

void SmartPlaylistFeature::slotEditQuery() {
    const int smartPlaylistId = smartPlaylistIdFromIndex(m_lastRightClickedIndex);
    VERIFY_OR_DEBUG_ASSERT(smartPlaylistId != kInvalidSmartPlaylistId) {
        return;
    }
    const QString oldQuery =
            m_smartPlaylistDao.getSmartPlaylistQuery(smartPlaylistId);
    SmartPlaylistDialog dialog(
            m_pSidebarWidget,
            tr("Edit Search Query"),
            m_smartPlaylistDao.getSmartPlaylistName(smartPlaylistId),
            oldQuery,
            false);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString newQuery = dialog.query();
    if (newQuery == oldQuery) {
        return;
    }
    if (!m_smartPlaylistDao.updateSmartPlaylistQuery(
                smartPlaylistId, newQuery)) {
        return;
    }
    // Apply the new query if the edited smart playlist is currently selected.
    if (currentSelectedSmartPlaylistId() == smartPlaylistId) {
        activateChild(indexFromSmartPlaylistId(smartPlaylistId));
    }
}

void SmartPlaylistFeature::slotDeleteSmartPlaylist() {
    const int smartPlaylistId = smartPlaylistIdFromIndex(m_lastRightClickedIndex);
    VERIFY_OR_DEBUG_ASSERT(smartPlaylistId != kInvalidSmartPlaylistId) {
        return;
    }
    const QString name = m_smartPlaylistDao.getSmartPlaylistName(smartPlaylistId);
    const QMessageBox::StandardButton btn = QMessageBox::question(nullptr,
            tr("Confirm Deletion"),
            tr("Do you really want to delete smart playlist <b>%1</b>?")
                    .arg(name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        return;
    }
    m_smartPlaylistDao.deleteSmartPlaylist(smartPlaylistId);
}

void SmartPlaylistFeature::slotSaveCurrentSearch() {
    const int smartPlaylistId = smartPlaylistIdFromIndex(m_lastRightClickedIndex);
    VERIFY_OR_DEBUG_ASSERT(smartPlaylistId != kInvalidSmartPlaylistId) {
        return;
    }
    m_smartPlaylistDao.updateSmartPlaylistQuery(
            smartPlaylistId, m_pLibrary->currentSearch());
}

QString SmartPlaylistFeature::smartPlaylistLabel(
        const QString& name, const QString& query) const {
    if (m_pLibraryTableModel == nullptr) {
        return name;
    }
    // Count the tracks that the query would show by running it through the
    // same library table model that the Tracks view uses.
    m_pLibraryTableModel->search(query);
    return QStringLiteral("%1 (%2)").arg(name, QString::number(
            m_pLibraryTableModel->rowCount()));
}

void SmartPlaylistFeature::rebuildSidebar() {
    // Preserve the current sidebar selection.
    const int selectedId = currentSelectedSmartPlaylistId();
    constructChildModel(selectedId);
    if (selectedId != kInvalidSmartPlaylistId && m_pSidebarWidget &&
            m_lastClickedIndex.isValid()) {
        m_pSidebarWidget->selectChildIndex(m_lastClickedIndex, false);
    }
}

void SmartPlaylistFeature::slotSmartPlaylistAdded(int smartPlaylistId) {
    Q_UNUSED(smartPlaylistId);
    rebuildSidebar();
}

void SmartPlaylistFeature::slotSmartPlaylistChanged(int smartPlaylistId) {
    Q_UNUSED(smartPlaylistId);
    rebuildSidebar();
}

void SmartPlaylistFeature::slotSmartPlaylistDeleted(int smartPlaylistId) {
    const bool wasSelected =
            currentSelectedSmartPlaylistId() == smartPlaylistId;
    const int deletedRow = m_lastClickedIndex.row();
    constructChildModel(kInvalidSmartPlaylistId);
    if (wasSelected && m_pSidebarWidget) {
        // Restore the selection to the smart playlist that took its place
        // to avoid the sidebar jumping back to the feature root item.
        TreeItem* pRootItem = m_pSidebarModel->getRootItem();
        VERIFY_OR_DEBUG_ASSERT(pRootItem != nullptr) {
            return;
        }
        const int rowCount = pRootItem->childRows();
        const int row = qMin(deletedRow, rowCount - 1);
        if (row >= 0) {
            m_pSidebarWidget->selectChildIndex(
                    m_pSidebarModel->index(row, 0), false);
        }
    }
}

void SmartPlaylistFeature::slotLibraryScanFinished() {
    // The library contents may have changed, so refresh the track counts
    // shown next to each smart playlist name.
    rebuildSidebar();
}

QString SmartPlaylistFeature::formatRootViewHtml() const {
    QString title = tr("Smart Playlists");
    QString summary1 =
            tr("Smart playlists are named search queries that filter the "
               "whole library.");
    QString summary2 =
            tr("Create a smart playlist for the Techno tracks at 124-128 BPM, "
               "for your favorite genre, or for any other search.");
    QString createLink = tr("Create New Smart Playlist");
    QString html;
    html.append(QStringLiteral("<h2>%1</h2>").arg(title));
    html.append(QStringLiteral("<p>%1</p>").arg(summary1));
    html.append(QStringLiteral("<p>%1</p>").arg(summary2));
    //Colorize links in lighter blue, instead of QT default dark blue.
    //Links are still different from regular text, but readable on dark/light backgrounds.
    //https://github.com/mixxxdj/mixxx/issues/9103
    html.append(
            QStringLiteral("<a style=\"color:#0496FF;\" href=\"create\">%1</a>")
                    .arg(createLink));
    return html;
}

void SmartPlaylistFeature::htmlLinkClicked(const QUrl& link) {
    if (QString(link.path()) == "create") {
        slotCreateSmartPlaylist();
    } else {
        qDebug() << "Unknown smart playlist link clicked" << link;
    }
}
