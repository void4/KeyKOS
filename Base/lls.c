#include <stdio.h>
typedef long long l;
l __ashldi3(l t, int y){
union {l L; struct{int hi; unsigned int lo;} D;} f;
f.L = t;
if(y >= 32) {y -= 32; f.D.hi = f.D.lo; f.D.lo = 0;}
f.D.hi = (f.D.hi << y) | (f.D.lo >> (32 - y));
f.D.lo = f.D.lo << y;
  return f.L;}

