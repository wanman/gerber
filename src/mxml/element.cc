/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    element.cc - this file is part of MediaTomb.
    
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

/// \file element.cc

#include <cassert>

#include "element.h"
#include "tools.h"

using namespace zmm;
using namespace mxml;

Element::Element(String name) : Node()
{
    type = mxml_node_element;
    this->name = name;
    arrayType = false;
    arrayName = nullptr;
    textKey = nullptr;
}
Element::Element(String name, shared_ptr<Context> context) : Node()
{
    type = mxml_node_element;
    this->name = name;
    this->context = context;
    arrayType = false;
    arrayName = nullptr;
    textKey = nullptr;
}
String Element::getAttribute(String name)
{
    if(attributes == nullptr)
        return nullptr;
    int len = attributes->size();
    for(int i = 0; i < len; i++)
    {
        shared_ptr<Attribute> attr = attributes->get(i);
        if(attr->name == name)
            return attr->value;
    }
    return nullptr;
}
void Element::addAttribute(String name, String value, enum mxml_value_type type)
{
    shared_ptr<Attribute> attr = shared_ptr<Attribute>(new Attribute(name, value, type));
    addAttribute(attr);
}

void Element::addAttribute(shared_ptr<Attribute> attr)
{
    if (attributes == nullptr)
        attributes = shared_ptr<Array<Attribute> >(new Array<Attribute>());
    attributes->append(attr);
}

void Element::setAttribute(String name, String value, enum mxml_value_type type)
{
    if (attributes == nullptr)
        attributes = shared_ptr<Array<Attribute> >(new Array<Attribute>());
    int len = attributes->size();
    for(int i = 0; i < len; i++)
    {
        shared_ptr<Attribute> attr = attributes->get(i);
        if(attr->name == name)
        {
            attr->setValue(value);
            attr->setVType(type);
            return;
        }
    }
    addAttribute(name, value, type);
}

int Element::childCount(enum mxml_node_types type)
{
    if (children == nullptr)
        return 0;
    
    if (type == mxml_node_all)
        return children->size();
    
    int countElements = 0;
    for(int i = 0; i < children->size(); i++)
    {
        shared_ptr<Node> nd = children->get(i);
        if (nd->getType() == type)
        {
            countElements++;
        }
    }
    return countElements;
}

shared_ptr<Node> Element::getChild(int index, enum mxml_node_types type, bool remove)
{
    if (children == nullptr)
        return nullptr;
    int countElements = 0;
    
    if (type == mxml_node_all)
    {
        if (index >= children->size())
            return nullptr;
        else
        {
            shared_ptr<Node> node = children->get(index);
            if (remove)
                children->remove(index);
            return node;
        }
    }
    
    for(int i = 0; i < children->size(); i++)
    {
        shared_ptr<Node> nd = children->get(i);
        if (nd->getType() == type)
        {
            if (countElements++ == index)
            {
                if (remove)
                    children->remove(i);
                return nd;
            }
        }
    }
    return nullptr;
}

bool Element::removeElementChild(String name, bool removeAll)
{
    int id = getChildIdByName(name);
    if (id < 0)
        return false;
    shared_ptr<Node> child = getChild(id, mxml_node_all, true);
    if (child == nullptr)
        return false;
    if (! removeAll)
        return true;
    removeElementChild(name, true);
    return true;
}

void Element::appendChild(shared_ptr<Node> child)
{
    if(children == nullptr)
        children = shared_ptr<Array<Node> >(new Array<Node>());
    children->append(child);
}

void Element::insertChild(int index, shared_ptr<Node> child)
{
    if (children == nullptr)
        children = shared_ptr<Array<Node> >(new Array<Node>());
    children->insert(index, child);
}

void Element::removeChild(int index, enum mxml_node_types type)
{
    if (type == mxml_node_all)
        children->remove(index);
    else
    {
        getChild(index, type, true);
    }
}

void Element::removeWhitespace()
{
    int numChildren = childCount();
    for (int i = 0; i < numChildren; i++)
    {
        shared_ptr<Node> node = getChild(i);
        if (node->getType() == mxml_node_text)
        {
            shared_ptr<Text> text = dynamic_pointer_cast<Text>(node);
            String trimmed = trim_string(text->getText());
            if (string_ok(trimmed))
            {
                text->setText(trimmed);
            }
            else
            {
                if (numChildren != 1)
                {
                    removeChild(i--);
                    --numChildren;
                }
            }
        }
        else if (node->getType() == mxml_node_element)
        {
            shared_ptr<Element> el = dynamic_pointer_cast<Element>(node);
            el->removeWhitespace();
        }
    }
}

void Element::indent(int level)
{
    assert(level >= 0);
    
    removeWhitespace();
    
    int numChildren = childCount();
    if (! numChildren)
        return;
    
    bool noTextChildren = true;
    for (int i = 0; i < numChildren; i++)
    {
        shared_ptr<Node> node = getChild(i);
        if (node->getType() == mxml_node_element)
        {
            shared_ptr<Element> el = dynamic_pointer_cast<Element>(node);
            el->indent(level+1);
        }
        else if (node->getType() == mxml_node_text)
        {
            noTextChildren = false;
        }
    }
    
    if (noTextChildren)
    {
        static const char *ind_str = "                                                               ";
        static const char *ind = ind_str + strlen(ind_str);
        const char *ptr = ind - (level + 1) * 2;
        if (ptr < ind_str)
            ptr = ind_str;
        
        for (int i = 0; i < numChildren; i++)
        {
            bool newlineBefore = true;
            if (getChild(i)->getType() == mxml_node_comment)
            {
                shared_ptr<Comment> comment = RefCast(getChild(i), Comment);
                newlineBefore = comment->getIndentWithLFbefore();
            }
            if (newlineBefore)
            {
                shared_ptr<Text> indentText(new Text(_("\n")+ptr));
                insertChild(i++,dynamic_pointer_cast<Node>(indentText));
                numChildren++;
            }
        }
        
        ptr += 2;
        
        shared_ptr<Text> indentTextAfter(new Text(_("\n")+ptr));
        appendChild(dynamic_pointer_cast<Node>(indentTextAfter));
    }
}

String Element::getText()
{
    shared_ptr<StringBuffer> buf(new StringBuffer());
    shared_ptr<Text> text;
    int i = 0;
    bool someText = false;
    while ((text = RefCast(getChild(i++, mxml_node_text), Text)) != nullptr)
    {
        someText = true;
        *buf << text->getText();
    }
    if (someText)
        return buf->toString();
    else
        return nullptr;
}

enum mxml_value_type Element::getVTypeText()
{
    shared_ptr<Text> text;
    int i = 0;
    bool someText = false;
    enum mxml_value_type vtype = mxml_string_type;
    while ((text = RefCast(getChild(i++, mxml_node_text), Text)) != nullptr)
    {
        if (! someText)
        {
            someText = true;
            vtype = text->getVType();
        }
        else
        {
            if (vtype != text->getVType())
                vtype = mxml_string_type;
        }
    }
    return vtype;
}

int Element::attributeCount()
{
    if (attributes == nullptr)
        return 0;
    return attributes->size();
}

shared_ptr<Attribute> Element::getAttribute(int index)
{
    if (attributes == nullptr)
        return nullptr;
    if (index >= attributes->size())
        return nullptr;
    return attributes->get(index);
}

void Element::setText(String str, enum mxml_value_type type)
{
    if (childCount() > 1)
        throw _Exception(_("Element::setText() cannot be called on an element which has more than one child"));
    
    if (childCount() == 1)
    {
        shared_ptr<Node> child = getChild(0);
        if (child == nullptr || child->getType() != mxml_node_text)
            throw _Exception(_("Element::setText() cannot be called on an element which has a non-text child"));
        shared_ptr<Text> text = dynamic_pointer_cast<Text>(child);
        text->setText(str);
    }
    else
    {
        shared_ptr<Text> text(new Text(str, type));
        appendChild(dynamic_pointer_cast<Node>(text));
    }
}

void Element::appendTextChild(String name, String text, enum mxml_value_type type)
{
    shared_ptr<Element> el = shared_ptr<Element>(new Element(name));
    el->setText(text, type);
    appendElementChild(el);
}



int Element::getChildIdByName(String name)
{
    if(children == nullptr)
        return -1;
    for(int i = 0; i < children->size(); i++)
    {
        shared_ptr<Node> nd = children->get(i);
        if (nd->getType() == mxml_node_element)
        {
            shared_ptr<Element> el = dynamic_pointer_cast<Element>(nd);
            if (name == nullptr || el->name == name)
                return i;
        }
    }
    return -1;
}

shared_ptr<Element> Element::getChildByName(String name)
{
    int id = getChildIdByName(name);
    if (id < 0)
        return nullptr;
    return RefCast(getChild(id), Element);
}

String Element::getChildText(String name)
{
    shared_ptr<Element> el = getChildByName(name);
    if(el == nullptr)
        return nullptr;
    return el->getText();
}

void Element::print_internal(shared_ptr<StringBuffer> buf, int indent)
{
    /*
    static char *ind_str = "                                                               ";
    static char *ind = ind_str + strlen(ind_str);
    char *ptr = ind - indent * 2;
    *buf << ptr;
    */
    
    int i;
    
    *buf << "<" << name;
    if (attributes != nullptr)
    {
        for(i = 0; i < attributes->size(); i++)
        {
            *buf << ' ';
            shared_ptr<Attribute> attr = attributes->get(i);
            *buf << attr->name << "=\"" << escape(attr->value) << '"';
        }
    }
    
    if (children != nullptr && children->size())
    {
        *buf << ">";
        
        for(i = 0; i < children->size(); i++)
        {
            children->get(i)->print_internal(buf, indent + 1);
        }
        
        *buf << "</" << name << ">";
    }
    else
    {
        *buf << "/>";
    }
}
