/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    xpath.h - this file is part of MediaTomb.
    
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

/// \file xpath.h

#ifndef __XPATH_H__
#define __XPATH_H__

#include "zmm/zmmf.h"
#include "mxml/mxml.h"

class XPath : public zmm::Object
{
protected:
    std::shared_ptr<mxml::Element> context;
public:
    XPath(std::shared_ptr<mxml::Element> context);
    zmm::String getText(zmm::String xpath);
    std::shared_ptr<mxml::Element> getElement(zmm::String xpath);

    static zmm::String getPathPart(zmm::String xpath);
    static zmm::String getAxisPart(zmm::String xpath);
    static zmm::String getAxis(zmm::String axisPart);
    static zmm::String getSpec(zmm::String axisPart);
protected:
    std::shared_ptr<mxml::Element> elementAtPath(zmm::String path);
};

#endif // __XPATH_H__
