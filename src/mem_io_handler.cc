/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    mem_io_handler.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file mem_io_handler.cc

#include "mem_io_handler.h"
#include "cds_objects.h"
#include "common.h"
#include "dictionary.h"
#include "process.h"
#include "server.h"
#include "storage.h"
#include "update_manager.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <upnp-1.8/ixml.h>

using namespace zmm;
using namespace mxml;

MemIOHandler::MemIOHandler(const void* buffer, size_t length)
    : buffer((char*)MALLOC(length))
    , length(length)
    , pos(0)
{
    memcpy(this->buffer, buffer, length);
}

MemIOHandler::MemIOHandler(String str)
    : buffer((char*)MALLOC(str.length()))
    , length(str.length())
    , pos(0)
{
    memcpy(this->buffer, str.c_str(), length);
}

MemIOHandler::~MemIOHandler()
{
    FREE(buffer);
}

void MemIOHandler::open(IN enum UpnpOpenFileMode mode)
{
    pos = 0;
    atEOF = false;
}

int MemIOHandler::read(OUT char* buf, IN size_t length)
{
    int ret = 0;

    if (atEOF) {
        return 0;
    }

    size_t rest = this->length - pos;
    if (length > (size_t)rest)
        length = rest;

    memcpy(buf, buffer + pos, length);
    pos = pos + length;
    ret = (int)length;

    if (pos >= this->length) {
        atEOF = true;
    }

    return ret;
}

void MemIOHandler::seek(IN off_t offset, IN int whence)
{
    if (whence == SEEK_SET) {
        // offset must be positive when SEEK_SET is used
        if (offset < 0) {
            throw _Exception(_("MemIOHandler seek failed: SEEK_SET used with negative offset"));
        }

        if (offset > length) {
            throw _Exception(_("MemIOHandler seek failed: trying to seek past the end of file"));
        }

        pos = static_cast<size_t>(offset);
    } else if (whence == SEEK_CUR) {
        long temp;

        if (atEOF) {
            temp = length;
        } else {
            temp = pos;
        }

        if (((temp + offset) > length) || ((temp + offset) < 0)) {
            throw _Exception(_("MemIOHandler seek failed: trying to seek before the beginning/past end of file"));
        }

        pos = static_cast<size_t>(temp + offset);
    } else if (whence == SEEK_END) {
        long temp = length;
        if (((temp + offset) > length) || ((temp + offset) < 0)) {
            throw _Exception(_("MemIOHandler seek failed: trying to seek before the beginning/past end of file"));
        }

        pos = static_cast<size_t>(temp + offset);
    } else {
        throw _Exception(_("MemIOHandler seek failed: unrecognized whence"));
    }
}
