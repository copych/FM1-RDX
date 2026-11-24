#include <cstring>
#include <cassert>
#include <cstdio>
#include "../src/rdx_looper.h"
#include "../src/rdx_drafts.h"
static uint8_t objects[28][3840],buf[3840],header[192],patch[150],perf[3]={0,32,17};
static int sizes[28],writes,fail_at;
static int load(uint32_t o,void* p,uint32_t n){if(o>=28||sizes[o]<=0)return -1;unsigned take=(unsigned)sizes[o];if(take>n)take=n;memcpy(p,objects[o],take);return (int)take;}
static int save(uint32_t o,const void* p,uint32_t n){++writes;if(writes==fail_at||o>=28||n>3840)return -1;memcpy(objects[o],p,n);sizes[o]=(int)n;return 0;}
static int size(uint32_t o){return o<28?sizes[o]:-1;}
static int valid(const uint8_t* p){for(unsigned i=0;i<150;++i)if(p[i]>127)return 0;return 1;}
static DraftIO io={0,load,save,valid,size};
static RdxLooper l,restored;
static void rehash(unsigned slot){auto* m=objects[slot*7];uint32_t hash=2166136261u;for(unsigned c=0;c<3;++c){auto* p=objects[slot*7+1+m[6]*3+c];hash=draft_hash(hash,p+16,draft_u32(p+12));}draft_w32(m+12,hash);}
int main(){
 loop_init(&l);l.tracks=4;l.length=57600;l.tempo=137;l.quantize=LOOP_QUANT_16T;
 for(unsigned t=0;t<4;++t){l.count[t]=512;for(unsigned i=0;i<512;++i)l.events[t][i]={(uint16_t)(i*96),LOOP_NOTE,(uint8_t)(50+t),(uint8_t)((i&1)?0:100),0};}
 patch[0]='A';assert(draft_save(&io,3,&l,patch,perf,buf));assert(writes==4);
 assert(draft_load(&io,3,&restored,header,buf));assert(restored.state==LOOP_STOP&&restored.tempo==137&&restored.length==57600&&restored.tracks==4&&restored.quantize==LOOP_QUANT_16T);
 assert(!memcmp(restored.events,l.events,sizeof l.events)&&!memcmp(header+32,patch,150)&&!memcmp(header+20,perf,3));
 // Every failure boundary during replacement preserves the committed generation.
 patch[0]='B';l.tempo=201;l.quantize=LOOP_QUANT_8T;
 for(unsigned fault=1;fault<=4;++fault){fail_at=writes+(int)fault;assert(!draft_save(&io,3,&l,patch,perf,buf));fail_at=0;assert(draft_load(&io,3,&restored,header,buf));assert(header[32]=='A'&&restored.tempo==137&&restored.quantize==LOOP_QUANT_16T);}
 assert(draft_save(&io,3,&l,patch,perf,buf));assert(draft_load(&io,3,&restored,header,buf)&&header[32]=='B'&&restored.tempo==201&&restored.quantize==LOOP_QUANT_8T);
 // Separate slot, exact object lengths, corrupt chunk, generation mismatch.
 assert(draft_save(&io,0,&l,patch,perf,buf));unsigned obj=1;uint8_t old=objects[obj][30];objects[obj][30]^=1;
 restored.tempo=77;assert(!draft_load(&io,0,&restored,header,buf)&&restored.tempo==77);objects[obj][30]=old;
 ++sizes[obj];assert(!draft_load(&io,0,&restored,header,buf));--sizes[obj];objects[obj][8]^=1;assert(!draft_load(&io,0,&restored,header,buf));objects[obj][8]^=1;
 assert(draft_load(&io,0,&restored,header,buf));
 objects[0][12]^=1;assert(!draft_load(&io,0,&restored,header,buf));objects[0][12]^=1;
 for(unsigned slot=1;slot<3;++slot){assert(draft_save(&io,slot,&l,patch,perf,buf));assert(draft_load(&io,slot,&restored,header,buf));}
 int before=writes;l.tracks=9;assert(!draft_save(&io,0,&l,patch,perf,buf)&&writes==before);l.tracks=4;
 l.events[0][3].type=LOOP_EVENT_TYPES_COUNT;assert(!draft_save(&io,0,&l,patch,perf,buf)&&writes==before);l.events[0][3].type=LOOP_NOTE;
 patch[12]=255;assert(!draft_save(&io,0,&l,patch,perf,buf)&&writes==before);patch[12]=0;
 assert(!draft_load(&io,4,&restored,header,buf));
 // Every straight/triplet choice survives a draft round trip.
 for(unsigned q=0;q<LOOP_QUANT_MODES_COUNT;++q){l.quantize=(uint8_t)q;assert(draft_save(&io,0,&l,patch,perf,buf));assert(draft_load(&io,0,&restored,header,buf)&&restored.quantize==q);}
 l.quantize=LOOP_QUANT_MODES_COUNT;before=writes;assert(!draft_save(&io,0,&l,patch,perf,buf)&&writes==before);
 l.quantize=LOOP_QUANT_OFF;assert(draft_save(&io,0,&l,patch,perf,buf));
 auto* payload=objects[1+objects[0][6]*3]+16;
 // Genuine v1 header fixture has no quantization; new reader restores OFF.
 payload[4]=DRAFT_LEGACY;payload[23]=0;rehash(0);restored.quantize=LOOP_QUANT_32T;assert(draft_load(&io,0,&restored,header,buf)&&restored.quantize==LOOP_QUANT_OFF);
 restored.quantize=LOOP_QUANT_8T;payload[23]=1;rehash(0);assert(!draft_load(&io,0,&restored,header,buf)&&restored.quantize==LOOP_QUANT_8T);
 payload[4]=DRAFT_QUANTIZED;payload[23]=255;rehash(0);assert(!draft_load(&io,0,&restored,header,buf)&&restored.quantize==LOOP_QUANT_8T);
 payload[4]=DRAFT_VERSIONS_COUNT;payload[23]=0;rehash(0);assert(!draft_load(&io,0,&restored,header,buf)&&restored.quantize==LOOP_QUANT_8T);
 puts("PASS all nine saved quantization modes, legacy v1 OFF migration, invalid IDs/versions and transactional quantization preservation");
 puts("PASS full-capacity draft roundtrip, four slots, commit-last failures, length/corruption/generation rejection, unchanged current phrase on validation failure and invalid-save bounds");
}
