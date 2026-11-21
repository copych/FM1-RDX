#include "../src/core/RDX_LFO.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main(){
    float worst=0;int failures=0;
    for(auto wave:{RDX_LFO::Waveform::SINE,RDX_LFO::Waveform::TRIANGLE})
    for(int delay:{1,43,100})for(int rate:{0,73,127}){
        RDX_LFO lfo;lfo.init(rate,delay,wave);float end=0;
        for(int b=0;b<10000;++b){
            lfo.updateState();float start=lfo.getValue(),increment=lfo.getIncrement();
            float gap=fabsf(start-end);if(gap>worst)worst=gap;
            if(gap>0.00001f)++failures;
            end=start+increment*DMA_BUFFER_LEN;
            if(!std::isfinite(end)||fabsf(end)>1.00001f)++failures;
        }
    }
    std::printf("LFO continuous-wave block seam: max gap %.9f; violations %d\n",worst,failures);
    return failures!=0;
}
