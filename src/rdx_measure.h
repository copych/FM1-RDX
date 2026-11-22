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
    uint32_t state=COST_IDLE,target=0,started=0,stageAt=0,measuredAt=0,stamp=0,prediction=0;
    bool active() const {return state>=COST_ARMED&&state<=COST_MEASURE;}
    void start(uint32_t now,uint32_t predicted){
        for(auto& row:rows)row=Row{};
        state=COST_ARMED;target=0;started=stageAt=stamp=now;prediction=predicted;
    }
    void begin(uint32_t now){state=COST_SETTLE;stageAt=stamp=now;}
    void stop(uint32_t reason){state=reason;}
    void overload(uint32_t voices){if(voices<COST_ROWS)target=voices;state=COST_LIMIT;}
    void observe(uint32_t now,uint32_t app,uint32_t synth,uint32_t fx,uint32_t voices,bool valid){
        stamp=now;
        if(!active()||state==COST_ARMED)return;
        Row& row=rows[target];++row.v[COST_BLOCKS];row.v[COST_ACTUAL]=voices;
        if(app>row.v[COST_APP]){row.v[COST_APP]=app;row.v[COST_PAIRED_SYN]=synth;row.v[COST_PAIRED_FX]=fx;}
        if(synth>row.v[COST_SYN])row.v[COST_SYN]=synth;
        if(fx>row.v[COST_FX])row.v[COST_FX]=fx;
        if(!valid){state=COST_CHANGED;return;}
        if(voices!=target){state=COST_MISMATCH;return;}
        if(app>guard){state=COST_LIMIT;return;}
        if(state==COST_SETTLE){if((uint32_t)(now-stageAt)>=2000u){state=COST_MEASURE;measuredAt=now;}}
        else {
            ++row.v[COST_STEADY_BLOCKS];
            if((uint32_t)(now-measuredAt)>=3000u){
                if(target==COST_ROWS-1u)state=COST_DONE;
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
