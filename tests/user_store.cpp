#include "rdx_user_store.h"
#include <cassert>
#include <cstdio>
static bool valid(const uint8_t* p){for(unsigned i=0;i<150;++i)if(p[i]>127)return false;return true;}
int main(){
    RdxUserStore store;uint8_t patch[150]{},bank[RdxUserStore::bankBytes];
    assert(!store.usedMask()&&!store.patch(32));
    patch[0]='A';assert(store.encodeWith(0,patch,true,bank,valid));assert(!store.used(0));
    assert(store.decode(0,bank,sizeof bank,valid)&&store.used(0)&&store.trimmed(0)&&store.patch(0)[0]=='A');
    patch[0]='B';assert(store.encodeWith(15,patch,false,bank,valid));
    assert(store.decode(0,bank,sizeof bank,valid)&&store.used(15)&&!store.trimmed(15)&&store.patch(0)[0]=='A');
    patch[0]='Z';assert(store.encodeWith(31,patch,false,bank,valid)&&store.decode(1,bank,sizeof bank,valid));
    assert(store.usedMask()==0x80008001u&&store.patch(31)[0]=='Z');
    bank[16+15*150]=255;assert(!store.decode(1,bank,sizeof bank,valid)&&store.patch(31)[0]=='Z');
    bank[4]=2;assert(!store.decode(1,bank,sizeof bank,valid));
    assert(!store.decode(0,bank,sizeof bank-1,valid)&&!store.decode(2,bank,sizeof bank,valid));
    patch[1]=255;assert(!store.encodeWith(0,patch,false,bank,valid));
    assert(!store.encodeWith(32,patch,false,bank,valid));
    puts("PASS user-store format, separate banks, overwrite preservation, trim metadata, transactional rejection and bounds");
}
