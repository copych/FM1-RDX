// Serialized control/UI owner only; never called by the audio renderer.
// Timing boundaries exclude cooperative audio service. Host exercises counts,
// while durations stay zero because they are not device measurements.
enum eUiPart {UI_DMA_WAIT,UI_CLEAR,UI_TEXT,UI_SUBMIT,UI_EDITOR,UI_PARTS_COUNT};
static uint32_t ui_peaks[UI_PARTS_COUNT],ui_calls[UI_PARTS_COUNT];
static uint32_t ui_ticks(void){
#ifdef PLAT_HOST
    return 0;
#else
    return plat_ticks24();
#endif
}
static void ui_record_excluding(uint32_t part,uint32_t start,uint32_t excluded){
    if(part>=UI_PARTS_COUNT)return;
    const uint32_t total=ui_ticks()-start;
    const uint32_t elapsed=excluded<=total?total-excluded:0;
    if(elapsed>ui_peaks[part])ui_peaks[part]=elapsed;
    if(ui_calls[part]!=UINT32_MAX)++ui_calls[part];
}
static void ui_record(uint32_t part,uint32_t start){ui_record_excluding(part,start,0);}
static void ui_profile_snapshot(uint32_t* out){
    for(uint32_t i=0;i<UI_PARTS_COUNT;++i)out[i]=ui_peaks[i];
}
static void ui_profile_reset(void){
    for(uint32_t i=0;i<UI_PARTS_COUNT;++i)ui_peaks[i]=ui_calls[i]=0;
}
