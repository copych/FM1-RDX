// Reface DX wire format: Yamaha reface Data List pp.6-9.
// Protocol baseline: user's RDX_before_UAC/RDX_Midi.h. No ESP runtime used.
#pragma once
static uint8_t sm_patch[150],sm_system[32];
static volatile uint8_t sm_patch_pending,sm_system_pending;
static uint8_t sm_patch_full;
static uint8_t sm_bulk[150],sm_bulk_open,sm_bulk_bad,sm_bulk_changed;
static uint32_t sm_bulk_ms,sm_session,sm_tx_ms;
static uint8_t sm_tx[1024];
static uint32_t sm_tx_read,sm_tx_write;
static void sm_reset(void){
    sm_patch_pending=sm_system_pending=0;sm_bulk_open=sm_bulk_bad=sm_bulk_changed=0;
    sm_tx_read=sm_tx_write=0;sm_session=plat_usb_session();
}
static void sm_audio(void){
    if(sm_system_pending){rdx_system_import(sm_system);sm_system_pending=0;}
    if(sm_patch_pending){
        if(sm_patch_full){rdx_patch_import(sm_patch);preset_home_requested=1;}
        else rdx_patch_edit(sm_patch);
        sm_patch_pending=0;
    }
}
static void sm_current(uint8_t* patch){
    uint32_t f=plat_lock();if(sm_patch_pending)memcpy(patch,sm_patch,150);else rdx_patch_export(patch);plat_unlock(f);
}
static void sm_current_system(uint8_t* data){
    uint32_t f=plat_lock();if(sm_system_pending)memcpy(data,sm_system,32);else rdx_system_export(data);plat_unlock(f);
}
static int sm_publish(const uint8_t* patch,int full){
    if(!rdx_patch_valid(patch))return 0;
    uint32_t f=plat_lock();memcpy(sm_patch,patch,150);sm_patch_full=(sm_patch_pending&&sm_patch_full)||full;sm_patch_pending=1;plat_unlock(f);return 1;
}
static int sm_publish_system(const uint8_t* data){
    if(data[1]>16||data[14]>1)return 0;
    uint32_t f=plat_lock();memcpy(sm_system,data,32);sm_system_pending=1;plat_unlock(f);return 1;
}
static void sm_queue(const uint8_t* frame,uint32_t n){
    if(sm_tx_read==sm_tx_write)sm_tx_read=sm_tx_write=0;
    if(n>sizeof(sm_tx)-sm_tx_write)return;
    memcpy(sm_tx+sm_tx_write,frame,n);sm_tx_write+=n;sm_tx_ms=plat_ms();
}
static void sm_block(uint8_t device,uint8_t hi,uint8_t mid,uint8_t lo,const uint8_t* data,uint32_t n){
    uint8_t frame[51]={0xf0,0x43,device,0x7f,0x1c,0,0,5,hi,mid,lo};
    if(n>38)return;
    frame[6]=(uint8_t)(n+4);if(n)memcpy(frame+11,data,n);
    uint32_t sum=0;for(uint32_t i=7;i<11+n;++i)sum+=frame[i];
    frame[11+n]=(uint8_t)((128u-(sum&127u))&127u);frame[12+n]=0xf7;sm_queue(frame,n+13);
}
static void sm_voice(uint8_t device,const uint8_t* patch){
    sm_block(device,0x0e,0x0f,0,0,0);sm_block(device,0x30,0,0,patch,38);
    for(uint8_t i=0;i<4;++i)sm_block(device,0x31,i,0,patch+38+28*i,28);
    sm_block(device,0x0f,0x0f,0,0,0);
}
void app_loop(void){
    uint32_t session=plat_usb_session(),now=plat_ms();
    if(session!=sm_session){uint32_t f=plat_lock();sm_reset();plat_unlock(f);}
    if(sm_bulk_open&&now-sm_bulk_ms>2000u)sm_bulk_open=0;
    if(sm_tx_read==sm_tx_write)return;
    if(now-sm_tx_ms>2000u){sm_tx_read=sm_tx_write=0;return;}
    uint32_t end=sm_tx_read;while(end<sm_tx_write&&sm_tx[end]!=0xf7)++end;
    if(end==sm_tx_write){sm_tx_read=sm_tx_write=0;return;}
    uint32_t n=end-sm_tx_read+1;
    if(plat_sysex_room()<((n+2u)/3u)*3u)return;
    if(!plat_sysex_send(sm_tx+sm_tx_read,n)){sm_tx_read=end+1;sm_tx_ms=now;}
}
int app_sysex(const uint8_t* p,uint32_t n){
    if(n>640u)return 0;
    if(n==4&&p[0]==0x7e&&p[2]==6&&p[3]==1){
        const uint8_t reply[]={0xf0,0x7e,0x7f,6,2,0x43,0,0x41,0x53,6,3,0,0,0x7f,0xf7};
        sm_queue(reply,sizeof(reply));return 1;
    }
    if(n<5||p[0]!=0x43||p[2]!=0x7f||p[3]!=0x1c)return 0;
    for(uint32_t i=0;i<n;++i)if(p[i]>127){if(sm_bulk_open)sm_bulk_bad=1;return 1;}
    uint8_t type=p[1]>>4,device=p[1]&15;
    if(type==0){
        if(n<11||p[6]!=5){if(sm_bulk_open)sm_bulk_bad=1;return 1;}
        uint32_t count=((uint32_t)p[4]<<7)|p[5],sum=0;
        if(count<4||count+7!=n){if(sm_bulk_open)sm_bulk_bad=1;return 1;}
        for(uint32_t i=6;i<n;++i)sum+=p[i];
        if(sum&127u){if(sm_bulk_open)sm_bulk_bad=1;return 1;}
        const uint8_t hi=p[7],mid=p[8],lo=p[9];const uint32_t size=count-4;
        if(hi==0x0e&&mid==0x0f&&!lo&&!size){sm_current(sm_bulk);sm_bulk_open=1;sm_bulk_bad=sm_bulk_changed=0;sm_bulk_ms=plat_ms();return 1;}
        if(hi==0x0f&&mid==0x0f&&!lo&&!size){
            if(sm_bulk_open&&!sm_bulk_bad&&sm_bulk_changed)sm_publish(sm_bulk,1);
            sm_bulk_open=0;return 1;
        }
        uint32_t offset=0;
        if(hi==0x30&&!mid&&!lo&&size==38)offset=0;
        else if(hi==0x31&&mid<4&&!lo&&size==28)offset=38+28*mid;
        else if(hi==0&&!mid&&!lo&&size==32&&!sm_bulk_open){sm_publish_system(p+10);return 1;}
        else {if(sm_bulk_open)sm_bulk_bad=1;return 1;}
        if(sm_bulk_open){memcpy(sm_bulk+offset,p+10,size);sm_bulk_changed=1;sm_bulk_ms=plat_ms();}
        else {uint8_t patch[150];sm_current(patch);memcpy(patch+offset,p+10,size);sm_publish(patch,1);}
        return 1;
    }
    if(n<8||p[4]!=5)return 1;
    const uint8_t hi=p[5],mid=p[6],lo=p[7];
    uint8_t patch[150],system[32];sm_current(patch);sm_current_system(system);
    // Reserve a complete largest response; never queue half of a voice dump.
    if((type==2||type==3)&&sm_tx_read!=sm_tx_write&&sizeof(sm_tx)-sm_tx_write<286u)return 1;
    uint8_t* data=0;uint32_t size=0;
    if(hi==0x30&&!mid){data=patch;size=38;}
    else if(hi==0x31&&mid<4){data=patch+38+28*mid;size=28;}
    else if(!hi&&!mid){data=system;size=32;}
    if(type==1){
        if(data&&n>8&&lo<size&&n-8<=size-lo){memcpy(data+lo,p+8,n-8);if(hi)sm_publish(patch,0);else sm_publish_system(system);}
    }else if(type==2&&n==8){
        if(!hi&&!mid&&!lo){sm_block(device,0,0,0,system,32);sm_voice(device,patch);}
        else if(hi==0x0e&&mid==0x0f&&!lo)sm_voice(device,patch);
        else if(data&&!lo)sm_block(device,hi,mid,0,data,size);
    }else if(type==3&&n==8){
        if(data&&lo<size){
            uint32_t length=!hi&&lo==2?4u:!hi&&lo==8?2u:1u;
            uint8_t frame[14]={0xf0,0x43,(uint8_t)(0x10|device),0x7f,0x1c,5,hi,mid,lo};
            memcpy(frame+9,data+lo,length);frame[9+length]=0xf7;sm_queue(frame,length+10);
        }else if(hi==0x0e&&mid==0x0f&&!lo)sm_voice(device,patch);
    }
    return 1;
}
