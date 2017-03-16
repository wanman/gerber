/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    edit_load.cc - this file is part of MediaTomb.
    
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

/// \file edit_load.cc

#include "pages.h"

#include <cstdio>
#include "common.h"
#include "cds_objects.h"
#include "tools.h"
#include "storage.h"

//#include "server.h"
//#include "content_manager.h"


using namespace zmm;
using namespace mxml;

web::edit_load::edit_load() : WebRequestHandler()
{}

void web::edit_load::process()
{
    check_request();
    
    shared_ptr<Storage> storage;
    
    String objID = param(_("object_id"));
    int objectID;
    if (objID == nullptr)
        throw _Exception(_("invalid object id"));
    else
        objectID = objID.toInt();
    
    storage = Storage::getInstance();
    shared_ptr<CdsObject> obj = storage->loadObject(objectID);
    
    shared_ptr<Element> item (new Element(_("item")));
    
    item->setAttribute(_("object_id"), objID, mxml_int_type);
    
    shared_ptr<Element> title (new Element(_("title")));
    title->setTextKey(_("value"));
    title->setText(obj->getTitle());
    title->setAttribute(_("editable"), obj->isVirtual() || objectID == CDS_ID_FS_ROOT ? _("1") : _("0"), mxml_bool_type);
    item->appendElementChild(title);
    
    shared_ptr<Element> classEl (new Element(_("class")));
    classEl->setTextKey(_("value"));
    classEl->setText(obj->getClass());
    classEl->setAttribute(_("editable"), _("1"), mxml_bool_type);
    item->appendElementChild(classEl);
    
    int objectType = obj->getObjectType();
    item->appendTextChild(_("obj_type"), CdsObject::mapObjectType(objectType));
    
    if (IS_CDS_ITEM(objectType))
    {
        shared_ptr<CdsItem> objItem = dynamic_pointer_cast<CdsItem>(obj);
        
        shared_ptr<Element> description (new Element(_("description")));
        description->setTextKey(_("value"));
        description->setText(objItem->getMetadata(_("dc:description")));
        description->setAttribute(_("editable"), _("1"), mxml_bool_type);
        item->appendElementChild(description);
        
        shared_ptr<Element> location (new Element(_("location")));
        location->setTextKey(_("value"));
        location->setText(objItem->getLocation());
        if (IS_CDS_PURE_ITEM(objectType) || ! objItem->isVirtual())
            location->setAttribute(_("editable"),_("0"), mxml_bool_type);
        else
            location->setAttribute(_("editable"),_("1"), mxml_bool_type);
        item->appendElementChild(location);
        
        shared_ptr<Element> mimeType (new Element(_("mime-type")));
        mimeType->setTextKey(_("value"));
        mimeType->setText(objItem->getMimeType());
        mimeType->setAttribute(_("editable"), _("1"), mxml_bool_type);
        item->appendElementChild(mimeType);
        
        if (IS_CDS_ITEM_EXTERNAL_URL(objectType))
        {
            shared_ptr<Element> protocol (new Element(_("protocol")));
            protocol->setTextKey(_("value"));
            protocol->setText(getProtocol(objItem->getResource(0)->getAttribute(_("protocolInfo"))));
            protocol->setAttribute(_("editable"), _("1"), mxml_bool_type);
            item->appendElementChild(protocol);
        }
        else if (IS_CDS_ACTIVE_ITEM(objectType))
        {
            shared_ptr<CdsActiveItem> objActiveItem = dynamic_pointer_cast<CdsActiveItem>(objItem);
            
            shared_ptr<Element> action (new Element(_("action")));
            action->setTextKey(_("value"));
            action->setText(objActiveItem->getAction());
            action->setAttribute(_("editable"), _("1"), mxml_bool_type);
            item->appendElementChild(action);
            
            shared_ptr<Element> state (new Element(_("state")));
            state->setTextKey(_("value"));
            state->setText(objActiveItem->getState());
            state->setAttribute(_("editable"), _("1"), mxml_bool_type);
            item->appendElementChild(state);
        }
    }
    
    root->appendElementChild(item);
    //log_debug("serving XML: \n%s\n",  root->print().c_str());
}
