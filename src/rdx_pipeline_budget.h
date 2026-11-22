#pragma once
#include "rdx_budget.h"
// CPU0-owned policy. Two rotating 1.5 s windows retain recent peaks for
// 1.5..3 s. Separate costs prevent queue delay from becoming per-voice work.
// Predictions include the publication offset: worker must finish before the
// NEXT callback, rather than one full block after its eventual dispatch.
class RdxPipelineBudget {
public:
    enum eReason {REASON_NONE,REASON_CPU0,REASON_LATE,REASON_TURN,REASON_PREDICT,REASON_GROW,REASONS_COUNT};
    uint8_t limit=RdxBudget::maximum;
    uint8_t reason=REASON_NONE;
    void reset(uint32_t now){
        banks_[0]=banks_[1]=Peak{};bank_=0;epoch_=changed_=now;
        limit=RdxBudget::maximum;reason=REASON_NONE;
    }
    uint32_t prediction(uint8_t voices) const {
        if(!peak(&Peak::per))return 0;
        const uint32_t cpu1=workerPrediction(voices);
        const uint32_t cpu0=peak(&Peak::cpu0);return cpu1>cpu0?cpu1:cpu0;
    }
    void observe(uint32_t now,uint32_t cpu0,uint32_t lead,uint32_t queue,uint32_t work,
                 uint8_t voices,bool valid,bool late){
        const uint32_t elapsed=now-epoch_;
        if(elapsed>=3000u){banks_[0]=banks_[1]=Peak{};bank_=0;epoch_=now;}
        else if(elapsed>=1500u){bank_^=1u;banks_[bank_]=Peak{};epoch_+=1500u;}
        // Clamp before sums/products; even corrupt diagnostic inputs stay bounded.
        cpu0=bound(cpu0);lead=bound(lead);queue=bound(queue);work=bound(work);
        if(late){shed(now,REASON_LATE);return;}
        const uint32_t urgent=RdxBudget::deadline*90u/100u;
        if(cpu0>urgent){shed(now,REASON_CPU0);return;}
        if(lead+queue+work>urgent){shed(now,REASON_TURN);return;}
        if(!valid)return;
        Peak& p=banks_[bank_];keep(p.cpu0,cpu0);keep(p.lead,lead);keep(p.queue,queue);
        if(voices&&voices<=8u&&work)keep(p.per,(work+voices-1u)/voices);
        if(!peak(&Peak::per))return;
        uint8_t safe=limit;
        // A fixed CPU0/FX peak must not make this loop shed every voice:
        // removing voices cannot remove that fixed workload. It blocks growth;
        // actual CPU0 deadline pressure is handled separately above.
        while(safe>1u&&workerPrediction(safe)>RdxBudget::target)--safe;
        if(safe<limit){limit=safe;changed_=now;reason=REASON_PREDICT;return;}
        // Quiet scheduling must persist before admitting another attack.
        if(limit<RdxBudget::maximum&&now-changed_>=1500u&&
           prediction(limit+1u)<RdxBudget::target-RdxBudget::deadline/50u){
            ++limit;changed_=now;reason=REASON_GROW;
        }
    }
private:
    struct Peak {uint32_t cpu0=0,lead=0,queue=0,per=0;};
    Peak banks_[2];uint32_t epoch_=0,changed_=0;uint8_t bank_=0;
    static uint32_t bound(uint32_t v){return v>RdxBudget::deadline?RdxBudget::deadline:v;}
    static void keep(uint32_t& dst,uint32_t v){if(v>dst)dst=v;}
    uint32_t peak(uint32_t Peak::*member) const {
        const uint32_t a=banks_[0].*member,b=banks_[1].*member;return a>b?a:b;
    }
    uint32_t workerPrediction(uint8_t voices) const {
        return peak(&Peak::lead)+peak(&Peak::queue)+peak(&Peak::per)*voices;
    }
    void shed(uint32_t now,eReason why){if(limit>1u)--limit;changed_=now;reason=why;}
};
