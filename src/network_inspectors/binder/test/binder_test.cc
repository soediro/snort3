//--------------------------------------------------------------------------
// Copyright (C) 2015-2016 Cisco and/or its affiliates. All rights reserved.
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

// binder_test.cc author Ed Borgoyn <eborgoyn@cisco.com>
// unit test main

#include "network_inspectors/binder/binder.cc"
#include "network_inspectors/binder/bind_module.h"

#include <thread>
#include <vector>

#include "flow/flow.h"
#include "framework/inspector.h"
#include "managers/inspector_manager.h"
#include "main/policy.h"
#include "profiler/profiler.h"
#include "stream/stream_splitter.h"
#include "utils/stats.h"

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>

THREAD_LOCAL SnortConfig* snort_conf = nullptr;
THREAD_LOCAL BindStats bstats;
static const PegInfo bind_pegs[] = { { nullptr, nullptr } };

static std::vector<Binding*> s_bindings;

static sfip_t s_src_ip, s_dst_ip;
static Inspector* s_inspector;

void show_stats(PegCount*, const PegInfo*, unsigned, const char*) { }
void show_stats(PegCount*, const PegInfo*, IndexVec&, const char*) { }
void show_stats(PegCount*, const PegInfo*, IndexVec&, const char*, FILE*) { }

void sfvar_free(sfip_var_t*) {}
int sfvar_ip_in(sfip_var_t*, const sfip_t*) { return 0; }
SO_PUBLIC Inspector* InspectorManager::get_inspector(const char*, bool) { return s_inspector; }
InspectorType InspectorManager::get_type(const char*) { return InspectorType::IT_BINDER; }
Inspector* InspectorManager::get_binder() { return nullptr; }

BinderModule::BinderModule() : Module("B", "B Help", nullptr, true) { }
BinderModule::~BinderModule() { }
ProfileStats* BinderModule::get_profile() const { return nullptr; }
bool BinderModule::set( const char*, Value&, SnortConfig* ) { return true; }
bool BinderModule::begin( const char*, int, SnortConfig* ) { return true; }
bool BinderModule::end( const char*, int, SnortConfig* ) { return true; }

std::vector<Binding*>& BinderModule::get_data() { return s_bindings; }
const PegInfo* BinderModule::get_pegs() const { return bind_pegs; }
PegCount* BinderModule::get_counts() const { return (PegCount*)&bstats; }

Inspector::Inspector()
{
    ref_count = new unsigned[1];
    ref_count[0] = 0;
}
Inspector::~Inspector()
{
    delete[] ref_count;
}

class MyInspector : public Inspector
{
    void eval(Packet*) { }
};

bool Inspector::get_buf(const char*, Packet*, InspectionBuffer&) { return false; }
StreamSplitter* Inspector::get_splitter(bool) { return nullptr; }
bool Inspector::likes(Packet*) { return false; }
unsigned THREAD_LOCAL Inspector::slot = 0;

void ParseError(const char*, ...) { }
void LogMessage(const char*,...) { }

void Stream::set_application_protocol_id(Flow*, HostAttributeEntry const*, int) { }
void Stream::set_splitter(Flow*, bool, class StreamSplitter*) { }
const char* get_protocol_name(uint16_t) { return ""; }
int16_t FindProtocolReference(const char*) { return 0; }
void set_policies(SnortConfig*, unsigned) { }
HostAttributeEntry* SFAT_LookupHostEntryByIP(const sfip_t*) { return nullptr; }

Flow::Flow() { }
Flow::~Flow() { }

extern const BaseApi* nin_binder;

TEST_GROUP(binder)
{
    void setup()
    {
    }
    void teardown()
    {
    }
};

TEST(binder, exec)
{
    uint8_t* conf = new uint8_t[sizeof(SnortConfig)];
    memset(conf,0,sizeof(SnortConfig));
    snort_conf = (SnortConfig*)conf;
    Flow* flow = new Flow;
    constexpr size_t offset = offsetof(Flow, flow_data);
    memset((uint8_t*)flow+offset, 0, sizeof(Flow)-offset);

    s_inspector = new MyInspector();

    flow->pkt_type = PktType::UDP;
    flow->key = nullptr;
    flow->session = nullptr;
    flow->clouseau = nullptr;
    flow->gadget = nullptr;
    flow->ssn_client = nullptr;
    flow->ssn_server = nullptr;
    flow->key = new FlowKey;
    ((FlowKey*)flow->key)->init(PktType::UDP, IpProtocol::UDP, &s_src_ip, (uint16_t)10, &s_dst_ip, (uint16_t)11, 0, 0, 0);
    InspectApi* api = (InspectApi*)nin_binder;
    CHECK(api != nullptr );
    BinderModule* m = (BinderModule*)(api->base.mod_ctor());
    CHECK(m != nullptr );
    Binding* binding = new Binding;
    binding->when.protos = (unsigned)PktType::UDP;
    binding->use.type = "wizard";
    binding->use.name = "wizard";
    s_bindings.push_back(binding);
    Binder* b = (Binder*)api->ctor(m);
    CHECK(b != nullptr );
    b->configure(snort_conf);
    b->exec(BinderSpace::ExecOperation::EVAL_STANDBY_FLOW,(Flow*)flow);
    CHECK(flow->ssn_client != nullptr);
    CHECK(flow->ssn_server != nullptr);
    api->dtor(b);
    api->base.mod_dtor(m);
    delete flow->key;
    delete flow;
    delete s_inspector;
    delete[] conf;
}

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

