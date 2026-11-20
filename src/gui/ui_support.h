#pragma once
#include "../core/rdx_platform.h"
#include <stdarg.h>
template<class T> inline T ui_min(T a,T b){return a<b?a:b;}
inline int ui_abs(int x){return x<0?-x:x;}
inline int ui_map(int x,int a,int b,int c,int d){return b==a?c:(x-a)*(d-c)/(b-a)+c;}
inline unsigned ui_strlen(const char* s){unsigned n=0;while(s[n])++n;return n;}
// The original screens need integer formatting only; do not pull libc printf
// or software-double formatting into a freestanding firmware.
inline int ui_snprintf(char* out,unsigned capacity,const char* format,...) {
    va_list args;va_start(args,format);unsigned n=0;
    auto put=[&](char c){if(n+1<capacity)out[n]=c;++n;};
    for(const char* p=format;*p;++p) {
        if(*p!='%'){put(*p);continue;}++p;
        bool plus=false,zero=false;if(*p=='+'){plus=true;++p;}if(*p=='0'){zero=true;++p;}
        unsigned width=0;while(*p>='0'&&*p<='9'){width=width*10+(*p-'0');++p;}
        if(*p=='s'){const char* s=va_arg(args,const char*);while(*s)put(*s++);}
        else if(*p=='c')put((char)va_arg(args,int));
        else if(*p=='d'||*p=='u') {
            int v=va_arg(args,int);bool neg=*p=='d'&&v<0;unsigned u=neg?(unsigned)(-v):(unsigned)v;
            char digits[12];unsigned count=0;do{digits[count++]=(char)('0'+u%10);u/=10;}while(u&&count<12);
            if(neg)put('-');else if(plus)put('+');
            while(width>count){put(zero?'0':' ');--width;}while(count)put(digits[--count]);
        } else if(*p=='%')put('%');
    }
    va_end(args);if(capacity)out[n<capacity?n:capacity-1]=0;return (int)n;
}
