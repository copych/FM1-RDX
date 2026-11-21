#include <stdint.h>
#include <assert.h>
#define PLAT_HOST
static int enabled,busy,fail;
static uint32_t now;
static void (*job)(uint32_t);
static int plat_cpu2_set(int on){enabled=on;return 0;}
static int plat_cpu2_on(void){return enabled;}
static void plat_cpu2_sync(void){}
static int plat_cpu2_busy(void){return busy;}
static uint32_t plat_ms(void){return now;}
static int plat_cpu2_run(void (*fn)(uint32_t),uint32_t arg){(void)arg;assert(!busy);job=fn;busy=1;return enabled;}
static int plat_cpu2_wait(void){if(fail){busy=enabled=0;return -1;}assert(!busy);return 0;}
#include "../app/core_test.h"
static void complete(void){assert(busy);job(0);busy=0;}
int main(void){
    core_test_init();core_test_tick();assert(!busy);
    core_mode=CORE_TEST_IDLE;core_test_tick();assert(busy);
    uint32_t seq=core_seq;now=1;core_test_tick();assert(core_seq==seq);
    complete();core_mode=CORE_TEST_OFF;core_test_tick();assert(core_done==1&&!core_errors&&!busy);
    core_mode=CORE_TEST_BUSY;core_test_tick();complete();core_mail.result^=1;
    core_mode=CORE_TEST_OFF;core_test_tick();assert(core_errors==1&&!busy);
    core_mode=CORE_TEST_BUSY;core_test_tick();complete();core_mode=CORE_TEST_OFF;core_test_tick();assert(core_done==2);
    core_mode=CORE_TEST_IDLE;core_test_tick();now+=10;fail=1;core_test_tick();
    assert(!core_pending&&!enabled&&core_mode==CORE_TEST_OFF&&core_late==1&&core_errors==2);
    core_test_tick();assert(!busy);
    return 0;
}
