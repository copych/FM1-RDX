"""Host-only reference from untouched snapshot; never used for FM-1 builds."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
out=root/'build/reference'
out.mkdir(parents=True,exist_ok=True)
for name in ['RDX_Types.h','RDX_State.h','RDX_Constants.h','RDX_Envelope.h','RDX_PEG.h','RDX_LFO.h','RDX_Operator.h','RDX_Voice.h','fx_base.h','fx_distortion.h','fx_touch_wah.h','fx_chorus.h','fx_flanger.h','fx_phaser.h','fx_delay.h','fx_reverb.h']:
    s=(root/'RDX_before_UAC'/name).read_text()
    s='#include "Arduino.h"\n'+s
    # Attributes on the source class declaration are ESP-specific, not sound math.
    s=s.replace('class  IRAM_ATTR __attribute__((always_inline)) RDX_Operator','class RDX_Operator')
    (out/name).write_text(s)
(out/'config.h').write_text('#pragma once\n#define SAMPLE_RATE 44100\n#define DMA_BUFFER_LEN 128\n#define BOARD_HAS_PSRAM 1\n')
(out/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <array>
#include <algorithm>
#include <initializer_list>
#include "config.h"
#define IRAM_ATTR
#define DRAM_ATTR
#define TWO_PI 6.283185307179586476925286766559
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define unlikely(x) (x)
inline void vTaskDelay(int) {}
''')
(out/'esp_log.h').write_text('#pragma once\n#include "Arduino.h"\n')
(out/'esp32-hal.h').write_text('#pragma once\n#include "Arduino.h"\n')
# The original floor reads an uninitialized local. A defined host reference
# requires this one explicit arithmetic repair; all other DSP formulas remain.
s=(root/'RDX_before_UAC/misc.h').read_text().replace('#include "esp_heap_caps.h"','#include "Arduino.h"')
a=s.index('void logMemoryStats(');b=s.index('inline uint32_t lfsr_',a)
s=s[:a]+s[b:]
s=s.replace('int i = (int)x - (int)(i>x);','int i = (int)x; i -= (int)((float)i>x);')
(out/'misc.h').write_text(s)
print('Prepared host source reference; floor UB repaired, diagnostics/platform calls stubbed.')
