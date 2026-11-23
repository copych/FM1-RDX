#pragma once
#include "rdx_looper.h"
// Memory primitives are supplied by the platform unity build (host tests use string.h).
// Application-owned objects, appended after the existing settings/panel/voice IDs.
// Three chunks in each of two generations; commit manifest last. Each object
// itself uses the BSP's CRC/A-B pair. An interrupted replacement keeps the
// previously committed generation intact. Main loop only, playback suspended.
enum {DRAFT_SLOTS=4,DRAFT_CHUNKS=3,DRAFT_OBJECTS=7,DRAFT_HEADER=192,DRAFT_CHUNK_DATA=3584,DRAFT_CHUNK_HEADER=16,DRAFT_BLOB=DRAFT_HEADER+LOOP_TRACKS*LOOP_EVENTS*5,DRAFT_MANIFEST=40};
typedef struct {uint32_t base;int (*load)(uint32_t,void*,uint32_t);int (*save)(uint32_t,const void*,uint32_t);int (*valid)(const uint8_t*);int (*size)(uint32_t);} DraftIO;
static uint16_t draft_u16(const uint8_t* p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t draft_u32(const uint8_t* p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void draft_w16(uint8_t* p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void draft_w32(uint8_t* p,uint32_t n){for(uint32_t i=0;i<4u;++i)p[i]=(uint8_t)(n>>(8u*i));}
static uint32_t draft_hash(uint32_t h,const uint8_t* p,uint32_t n){while(n--)h=(h^*p++)*16777619u;return h;}
static uint32_t draft_object(const DraftIO* io,uint32_t slot,uint32_t part){return io->base+slot*DRAFT_OBJECTS+part;}
static int draft_manifest(const DraftIO* io,uint32_t slot,uint8_t* m){
    if(slot>=DRAFT_SLOTS||io->size(draft_object(io,slot,0))!=DRAFT_MANIFEST||io->load(draft_object(io,slot,0),m,DRAFT_MANIFEST)!=DRAFT_MANIFEST)return 0;
    if(memcmp(m,"RDM1",4)||m[4]!=1||m[5]!=slot||m[6]>1||m[7]||!draft_u32(m+8)||draft_u32(m+16)!=DRAFT_BLOB)return 0;
    if(draft_u16(m+30)<30||draft_u16(m+30)>300||!draft_u16(m+32)||draft_u16(m+32)>LOOP_MAX_TICKS)return 0;
    for(uint32_t i=34;i<DRAFT_MANIFEST;++i)if(m[i])return 0;
    return 1;
}
static uint32_t draft_chunk_length(uint32_t chunk){uint32_t n=DRAFT_BLOB-chunk*DRAFT_CHUNK_DATA;return n>DRAFT_CHUNK_DATA?DRAFT_CHUNK_DATA:n;}
static int draft_header_valid(const DraftIO* io,const uint8_t* h){
    if(memcmp(h,"RDLP",4)||h[4]!=1||!h[5]||h[5]>LOOP_TRACKS||draft_u16(h+6)<30||draft_u16(h+6)>300||!draft_u16(h+8)||draft_u16(h+8)>LOOP_MAX_TICKS||draft_u16(h+20)>16383||h[22]>127)return 0;
    for(uint32_t i=0;i<LOOP_TRACKS;++i)if(draft_u16(h+12+2*i)>LOOP_EVENTS||(i>=h[5]&&draft_u16(h+12+2*i)))return 0;
    if(h[10]||h[11])return 0;for(uint32_t i=23;i<32u;++i)if(h[i])return 0;for(uint32_t i=182;i<DRAFT_HEADER;++i)if(h[i])return 0;
    return io->valid(h+32);
}
static uint8_t draft_byte(const RdxLooper* l,const uint8_t* h,uint32_t at){
    if(at<DRAFT_HEADER)return h[at];at-=DRAFT_HEADER;
    uint32_t event=at/5u,part=at%5u,track=event/LOOP_EVENTS,index=event%LOOP_EVENTS;
    if(track>=l->tracks||index>=l->count[track])return 0;
    const LoopEvent* e=&l->events[track][index];
    if(part==0)return (uint8_t)e->tick;if(part==1)return (uint8_t)(e->tick>>8);if(part==2)return e->type;if(part==3)return e->a;return e->b;
}
static int draft_save(const DraftIO* io,uint32_t slot,const RdxLooper* l,const uint8_t* patch,const uint8_t* performance,uint8_t* buf){
    if(slot>=DRAFT_SLOTS||l->state!=LOOP_STOP||!l->length||!l->tracks||l->tracks>LOOP_TRACKS)return 0;
    uint8_t m[DRAFT_MANIFEST],h[DRAFT_HEADER];uint32_t seq=1,bank=0;
    if(draft_manifest(io,slot,m)){seq=draft_u32(m+8);if(seq==0xffffffffu)return 0;++seq;bank=1u-m[6];}
    memset(h,0,sizeof h);memcpy(h,"RDLP",4);h[4]=1;h[5]=l->tracks;draft_w16(h+6,l->tempo);draft_w16(h+8,l->length);
    for(uint32_t t=0;t<l->tracks;++t)draft_w16(h+12+2*t,l->count[t]);memcpy(h+20,performance,3);memcpy(h+32,patch,150);
    if(!draft_header_valid(io,h))return 0;
    for(uint32_t t=0;t<l->tracks;++t){uint16_t before=0;for(uint32_t i=0;i<l->count[t];++i){const LoopEvent* e=&l->events[t][i];
        if(e->tick>=l->length||e->tick<before||e->type>=LOOP_EVENT_TYPES_COUNT||e->a>127||e->b>127)return 0;
        if(e->type==LOOP_CC&&e->a!=1&&e->a!=7&&e->a!=11&&e->a!=64)return 0;before=e->tick;
    }}
    uint32_t hash=2166136261u;
    for(uint32_t c=0;c<DRAFT_CHUNKS;++c){
        uint32_t n=draft_chunk_length(c);memset(buf,0,DRAFT_CHUNK_HEADER);memcpy(buf,"RDC1",4);buf[4]=1;buf[5]=(uint8_t)slot;buf[6]=(uint8_t)bank;buf[7]=(uint8_t)c;draft_w32(buf+8,seq);draft_w32(buf+12,n);
        for(uint32_t i=0;i<n;++i)buf[DRAFT_CHUNK_HEADER+i]=draft_byte(l,h,c*DRAFT_CHUNK_DATA+i);
        hash=draft_hash(hash,buf+DRAFT_CHUNK_HEADER,n);uint32_t own=draft_hash(2166136261u,buf,n+DRAFT_CHUNK_HEADER),obj=draft_object(io,slot,1+bank*DRAFT_CHUNKS+c);
        if(io->save(obj,buf,n+DRAFT_CHUNK_HEADER)||io->size(obj)!=(int)(n+DRAFT_CHUNK_HEADER)||io->load(obj,buf,n+DRAFT_CHUNK_HEADER)!=(int)(n+DRAFT_CHUNK_HEADER)||draft_hash(2166136261u,buf,n+DRAFT_CHUNK_HEADER)!=own)return 0;
    }
    memset(m,0,sizeof m);memcpy(m,"RDM1",4);m[4]=1;m[5]=(uint8_t)slot;m[6]=(uint8_t)bank;draft_w32(m+8,seq);draft_w32(m+12,hash);draft_w32(m+16,DRAFT_BLOB);memcpy(m+20,patch,10);draft_w16(m+30,l->tempo);draft_w16(m+32,l->length);
    uint32_t own=draft_hash(2166136261u,m,sizeof m);
    if(io->save(draft_object(io,slot,0),m,sizeof m)||!draft_manifest(io,slot,buf)||draft_hash(2166136261u,buf,DRAFT_MANIFEST)!=own)return 0;
    return 1;
}
static int draft_read_pass(const DraftIO* io,uint32_t slot,const uint8_t* m,uint8_t* h,uint8_t* buf,RdxLooper* apply){
    uint8_t pending[5];uint16_t previous[LOOP_TRACKS]={0};uint32_t hash=2166136261u;
    for(uint32_t c=0;c<DRAFT_CHUNKS;++c){
        uint32_t n=draft_chunk_length(c),obj=draft_object(io,slot,1+m[6]*DRAFT_CHUNKS+c);
        if(io->size(obj)!=(int)(n+DRAFT_CHUNK_HEADER)||io->load(obj,buf,n+DRAFT_CHUNK_HEADER)!=(int)(n+DRAFT_CHUNK_HEADER)||memcmp(buf,"RDC1",4)||buf[4]!=1||buf[5]!=slot||buf[6]!=m[6]||buf[7]!=c||draft_u32(buf+8)!=draft_u32(m+8)||draft_u32(buf+12)!=n)return 0;
        hash=draft_hash(hash,buf+DRAFT_CHUNK_HEADER,n);
        for(uint32_t i=0;i<n;++i){uint32_t at=c*DRAFT_CHUNK_DATA+i;uint8_t v=buf[DRAFT_CHUNK_HEADER+i];
            if(at<DRAFT_HEADER){h[at]=v;if(at==DRAFT_HEADER-1&&!draft_header_valid(io,h))return 0;continue;}
            uint32_t event=(at-DRAFT_HEADER)/5u,part=(at-DRAFT_HEADER)%5u;pending[part]=v;if(part!=4u)continue;
            uint32_t track=event/LOOP_EVENTS,index=event%LOOP_EVENTS;uint16_t tick=draft_u16(pending);
            if(index>=draft_u16(h+12+2*track)){for(uint32_t k=0;k<5u;++k)if(pending[k])return 0;continue;}
            if(tick>=draft_u16(h+8)||tick<previous[track]||pending[2]>=LOOP_EVENT_TYPES_COUNT||pending[3]>127||pending[4]>127)return 0;
            if(pending[2]==LOOP_CC&&pending[3]!=1&&pending[3]!=7&&pending[3]!=11&&pending[3]!=64)return 0;
            previous[track]=tick;if(apply)apply->events[track][index]=(LoopEvent){tick,pending[2],pending[3],pending[4],0};
        }
    }
    return hash==draft_u32(m+12)&&draft_u16(h+6)==draft_u16(m+30)&&draft_u16(h+8)==draft_u16(m+32)&&!memcmp(h+32,m+20,10);
}
static int draft_load(const DraftIO* io,uint32_t slot,RdxLooper* l,uint8_t* h,uint8_t* buf){
    uint8_t m[DRAFT_MANIFEST];if(!draft_manifest(io,slot,m)||!draft_read_pass(io,slot,m,h,buf,0))return 0;
    loop_init(l);if(!draft_read_pass(io,slot,m,h,buf,l)){loop_init(l);return 0;}
    l->tracks=h[5];l->tempo=draft_u16(h+6);l->length=draft_u16(h+8);for(uint32_t t=0;t<LOOP_TRACKS;++t)l->count[t]=draft_u16(h+12+2*t);
    return 1;
}
