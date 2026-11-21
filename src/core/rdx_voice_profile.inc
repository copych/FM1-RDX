// Diagnostic block stages; same equations and accumulation order as stepRouted.
template<bool MONO> inline __attribute__((always_inline)) void profileMods(float* out){
    updateMods<MONO>();
    for(uint32_t i=0;i<4;++i){out[i]=phaseMod_[i];out[4+i]=ampMod_[i];}
}
inline __attribute__((always_inline)) void profileEnvelopes(float* out){
    // Production evaluates operators in descending order.
    RDX_UNROLL4
    for(int i=3;i>=0;--i)out[i]=ops_[i].profileEnvelope();
}
inline __attribute__((always_inline)) void profileEnvelopeAdvance(RdxEnvelopePoint* out){
    RDX_UNROLL4
    for(int i=3;i>=0;--i)ops_[i].profileEnvelopeAdvance(out+i);
}
inline __attribute__((always_inline)) void profilePitch(const float* m,float* before){
    RDX_UNROLL4
    for(int i=3;i>=0;--i)before[i]=ops_[i].profilePitch(m[i]);
}
inline __attribute__((always_inline)) float profileWavePrepared(const float* m,const float* e,const float* phase){
    const float v3=m[7]*ops_[3].profileWavePrepared(0.0f,phase[3],e[3]);
    const float v2=m[6]*ops_[2].profileWavePrepared(route2_?v3:0.0f,phase[2],e[2]);
    const float in1=route1_==12?v3+v2:route1_==4?v2:route1_==8?v3:0.0f;
    const float v1=m[5]*ops_[1].profileWavePrepared(in1,phase[1],e[1]);
    float in0=0.0f;
    switch(route0_){case 2:in0=v1;break;case 4:in0=v2;break;case 6:in0=v1+v2;break;
      case 8:in0=v3;break;case 10:in0=v1+v3;break;case 14:in0=(v1+v2)+v3;break;}
    const float v0=m[4]*ops_[0].profileWavePrepared(in0,phase[0],e[0]);
    switch(carriers_){case 3:return v0+v1;case 5:return v0+v2;case 7:return (v0+v1)+v2;
      case 15:return ((v0+v1)+v2)+v3;default:return v0;}
}
inline __attribute__((always_inline)) float profileWave(const float* m,const float* e){
    const float v3=m[7]*ops_[3].profileWave(0.0f,m[3],e[3]);
    const float v2=m[6]*ops_[2].profileWave(route2_?v3:0.0f,m[2],e[2]);
    const float in1=route1_==12?v3+v2:route1_==4?v2:route1_==8?v3:0.0f;
    const float v1=m[5]*ops_[1].profileWave(in1,m[1],e[1]);
    float in0=0.0f;
    switch(route0_){case 2:in0=v1;break;case 4:in0=v2;break;case 6:in0=v1+v2;break;
      case 8:in0=v3;break;case 10:in0=v1+v3;break;case 14:in0=(v1+v2)+v3;break;}
    const float v0=m[4]*ops_[0].profileWave(in0,m[0],e[0]);
    switch(carriers_){case 3:return v0+v1;case 5:return v0+v2;case 7:return (v0+v1)+v2;
      case 15:return ((v0+v1)+v2)+v3;default:return v0;}
}
