#pragma once
#include "core/rdx_platform.h"
// Versioned payload inside the platform's CRC-checked A/B object. No compiler
// structs/enums are serialized. Two banks leave all factory patches intact.
class RdxUserStore {
public:
    enum {banks=2,perBank=16,slots=banks*perBank,patchBytes=150,headerBytes=16,bankBytes=headerBytes+perBank*patchBytes};
    typedef bool (*Validator)(const uint8_t*);
    void reset(){memset(data_,0,sizeof data_);memset(valid_,0,sizeof valid_);memset(trim_,0,sizeof trim_);}
    bool used(uint32_t slot)const{return slot<slots&&(valid_[slot/perBank]&(1u<<(slot%perBank)));}
    bool trimmed(uint32_t slot)const{return used(slot)&&(trim_[slot/perBank]&(1u<<(slot%perBank)));}
    const uint8_t* patch(uint32_t slot)const{return used(slot)?data_[slot]:nullptr;}
    uint32_t usedMask()const{return valid_[0]|((uint32_t)valid_[1]<<16);}
    bool decode(uint32_t bank,const uint8_t* in,uint32_t len,Validator valid){
        if(bank>=banks||len!=bankBytes||!in||!valid)return false;
        if(in[0]!='R'||in[1]!='D'||in[2]!='U'||in[3]!='1'||in[4]!=1||in[5]!=perBank||in[6]!=patchBytes||in[7])return false;
        for(uint32_t i=12;i<headerBytes;++i)if(in[i])return false;
        const uint16_t mask=(uint16_t)(in[8]|(in[9]<<8)),trim=(uint16_t)(in[10]|(in[11]<<8));
        if(trim&~mask)return false;
        // Validate the complete object before publishing any cache changes.
        for(uint32_t i=0;i<perBank;++i)if((mask&(1u<<i))&&!valid(in+headerBytes+i*patchBytes))return false;
        memcpy(data_[bank*perBank],in+headerBytes,perBank*patchBytes);
        valid_[bank]=mask;trim_[bank]=trim;return true;
    }
    bool encodeWith(uint32_t slot,const uint8_t* patch,bool trim,uint8_t* out,Validator valid)const{
        if(slot>=slots||!patch||!out||!valid||!valid(patch))return false;
        const uint32_t bank=slot/perBank,at=slot%perBank;
        const uint16_t mask=(uint16_t)(valid_[bank]|(1u<<at));
        const uint16_t t=(uint16_t)((trim_[bank]&~(1u<<at))|(trim?(1u<<at):0));
        memset(out,0,headerBytes);out[0]='R';out[1]='D';out[2]='U';out[3]='1';out[4]=1;out[5]=perBank;out[6]=patchBytes;
        out[8]=(uint8_t)mask;out[9]=(uint8_t)(mask>>8);out[10]=(uint8_t)t;out[11]=(uint8_t)(t>>8);
        memcpy(out+headerBytes,data_[bank*perBank],perBank*patchBytes);
        memcpy(out+headerBytes+at*patchBytes,patch,patchBytes);return true;
    }
private:
    uint8_t data_[slots][patchBytes]{};
    uint16_t valid_[banks]{},trim_[banks]{};
};
