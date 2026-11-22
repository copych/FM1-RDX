// Class fragment included by RDX_Voice; hand-maintained FM-1 hot kernel.
// Addition order matches the inherited step() used by equivalence tests.
private:
    uint8_t route2_=0,route1_=0,route0_=0,carriers_=1;
    bool sharedPitch_=false;
public:
    #include "rdx_voice_profile.inc"
    void cachePitchRouting(){
        sharedPitch_=true;
        for(int i=0;i<3;++i)if(patch_.ops[i].pegEnable!=patch_.ops[3].pegEnable||patch_.ops[i].lfoPMDEnable!=patch_.ops[3].lfoPMDEnable)sharedPitch_=false;
    }
    void cacheRouting(){
        cachePitchRouting();
        static constexpr uint8_t routes[12][4]={
            {1,4,2,1},{0,12,2,1},{0,4,10,1},{1,8,6,1},
            {0,0,14,1},{1,4,0,3},{1,4,4,3},{1,0,2,5},
            {1,8,8,7},{1,8,0,7},{1,0,0,7},{0,0,0,15}};
        const uint8_t* route=routes[algorithm_<12?algorithm_:0];
        route2_=route[0];route1_=route[1];route0_=route[2];carriers_=route[3];
    }
    template <bool WITH_PORTAMENTO>
    inline __attribute__((always_inline, hot)) float stepRouted(){
        updateMods<WITH_PORTAMENTO>();
        // Independent per-operator stages stay sample-local. Preserve old
        // phases for FM lookup; feedback/waves still execute in routing order.
        const float e3=ops_[3].profileEnvelope(),e2=ops_[2].profileEnvelope();
        const float e1=ops_[1].profileEnvelope(),e0=ops_[0].profileEnvelope();
        float p3,p2,p1,p0;
        if(sharedPitch_){
            // Identical PEG/LFO enable coefficients produce identical pitch
            // inputs. Reuse this sample's exact LUT result, never a block hold.
            const float ratio=semitonesToRatio(phaseMod_[3]);
            p3=ops_[3].preparePitchRatio(ratio);p2=ops_[2].preparePitchRatio(ratio);
            p1=ops_[1].preparePitchRatio(ratio);p0=ops_[0].preparePitchRatio(ratio);
        }else{
            p3=ops_[3].profilePitch(phaseMod_[3]);p2=ops_[2].profilePitch(phaseMod_[2]);
            p1=ops_[1].profilePitch(phaseMod_[1]);p0=ops_[0].profilePitch(phaseMod_[0]);
        }
        const float v3=ampMod_[3]*ops_[3].profileWavePrepared(0.0f,p3,e3);
        const float v2=ampMod_[2]*ops_[2].profileWavePrepared(route2_?v3:0.0f,p2,e2);
        const float in1=route1_==12?v3+v2:route1_==4?v2:route1_==8?v3:0.0f;
        const float v1=ampMod_[1]*ops_[1].profileWavePrepared(in1,p1,e1);
        float in0=0.0f;
        switch(route0_){
            case 2:in0=v1;break;case 4:in0=v2;break;case 6:in0=v1+v2;break;
            case 8:in0=v3;break;case 10:in0=v1+v3;break;case 14:in0=(v1+v2)+v3;break;
        }
        const float v0=ampMod_[0]*ops_[0].profileWavePrepared(in0,p0,e0);
        switch(carriers_){case 3:return v0+v1;case 5:return v0+v2;case 7:return (v0+v1)+v2;case 15:return ((v0+v1)+v2)+v3;default:return v0;}
    }
