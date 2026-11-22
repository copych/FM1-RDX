// Diagnostic split of compute(): envelope state is independent of oscillator state.
inline __attribute__((always_inline)) float profileEnvelope(){return env_.processAEG();}
inline __attribute__((always_inline)) void profileEnvelopeAdvance(RdxEnvelopePoint* out){env_.profileAdvance(out);}
inline __attribute__((always_inline)) float profilePitch(float modulation){
    return preparePitchRatio(semitonesToRatio(modulation));
}
inline __attribute__((always_inline)) float preparePitchRatio(float ratio){
    const float before=phase_;
    phase_=phase_+phaseInc_*ratio;
    phase_=wrapPositive01(phase_);return before;
}
inline __attribute__((always_inline)) float profileWavePrepared(float input,float before,float envelope){
    if(fbRectify_&&fbAcc_<0.0f)fbAcc_=-fbAcc_;
    fbFilter_+=fbLpCoef_*(fbAcc_-fbFilter_);
    const float lookup=wrap01(before+input+fbFilter_*fbScale_);
    fbAcc_=sin01(lookup);
    const float sample=fbAcc_*outGain_*envelope;
    return params_.enable?sample:0.0f;
}
inline __attribute__((always_inline)) float profileWave(float input,float modulation,float envelope){
    if(fbRectify_&&fbAcc_<0.0f)fbAcc_=-fbAcc_;
    fbFilter_+=fbLpCoef_*(fbAcc_-fbFilter_);
    const float lookup=wrap01(phase_+input+fbFilter_*fbScale_);
    phase_=phase_+phaseInc_*semitonesToRatio(modulation);
    phase_=wrapPositive01(phase_);
    fbAcc_=sin01(lookup);
    const float sample=fbAcc_*outGain_*envelope;
    return params_.enable?sample:0.0f;
}
