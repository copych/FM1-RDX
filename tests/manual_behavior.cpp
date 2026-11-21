#include "engine_api.h"
#include "editor_api.h"
#include "../src/rdx_engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static float l[128],r[128];
static void run(int blocks){for(int b=0;b<blocks;++b){rdx_render(l,r);for(int i=0;i<128;++i)CHECK(std::isfinite(l[i])&&std::isfinite(r[i]));}}
static EditorSnapshot view;
static RDX_Patch patch(){rdx_editor_snapshot(&view);RDX_Patch p;memcpy(&p,view.patch,sizeof p);return p;}
static float rms(int blocks){double energy=0;for(int b=0;b<blocks;++b){run(1);for(float x:l)energy+=(double)x*x;}return (float)std::sqrt(energy/(blocks*128));}
static float frequency(){int crossings=0;float previous=l[127];for(int b=0;b<200;++b){run(1);for(float x:l){if(previous<=0&&x>0)++crossings;previous=x;}}return crossings*(44100.0f/(200.0f*128.0f));}
static void init_voice(){rdx_boot();rdx_preset_request(0);run(40);rdx_fx_toggle();rdx_note_on(69,100);run(100);}
int main(){
  // Reference pp.5-6: note-off enters Rate 4 without waiting for attack/decay.
  const uint8_t rates[4]={0,0,0,127},levels[4]={127,100,90,0};
  for(int mode=0;mode<3;++mode){RDX_State::getState().workingPatch.common.monoPoly=(uint8_t)mode;RDX_State::getState().workingPatch.common.portaTime=127;
    RDX_Envelope env;env.initAEG(rates,levels);env.gate(true);for(int i=0;i<20;++i)env.processAEG();env.gate(false);CHECK(env.getStage()==RDX_Envelope::Stage::RELEASE);
  }
  const uint8_t fast[4]={127,127,127,127},nonzeroRelease[4]={127,127,127,64};
  RDX_Envelope tail;tail.initAEG(fast,nonzeroRelease);tail.gate(true);for(int i=0;i<1000;++i)tail.processAEG();tail.gate(false);
  for(int i=0;i<100000&&tail.isActive();++i)tail.processAEG();CHECK(!tail.isActive()&&tail.processAEG()==0);
  // A disabled operator still completes its envelope; it cannot resume a stuck note.
  RdxEngine core;auto p=RdxEngine::initPatch();p.ops[0].enable=0;p.ops[0].egRate[3]=127;core.applyPatch(p);
  RDX_Operator op(0);op.updateParams();op.setParams(60,100,261.6256f);op.gate(true);for(int i=0;i<1000;++i)CHECK(op.compute(0)==0);
  op.gate(false);for(int i=0;i<100000&&op.isActive();++i)CHECK(op.compute(0)==0);CHECK(!op.isActive());
  // Reference p.7: modulation remains zero during the configured delay.
  RDX_LFO lfo;lfo.init(64,100,RDX_LFO::Waveform::SQUARE);
  const int delayBlocks=(int)std::ceil(DELAY_TIME_MS[100]*0.001f*SAMPLE_RATE/DMA_BUFFER_LEN);
  for(int b=0;b<delayBlocks;++b){lfo.updateState();CHECK(lfo.getValue()==0&&lfo.getIncrement()==0);}
  lfo.updateState();CHECK(fabsf(lfo.getValue())>0);
  lfo.init(64,0,RDX_LFO::Waveform::SQUARE);lfo.updateState();CHECK(fabsf(lfo.getValue())>0);
  // Owner's p.16/21: +/- range and inverted pitch-bend act on audible pitch.
  init_voice();CHECK(fabsf(frequency()-440.0f)<4.0f);
  rdx_editor_adjust(EDITOR_VOICE,0,3,10,0);run(2);rdx_pitch_bend(16383);run(2);CHECK(fabsf(frequency()-880.0f)<5.0f);
  rdx_pitch_bend(0);run(2);CHECK(fabsf(frequency()-220.0f)<4.0f);
  rdx_editor_adjust(EDITOR_VOICE,0,3,-24,0);run(2);rdx_pitch_bend(16383);run(2);CHECK(fabsf(frequency()-220.0f)<4.0f);
  rdx_pitch_bend(8192);run(2);CHECK(fabsf(frequency()-440.0f)<4.0f);
  // Data list p.6: volume and expression multiply, reset preserves main volume.
  init_voice();const float base=rms(100);rdx_control_change(7,64);run(50);const float half=rms(100);CHECK(fabsf(half/base-64.0f/127.0f)<0.01f);
  rdx_control_change(11,64);run(50);CHECK(fabsf(rms(100)/half-64.0f/127.0f)<0.01f);
  rdx_control_change(1,127);rdx_pitch_bend(16383);rdx_control_change(121,0);run(50);CHECK(fabsf(rms(100)/half-1.0f)<0.01f);
  CHECK(RDX_State::getState().controls.pitchbendSemitones==0&&RDX_State::getState().controls.modWheelFactor==0);
  // All Notes Off respects sustain; All Sound Off immediately clears sound/tails.
  init_voice();rdx_sustain(1);rdx_control_change(123,0);CHECK(rms(50)>0.001f);rdx_sustain(0);run(1000);CHECK(rms(10)==0);
  init_voice();rdx_control_change(120,0);run(30);CHECK(rms(5)==0);
  // Data list p.8: optional FM CCs, threshold bands, per-operator parameters.
  rdx_boot();run(30);const uint8_t original=patch().ops[0].outLevel;rdx_control_change(85,0);run(1);CHECK(patch().ops[0].outLevel==original);
  rdx_editor_adjust(EDITOR_MIDI,0,1,1,0);run(1);const uint8_t bases[4]={85,102,108,114};
  for(int i=0;i<4;++i){rdx_control_change(bases[i],41+i);rdx_control_change(bases[i]+1,50+i);rdx_control_change(bases[i]+2,64);rdx_control_change(bases[i]+3,127);rdx_control_change(bases[i]+4,127);rdx_control_change(bases[i]+5,127);}
  run(1);p=patch();for(int i=0;i<4;++i){const auto& q=p.ops[i];CHECK(q.outLevel==41+i&&q.feedback==50+i&&q.fbType==1&&q.freqMode==1&&q.freqCoarse==31&&q.freqFine==99);}
  const uint8_t upper[12]={11,21,32,42,53,63,74,85,95,106,116,127};
  for(int i=0;i<12;++i){rdx_control_change(80,upper[i]);run(1);CHECK(patch().common.algorithm==i);if(i<11){rdx_control_change(80,upper[i]+1);run(1);CHECK(patch().common.algorithm==i+1);}}
  // Curve selection cannot change left/right depth polarity (reference p.4).
  rdx_editor_adjust(EDITOR_OP3,0,0,-127,0);run(1);CHECK(patch().ops[0].scaleLC<2);
  rdx_editor_adjust(EDITOR_OP3,0,1,127,0);run(1);CHECK(patch().ops[0].scaleLC==1);
  rdx_editor_adjust(EDITOR_OP3,0,1,-127,0);run(1);CHECK(patch().ops[0].scaleLC==0);
  for(int i=0;i<4;++i){rdx_editor_adjust(EDITOR_PEG_ON,0,i,1,0);run(1);CHECK(patch().ops[i].pegEnable==1);rdx_editor_adjust(EDITOR_PEG_ON,0,i,-1,0);run(1);CHECK(patch().ops[i].pegEnable==0);}
  rdx_editor_adjust(EDITOR_MIDI,0,0,127,0);CHECK(rdx_midi_receive_channel()==16);rdx_editor_adjust(EDITOR_MIDI,0,0,-127,0);CHECK(rdx_midi_receive_channel()==0);
  rdx_control_change(124,0);CHECK(rdx_midi_receive_channel()==1);
  std::puts("PASS: documented release, muted-operator retirement, LFO wait, audible pitch bend, volume/expression/reset, sustain-aware note-off, FM CCs, curve sign, PEG switches and MIDI channel bounds.");
}
