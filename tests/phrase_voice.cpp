#include "../src/rdx_engine.h"
#include <cassert>
#include <cstdio>
#include <cmath>
static float a[128],ar[128],b[128],br[128],bus[8][128];
static void compare(const float* x,const float* y){for(unsigned i=0;i<128;++i)assert(std::fabs(x[i]-y[i])<0.000002f);}
int main(){
 RDX_Patch p=RdxEngine::initPatch(),q;assert(RdxEngine::presetPatch(2,q));
 // Frozen phrase output stays the same after live algorithm, tuning and EG edits.
 RdxEngine ref,test;assert(ref.applyPatch(p)&&ref.capturePhrase(p));assert(test.applyPatch(p)&&test.capturePhrase(p));
 ref.phraseNoteOn(60,100);test.phraseNoteOn(60,100);
 for(unsigned block=0;block<100;++block){
   if(block==20){q.common.monoPoly=0;assert(test.applyPatch(q));}
   if(block==40){q.ops[0].outLevel=0;q.common.transpose=88;q.common.algorithm=0;assert(test.updatePatch(q));}
   if(block==60){RDX_State::getState().controls.pitchbendSemitones=12.0f;RDX_State::getState().controls.modWheelFactor=1.0f;}
   ref.renderBlock(a,ar);test.renderBlock(b,br);compare(a,b);
 }
 // Same-pitch live/phrase notes and sustain remain independently releasable.
 test.panic();test.applyPatch(p);test.capturePhrase(p);test.setVoiceLimit(7);
 test.noteOn(60,100);test.phraseNoteOn(60,100);assert(test.activeVoices()==2);
 test.noteOff(60);test.renderBlock(b,br);assert(test.activeVoices()>=1);
 test.phraseSustain(true);test.phraseNoteOff(60);test.sustain(false);test.renderBlock(b,br);
 for(unsigned i=0;i<200;++i)test.renderBlock(b,br);assert(test.activeVoices()==1);
 test.phraseSustain(false);for(unsigned i=0;i<200;++i)test.renderBlock(b,br);assert(!test.activeVoices());test.panic();
 // Overlapping physical keys can map to the same pitch after an octave shift.
 test.noteOn(60,100);test.noteOn(60,100);test.noteOff(60);
 for(unsigned i=0;i<200;++i)test.renderBlock(b,br);assert(test.activeVoices()==1);
 test.noteOff(60);for(unsigned i=0;i<200;++i)test.renderBlock(b,br);assert(!test.activeVoices());
 // One pool: many notes in both parts never allocate beyond the common limit.
 for(unsigned n=0;n<20;++n){test.noteOn(45+n,100);test.phraseNoteOn(70+n,100);assert(test.activeVoices()<=7);}
 test.panic();
 // Mixed mono parts coexist; clearing the phrase retains a live mono in a nonzero slot.
 p.common.monoPoly=1;test.applyPatch(p);test.capturePhrase(p);test.phraseNoteOn(48,100);test.noteOn(67,100);
 assert(test.activeVoices()==2);test.clearPhrase();test.renderBlock(b,br);
 assert(test.renderedVoices()==1);bool audible=false;for(float x:b)audible|=std::fabs(x)>0.000001f;assert(audible);
 // Repeated mono note-ons require both note-offs to reach the per-part stack.
 test.noteOn(67,100);test.noteOff(67);test.noteOff(67);
 for(unsigned i=0;i<200;++i)test.renderBlock(b,br);assert(!test.activeVoices());
 // Captured-source normal/dry equivalence, including live patch switching.
 p=RdxEngine::initPatch();RdxEngine direct,dry;
 direct.applyPatch(p);direct.capturePhrase(p);dry.applyPatch(p);dry.capturePhrase(p);
 direct.phraseNoteOn(55,110);dry.phraseNoteOn(55,110);
 for(unsigned block=0;block<100;++block){
   if(block==30){direct.applyPatch(q);dry.applyPatch(q);}
   if(block==60){direct.phraseNoteOff(55);dry.phraseNoteOff(55);}
   direct.renderBlock(a,ar);dry.renderDry(bus);RdxEngine::mixDry(bus,dry.outputGain(),b,br);compare(a,b);
 }
 puts("PASS captured phrase timbre, live edits/presets, independent note/sustain ownership, shared limit, mixed mono clear and dry equivalence");
}
