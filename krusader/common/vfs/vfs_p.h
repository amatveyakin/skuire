/*****************************************************************************
 * Copyright (C) 2012 Jan Lepper <jan_lepper@gmx.de>                         *
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

#ifndef __VFS_VFS_P_H__
#define __VFS_VFS_P_H__

#include <kurl.h>


namespace VFS
{

inline bool isVirtUrl(const KUrl &url)
{
    return (url.isValid() && url.protocol() == "virt") ? true : false;
}

inline QString virtualDirFromUrl(const KUrl &url)
{
    if (isVirtUrl(url)) {
        if (url.upUrl().path() == "/")
            return url.fileName();
    }
    return QString();
}

KUrl getVirtualBaseURL(KUrl srcBaseUrl, KUrl::List srcUrls, KUrl dest);


} // namespace VFS

#endif // __VFS_VFS_P_H__
