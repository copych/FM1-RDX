#pragma once
#include <stdint.h>
// 24 MHz platform ticks. Models are patch-local maxima, never host CPU claims.
class RdxBudget {
public:
    static constexpr uint32_t deadline=128u*24000000u/44100u;
    static constexpr uint32_t target=deadline*85u/100u;
    uint8_t limit=4;
    void reset(uint32_t now){limit=4;voiceCost_=fixedCost_=0;changed_=now;}
    void overload(uint32_t now){if(limit>1)--limit;changed_=now;}
    void observe(uint32_t now,uint32_t app,uint32_t synth,uint8_t voices,bool valid){
        if(app>target){overload(now);return;}
        if(!valid||!voices||!synth||synth>app)return;
        const uint32_t per=(synth+voices-1u)/voices,fixed=app-synth;
        if(per>voiceCost_)voiceCost_=per;
        if(fixed>fixedCost_)fixedCost_=fixed;
        // Clamp before multiplying: no wide arithmetic in the deadline path.
        if(voiceCost_>deadline)voiceCost_=deadline;
        if(fixedCost_>deadline)fixedCost_=deadline;
        uint8_t safe=limit;
        while(safe>1&&fixedCost_+voiceCost_*safe>target)--safe;
        if(safe<limit){limit=safe;changed_=now;return;}
        if(limit<8&&now-changed_>=1500u&&fixedCost_+voiceCost_*(limit+1u)<target-deadline/20u){
            ++limit;changed_=now;
        }
    }
private:
    uint32_t voiceCost_=0,fixedCost_=0,changed_=0;
};
