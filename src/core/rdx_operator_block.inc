// Routing/rectification mode is selected once, outside this operator's loop.
template<bool INPUT,bool RECTIFY>
RDX_RAM_CODE void blockKernel(float* out,const float* input,const float* ratio,const float* amp,uint32_t n){
    float phase=phase_,feedback=fbAcc_,filter=fbFilter_;
    const float increment=phaseInc_,gain=outGain_,scale=fbScale_,coef=fbLpCoef_;
    const bool enabled=params_.enable;
    for(uint32_t i=0;i<n;++i){
        if constexpr(RECTIFY)if(feedback<0.0f)feedback=-feedback;
        filter+=coef*(feedback-filter);
        const float offset=INPUT?input[i]:0.0f;
        const float lookup=wrap01(phase+offset+filter*scale);
        phase=wrapPositive01(phase+increment*ratio[i]);
        feedback=sin01(lookup);
        const float sample=feedback*gain*env_.processAEG();
        out[i]=amp[i]*(enabled?sample:0.0f);
    }
    phase_=phase;fbAcc_=feedback;fbFilter_=filter;
}
inline __attribute__((always_inline)) void blockRun(float* out,const float* input,const float* ratio,const float* amp,uint32_t n){
    if(input){
        if(fbRectify_)blockKernel<true,true>(out,input,ratio,amp,n);
        else blockKernel<true,false>(out,input,ratio,amp,n);
    }else{
        if(fbRectify_)blockKernel<false,true>(out,nullptr,ratio,amp,n);
        else blockKernel<false,false>(out,nullptr,ratio,amp,n);
    }
}
