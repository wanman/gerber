/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    server.cc - this file is part of MediaTomb.
    
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

/// \file server.cc

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#ifdef HAVE_LASTFMLIB
#include "lastfm_scrobbler.h"
#endif

#include "content_manager.h"
#include "server.h"
#include "update_manager.h"
#include "web_callbacks.h"

using namespace zmm;
using namespace mxml;

Ref<Storage> Server::storage = nullptr;

#ifdef UPNP_OLD_SNAPSHOT
static int static_upnp_callback(Upnp_EventType eventtype, void* event, void* cookie)
#else
static int static_upnp_callback(Upnp_EventType eventtype, const void* event, void* cookie)
#endif
{
    return static_cast<Server*>(cookie)->upnp_callback(eventtype, event);
}

void Server::static_cleanup_callback()
{
    if (storage != nullptr) {
        try {
            storage->threadCleanup();
        } catch (const Exception& ex) {
        }
    }
}

Server::Server(std::shared_ptr<ConfigManager> configManager)
    : configManager(configManager)
{
    server_shutdown_flag = false;
    virtual_directory = _(SERVER_VIRTUAL_DIR);

    // Create a CDS
    cds = ContentDirectoryService{configManager->getIntOption(CFG_SERVER_UPNP_TITLE_AND_DESC_STRING_LIMIT)};

    // Create Connection Manager
    cmgr = ConnectionManagerService{};

#if defined(ENABLE_MRREG)
    MRRegistrarService::setStaticArgs(_(DESC_MRREG_SERVICE_TYPE),
        _(DESC_MRREG_SERVICE_ID));
    mrreg = MRRegistrarService::getInstance();
#endif

    serverUDN = configManager->getOption(CFG_SERVER_UDN);
    alive_advertisement = configManager->getIntOption(CFG_SERVER_ALIVE_INTERVAL);

#ifdef HAVE_CURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif

#ifdef HAVE_LASTFMLIB
    LastFm::getInstance();
#endif
}

void Server::upnp_init()
{
    int ret = 0; // general purpose error code
    SPDLOG_TRACE(l, "start");

    String iface = configManager->getOption(CFG_SERVER_NETWORK_INTERFACE);
    String ip = configManager->getOption(CFG_SERVER_IP);

    if (string_ok(ip) && string_ok(iface))
        throw _Exception(_("You can not specify interface and IP at the same time!"));

    if (!string_ok(iface))
        iface = ipToInterface(ip);

    if (string_ok(ip) && !string_ok(iface))
        throw _Exception(_("Could not find ip: ") + ip);

    int port = configManager->getIntOption(CFG_SERVER_PORT);

    // this is important, so the storage lives a little longer when
    // shutdown is initiated
    // FIMXE: why?
    storage = Storage::getInstance();

    SPDLOG_TRACE(l, "Initialising libupnp with interface: {}, port: {}", iface.c_str(), port);
    ret = UpnpInit2(iface.c_str(), port);
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpInit failed"));
    }

    port = UpnpGetServerPort();
    l->info("Initialized port: {}", port);

    if (!string_ok(ip)) {
        ip = UpnpGetServerIpAddress();
    }

    l->info("Server bound to: {}", ip.c_str());

    virtual_url = _("http://") + ip + ":" + port + "/" + virtual_directory;

    // next set webroot directory
    String web_root = configManager->getOption(CFG_SERVER_WEBROOT);

    if (!string_ok(web_root)) {
        throw _Exception(_("invalid web server root directory"));
    }

    ret = UpnpSetWebServerRootDir(web_root.c_str());
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpSetWebServerRootDir failed"));
    }

    SPDLOG_TRACE(l, "webroot: {}", web_root.c_str());

    Ref<Array<StringBase>> arr = configManager->getStringArrayOption(CFG_SERVER_CUSTOM_HTTP_HEADERS);

    if (arr != nullptr) {
        String tmp;
        for (int i = 0; i < arr->size(); i++) {
            tmp = arr->get(i);
            if (string_ok(tmp)) {
                l->info("(NOT) Adding HTTP header \"{}\"", tmp.c_str());
                // FIXME upstream upnp
                //ret = UpnpAddCustomHTTPHeader(tmp.c_str());
                //if (ret != UPNP_E_SUCCESS)
                //{
                //    throw _UpnpException(ret, _("upnp_init: UpnpAddCustomHTTPHeader failed"));
                //}
            }
        }
    }

    SPDLOG_TRACE(l, "Setting virtual dir to: {}", virtual_directory.c_str());
    ret = UpnpAddVirtualDir(virtual_directory.c_str());
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpAddVirtualDir failed"));
    }

    ret = register_web_callbacks();
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpSetVirtualDirCallbacks failed"));
    }

    String presentationURL = configManager->getOption(CFG_SERVER_PRESENTATION_URL);
    if (!string_ok(presentationURL)) {
        presentationURL = _("http://") + ip + ":" + port + "/";
    } else {
        String appendto = configManager->getOption(CFG_SERVER_APPEND_PRESENTATION_URL_TO);
        if (appendto == "ip") {
            presentationURL = _("http://") + ip + ":" + presentationURL;
        } else if (appendto == "port") {
            presentationURL = _("http://") + ip + ":" + port + "/" + presentationURL;
        } // else appendto is none and we take the URL as it entered by user
    }

    // register root device with the library
    String device_description = _("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") + UpnpXML_RenderDeviceDescription(presentationURL)->print();

    //SPDLOG_TRACE(l, "Device Description: \n%s\n", device_description.c_str());

    SPDLOG_TRACE(l, "Registering with UPnP...\n");
    ret = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC,
        device_description.c_str(),
        device_description.length() + 1,
        true,
        static_upnp_callback,
        this,
        &device_handle);
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpRegisterRootDevice failed"));
    }

    SPDLOG_TRACE(l, "Sending UPnP Alive advertisment\n");
    ret = UpnpSendAdvertisement(device_handle, alive_advertisement);
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_init: UpnpSendAdvertisement failed"));
    }

    // initializing UpdateManager
    updateManager = UpdateManager{};

    // initializing ContentManager
    contentManager = ContentManager{};

    configManager->writeBookmark(ip, String::from(port));
    l->info("The Web UI can be reached by following this link: http://{}:{}/", ip.c_str(), port);

    SPDLOG_TRACE(l, "end");
}

bool Server::getShutdownStatus()
{
    return server_shutdown_flag;
}

void Server::shutdown()
{
    int ret = 0; // return code

    /*
    ContentManager::getInstance()->shutdown();
    UpdateManager::getInstance()->shutdown();
    Storage::getInstance()->shutdown();
    */

    server_shutdown_flag = true;

    SPDLOG_TRACE(l, "Server shutting down\n");

    ret = UpnpUnRegisterRootDevice(device_handle);
    if (ret != UPNP_E_SUCCESS) {
        throw _UpnpException(ret, _("upnp_cleanup: UpnpUnRegisterRootDevice failed"));
    }

#ifdef HAVE_CURL
    curl_global_cleanup();
#endif

    SPDLOG_TRACE(l, "now calling upnp finish\n");
    UpnpFinish();
    if (storage != nullptr && storage->threadCleanupRequired()) {
        static_cleanup_callback();
    }
    storage = nullptr;
}

String Server::getVirtualURL()
{
    return virtual_url;
}

int Server::upnp_callback(Upnp_EventType eventtype, const void* event)
{
    int ret = UPNP_E_SUCCESS; // general purpose return code

    SPDLOG_TRACE(l, "start");

    // check parameters
    if (event == nullptr) {
        SPDLOG_TRACE(l, "upnp_callback: NULL event structure\n");
        return UPNP_E_BAD_REQUEST;
    }

    //l->info("event is ok\n");
    // get device wide mutex (have to figure out what the hell that is)
    AutoLock lock(mutex);

    // dispatch event based on event type
    switch (eventtype) {

    case UPNP_CONTROL_ACTION_REQUEST:
        // a CP is invoking an action
        //l->info("UPNP_CONTROL_ACTION_REQUEST\n");
        try {
            // https://github.com/mrjimenez/pupnp/blob/master/upnp/sample/common/tv_device.c

            Ref<ActionRequest> request(new ActionRequest((UpnpActionRequest*)event));
            upnp_actions(request);
            request->update();
            // set in update() ((struct Upnp_Action_Request *)event)->ErrCode = ret;
        } catch (const UpnpException& upnp_e) {
            ret = upnp_e.getErrorCode();
            UpnpActionRequest_set_ErrCode((UpnpActionRequest*)event, ret);
        } catch (const Exception& e) {
            l->info("Exception: {}", e.getMessage().c_str());
        }

        break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        // a cp wants a subscription
        //l->info("UPNP_EVENT_SUBSCRIPTION_REQUEST\n");
        try {
            Ref<SubscriptionRequest> request(new SubscriptionRequest((UpnpSubscriptionRequest*)event));
            upnp_subscriptions(request);
        } catch (const UpnpException& upnp_e) {
            l->warn("Subscription exception: {}", upnp_e.getMessage().c_str());
            ret = upnp_e.getErrorCode();
        }

        break;

    default:
        // unhandled event type
        l->warn("unsupported event type: {}", eventtype);
        ret = UPNP_E_BAD_REQUEST;
        break;
    }

    SPDLOG_TRACE(l, "returning {}", ret);
    return ret;
}

UpnpDevice_Handle Server::getDeviceHandle()
{
    return device_handle;
}

zmm::String Server::getIP()
{
    return UpnpGetServerIpAddress();
}

zmm::String Server::getPort()
{
    return String::from(UpnpGetServerPort());
}

void Server::upnp_actions(Ref<ActionRequest> request)
{
    SPDLOG_TRACE(l, "start");

    // make sure the request is for our device
    if (request->getUDN() != serverUDN) {
        // not for us
        throw _UpnpException(UPNP_E_BAD_REQUEST,
            _("upnp_actions: request not for this device"));
    }

    // we need to match the serviceID to one of our services
    if (request->getServiceID() == DESC_CM_SERVICE_ID) {
        // this call is for the lifetime stats service
        // SPDLOG_TRACE(l, "request for connection manager service\n");
        cmgr->process_action_request(request);
    } else if (request->getServiceID() == DESC_CDS_SERVICE_ID) {
        // this call is for the toaster control service
        //SPDLOG_TRACE(l, "upnp_actions: request for content directory service\n");
        cds->process_action_request(request);
    }
#if defined(ENABLE_MRREG)
    else if (request->getServiceID() == DESC_MRREG_SERVICE_ID) {
        mrreg->process_action_request(request);
    }
#endif
    else {
        // cp is asking for a nonexistent service, or for a service
        // that does not support any actions
        throw _UpnpException(UPNP_E_BAD_REQUEST,
            _("Service does not exist or action not supported"));
    }
}

void Server::upnp_subscriptions(Ref<SubscriptionRequest> request)
{
    // make sure that the request is for our device
    if (request->getUDN() != serverUDN) {
        // not for us
        l->debug("upnp_subscriptions: request not for this device: {} vs {}",
            request->getUDN().c_str(), serverUDN.c_str());
        throw _UpnpException(UPNP_E_BAD_REQUEST,
            _("upnp_actions: request not for this device"));
    }

    // we need to match the serviceID to one of our services

    if (request->getServiceID() == DESC_CDS_SERVICE_ID) {
        // this call is for the content directory service
        //SPDLOG_TRACE(l, "upnp_subscriptions: request for content directory service\n");
        cds->process_subscription_request(request);
    } else if (request->getServiceID() == DESC_CM_SERVICE_ID) {
        // this call is for the connection manager service
        //SPDLOG_TRACE(l, "upnp_subscriptions: request for connection manager service\n");
        cmgr->process_subscription_request(request);
    }
#if defined(ENABLE_MRREG)
    else if (request->getServiceID() == DESC_MRREG_SERVICE_ID) {
        mrreg->process_subscription_request(request);
    }
#endif
    else {
        // cp asks for a nonexistent service or for a service that
        // does not support subscriptions
        throw _UpnpException(UPNP_E_BAD_REQUEST,
            _("Service does not exist or subscriptions not supported"));
    }
}
