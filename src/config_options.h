/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    config_options.h - this file is part of MediaTomb.
    
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

/// \file config_options.h
///\brief Definitions of the ConfigManager class. 

#ifndef __CONFIG_OPTIONS_H__
#define __CONFIG_OPTIONS_H__

#include <assert.h>
#include "zmm/zmmf.h"
#include "exceptions.h"
#include "autoscan.h"
#include "object_dictionary.h"

#ifdef EXTERNAL_TRANSCODING
    #include "transcoding/transcoding.h"
#endif

class ConfigOption : public zmm::Object
{
public:
    virtual zmm::String getOption()
    {
        throw _Exception(_("Wrong option type"));
    };

    virtual int getIntOption()
    {
        throw _Exception(_("Wrong option type"));
    };

    virtual bool getBoolOption()
    {
        throw _Exception(_("Wrong option type"));
    };


    virtual std::shared_ptr<Dictionary> getDictionaryOption()
    {
        throw _Exception(_("Wrong option type"));
    };

    virtual std::shared_ptr<AutoscanList> getAutoscanListOption()
    {
        throw _Exception(_("Wrong option type"));
    };

    virtual std::shared_ptr<zmm::Array<zmm::StringBase> > getStringArrayOption()
    {
        throw _Exception(_("Wrong option type"));
    };
#ifdef EXTERNAL_TRANSCODING
    virtual std::shared_ptr<TranscodingProfileList> getTranscodingProfileListOption()
    {
        throw _Exception(_("Wrong option type"));
    };
#endif
#ifdef ONLINE_SERVICES
    virtual std::shared_ptr<zmm::Array<zmm::Object> > getObjectArrayOption()
    {
        throw _Exception(_("Wrong option type"));
    }
#endif
    virtual std::shared_ptr<ObjectDictionary<zmm::Object> > getObjectDictionaryOption()
    {
        throw _Exception(_("Wrong option type"));
    }
};

class Option : public ConfigOption
{
public:
    Option(zmm::String option) { this->option = option; };

    virtual zmm::String getOption() { return option; };

protected:
    zmm::String option;
};

class IntOption : public ConfigOption
{
public:
    IntOption(int option) { this->option = option; };

    virtual int getIntOption() { return option; };

protected:
    int option;
};

class BoolOption : public ConfigOption
{
public:
    BoolOption(bool option) { this->option = option; };

    virtual bool getBoolOption() { return option; };

protected:
    BoolOption() {};
    bool option;
};


class DictionaryOption : public ConfigOption
{
public:
    DictionaryOption(std::shared_ptr<Dictionary> option) { this->option = option; };

    virtual std::shared_ptr<Dictionary> getDictionaryOption() { return option; };

protected:
    std::shared_ptr<Dictionary> option;
};

class StringArrayOption : public ConfigOption
{
public:
    StringArrayOption(std::shared_ptr<zmm::Array<zmm::StringBase> > option) 
    { 
        this->option = option; 
    };

    virtual std::shared_ptr<zmm::Array<zmm::StringBase> > getStringArrayOption() 
    { 
        return option; 
    };

protected:
    std::shared_ptr<zmm::Array<zmm::StringBase> > option;
};

class AutoscanListOption : public ConfigOption
{
public:
    AutoscanListOption(std::shared_ptr<AutoscanList> option) 
    { 
        this->option = option; 
    };

    virtual std::shared_ptr<AutoscanList> getAutoscanListOption() { return option; };

protected:
    std::shared_ptr<AutoscanList> option;
};

#ifdef EXTERNAL_TRANSCODING
class TranscodingProfileListOption : public ConfigOption
{
public:
    TranscodingProfileListOption(std::shared_ptr<TranscodingProfileList> option)
    {
        this->option = option;
    };

    virtual std::shared_ptr<TranscodingProfileList> getTranscodingProfileListOption()
    {
        return option;
    };
protected:
    std::shared_ptr<TranscodingProfileList> option;
};
#endif//EXTERNAL_TRANSCODING

#ifdef ONLINE_SERVICES
class ObjectArrayOption : public ConfigOption
{
public:
    ObjectArrayOption(std::shared_ptr<zmm::Array<zmm::Object> > option) 
    { 
        this->option = option; 
    };

    virtual std::shared_ptr<zmm::Array<zmm::Object> > getObjectArrayOption() 
    { 
        return option; 
    };

protected:
    std::shared_ptr<zmm::Array<zmm::Object> > option;
};

#endif//ONLINE_SERVICES

class ObjectDictionaryOption : public ConfigOption
{
public:
    ObjectDictionaryOption(std::shared_ptr<ObjectDictionary<zmm::Object> > option)
    {
        this->option = option;
    };

    virtual std::shared_ptr<ObjectDictionary<zmm::Object> > getObjectDictionaryOption()
    {
        return option;
    };
protected:
    std::shared_ptr<ObjectDictionary<zmm::Object> > option;
};

#endif // __CONFIG_MANAGER_H__
