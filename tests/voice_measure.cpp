#include "rdx_measure.h"
#include <cassert>
int main(){
    RdxVoiceMeasure m;uint32_t now=0xfffffff0u;m.start(now,777,0,COST_ROWS-1);
    for(uint32_t n=0;n<COST_ROWS;++n){
        assert(m.state==COST_ARMED&&m.target==n);m.begin(now);
        m.observe(now+100,8000+n*6000,1000+n*5000,6000,n,true);
        m.observe(now+2001,8001+n*6000,1001+n*5000,6001,n,true);
        m.observe(now+5002,8002+n*6000,1002+n*5000,6002,n,true);
        now+=5003;
    }
    assert(m.state==COST_DONE&&!m.active());
    uint32_t out[COST_SNAPSHOT_WORDS];m.snapshot(out);assert(out[3]==777&&out[4]==m.guard);
    for(uint32_t n=0;n<COST_ROWS;++n){auto& r=m.rows[n];assert(r.v[COST_BLOCKS]==3&&r.v[COST_STEADY_BLOCKS]==1&&r.v[COST_ACTUAL]==n);assert(r.v[COST_PAIRED_SYN]==1002+n*5000);}
    m.start(0,0);m.begin(0);m.observe(10,m.guard+1,100,100,0,true);assert(m.state==COST_LIMIT&&!m.active());
    m.start(0,0);m.target=3;m.begin(0);m.observe(10,100,50,20,2,true);assert(m.state==COST_MISMATCH);
    m.start(0,0);m.begin(0);m.observe(10,100,50,20,0,false);assert(m.state==COST_CHANGED);
    m.start(0,0);m.overload(4);assert(m.state==COST_LIMIT&&m.target==4);
    m.start(0,0,6,8);m.begin(0);m.overload(6);
    m.record(90000,70000,100,6,40,80000,100);
    assert(m.rows[6].v[COST_APP]==90000&&m.rows[6].v[COST_SYN]==70000&&m.rows[6].v[COST_TURN]==80000);
    assert(m.rows[6].v[COST_STEADY_BLOCKS]==0&&m.state==COST_LIMIT);
    m.start(0,0,8,9);assert(m.state==COST_CHANGED&&!m.active());
    m.start(0,0,8,7);assert(m.state==COST_CHANGED&&!m.active());
    return 0;
}
