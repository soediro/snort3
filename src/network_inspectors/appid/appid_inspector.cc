//--------------------------------------------------------------------------
// Copyright (C) 2016-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// appid_inspector.cc author davis mcpherson <davmcphe@cisco.com>
// Created on: May 10, 2016

#include "appid_inspector.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "profiler/profiler.h"
#include "appid_session.h"
#include "fw_appid.h"

//-------------------------------------------------------------------------
// class stuff
//-------------------------------------------------------------------------

AppIdInspector::AppIdInspector(const AppIdModuleConfig* pc)
{
    assert(pc);
    config = pc;
}

AppIdInspector::~AppIdInspector()
{
    if(config->debug)
        dump_appid_stats();
    delete active_config;
    delete config;
}

bool AppIdInspector::configure(SnortConfig*)
{
    active_config = new AppIdConfig( ( AppIdModuleConfig* )config);
    if(config->debug)
    	show(nullptr);
    return active_config->init_appid();

    // FIXIT some of this stuff may be needed in some fashion...
#ifdef REMOVED_WHILE_NOT_IN_USE
    _dpd.registerGeAppId(getOpenAppId);
    if (!thirdparty_appid_module)
        _dpd.streamAPI->register_http_header_callback(httpHeaderCallback);
    _dpd.registerSslAppIdLookup(sslAppGroupIdLookup);

    // FIXIT AppID will need to register for SIP events for sip detection to work...
    if (_dpd.streamAPI->service_event_subscribe(PP_SIP, SIP_EVENT_TYPE_SIP_DIALOG,
        SipSessionSnortCallback) == false)
        DynamicPreprocessorFatalMessage("failed to subscribe to SIP_DIALOG\n");
#endif
}

void AppIdInspector::show(SnortConfig*)
{
    LogMessage("AppId Configuration\n");

    LogMessage("    Detector Path:          %s\n", config->app_detector_dir);
    LogMessage("    appStats Files:         %s\n", config->app_stats_filename);
    LogMessage("    appStats Period:        %lu secs\n", config->app_stats_period);
    LogMessage("    appStats Rollover Size: %lu bytes\n",
        config->app_stats_rollover_size);
    LogMessage("    appStats Rollover time: %lu secs\n",
        config->app_stats_rollover_time);
    LogMessage("\n");
}

void AppIdInspector::eval(Packet* pkt)
{
    Profile profile(appidPerfStats);

    appid_stats.packets++;
    AppIdSession::do_application_discovery(pkt);
}

//-------------------------------------------------------------------------
// api stuff
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new AppIdModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static void appid_inspector_init()
{
    AppIdSession::init();
}

static Inspector* appid_inspector_ctor(Module* m)
{
    AppIdModule* mod = (AppIdModule*)m;
    return new AppIdInspector(mod->get_data());
}

static void appid_inspector_dtor(Inspector* p)
{
    delete p;
}

const InspectApi appid_inspector_api =
{
    {
        PT_INSPECTOR,
        sizeof(InspectApi),
        INSAPI_VERSION,
        0,
        API_RESERVED,
        API_OPTIONS,
        MOD_NAME,
        MOD_HELP,
        mod_ctor,
        mod_dtor
    },
    IT_NETWORK,
    (uint16_t)PktType::ANY_IP,
    nullptr, // buffers
    nullptr, // service
    appid_inspector_init, // pinit
    nullptr, // pterm
    nullptr, // tinit
    nullptr, // tterm
    appid_inspector_ctor,
    appid_inspector_dtor,
    nullptr, // ssn
    nullptr  // reset
};

#ifdef BUILDING_SO
extern const BaseApi* ips_appid;

SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &appid_inspector_api.base,
    ips_appid,
    nullptr
};
#else
const BaseApi* nin_appid = &appid_inspector_api.base;
#endif

#ifdef REMOVED_WHILE_NOT_IN_USE
// FIXIT-M: This is to be replace with snort3 inspection events
void httpHeaderCallback(Packet* p, HttpParsedHeaders* const headers)
{
    AppIdSession* session;
    int direction;
    AppIdConfig* pConfig = pAppidActiveConfig;

    if (thirdparty_appid_module)
        return;
    if (!p || !(session = appid_api.get_appid_data(p->flow)))
        return;

    direction = p->is_from_client() ? APP_ID_FROM_INITIATOR : APP_ID_FROM_RESPONDER;

    if (!session->hsession)
        session->hsession = (decltype(session->hsession))snort_calloc(sizeof(httpSession));

    if (direction == APP_ID_FROM_INITIATOR)
    {
        if (headers->host.start)
        {
            snort_free(session->hsession->host);
            session->hsession->host = snort_strndup((char*)headers->host.start, headers->host.len);
            session->scan_flags |= SCAN_HTTP_HOST_URL_FLAG;

            if (headers->url.start)
            {
                snort_free(session->hsession->url);
                session->hsession->url = (char*)snort_calloc(sizeof(HTTP_PREFIX) +
                    headers->host.len + headers->url.len);
                strcpy(session->hsession->url, HTTP_PREFIX);
                strncat(session->hsession->url, (char*)headers->host.start, headers->host.len);
                strncat(session->hsession->url, (char*)headers->url.start, headers->url.len);
                session->scan_flags |= SCAN_HTTP_HOST_URL_FLAG;
            }
        }
        if (headers->userAgent.start)
        {
            snort_free(session->hsession->useragent);
            session->hsession->useragent = snort_strndup((char*)headers->userAgent.start,
                headers->userAgent.len);
            session->scan_flags |= SCAN_HTTP_USER_AGENT_FLAG;
        }
        if (headers->referer.start)
        {
            snort_free(session->hsession->referer);
            session->hsession->referer = snort_strndup((char*)headers->referer.start,
                headers->referer.len);
        }
        if (headers->via.start)
        {
            snort_free(session->hsession->via);
            session->hsession->via = snort_strndup((char*)headers->via.start, headers->via.len);
            session->scan_flags |= SCAN_HTTP_VIA_FLAG;
        }
    }
    else
    {
        if (headers->via.start)
        {
            snort_free(session->hsession->via);
            session->hsession->via = snort_strndup((char*)headers->via.start, headers->via.len);
            session->scan_flags |= SCAN_HTTP_VIA_FLAG;
        }
        if (headers->contentType.start)
        {
            snort_free(session->hsession->content_type);
            session->hsession->content_type = snort_strndup((char*)headers->contentType.start,
                headers->contentType.len);
        }
        if (headers->responseCode.start)
        {
            long responseCodeNum;
            responseCodeNum = strtoul((char*)headers->responseCode.start, nullptr, 10);
            if (responseCodeNum > 0 && responseCodeNum < 700)
            {
                snort_free(session->hsession->response_code);
                session->hsession->response_code = snort_strndup((char*)headers->responseCode.start,
                    headers->responseCode.len);
            }
        }
    }

    session->processHTTPPacket(p, direction, headers, pConfig);
    session->setAppIdFlag(APPID_SESSION_SERVICE_DETECTED | APPID_SESSION_HTTP_SESSION);
    p->flow->set_application_ids(session->pick_service_app_id(), session->pick_client_app_id(),
            session->pick_payload_app_id(), session->pick_misc_app_id());
}
#endif

/**
 * @returns 1 if some appid is found, 0 otherwise.
 */
//int sslAppGroupIdLookup(void* ssnptr, const char* serverName, const char* commonName,
//    AppId* serviceAppId, AppId* ClientAppId, AppId* payloadAppId)
int sslAppGroupIdLookup(void*, const char*, const char*, AppId*, AppId*, AppId*)
{
    // FIXIT-M: detemine need and proper location for this code when support for ssl is implemented
#ifdef REMOVED_WHILE_NOT_IN_USE
    AppIdSession* session;
    *serviceAppId = *ClientAppId = *payload_app_id = APP_ID_NONE;

    if (commonName)
    {
        ssl_scan_cname((const uint8_t*)commonName, strlen(commonName), ClientAppId, payload_app_id,
            &pAppidActiveConfig->serviceSslConfig);
    }
    if (serverName)
    {
        ssl_scan_hostname((const uint8_t*)serverName, strlen(serverName), ClientAppId,
            payload_app_id, &pAppidActiveConfig->serviceSslConfig);
    }

    if (ssnptr && (session = appid_api.get_appid_data(ssnptr)))
    {
        *serviceAppId = pick_service_app_id(session);
        if (*ClientAppId == APP_ID_NONE)
        {
            *ClientAppId = pick_client_app_id(session);
        }
        if (*payload_app_id == APP_ID_NONE)
        {
            *payload_app_id = pick_payload_app_id(session);
        }
    }
    if (*serviceAppId != APP_ID_NONE ||
        *ClientAppId != APP_ID_NONE ||
        *payload_app_id != APP_ID_NONE)
    {
        return 1;
    }
#endif

    return 0;
}

AppId getOpenAppId(Flow* flow)
{
    assert(flow);
    AppIdSession* session = appid_api.get_appid_data(flow);
    return session->payload_app_id;
}

