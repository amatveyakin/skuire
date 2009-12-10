/***************************************************************************
                         listpanel.cpp
                      -------------------
copyright            : (C) 2000 by Shie Erlich & Rafi Yanai
e-mail               : krusader@users.sourceforge.net
web site             : http://krusader.sourceforge.net
---------------------------------------------------------------------------
Description
***************************************************************************

A

db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                              S o u r c e    F i l e

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "listpanel.h"

#include <unistd.h>
#include <sys/param.h>

#include <QtGui/QBitmap>
#include <QtCore/QStringList>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QList>
#include <QPixmap>
#include <QFrame>
#include <QDropEvent>
#include <QHideEvent>
#include <QEvent>
#include <QShowEvent>
#include <QDrag>
#include <QMimeData>
#include <QtCore/QTimer>
#include <QtCore/QRegExp>
#include <QtGui/QSplitter>
#include <QtGui/QImage>
#include <qtabbar.h>

#include <kmenu.h>
#include <kdiskfreespace.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kmimetype.h>
#include <kurl.h>
#include <ktrader.h>
#include <kiconloader.h>
#include <kcursor.h>
#include <kstandarddirs.h>
#include <kglobalsettings.h>
#include <kdeversion.h>
#include <kdebug.h>
#include <kurlrequester.h>
#include <kmountpoint.h>
#include <kcolorscheme.h>

//#ifdef __LIBKONQ__
//#include <konq_popupmenu.h>
//#include <kbookmarkmanager.h>
//#endif

#include "../krusader.h"
#include "../krslots.h"
#include "panelfunc.h"
#include "../kicons.h"
#include "../VFS/krpermhandler.h"
#include "../krusaderview.h"
#include "../panelmanager.h"
#include "../defaults.h"
#include "../resources.h"
#include "../MountMan/kmountman.h"
#include "../Dialogs/krdialogs.h"
#include "../BookMan/krbookmarkbutton.h"
#include "../Dialogs/krspwidgets.h"
#include "../Dialogs/krspecialwidgets.h"
#include "../GUI/kcmdline.h"
#include "../Dialogs/percentalsplitter.h"
#include "krpreviewpopup.h"
#include "../GUI/dirhistorybutton.h"
#include "../GUI/dirhistoryqueue.h"
#include "../GUI/mediabutton.h"
#include "../GUI/syncbrowsebutton.h"
#include "../krservices.h"
#include "panelpopup.h"
#include "../UserAction/useractionpopupmenu.h"
#include "../Dialogs/popularurls.h"
#include "krpopupmenu.h"
#include "krviewfactory.h"
#include "krcolorcache.h"

typedef QList<KServiceOffer> OfferList;

/////////////////////////////////////////////////////
//      The list panel constructor       //
/////////////////////////////////////////////////////
ListPanel::ListPanel(int typeIn, QWidget *parent, bool &left) :
        QWidget(parent), panelType(typeIn), colorMask(255), compareMode(false), statsAgent(0),
        quickSearch(0), cdRootButton(0), cdUpButton(0), popupBtn(0), popup(0), inlineRefreshJob(0), _left(left),
        _locked(false)
{

    layout = new QGridLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    func = new ListPanelFunc(this);
    setAcceptDrops(true);

    layout->setContentsMargins(0, 0, 0, 0);

    mediaButton = new MediaButton(this);
    connect(mediaButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(mediaButton, SIGNAL(openUrl(const KUrl&)), func, SLOT(openUrl(const KUrl&)));

    status = new KrSqueezedTextLabel(this);
    KConfigGroup group(krConfig, "Look&Feel");
    status->setFont(group.readEntry("Filelist Font", *_FilelistFont));
    status->setBackgroundRole(QPalette::Window);
    bool statusFrame = group.readEntry("Status Frame", true);
    if(statusFrame)
        status->setFrameStyle(QFrame::Box | QFrame::Raised);
    bool statusBackground = group.readEntry("Statusbar Background", true);
    status->setAutoFillBackground(statusBackground);
    status->setLineWidth(1);    // a nice 3D touch :-)
    status->setText("");          // needed for initialization code!
    status->enableDrops(true);
    int sheight = QFontMetrics(status->font()).height() + 4;
    status->setMaximumHeight(sheight);
    status->setWhatsThis(i18n("The statusbar displays information about the FILESYSTEM "
                              "which holds your current directory: Total size, free space, "
                              "type of filesystem, etc."));
    connect(status, SIGNAL(clicked()), this, SLOT(slotFocusOnMe()));
    connect(status, SIGNAL(dropped(QDropEvent *)), this, SLOT(handleDropOnStatus(QDropEvent *)));

    // ... create the history button
    dirHistoryQueue = new DirHistoryQueue(this);
    historyButton = new DirHistoryButton(dirHistoryQueue, this);
    connect(historyButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(historyButton, SIGNAL(openUrl(const KUrl&)), func, SLOT(openUrl(const KUrl&)));

    bookmarksButton = new KrBookmarkButton(this);
    connect(bookmarksButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(bookmarksButton, SIGNAL(openUrl(const KUrl&)), func, SLOT(openUrl(const KUrl&)));
    bookmarksButton->setWhatsThis(i18n("Open menu with bookmarks. You can also add "
                                       "current location to the list, edit bookmarks "
                                       "or add subfolder to the list."));

    QHBoxLayout *totalsLayout = new QHBoxLayout;
    totals = new KrSqueezedTextLabel(this);
    totals->setFont(group.readEntry("Filelist Font", *_FilelistFont));
    if(statusFrame)
        totals->setFrameStyle(QFrame::Box | QFrame::Raised);
    totals->setAutoFillBackground(statusBackground);
    totals->setBackgroundRole(QPalette::Window);
    totals->setLineWidth(1);    // a nice 3D touch :-)
    totals->setMaximumHeight(sheight);
    totals->enableDrops(true);
    totals->setWhatsThis(i18n("The totals bar shows how many files exist, "
                              "how many selected and the bytes math"));
    connect(totals, SIGNAL(clicked()), this, SLOT(slotFocusOnMe()));
    connect(totals, SIGNAL(dropped(QDropEvent *)), this, SLOT(handleDropOnTotals(QDropEvent *)));

    // a cancel button for the inplace refresh mechanism
    inlineRefreshCancelButton = new QToolButton(this);
    inlineRefreshCancelButton->setFixedSize(22, 20);
    inlineRefreshCancelButton->setIcon(krLoader->loadIcon("dialog-cancel", KIconLoader::Toolbar, 16));
    connect(inlineRefreshCancelButton, SIGNAL(clicked()), this, SLOT(inlineRefreshCancel()));

    // a quick button to open the popup panel
    popupBtn = new QToolButton(this);
    popupBtn->setFixedSize(22, 20);
    popupBtn->setIcon(krLoader->loadIcon("arrow-up", KIconLoader::Toolbar, 16));
    connect(popupBtn, SIGNAL(clicked()), this, SLOT(togglePanelPopup()));
    popupBtn->setToolTip(i18n("Open the popup panel"));
    totalsLayout->addWidget(totals);
    totalsLayout->addWidget(inlineRefreshCancelButton); inlineRefreshCancelButton->hide();
    totalsLayout->addWidget(popupBtn);

    quickSearch = new KrQuickSearch(this);
    quickSearch->setFont(group.readEntry("Filelist Font", *_FilelistFont));
    quickSearch->setMaximumHeight(sheight);

    QWidget * hboxWidget = new QWidget(this);
    QHBoxLayout * hbox = new QHBoxLayout(hboxWidget);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(0);

    QuickNavLineEdit *qnle = new QuickNavLineEdit(this);
    origin = new KUrlRequester(qnle, hboxWidget);
    hbox->addWidget(origin);
    QSize iconSize = QSize(22, 22); // also hardcoded to 22 in the support libs
    origin->button() ->setFixedSize(iconSize.width() + 4, iconSize.height() + 4);
    origin->setWhatsThis(i18n("Use superb KDE file dialog to choose location. "));
    origin->lineEdit() ->setUrlDropsEnabled(true);
    origin->lineEdit() ->installEventFilter(this);
    origin->lineEdit()->setWhatsThis(i18n("Name of directory where you are. You can also "
                                          "enter name of desired location to move there. "
                                          "Use of Net protocols like ftp or fish is possible."));
    origin->setMode(KFile::Directory | KFile::ExistingOnly);
    connect(origin, SIGNAL(returnPressed(const QString&)), func, SLOT(openUrl(const QString&)));
    connect(origin, SIGNAL(returnPressed(const QString&)), this, SLOT(slotFocusOnMe()));
    connect(origin, SIGNAL(urlSelected(const KUrl &)), func, SLOT(openUrl(const KUrl &)));
    connect(origin, SIGNAL(urlSelected(const KUrl &)), this, SLOT(slotFocusOnMe()));

    cdOtherButton = new QToolButton(hboxWidget);
    cdOtherButton->setFixedSize(20, origin->button() ->height());
    cdOtherButton->setText(i18n("="));
    hbox->addWidget(cdOtherButton);
    cdOtherButton->setToolTip(i18n("Equal"));
    connect(cdOtherButton, SIGNAL(clicked()), this, SLOT(slotFocusAndCDOther()));

    cdUpButton = new QToolButton(hboxWidget);
    cdUpButton->setFixedSize(20, origin->button() ->height());
    cdUpButton->setText(i18n(".."));
    hbox->addWidget(cdUpButton);
    cdUpButton->setToolTip(i18n("Up"));
    connect(cdUpButton, SIGNAL(clicked()), this, SLOT(slotFocusAndCDup()));

    cdHomeButton = new QToolButton(hboxWidget);
    cdHomeButton->setFixedSize(20, origin->button() ->height());
    cdHomeButton->setText(i18n("~"));
    hbox->addWidget(cdHomeButton);
    cdHomeButton->setToolTip(i18n("Home"));
    connect(cdHomeButton, SIGNAL(clicked()), this, SLOT(slotFocusAndCDHome()));

    cdRootButton = new QToolButton(hboxWidget);
    cdRootButton->setFixedSize(20, origin->button() ->height());
    cdRootButton->setText(i18n("/"));
    hbox->addWidget(cdRootButton);
    cdRootButton->setToolTip(i18n("Root"));
    connect(cdRootButton, SIGNAL(clicked()), this, SLOT(slotFocusAndCDRoot()));

    // ... creates the button for sync-browsing
    syncBrowseButton = new SyncBrowseButton(hboxWidget);
    hbox->addWidget(syncBrowseButton);

    setPanelToolbar();

    // create a splitter to hold the view and the popup
    splt = new PercentalSplitter(this);
    splt->setChildrenCollapsible(true);
    splt->setOrientation(Qt::Vertical);

    createView();

    // make sure that a focus/path change reflects in the command line and activePanel
    connect(this, SIGNAL(cmdLineUpdate(QString)), SLOTS, SLOT(slotCurrentChanged(QString)));
    connect(this, SIGNAL(activePanelChanged(ListPanel *)), SLOTS, SLOT(slotSetActivePanel(ListPanel *)));

    // add a popup
    popup = new PanelPopup(splt, left);
    connect(popup, SIGNAL(selection(const KUrl&)), SLOTS, SLOT(refresh(const KUrl&)));
    connect(popup, SIGNAL(hideMe()), this, SLOT(togglePanelPopup()));
    popup->hide();

    // finish the layout
    int spltPos, quickSearchPos;
    QString qsPos = group.readEntry("Quicksearch position", "bottom");
    if(qsPos == "top") {
        spltPos = 4; quickSearchPos = 3;
    } else {
        spltPos = 3; quickSearchPos = 4;
    }
    layout->addWidget(hboxWidget, 0, 0, 1, 4);
    layout->addWidget(mediaButton, 1, 0);
    layout->addWidget(status, 1, 1);
    layout->addWidget(historyButton, 1, 2);
    layout->addWidget(bookmarksButton, 1, 3);
    layout->addWidget(splt, spltPos, 0, 1, 4);
    layout->addWidget(quickSearch, quickSearchPos, 0, 1, 4);
    quickSearch->hide();
    layout->addLayout(totalsLayout, 5, 0, 1, 4);
    //filter = ALL;
    setLayout(layout);

    connect(&KrColorCache::getColorCache(), SIGNAL(colorsRefreshed()), this, SLOT(refreshColors()));
}

void ListPanel::createView()
{
    view = KrViewFactory::createView(panelType, splt, _left, krConfig);

    view->init();
    view->redraw();
    view->op()->setQuickSearch(quickSearch);
    quickSearch->setFocusProxy(view->widget());

    splt->insertWidget(0, view->widget());

    connect(view->op(), SIGNAL(middleButtonClicked(KrViewItem *)), SLOTS, SLOT(newTab(KrViewItem *)));
    connect(view->op(), SIGNAL(currentChanged(KrViewItem *)), SLOTS, SLOT(updatePopupPanel(KrViewItem*)));
    connect(view->op(), SIGNAL(renameItem(const QString &, const QString &)),
            func, SLOT(rename(const QString &, const QString &)));
    connect(view->op(), SIGNAL(executed(const QString&)), func, SLOT(execute(const QString&)));
    connect(view->op(), SIGNAL(goInside(const QString&)), func, SLOT(goInside(const QString&)));
    connect(view->op(), SIGNAL(needFocus()), this, SLOT(slotFocusOnMe()));
    connect(view->op(), SIGNAL(selectionChanged()), this, SLOT(slotUpdateTotals()));
    connect(view->op(), SIGNAL(itemDescription(QString&)), krApp, SLOT(statusBarUpdate(QString&)));
    connect(view->op(), SIGNAL(contextMenu(const QPoint &)), this, SLOT(popRightClickMenu(const QPoint &)));
    connect(view->op(), SIGNAL(emptyContextMenu(const QPoint &)),
            this, SLOT(popEmptyRightClickMenu(const QPoint &)));
    connect(view->op(), SIGNAL(letsDrag(QStringList, QPixmap)), this, SLOT(startDragging(QStringList, QPixmap)));
    connect(view->op(), SIGNAL(gotDrop(QDropEvent *)), this, SLOT(handleDropOnView(QDropEvent *)));
}

void ListPanel::changeType(int type)
{
    if (panelType != type) {
        panelType = type;
        quickSearch->setFocusProxy(0);
        delete view;
        createView();

        slotStartUpdate();

        view->redraw();
    }
}

ListPanel::~ListPanel()
{
    delete func;
    delete view;
    delete status;
    delete bookmarksButton;
    delete totals;
    delete quickSearch;
    delete origin;
    delete cdRootButton;
    delete cdHomeButton;
    delete cdUpButton;
    delete cdOtherButton;
    delete syncBrowseButton;
    delete layout;
}

int ListPanel::getProperties()
{
    int props = 0;
    if (syncBrowseButton->state() == SYNCBROWSE_CD)
        props |= PROP_SYNC_BUTTON_ON;
    if (_locked)
        props |= PROP_LOCKED;
    return props;
}

void ListPanel::setProperties(int prop)
{
    if (prop & PROP_SYNC_BUTTON_ON)
        syncBrowseButton->setChecked(true);
    else
        syncBrowseButton->setChecked(false);

    if (prop & PROP_LOCKED)
        _locked = true;
    else
        _locked = false;
}

bool ListPanel::eventFilter(QObject * watched, QEvent * e)
{
    if (e->type() == QEvent::KeyPress && origin->lineEdit() == watched) {
        QKeyEvent *ke = (QKeyEvent *)e;

        if ((ke->key() ==  Qt::Key_Down) && (ke->modifiers() == Qt::ControlModifier)) {
            slotFocusOnMe();
            return true;
        }
        if ((ke->key() ==  Qt::Key_Escape) && (ke->modifiers() == 0)) {
            slotFocusOnMe();
            return true;
        }
    }
    return false;
}


void ListPanel::togglePanelPopup()
{
    if (popup->isHidden()) {
        if (popupSizes.count() > 0) {
            dynamic_cast<QSplitter*>(popup->parent())->setSizes(popupSizes);
        } else { // on the first time, resize to 50%
            QList<int> lst;
            lst << height() / 2 << height() / 2;
            dynamic_cast<QSplitter*>(popup->parent())->setSizes(lst);
        }

        popup->show();
        popupBtn->setIcon(krLoader->loadIcon("arrow-down", KIconLoader::Toolbar, 16));
        popupBtn->setToolTip(i18n("Close the popup panel"));
    } else {
        popupSizes.clear();
        popupSizes = dynamic_cast<QSplitter*>(popup->parent())->sizes();
        popup->hide();
        popupBtn->setIcon(krLoader->loadIcon("arrow-up", KIconLoader::Toolbar, 16));
        popupBtn->setToolTip(i18n("Open the popup panel"));

        QList<int> lst;
        lst << height() << 0;
        dynamic_cast<QSplitter*>(popup->parent())->setSizes(lst);
        if (ACTIVE_PANEL)
            ACTIVE_PANEL->slotFocusOnMe();
    }
}

KUrl ListPanel::virtualPath() const
{
    return func->files()->vfs_getOrigin();
}

QString ListPanel::realPath() const
{
    return _realPath.path();
}


void ListPanel::setPanelToolbar()
{
    KConfigGroup group(krConfig, "Look&Feel");

    bool panelToolBarVisible = group.readEntry("Panel Toolbar visible", _PanelToolBar);

    if (panelToolBarVisible && (group.readEntry("Root Button Visible", _cdRoot)))
        cdRootButton->show();
    else
        cdRootButton->hide();

    if (panelToolBarVisible && (group.readEntry("Home Button Visible", _cdHome)))
        cdHomeButton->show();
    else
        cdHomeButton->hide();

    if (panelToolBarVisible && (group.readEntry("Up Button Visible", _cdUp)))
        cdUpButton->show();
    else
        cdUpButton->hide();

    if (panelToolBarVisible && (group.readEntry("Equal Button Visible", _cdOther)))
        cdOtherButton->show();
    else
        cdOtherButton->hide();

    if (!panelToolBarVisible || (group.readEntry("Open Button Visible", _Open)))
        origin->button() ->show();
    else
        origin->button() ->hide();

    if (panelToolBarVisible && (group.readEntry("SyncBrowse Button Visible", _syncBrowseButton)))
        syncBrowseButton->show();
    else
        syncBrowseButton->hide();
}

void ListPanel::slotUpdateTotals()
{
    totals->setText(view->statistics());
}

void ListPanel::slotFocusAndCDOther()
{
    slotFocusOnMe();
    func->openUrl(otherPanel->func->files() ->vfs_getOrigin());

}

void ListPanel::slotFocusAndCDHome()
{
    slotFocusOnMe();
    func->openUrl(QString("~"), QString());
}

void ListPanel::slotFocusAndCDup()
{
    slotFocusOnMe();
    func->dirUp();
}

void ListPanel::slotFocusAndCDRoot()
{
    slotFocusOnMe();
    func->openUrl(QString(ROOT_DIR), QString());
}

void ListPanel::select(KRQuery query, bool select)
{
    if (!query.isNull()) {
        if (select)
            view->select(query);
        else
            view->unselect(query);
    }
}

void ListPanel::select(bool select, bool all)
{
    if (all) {
        if (select)
            view->select(KRQuery("*"));
        else
            view->unselect(KRQuery("*"));
    } else {
        KRQuery query = KRSpWidgets::getMask((select ? i18n(" Select Files ") : i18n(" Unselect Files ")));
        // if the user canceled - quit
        if (query.isNull())
            return ;
        if (select)
            view->select(query);
        else
            view->unselect(query);
    }
}

void ListPanel::invertSelection()
{
    view->invertSelection();
}

void ListPanel::compareDirs()
{
    KConfigGroup pg(krConfig, "Private");
    int compareMode = pg.readEntry("Compare Mode", 0);
    KConfigGroup group(krConfig, "Look&Feel");
    bool selectDirs = group.readEntry("Mark Dirs", false);

    KrViewItem *item, *otherItem;

    for (item = view->getFirst(); item != 0; item = view->getNext(item)) {
        if (item->name() == "..")
            continue;

        for (otherItem = otherPanel->view->getFirst(); otherItem != 0 && otherItem->name() != item->name() ;
                otherItem = otherPanel->view->getNext(otherItem));

        bool isSingle = (otherItem == 0), isDifferent = false, isNewer = false;

        if (func->getVFile(item)->vfile_isDir() && !selectDirs) {
            item->setSelected(false);
            continue;
        }

        if (otherItem) {
            if (!func->getVFile(item)->vfile_isDir())
                isDifferent = ITEM2VFILE(otherPanel, otherItem)->vfile_getSize() != func->getVFile(item)->vfile_getSize();
            isNewer = func->getVFile(item)->vfile_getTime_t() > ITEM2VFILE(otherPanel, otherItem)->vfile_getTime_t();
        }

        switch (compareMode) {
        case 0:
            item->setSelected(isNewer || isSingle);
            break;
        case 1:
            item->setSelected(isNewer);
            break;
        case 2:
            item->setSelected(isSingle);
            break;
        case 3:
            item->setSelected(isDifferent || isSingle);
            break;
        case 4:
            item->setSelected(isDifferent);
            break;
        }
    }

    view->updateView();
}

QColor ListPanel::getColor(KConfigGroup &cg, QString name, const QColor &def, const QColor &kdedef)
{
    if (cg.readEntry(name, QString()) == "KDE default")
        return kdedef;
    else if(!cg.readEntry(name, QString()).isEmpty())
        return cg.readEntry(name, def);
    return def;
}

void ListPanel::refreshColors()
{
    QColor windowForeground = KColorScheme(QPalette::Active, KColorScheme::Window).foreground().color();
    QColor windowBackground = KColorScheme(QPalette::Active, KColorScheme::Window).background().color();
    KConfigGroup gc(krConfig, "Colors");
    QPalette p(status->palette());

    if(this == ACTIVE_PANEL) {
        p.setColor(QPalette::WindowText, getColor(gc, "Statusbar Foreground Active",
                        KColorScheme(QPalette::Active, KColorScheme::Selection).foreground().color(), windowForeground));
        p.setColor(QPalette::Window, getColor(gc, "Statusbar Background Active",
                    KColorScheme(QPalette::Active, KColorScheme::Selection).background().color(), windowBackground));
    } else {
        p.setColor(QPalette::WindowText, getColor(gc, "Statusbar Foreground Inactive",
                    KColorScheme(QPalette::Active, KColorScheme::View).foreground().color(), windowForeground));
        p.setColor(QPalette::Window, getColor(gc, "Statusbar Background Inactive",
                  KColorScheme(QPalette::Active, KColorScheme::View).background().color(), windowBackground));
    }

    status->setPalette(p);
    totals->setPalette(p);
}

void ListPanel::slotFocusOnMe()
{
    // give this VFS the focus (the path bar)
    // we start by calling the KVFS function

    krApp->setUpdatesEnabled(false);

    otherPanel->view->prepareForPassive();
    view->prepareForActive();

    emit cmdLineUpdate(realPath());
    emit activePanelChanged(this);

    otherPanel->refreshColors();
    refreshColors();

    func->refreshActions();

    if (Krusader::actView0) Krusader::actView0->setEnabled(panelType != 0);
    if (Krusader::actView1) Krusader::actView1->setEnabled(panelType != 1);
    if (Krusader::actView2) Krusader::actView2->setEnabled(panelType != 2);
    if (Krusader::actView3) Krusader::actView3->setEnabled(panelType != 3);
    if (Krusader::actView4) Krusader::actView4->setEnabled(panelType != 4);
    if (Krusader::actView5) Krusader::actView5->setEnabled(panelType != 5);

    view->refreshColors();
    otherPanel->view->refreshColors();

    krApp->setUpdatesEnabled(true);
}

// this is used to start the panel, AFTER setOther() has been used
//////////////////////////////////////////////////////////////////
void ListPanel::start(KUrl url, bool immediate)
{
    KUrl virt;

    virt = url;

    if (!virt.isValid())
        virt = KUrl(ROOT_DIR);
    if (virt.isLocalFile()) _realPath = virt;
    else _realPath = KUrl(ROOT_DIR);

    if (immediate)
        func->immediateOpenUrl(virt, true);
    else
        func->openUrl(virt);

    slotFocusOnMe();
    setJumpBack(virt);
}

void ListPanel::slotStartUpdate()
{
    while (func->inRefresh)
        ; // wait until the last refresh finish
    func->inRefresh = true;  // make sure the next refresh wait for this one
    if (inlineRefreshJob)
        inlineRefreshListResult(0);

    if (this == ACTIVE_PANEL) {
        slotFocusOnMe();
    }

    setCursor(Qt::BusyCursor);
    view->clear();

    if (func->files() ->vfs_getType() == vfs::VFS_NORMAL)
        _realPath = virtualPath();
    this->origin->setUrl(virtualPath().pathOrUrl());
    emit pathChanged(this);
    emit cmdLineUpdate(realPath());   // update the command line

    slotGetStats(virtualPath());
    slotUpdate();
    if (compareMode) {
        otherPanel->view->clear();
        otherPanel->slotUpdate();
    }
    // return cursor to normal arrow
    setCursor(Qt::ArrowCursor);
    slotUpdateTotals();
    krApp->popularUrls->addUrl(virtualPath());
}

void ListPanel::slotUpdate()
{
    // if we are not at the root add the ".." entery
    QString protocol = func->files() ->vfs_getOrigin().protocol();
    bool isFtp = (protocol == "ftp" || protocol == "smb" || protocol == "sftp" || protocol == "fish");

    QString origin = virtualPath().prettyUrl(KUrl::RemoveTrailingSlash);
    if (origin.right(1) != "/" && !((func->files() ->vfs_getType() == vfs::VFS_FTP) && isFtp &&
                                    origin.indexOf('/', origin.indexOf(":/") + 3) == -1)) {
        view->addItems(func->files());
    } else
        view->addItems(func->files(), false);

    func->inRefresh = false;
}


void ListPanel::slotGetStats(const KUrl& url)
{
    if (!url.isLocalFile()) {
        status->setText(i18n("No space information on non-local filesystems"));
        return ;
    }

    // check for special filesystems;
    QString path = url.path(); // must be local url
    if (path.left(4) == "/dev") {
        status->setText(i18n("No space information on [dev]"));
        return;
    }
#ifdef BSD
    if (path.left(5) == "/procfs") {     // /procfs is a special case - no volume information
        status->setText(i18n("No space information on [procfs]"));
        return;
    }
#else
    if (path.left(5) == "/proc") {     // /proc is a special case - no volume information
        status->setText(i18n("No space information on [proc]"));
        return;
    }
#endif

    status->setText(i18n("Mt.Man: working ..."));
    statsAgent = KDiskFreeSpace::findUsageInfo(path);
    connect(statsAgent, SIGNAL(foundMountPoint(const QString &, quint64, quint64, quint64)),
            this, SLOT(gotStats(const QString &, quint64, quint64, quint64)));
}

void ListPanel::gotStats(const QString &mountPoint, quint64 kBSize,
                         quint64,  quint64 kBAvail)
{
    int perc = 0;
    if (kBSize != 0) { // make sure that if totalsize==0, then perc=0
        perc = (int)(((float)kBAvail / (float)kBSize) * 100.0);
    }
    // mount point information - find it in the list first
    KMountPoint::List lst = KMountPoint::currentMountPoints();
    QString fstype = i18n("unknown");
    for (KMountPoint::List::iterator it = lst.begin(); it != lst.end(); ++it) {
        if ((*it)->mountPoint() == mountPoint) {
            fstype = (*it)->mountType();
            break;
        }
    }

    QString stats = i18n("%1 free out of %2 (%3%) on %4 [ (%5) ]",
                         KIO::convertSizeFromKiB(kBAvail),
                         KIO::convertSizeFromKiB(kBSize), perc,
                         mountPoint, fstype);
    status->setText(stats);
}

void ListPanel::handleDropOnTotals(QDropEvent *e)
{
    handleDropOnView(e, totals);
}

void ListPanel::handleDropOnStatus(QDropEvent *e)
{
    handleDropOnView(e, status);
}

void ListPanel::handleDropOnView(QDropEvent *e, QWidget *widget)
{
    // if copyToPanel is true, then we call a simple vfs_addfiles
    bool copyToDirInPanel = false;
    bool dragFromOtherPanel = false;
    bool dragFromThisPanel = false;
    bool isWritable = func->files() ->vfs_isWritable();

    vfs* tempFiles = func->files();
    vfile *file;
    KrViewItem *i = 0;
    if (widget == 0) {
        i = view->getKrViewItemAt(e->pos());
        widget = this;
    }

    if (e->source() == otherPanel)
        dragFromOtherPanel = true;
    if (e->source() == this)
        dragFromThisPanel = true;

    if (i) {
        file = func->files() ->vfs_search(i->name());

        if (!file) {   // trying to drop on the ".."
            copyToDirInPanel = true;
        } else {
            if (file->vfile_isDir()) {
                copyToDirInPanel = true;
                isWritable = file->vfile_isWriteable();
                if (isWritable) {
                    // keep the folder_open icon until we're finished, do it only
                    // if the folder is writeable, to avoid flicker
                }
            } else if (e->source() == this)
                return ; // no dragging onto ourselves
        }
    } else    // if dragged from this panel onto an empty spot in the panel...
        if (dragFromThisPanel) {    // leave!
            e->ignore();
            return ;
        }

    if (!isWritable && getuid() != 0) {
        e->ignore();
        KMessageBox::sorry(0, i18n("Can't drop here, no write permissions."));
        return ;
    }
    //////////////////////////////////////////////////////////////////////////////
    // decode the data
    KUrl::List URLs = KUrl::List::fromMimeData(e->mimeData());
    if (URLs.isEmpty()) {
        e->ignore(); // not for us to handle!
        return ;
    }

    bool isLocal = true;
    for (int u = 0; u != URLs.count(); u++)
        if (!URLs[ u ].isLocalFile()) {
            isLocal = false;
            break;
        }

    KIO::CopyJob::CopyMode mode = KIO::CopyJob::Copy;

    // the KUrl::List is finished, let's go
    // --> display the COPY/MOVE/LINK menu

    QMenu popup(this);
    QAction * act;

    act = popup.addAction(i18n("Copy Here"));
    act->setData(QVariant(1));
    if (func->files() ->vfs_isWritable()) {
        act = popup.addAction(i18n("Move Here"));
        act->setData(QVariant(2));
    }
    if (func->files() ->vfs_getType() == vfs::VFS_NORMAL && isLocal) {
        act = popup.addAction(i18n("Link Here"));
        act->setData(QVariant(3));
    }
    act = popup.addAction(i18n("Cancel"));
    act->setData(QVariant(4));

    QPoint tmp = widget->mapToGlobal(e->pos());

    int result = -1;
    QAction * res = popup.exec(tmp);
    if (res && res->data().canConvert<int> ())
        result = res->data().toInt();

    switch (result) {
    case 1 :
        mode = KIO::CopyJob::Copy;
        break;
    case 2 :
        mode = KIO::CopyJob::Move;
        break;
    case 3 :
        mode = KIO::CopyJob::Link;
        break;
    default :         // user pressed outside the menu
        return ;          // or cancel was pressed;
    }

    QString dir = "";
    if (copyToDirInPanel) {
        dir = i->name();
    }
    QWidget *notify = (!e->source() ? 0 : e->source());
    tempFiles->vfs_addFiles(&URLs, mode, notify, dir);
}

void ListPanel::vfs_refresh(KJob *job)
{
    if (func)
        func->refresh();
}

void ListPanel::startDragging(QStringList names, QPixmap px)
{
    KUrl::List * urls = func->files() ->vfs_getFiles(&names);

    if (urls->isEmpty()) {   // avoid dragging empty urls
        delete urls;
        return ;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    drag->setPixmap(px);
    urls->populateMimeData(mimeData);
    drag->setMimeData(mimeData);

    drag->start();

    delete urls; // free memory
}

// pops a right-click menu for items
void ListPanel::popRightClickMenu(const QPoint &loc)
{
    // run it, on the mouse location
    int j = QFontMetrics(font()).height() * 2;
    KrPopupMenu::run(QPoint(loc.x() + 5, loc.y() + j), this);
}

void ListPanel::popEmptyRightClickMenu(const QPoint &loc)
{
    KrPopupMenu::run(loc, this);
}

void ListPanel::setFilter(KrViewProperties::FilterSpec f)
{
    switch (f) {
    case KrViewProperties::All :
        //case KrView::EXEC:
        break;
    case KrViewProperties::Custom :
        filterMask = KRSpWidgets::getMask(i18n(" Select Files "));
        // if the user canceled - quit
        if (filterMask.isNull())
            return;
        view->setFilterMask(filterMask);
        break;
    default:
        return ;
    }
    view->setFilter(f);   // do that in any case
    func->files()->vfs_invalidate();
    func->refresh();
}

QString ListPanel::getCurrentName()
{
    QString name = view->getCurrentItem();
    if (name != "..")
        return name;
    else
        return QString();
}

void ListPanel::prepareToDelete()
{
    view->setNameToMakeCurrent(view->firstUnmarkedBelowCurrent());
}

void ListPanel::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Enter :
    case Qt::Key_Return :
        if (e->modifiers() & Qt::ControlModifier) {
            if (e->modifiers() & Qt::AltModifier) {
                vfile *vf = func->files()->vfs_search(view->getCurrentKrViewItem()->name());
                if (vf && vf->vfile_isDir()) SLOTS->newTab(vf->vfile_getUrl());
            } else {
                SLOTS->insertFileName((e->modifiers() & Qt::ShiftModifier) != 0);
            }
        } else e->ignore();
        break;
    case Qt::Key_Right :
    case Qt::Key_Left :
        if (e->modifiers() == Qt::ControlModifier) {
            // user pressed CTRL+Right/Left - refresh other panel to the selected path if it's a
            // directory otherwise as this one
            if ((_left && e->key() == Qt::Key_Right) || (!_left && e->key() == Qt::Key_Left)) {
                KUrl newPath;
                KrViewItem *it = view->getCurrentKrViewItem();

                if (it->name() == "..") {
                    newPath = func->files()->vfs_getOrigin().upUrl();
                } else {
                    vfile *v = func->getVFile(it);
                    if (v && v->vfile_isDir() && v->vfile_getName() != "..") {
                        newPath = v->vfile_getUrl();
                    } else {
                        newPath = func->files() ->vfs_getOrigin();
                    }
                }
                otherPanel->func->openUrl(newPath);
            } else func->openUrl(otherPanel->func->files() ->vfs_getOrigin());
            return ;
        } else
            e->ignore();
        break;

    case Qt::Key_Down :
        if (e->modifiers() == Qt::ControlModifier) {   // give the keyboard focus to the command line
            if (MAIN_VIEW->cmdLine->isVisible())
                MAIN_VIEW->cmdLineFocus();
            else
                MAIN_VIEW->focusTerminalEmulator();
            return ;
        } else if (e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {     // give the keyboard focus to TE
            MAIN_VIEW->focusTerminalEmulator();
        } else
            e->ignore();
        break;

    case Qt::Key_Up :
        if (e->modifiers() == Qt::ControlModifier) {   // give the keyboard focus to the command line
            origin->lineEdit()->setFocus();
            origin->lineEdit()->selectAll();
            return ;
        } else
            e->ignore();
        break;

    default:
        // if we got this, it means that the view is not doing
        // the quick search thing, so send the characters to the commandline, if normal key
        if (e->modifiers() == Qt::NoModifier)
            MAIN_VIEW->cmdLine->addText(e->text());

        //e->ignore();
    }
}

void ListPanel::slotItemAdded(vfile *vf)
{
    view->addItem(vf);
}

void ListPanel::slotItemDeleted(const QString& name)
{
    view->delItem(name);
}

void ListPanel::slotItemUpdated(vfile *vf)
{
    view->updateItem(vf);
}

void ListPanel::slotCleared()
{
    view->clear();
}

void ListPanel::showEvent(QShowEvent *e)
{
    panelActive();
    QWidget::showEvent(e);
}

void ListPanel::hideEvent(QHideEvent *e)
{
    panelInactive();
    QWidget::hideEvent(e);
}

void ListPanel::panelActive()
{
    // don't refresh when not active (ie: hidden, application isn't focused ...)
    if (!func->files()->vfs_enableRefresh(true))
        func->popErronousUrl();
}

void ListPanel::panelInactive()
{
    func->files()->vfs_enableRefresh(false);
}

void ListPanel::slotJobStarted(KIO::Job* job)
{
    // disable the parts of the panel we don't want touched
    status->setEnabled(false);
    origin->setEnabled(false);
    cdRootButton->setEnabled(false);
    cdHomeButton->setEnabled(false);
    cdUpButton->setEnabled(false);
    cdOtherButton->setEnabled(false);
    popupBtn->setEnabled(false);
    popup->setEnabled(false);
    bookmarksButton->setEnabled(false);
    historyButton->setEnabled(false);
    syncBrowseButton->setEnabled(false);

    // connect to the job interface to provide in-panel refresh notification
    connect(job, SIGNAL(infoMessage(KJob*, const QString &)),
            SLOT(inlineRefreshInfoMessage(KJob*, const QString &)));
    connect(job, SIGNAL(percent(KJob*, unsigned long)),
            SLOT(inlineRefreshPercent(KJob*, unsigned long)));
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(inlineRefreshListResult(KJob*)));

    inlineRefreshJob = job;

    totals->setText(i18n(">> Reading..."));
    inlineRefreshCancelButton->show();
}

void ListPanel::inlineRefreshCancel()
{
    if (inlineRefreshJob) {
        inlineRefreshJob->kill(KJob::EmitResult);
        inlineRefreshJob = 0;
    }
}

void ListPanel::inlineRefreshPercent(KJob*, unsigned long perc)
{
    QString msg = QString(">> %1: %2 % complete...").arg(i18n("Reading")).arg(perc);
    totals->setText(msg);
}

void ListPanel::inlineRefreshInfoMessage(KJob*, const QString &msg)
{
    totals->setText(">> " + i18n("Reading: ") + msg);
}

void ListPanel::inlineRefreshListResult(KJob*)
{
    inlineRefreshJob = 0;
    // reenable everything
    status->setEnabled(true);
    origin->setEnabled(true);
    cdRootButton->setEnabled(true);
    cdHomeButton->setEnabled(true);
    cdUpButton->setEnabled(true);
    cdOtherButton->setEnabled(true);
    popupBtn->setEnabled(true);
    popup->setEnabled(true);
    bookmarksButton->setEnabled(true);
    historyButton->setEnabled(true);
    syncBrowseButton->setEnabled(true);

    inlineRefreshCancelButton->hide();
}

void ListPanel::jumpBack()
{
    func->openUrl(_jumpBackURL);
}

void ListPanel::setJumpBack(KUrl url)
{
    _jumpBackURL = url;
}

#include "listpanel.moc"
