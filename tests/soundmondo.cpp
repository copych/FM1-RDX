#include "engine_api.h"
#include "../src/rdx_engine.h"
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
static uint32_t now,session=1,room=4096;
static uint8_t preset_home_requested;
static std::vector<std::vector<uint8_t>> sent;
static uint32_t plat_lock(){return 0;}static void plat_unlock(uint32_t){}
static uint32_t plat_ms(){return now;}static uint32_t plat_usb_session(){return session;}
static uint32_t plat_sysex_room(){return room;}
static int plat_sysex_send(const uint8_t* p,uint32_t n){sent.emplace_back(p,p+n);return 0;}
#include "../app/soundmondo.h"
static void drain(){for(int i=0;i<16;++i)app_loop();}
static void run(){sm_audio();float l[128],r[128];for(int i=0;i<50;++i)rdx_render(l,r);}
static void receive(const std::vector<uint8_t>& bytes){
    for(size_t i=0;i<bytes.size();){size_t e=i;while(e<bytes.size()&&bytes[e]!=0xf7)++e;
        CHECK(e<bytes.size()&&bytes[i]==0xf0);CHECK(app_sysex(bytes.data()+i+1,e-i-1));i=e+1;
    }
}
int main(){
    rdx_boot();sm_reset();
    const uint8_t identity[]={0x7e,0x7f,6,1};CHECK(app_sysex(identity,4));drain();
    CHECK(sent.size()==1&&sent[0]==std::vector<uint8_t>({0xf0,0x7e,0x7f,6,2,0x43,0,0x41,0x53,6,3,0,0,0x7f,0xf7}));
    std::vector<std::filesystem::path> paths;for(auto& e:std::filesystem::directory_iterator("RDX_before_UAC/data/patches"))if(e.path().extension()==".syx")paths.push_back(e.path());
    std::sort(paths.begin(),paths.end());CHECK(paths.size()==33);
    for(unsigned index=0;index<paths.size();++index){
        std::ifstream f(paths[index],std::ios::binary);std::vector<uint8_t> wire((std::istreambuf_iterator<char>(f)),{});
        receive(wire);CHECK(sm_patch_pending);run();
        uint8_t actual[150];rdx_patch_export(actual);RDX_Patch expected;CHECK(RdxEngine::presetPatch(index,expected));CHECK(!memcmp(actual,&expected,150));
        sent.clear();const uint8_t request[]={0x43,0x20,0x7f,0x1c,5,0x0e,0x0f,0};CHECK(app_sysex(request,8));drain();CHECK(sent.size()==7);
        std::vector<uint8_t> dump;for(auto& frame:sent){unsigned sum=0;CHECK(frame.size()==((frame[5]<<7)|frame[6])+9u);for(size_t k=7;k+1<frame.size();++k)sum+=frame[k];CHECK(!(sum&127));dump.insert(dump.end(),frame.begin(),frame.end());}
        CHECK(dump==wire); // byte-exact round-trip of authentic source fixtures
    }
    uint8_t previous[150];rdx_patch_export(previous);
    const uint8_t change[]={0x43,0x10,0x7f,0x1c,5,0x30,0,16,2};CHECK(app_sysex(change,9));
    uint8_t actual[150];rdx_patch_export(actual);CHECK(!memcmp(actual,previous,150)); // audio owner has not applied it yet
    run();rdx_patch_export(actual);CHECK(actual[16]==2);
    const uint8_t invalid[]={0x43,0x10,0x7f,0x1c,5,0x30,0,16,127};app_sysex(invalid,9);CHECK(!sm_patch_pending);
    // Truncation, wrong model and every single-byte corruption of a real frame.
    const auto valid=sent[1];
    for(size_t cut=0;cut<valid.size()-2;++cut){app_sysex(valid.data()+1,cut);CHECK(!sm_patch_pending);}
    for(size_t at=1;at+1<valid.size();++at){if(at==2)continue;auto corrupt=valid;corrupt[at]^=1;app_sysex(corrupt.data()+1,corrupt.size()-2);CHECK(!sm_patch_pending);}
    // Whole transaction must reject a corrupt block, including otherwise valid later blocks.
    for(size_t k=0;k<sent.size();++k){auto frame=sent[k];if(k==2)frame[12]^=1;app_sysex(frame.data()+1,frame.size()-2);}CHECK(!sm_patch_pending);
    app_sysex(sent[0].data()+1,sent[0].size()-2);now+=2001;app_loop();CHECK(!sm_bulk_open);
    // No ring space must not block or discard the queued identity reply.
    sent.clear();room=0;app_sysex(identity,4);app_loop();CHECK(sent.empty());room=4096;drain();CHECK(sent.size()==1);
    app_sysex(identity,4);++session;app_loop();CHECK(sm_tx_read==sm_tx_write);
    std::puts("PASS: Soundmondo identity, 33 exact fixture round-trips, owner handoff, malformed/checksum/transaction/timeout/session/backpressure checks");
}
