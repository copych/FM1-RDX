#pragma once
#include "../app/engine_api.h"
#include "rdx_budget.h"
static_assert((unsigned)COST_FIELDS_COUNT==(unsigned)COST_ROW_WORDS,"cost snapshot row layout");
// Fixed duration, bounded recording; platform protection may terminate earlier.
class RdxVoiceMeasure {
public:
    static constexpr uint32_t guard=RdxBudget::deadline*9u/10u;
    struct Row {uint32_t v[COST_ROW_WORDS]{};};
    Row rows[COST_ROWS];
    uint32_t state=COST_IDLE,target=0,started=0,stageAt=0,measuredAt=0,stamp=0,prediction=0,lastTarget=6;
    bool active() const {return state>=COST_ARMED&&state<=COST_MEASURE;}
    void start(uint32_t now,uint32_t predicted,uint32_t first=0,uint32_t last=6){
        for(auto& row:rows)row=Row{};
        state=COST_ARMED;target=first;lastTarget=last;started=stageAt=stamp=now;prediction=predicted;
        if(first>last||last>=COST_ROWS)state=COST_CHANGED;
    }
    void begin(uint32_t now){state=COST_SETTLE;stageAt=stamp=now;}
    void stop(uint32_t reason){state=reason;}
    void overload(uint32_t voices){if(voices<COST_ROWS)target=voices;state=COST_LIMIT;}
    void record(uint32_t app,uint32_t synth,uint32_t fx,uint32_t voices,uint32_t queue=0,uint32_t turn=0,uint32_t lead=0){
        if(target>=COST_ROWS)return;
        Row& row=rows[target];++row.v[COST_BLOCKS];row.v[COST_ACTUAL]=voices;
        if(app>row.v[COST_APP]){row.v[COST_APP]=app;row.v[COST_PAIRED_SYN]=synth;row.v[COST_PAIRED_FX]=fx;}
        if(synth>row.v[COST_SYN])row.v[COST_SYN]=synth;
        if(fx>row.v[COST_FX])row.v[COST_FX]=fx;
        if(queue>row.v[COST_QUEUE])row.v[COST_QUEUE]=queue;
        if(turn>row.v[COST_TURN])row.v[COST_TURN]=turn;
        if(lead>row.v[COST_LEAD])row.v[COST_LEAD]=lead;
    }
    void observe(uint32_t now,uint32_t app,uint32_t synth,uint32_t fx,uint32_t voices,bool valid,
                 uint32_t queue=0,uint32_t turn=0,uint32_t lead=0){
        stamp=now;
        if(!active()||state==COST_ARMED)return;
        record(app,synth,fx,voices,queue,turn,lead);
        if(!valid){state=COST_CHANGED;return;}
        if(voices!=target){state=COST_MISMATCH;return;}
        if(app>guard){state=COST_LIMIT;return;}
        if(state==COST_SETTLE){if((uint32_t)(now-stageAt)>=2000u){state=COST_MEASURE;measuredAt=now;}}
        else {
            ++rows[target].v[COST_STEADY_BLOCKS];
            if((uint32_t)(now-measuredAt)>=3000u){
                if(target==lastTarget)state=COST_DONE;
                else {++target;state=COST_ARMED;stageAt=now;}
            }
        }
    }
    void snapshot(uint32_t* out) const {
        out[0]=state;out[1]=target;out[2]=stamp-started;out[3]=prediction;out[4]=guard;out[5]=COST_ROWS;
        for(uint32_t row=0;row<COST_ROWS;++row)for(uint32_t field=0;field<COST_ROW_WORDS;++field)
            out[COST_HEADER_WORDS+row*COST_ROW_WORDS+field]=rows[row].v[field];
    }
};
