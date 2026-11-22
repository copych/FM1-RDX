template<bool MONO>
RDX_RAM_CODE void prepareChunk(RdxVoiceBlockScratch& s,uint32_t n){
    for(uint32_t i=0;i<n;++i){
        updateMods<MONO>();
        const float ratio=semitonesToRatio(phaseMod_[3]);
        s.ratio[3][i]=ratio;
        if(sharedPitch_){s.ratio[2][i]=ratio;s.ratio[1][i]=ratio;s.ratio[0][i]=ratio;}
        else for(int op=2;op>=0;--op)s.ratio[op][i]=semitonesToRatio(phaseMod_[op]);
        for(int op=0;op<4;++op)s.amp[op][i]=ampMod_[op];
    }
}
inline __attribute__((always_inline)) void renderChunk(RdxVoiceBlockScratch& s,uint32_t n,bool mono){
    if(mono)prepareChunk<true>(s,n);else prepareChunk<false>(s,n);
    ops_[3].blockRun(s.wave[3],nullptr,s.ratio[3],s.amp[3],n);
    ops_[2].blockRun(s.wave[2],route2_?s.wave[3]:nullptr,s.ratio[2],s.amp[2],n);
    const float* input=nullptr;
    if(route1_==12){for(uint32_t i=0;i<n;++i)s.input[i]=s.wave[3][i]+s.wave[2][i];input=s.input;}
    else if(route1_==4)input=s.wave[2];else if(route1_==8)input=s.wave[3];
    ops_[1].blockRun(s.wave[1],input,s.ratio[1],s.amp[1],n);
    input=nullptr;
    switch(route0_){
    case 2:input=s.wave[1];break;case 4:input=s.wave[2];break;case 8:input=s.wave[3];break;
    case 6:for(uint32_t i=0;i<n;++i)s.input[i]=s.wave[1][i]+s.wave[2][i];input=s.input;break;
    case 10:for(uint32_t i=0;i<n;++i)s.input[i]=s.wave[1][i]+s.wave[3][i];input=s.input;break;
    case 14:for(uint32_t i=0;i<n;++i)s.input[i]=(s.wave[1][i]+s.wave[2][i])+s.wave[3][i];input=s.input;break;
    }
    ops_[0].blockRun(s.wave[0],input,s.ratio[0],s.amp[0],n);
    // Keep the original carrier summation order, overwriting only the final bus.
    switch(carriers_){
    case 3:for(uint32_t i=0;i<n;++i)s.wave[0][i]+=s.wave[1][i];break;
    case 5:for(uint32_t i=0;i<n;++i)s.wave[0][i]+=s.wave[2][i];break;
    case 7:for(uint32_t i=0;i<n;++i)s.wave[0][i]=(s.wave[0][i]+s.wave[1][i])+s.wave[2][i];break;
    case 15:for(uint32_t i=0;i<n;++i)s.wave[0][i]=((s.wave[0][i]+s.wave[1][i])+s.wave[2][i])+s.wave[3][i];break;
    default:break;
    }
}
