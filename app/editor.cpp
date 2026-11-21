#include "editor_api.h"
#include "../src/gui/RDX_GUI.h"
alignas(UiRamDisplay) static unsigned char display_storage[sizeof(UiRamDisplay)];
alignas(RDX_GUI) static unsigned char gui_storage[sizeof(RDX_GUI)];
static UiRamDisplay* screen;
static RDX_GUI* gui;
static RDX_Patch view_patch;
static UiPhaserView phaser;
static uint8_t tune_selection;
static unsigned preset_index;
static unsigned midi_channel;
static bool midi_control_enabled;
UiRamDisplay& ui_display(){return *screen;}
unsigned ui_preset_index(){return preset_index;}
unsigned ui_midi_channel(){return midi_channel;}
bool ui_midi_control(){return midi_control_enabled;}
extern "C" void rdx_editor_boot(void) {
    screen=new(display_storage) UiRamDisplay;
    gui=new(gui_storage) RDX_GUI(view_patch);gui->begin();
}
extern "C" const uint8_t* rdx_editor_draw(const EditorSnapshot* s,uint8_t page,uint8_t op,uint8_t tune) {
    memcpy(&view_patch,s->patch,150);memcpy(phaser.values,s->phaser,sizeof phaser.values);
    preset_index=s->preset;tune_selection=tune<5?tune:0;
    midi_channel=s->midi_rx;midi_control_enabled=s->midi_control!=0;
    gui->setPhaserLab(&phaser,&tune_selection,nullptr,nullptr);
    gui->setOperator(op);gui->setPage((RDX_GUI::Page)(page<EDITOR_PAGES_COUNT?page:EDITOR_HOME));
    gui->pause(0);gui->push();gui->draw();return screen->pixels;
}
extern "C" const char* rdx_editor_title(uint8_t p) {
    static const char* names[EDITOR_PAGES_COUNT]={"HOME","LEVEL","FEEDBACK","FREQUENCY","ALGORITHM","VOICE","OP FREQUENCY","OP LEVEL / FB","KEY SCALING","LFO","LFO PITCH ON","LFO AMP DEPTH","PHASER LAB","EFFECT 1","EFFECT 2","AEG RATES","AEG LEVELS","PEG RATES","PEG LEVELS","PITCH EG ON/OFF","MIDI"};
    return names[p<EDITOR_PAGES_COUNT?p:EDITOR_HOME];
}
extern "C" const char* rdx_editor_label(uint8_t p,uint8_t k) {
    if((p==EDITOR_FX1||p==EDITOR_FX2)&&k>=2&&k<4){
      static const char* effect_labels[8][2]={{"--","--"},{"DRIVE","TONE"},{"SENS","REZ"},{"DEPTH","RATE"},{"DEPTH","RATE"},{"DEPTH","RATE"},{"DEPTH","TIME"},{"DEPTH","TIME"}};
      unsigned id=view_patch.common.effects[p==EDITOR_FX1?0:1][0];return effect_labels[id<8?id:0][k-2];
    }
    static const char* labels[EDITOR_PAGES_COUNT][4]={
      {"OP1","OP2","OP3","OP4"},{"OP1","OP2","OP3","OP4"},{"OP1","OP2","OP3","OP4"},{"OP1","OP2","OP3","OP4"},
      {"ALGO","--","--","--"},{"TRANS","MODE","PORTA","BEND"},{"ON/OFF","MODE","RATIO","DETUNE"},{"LEVEL","VEL.S","FB","KSC-R"},
      {"LEFT","L.CRV","R.CRV","RIGHT"},{"WAVE","SPEED","DELAY","PMD"},{"OP1","OP2","OP3","OP4"},{"OP1","OP2","OP3","OP4"},
      {"SELECT","VALUE","FL.DEP","FL.OFF"},{"TYPE","--","DEPTH","RATE"},{"TYPE","--","DEPTH","RATE"},
      {"R1","R2","R3","R4"},{"L1","L2","L3","L4"},{"R1","R2","R3","R4"},{"L1","L2","L3","L4"},
      {"OP1","OP2","OP3","OP4"},{"RX CH","FM CC","--","--"}};
    return labels[p<EDITOR_PAGES_COUNT?p:EDITOR_HOME][k<4?k:0];
}
