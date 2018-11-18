/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */


#include <kktypes.h>
#include <keykos.h>
#include <node.h>
#include <domain.h>
#include <datacopy.h>
#include "setjmp.h"
#include <string.h>
#include "ocrc.h"

KEY comp     =0;
KEY dk       =1;  
KEY caller   =2;
KEY dom      =3;
KEY sb       =4;
KEY meter    =5;
KEY dc       =6;

KEY inseg    =7;
KEY outseg   =8;
KEY memory   =9;
KEY maindom  =10;
KEY mainkeep =11;

KEY k2       =13;
KEY k1       =14;
KEY k0       =15;

	void exit(),trap_function();

	char title[]="DataCopy";
	int  stacksize=4096;

#define FROMSLOT 2
#define TOSLOT 3

#define FROMWINDOW 4
#define FROM 0x00400000
#define TOWINDOW 6
#define TO   0x00600000

factory(oc,ord)
	UINT32 oc,ord;
{
        JUMPBUF;

        UINT32 errcode;
	jmp_buf jump_buffer; /* longjump buffer */
        struct Domain_SPARCRegistersAndControl drac;
	struct DatacopyArgs args;
	UINT32 from,to,length,froff,tooff,did;
	struct Node_KeyValues nkv;
	unsigned char byte;
	unsigned char *iptr,*optr;
        UINT32 rc;

	KALL(dom,Domain_GetMemory) KEYSTO(memory);  /* lss 5 node */
	KALL(dom,Domain_GetKey+dom) KEYSTO(maindom);
	
        if(!(rc=fork())) {  /* start my keeper */
		KALL(dom,Domain_MakeStart) KEYSTO(k0);
		KALL(maindom,Domain_SwapKeeper) KEYSFROM(k0) KEYSTO(mainkeep);
                LDEXBL (comp,0);
		for (;;) {
			LDENBL OCTO(oc) KEYSTO(,,dk,caller) STRUCTTO(drac);
			RETJUMP();

			if(4 == oc) exit();	
                        if(0 == oc) {
				LDEXBL(caller,errcode) STRUCTFROM(drac);
				continue;
			}
                        if(0x80000097 == oc) {
                                LDEXBL(mainkeep,oc) KEYSFROM(,,dk,caller);
                                continue;
                        }
                           
			errcode=oc;  /* KT+atc */
			if(drac.Control.TRAPEXT[0])
				errcode=drac.Control.TRAPEXT[0];  /* address */

			drac.Control.PC=(int)trap_function;
			drac.Control.NPC=drac.Control.PC+4;
			drac.Regs.o[0]=(int)jump_buffer;
/* the address of the jump buffer in the parent (who will do the longjmp) */
			drac.Regs.o[1]=errcode;
			LDEXBL(dk,Domain_ResetSPARCStuff) KEYSFROM(,,,caller)
				STRUCTFROM(drac);
		}
	}  /* end keeper */
        if(rc > 1) {
            exit(NOSPACE_RC);
        }

	nkv.StartSlot=FROMWINDOW;
	nkv.EndSlot=TOWINDOW;
	memset(&nkv.Slots[0],0,16);
	memset(&nkv.Slots[1],0,16);
        memset(&nkv.Slots[2],0,16);
	
        KALL(dom,Domain_MakeStart) KEYSTO(k0);
	LDEXBL (caller,0) KEYSFROM(k0);

	for(;;) {
		LDENBL OCTO(oc) STRUCTTO(args) KEYSTO(inseg,outseg,,caller);
		RETJUMP();

		if(DESTROY_OC == oc) break;
		if(KT == oc) {
			LDEXBL(caller,Datacopy_AKT);
			continue;
		}
		if(oc) {
			LDEXBL(caller,INVALIDOC_RC);
			continue;
		}
		from=args.fromoffset;
                froff=(from & 0xFFF00000) | (FROMSLOT<<4) | 0x0A;
		to=args.tooffset;
		tooff=(to & 0xFFF00000) | (TOSLOT<<4) | 0x02; 

		length=args.length;  /* convert to 32 bits.. temp temp */
		KC (memory,Node_Swap+FROMSLOT) KEYSFROM(inseg);
		KC (memory,Node_Swap+TOSLOT) KEYSFROM(outseg);
		memcpy(&nkv.Slots[0].Byte[12],&froff,4);
		memcpy(&nkv.Slots[2].Byte[12],&tooff,4);
		KC (memory,Node_WriteData) STRUCTFROM(nkv,56);
		iptr=(unsigned char *)((from & 0xFFFFF)+FROM); 

		optr=(unsigned char *)((to & 0xFFFFF) + TO);

		if(errcode=setjmp(jump_buffer)) {
/* errcode contains the fault address or some other error */
			if((errcode < FROM) || (errcode > KT)) {
                                KC(comp,errcode) RCTO(rc);
                                crash();
                        }
			if(errcode < TO) {
                                did=errcode-(UINT32)iptr;
                		args.fromoffset += did;
               			args.tooffset += did;
                		args.length -= did;
				LDEXBL (caller,2) STRUCTFROM(args);
			}
			else {
                                did=errcode-(UINT32)optr;
                		args.fromoffset += did;
               			args.tooffset += did;
                		args.length -= did;
				LDEXBL (caller,3) STRUCTFROM(args);
			}
			continue;
		}	

		did=length;
		if(did > 0x00100000) did = 0x00100000;
/* a fault here will look like a toseg fault because not reset setjmp */
		memcpy(optr,iptr,did);
                args.fromoffset += did;
                args.tooffset += did;
                args.length=0;
		if(did < length) {
			LDEXBL (caller,Datacopy_Failed) STRUCTFROM(args);
		}
		else {
			LDEXBL (caller,0) STRUCTFROM(args);
		}
        }
/* kill keeper and exit */
	KC (dom,Domain_GetKeeper) KEYSTO(k0);
	KFORK (k0,4); 
}

void trap_function(j,k)
	jmp_buf j;
	UINT32 k;
{
	longjmp(j,k);
}
