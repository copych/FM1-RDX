#include <cstring>
#include <cassert>
#include <cstdio>
#include "../src/rdx_looper.h"
static unsigned notes,resets;
static void emit(uint8_t t,uint8_t,uint8_t){if(t==LOOP_RELEASE)++resets;else ++notes;}
static void advance(RdxLooper& l,unsigned blocks){while(blocks--)loop_advance(&l,emit);}
int main(){
 RdxLooper l;loop_init(&l);assert(l.tempo==120&&l.state==LOOP_STOP);
 loop_command(&l,LOOP_REC,0);advance(l,100);assert(l.state==LOOP_ARMED&&l.position==0);
 loop_capture(&l,LOOP_NOTE,60,100);assert(l.state==LOOP_RECORD&&l.count[0]==1);
 advance(l,100);loop_capture(&l,LOOP_NOTE,60,0);loop_command(&l,LOOP_REC,0);
 assert(l.state==LOOP_PLAY&&l.length==96&&l.tracks==1);
 advance(l,400);assert(notes>=4&&resets>=1);
 loop_command(&l,LOOP_REC,0);assert(l.state==LOOP_OVERDUB_ARMED);
 while(l.state==LOOP_OVERDUB_ARMED)advance(l,1);
 loop_capture(&l,LOOP_NOTE,64,100);advance(l,10);loop_capture(&l,LOOP_NOTE,64,0);
 while(l.state==LOOP_OVERDUB)advance(l,1);
 assert(l.tracks==2&&l.count[1]==2);
 loop_command(&l,LOOP_TOGGLE,0);assert(l.state==LOOP_STOP&&l.position==0);
 loop_command(&l,LOOP_TOGGLE,0);assert(l.state==LOOP_PLAY);
 loop_command(&l,LOOP_CLEAR,0);assert(l.tracks==2); // clear only stopped
 loop_command(&l,LOOP_TOGGLE,0);loop_command(&l,LOOP_CLEAR,0);assert(!l.length&&!l.count[0]);
 loop_command(&l,LOOP_TEMPO,999);assert(l.tempo==300);
 loop_command(&l,LOOP_REC,0);loop_capture(&l,LOOP_NOTE,60,100);advance(l,1000);
 assert(l.position==1393&&l.fraction==522000); // exact integer musical clock
 for(unsigned i=0;i<600;++i)loop_capture(&l,LOOP_NOTE,60,100);
 assert(l.count[0]==LOOP_EVENTS&&l.full);
 loop_command(&l,LOOP_TOGGLE,0);loop_command(&l,LOOP_REC,0);assert(l.count[0]==0&&!l.full);
 loop_capture(&l,LOOP_NOTE,60,100);loop_command(&l,LOOP_REC,0);notes=0;resets=0;
 // Dense same-tick data is bounded and stops safely instead of delaying audio.
 l.count[0]=40;for(unsigned i=0;i<40;++i)l.events[0][i]={0,LOOP_NOTE,60,100,0};
 advance(l,1);assert(l.state==LOOP_STOP&&notes==32&&resets==1);
 loop_init(&l);l.state=LOOP_RECORD;l.position=96;loop_command(&l,LOOP_REC,0);assert(l.length==96);
 puts("PASS looper key-on arm, recording, playback, overdub, clear, tempo clock, capacity and bounded dispatch");
}
