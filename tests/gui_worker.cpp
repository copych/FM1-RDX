#include <cassert>
#include <cstdint>
#include <cstring>
#include "../app/editor_api.h"
static uint32_t now,core;
static bool enabled=true,busy,fail;
static void (*pending_job)(uint32_t);
static uint32_t plat_ticks24(){return now*24000u;}
static uint32_t plat_ms(){return now;}
static uint32_t plat_core(){return core;}
static int plat_cpu2_on(){return enabled;}
static int plat_cpu2_busy(){return busy;}
static void plat_cpu2_sync(){}
static int plat_cpu2_run(void (*fn)(uint32_t),uint32_t){assert(!busy);pending_job=fn;busy=true;return 1;}
static int plat_cpu2_wait(){if(fail){busy=enabled=false;return -1;}assert(!busy);return 0;}
#include "../app/gui_worker.h"
static void complete(){assert(busy);core=1;pending_job(0);core=0;busy=false;}
int main(){
    rdx_editor_boot();EditorSnapshot view={};
    view.patch[16]=11;view.patch[17]=1;
    for(unsigned page=0;page<EDITOR_PAGES_COUNT;++page)for(unsigned op=0;op<4;++op)for(unsigned tune=0;tune<5;++tune){
        uint8_t expected[1024];memcpy(expected,rdx_editor_draw(&view,page,op,tune),1024);
        gui_start(&view,page,op,tune);assert(gui_pending&&!gui_ready);
        assert(!gui_collect(0));assert(gui_pending);
        complete();assert(gui_collect(0));assert(gui_ready&&gui_last_core==1&&!gui_errors);
        for(unsigned y=0;y<120;++y)for(unsigned x=0;x<240;++x){
            unsigned sx=x*128u/240u,sy=y*64u/120u,at=y*240u+x;
            assert(((expected[(sy>>3)*128u+sx]>>(sy&7u))&1u)==((gui_reply.pixels[at>>3]>>(at&7u))&1u));
        }
        for(unsigned k=0;k<4;++k)assert(!strcmp(gui_reply.labels[k],rdx_editor_label(page,k)));
    }
    assert(gui_jobs==EDITOR_PAGES_COUNT*4u*5u);
    gui_start(&view,0,0,0);gui_request_hash^=1;complete();gui_collect(0);assert(gui_errors==1&&gui_failed&&!gui_ready);
    gui_failed=0;gui_start(&view,0,0,0);complete();gui_reply.pixels[0]^=1;gui_collect(0);assert(gui_errors==2&&!gui_ready);
    gui_failed=0;gui_start(&view,0,0,0);complete();++gui_reply.sequence;gui_collect(0);assert(gui_errors==3&&!gui_ready);
    gui_failed=0;gui_start(&view,0,0,0);now+=100;fail=true;
    assert(gui_collect(0)&&gui_failed&&!busy&&!enabled&&gui_errors==4);
    gui_start(&view,0,0,0);assert(gui_ready&&!gui_pending&&gui_last_core==0);
    return 0;
}
