/*****************************************************************************
 * Copyright (C) 2010 Jan Lepper <dehtris@yahoo.de>                          *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This package is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this package; if not, write to the Free Software               *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#ifndef __ABSTRACTPANELMANAGER_H__
#define __ABSTRACTPANELMANAGER_H__

#include <kurl.h>

class AbstractListPanel;
class AbstractTwinPanelFM;

class AbstractPanelManager
{
public:
    virtual ~AbstractPanelManager() {}
    virtual bool isActive() = 0;
    virtual bool isLeft() = 0;
    virtual AbstractPanelManager *otherManager() = 0;
    virtual AbstractListPanel *currentPanel() = 0;
    virtual void newTab(const KUrl&, AbstractListPanel *nextTo = 0) = 0;
    virtual AbstractTwinPanelFM *mainWindow() = 0;
};

class CurrentPanelCallback
{
public:
    virtual ~CurrentPanelCallback() {}
    virtual void onPanelCreated(AbstractListPanel *panel) = 0;
    virtual void onCurrentPanelChanged(AbstractListPanel *panel) = 0;
};

#endif // __ABSTRACTPANELMANAGER_H__
