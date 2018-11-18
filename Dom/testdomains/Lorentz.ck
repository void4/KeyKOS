/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "domain.h"
#include "keykos.h"

KEY   CALLER     = 2;
KEY   DOMKEY   = 3; // From factory
char title[]="Lorentz ";
typedef struct{int i1; int i2;} routine;
const routine ld = {0x81c3e008, 0xfd1a0000}; // retl; ldd [%o0], %f30
const routine st = {0x81c3e008, 0xfd3a0000}; // retl; std %f30, [%o0]
typedef void load(double *);

SINT32 factory(unsigned int sz)
{
  JUMPBUF;
   struct {double x, y, z, q;} rp;
  double p = 42, q=13;
  double x = 1, y = 1.3, z = 1.4;
  const double sig = 10, rho = 28, beta = 2.6667;
  double const dt = .0001;
  int j = 1000000;
  ((load*)&ld)(&p);
  while(--j){
    x += sig*(y - x) * dt;
    y +=(rho*x - y - x*z)*dt;
    z += (x*y - beta*z)*dt;}
  ((load*)&st)(&q);
  rp.x=x; rp.y=y; rp.z=z; rp.q=q;
  KFORK (CALLER, sz) STRUCTFROM(rp);
}

// See <http://www.geom.umn.edu/~worfolk/apps/Lorenz/> for nice Java demo!
