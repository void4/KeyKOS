/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykosasm.h"
#include "trap.h"
#include "stack.h"


!     KEY COMP=0;
!     KEY W   =1;
!     KEY CALLER  =2;
!     KEY DOMKEY  =3;
!     KEY PSB     =4;
!     KEY M       =5;
!     KEY DOMCRE  =6;
!     KEY K2      =13;  /* don't change this and k1 k0 */
!     KEY K1      =14;
!     KEY K0      =15;

#define    DC_DestroyMe  8
#define    SB_CreatePage 16
#define    SB_CreateNode 0
#define    SB_DestroyPage 17
#define    SB_DestroyNode 1
#define    Domain_GetMemory 3
#define    Domain_SwapMemory 35
#define    Node_Fetch 0
#define    Node_Swap 16
#define    Node_MakeNodeKey 35
#define    Domain_ReplaceMemory 74
#define    Domain_Swap 32
#define    Domain_SwapKey 48
#define    Domain_GetKey 16
#define    Domain_PutSPARCControl 202
#define    Domain_PutSPARCRegs 205
#define    Domain_ClearSPARCOldWindows 212
#define    Domain_Get 0
#define    Domain_MakeBusy 67
#define    Domain_SPARCCopyCaller 213
#define    K0 15
#define    K1 14
#define    K2 13

/*  cstart() - initialize C run-time environment */
/*              For WOMB factories with RO code (no static variables) */

/*  %i3 - oc */
/*  %i4 - ord */

/*   0000000-00FFFFF  CODE */
/*   0100000-010FFFF  STACK (NOMINALLY 1 PAGE */

	 .seg ".text"

         .type _start, #function
         .global _start
         .type factory, #function
         .global factory
         .ascii "FACTORY "
         .word   factory
         .word   title

_start:    /*  oc in %i3  ord in %i4  */
         sethi %hi(bootwomb), %l0
         orcc  %l0, %lo(bootwomb), %l0
         bnz    goboot     /*  defined weak, contents dont care */
         nop

         !"         KALL(DOMKEY,Domain_ClearSPARCOldWindows);"
OC(Domain_ClearSPARCOldWindows)
XB(0x00300000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         or    %g0, %g0, %i6  /* clear frame pointer */

         !"         KALL(PSB, SB_CreateNode) RCTO(ZZZ) KEYSTO (K0);"
OC(SB_CreateNode)
XB(0x00400000)
RC(ZZZ)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)

         tst  %o0
         bnz  die1
         nop
         !"         KALL(DOMKEY, Domain_GetMemory) KEYSTO (K1);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K0, Node_Swap+0) KEYSFROM (K1);"
OC(Node_Swap+0)
XB(0x80F0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K0, Node_MakeNodeKey) CHARFROM(db5,1) KEYSTO (K0);"
OC(Node_MakeNodeKey)
PS2(db5,1)
XB(0x04F00000)
NB(0x0080F000)
cjcc(0x08000000,&_jumpbuf)

         !"         KALL(DOMKEY, Domain_SwapMemory) KEYSFROM (K0);"
OC(Domain_SwapMemory)
XB(0x8030F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(PSB, SB_CreatePage) RCTO(ZZZ) KEYSTO (K1);  /* stack page */"
OC(SB_CreatePage)
XB(0x00400000)
RC(ZZZ)
NB(0x0880E000)
cjcc(0x00000000,&_jumpbuf)
  /* stack page */
         tst  %o0
         bnz  die2
         nop
         !"         KALL(K0, Node_Swap+1) KEYSFROM (K1); /* 1 PAGE AT 100000 */"
OC(Node_Swap+1)
XB(0x80F0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* 1 PAGE AT 100000 */

         set 0x100F80, %sp
         sethi %hi(stacksiz), %l0
         orcc %l0, %lo(stacksiz), %l0
         bz  onepage    /* undefined weak */
         nop
           ld   [%l0], %l0
           add  %l0, 4095, %l0
           srl  %l0, 12, %l0
           cmp  %l0, 1
           ble  onepage
           nop
              dec %l0

              !"              KALL(PSB, SB_CreateNode) RCTO(ZZZ) KEYSTO (K2); /* STACK NODE */"
OC(SB_CreateNode)
XB(0x00400000)
RC(ZZZ)
NB(0x0880D000)
cjcc(0x00000000,&_jumpbuf)
 /* STACK NODE */
              tst %o0
              bnz die3
              nop
              !"              KALL(K0, Node_Fetch+1) KEYSTO (K1);  /* GET OLD STACK PAGE */"
OC(Node_Fetch+1)
XB(0x00F00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)
  /* GET OLD STACK PAGE */
              !"              KALL(K2, Node_Swap+0) KEYSFROM (K1); /* PUT IN FIRST PAGE */"
OC(Node_Swap+0)
XB(0x80D0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* PUT IN FIRST PAGE */
              !"              KALL(K2, Node_MakeNodeKey) CHARFROM(db3,1) KEYSTO (K2);"
OC(Node_MakeNodeKey)
PS2(db3,1)
XB(0x04D00000)
NB(0x0080D000)
cjcc(0x08000000,&_jumpbuf)

              !"              KALL(K0, Node_Swap+1) KEYSFROM (K2); /* PUT IN NODE */"
OC(Node_Swap+1)
XB(0x80F0D000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* PUT IN NODE */

              set  Node_Swap+1,%l1      /* first ordercode */
stackloop:
                !"                KALL(PSB, SB_CreatePage) RCTO(ZZZ) KEYSTO (K1);"
OC(SB_CreatePage)
XB(0x00400000)
RC(ZZZ)
NB(0x0880E000)
cjcc(0x00000000,&_jumpbuf)

                tst %o0
                bnz die4
                nop
                !"                KALL(K2, %l1) KEYSFROM (K1);"
OCR(%l1)
XB(0x80D0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

                inc %l1
                add  %sp,4095,%sp /* add 1 page to start of stack for each page added */
                inc  %sp
                deccc %l0
                bnz  stackloop
                nop
onepage:
goboot:	
          or    %i3, %g0, %o0
          or    %i4, %g0, %o1  /* becomes i0,i1 after factory() does save */
          call  factory  /*  %i0 = oc  %i1 = ordinal */
          nop
/*  return */
          set  0, %o0

         .type exit, #function
         .global exit
exit:
/*  return code is in %o0   */
         or    %o0, 0, %l6   /*  save return value */

         !"         KALL(DOMKEY, Domain_GetMemory) KEYSTO (K0);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K0, Node_Fetch+0) KEYSTO (K1);"
OC(Node_Fetch+0)
XB(0x00F00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(DOMKEY, Domain_SwapMemory) KEYSFROM (K1);"
OC(Domain_SwapMemory)
XB(0x8030E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K0, Node_Fetch+1) KEYSTO (K1); /* STACK PAGE OR NODE */"
OC(Node_Fetch+1)
XB(0x00F00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)
 /* STACK PAGE OR NODE */

         sethi %hi(stacksiz), %l0
         orcc  %l0, %lo(stacksiz), %l0
         bz    onepage1   /* undefined means 1 */
         nop
           ld  [%l0], %l0
           add %l0, 4095, %l0
           srl %l0, 12, %l0
           cmp %l0, 1
           ble onepage1
           nop
             !"             KALL(K0, 1) KEYSTO (K2); /* GET NODE TO K2 */"
OC(1)
XB(0x00F00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)
 /* GET NODE TO K2 */
die4:
             set  Node_Fetch+0, %l1        /* starting slot */
stackloop1:
             !"             KALL(K2, %l1) KEYSTO (K1);"
OCR(%l1)
XB(0x00D00000)
NB(0x0080E000)
cjcc(0x00000000,&_jumpbuf)

             !"             KALL(PSB, SB_DestroyPage) KEYSFROM (K1) RCTO(ZZZ); /* SELL PAGE */"
OC(SB_DestroyPage)
XB(0x8040E000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)
 /* SELL PAGE */
             inc  %l1
             deccc %l0
             bnz  stackloop1
             nop
             !"             KALL(PSB, SB_DestroyNode) KEYSFROM (K2) RCTO(ZZZ); /* SELL NODE */"
OC(SB_DestroyNode)
XB(0x8040D000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)
 /* SELL NODE */
             ba  sellmem
             nop
die3:
onepage1:	
             !"             KALL(PSB, SB_DestroyPage) KEYSFROM (K1) RCTO(ZZZ); /* SELL PAGE */"
OC(SB_DestroyPage)
XB(0x8040E000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)
 /* SELL PAGE */
sellmem:
die2:
         !"         KALL(PSB, SB_DestroyNode) KEYSFROM (K0) RCTO(ZZZ); /* SELL mem root NODE */"
OC(SB_DestroyNode)
XB(0x8040F000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)
 /* SELL mem root NODE */
die1:	
/*         KALL(DOMCRE, DC_DestroyMe) CHARFROM(R14*4,4,R) KEYSFROM (CALLER, PSB); */
         OC(DC_DestroyMe)
         XB(0xCC602400)
         NB(0x00000000)
         PS2(22*4,4)    /*  %l6 */
         cjcc(0,0)

         ta  ST_KEYERROR

         .type repmem, #function
         .global repmem
/*  %o0=oc %o1=ordinal %o2=sa */
repmem:   /* all registers scratch */
         or    %o0, 0, %i3   /*  where lsfsim expects them */
         or    %o1, 0, %i4
         or    %o2, 0, %l2
         !"         KALL(DOMKEY, Domain_GetMemory) KEYSTO (K2); /* NODE */"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)
 /* NODE */
         sethi %hi(stacksiz), %l0
         orcc  %l0, %lo(stacksiz), %l0
         bz    onepage2
         nop
           ld    [%l0], %l0
           add   %l0, 4095, %l0
           srl   %l0, 12, %l0
           cmp   %l0, 1
           ble    onepage2
           nop
           !"           KALL(K2, Node_Fetch+1) KEYSTO (K2); /* GET NODE OF PAGES */"
OC(Node_Fetch+1)
XB(0x00D00000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)
 /* GET NODE OF PAGES */
           set   Node_Fetch+0, %l1
stackloop2:
           !"           KALL(K2, %l1) KEYSTO (K0);"
OCR(%l1)
XB(0x00D00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

           !"           KALL(PSB, SB_DestroyPage) KEYSFROM (K0) RCTO(ZZZ);"
OC(SB_DestroyPage)
XB(0x8040F000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)

           inc   %l1
           deccc  %l0
           bnz   stackloop2
           nop
           !"           KALL(PSB, SB_DestroyNode) KEYSFROM (K2) RCTO(ZZZ);"
OC(SB_DestroyNode)
XB(0x8040D000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)

           ba    swapmem          /*  get rid of my mem and put in new mem */
           nop
onepage2:
         !"         KALL(K2, Node_Fetch+1) KEYSTO (K0); /* GET PAGE */"
OC(Node_Fetch+1)
XB(0x00D00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)
 /* GET PAGE */
         !"         KALL(PSB, SB_DestroyPage) KEYSFROM (K0) RCTO(ZZZ);"
OC(SB_DestroyPage)
XB(0x8040F000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)

swapmem:
         !"         KALL(DOMKEY, Domain_GetMemory) KEYSTO (K2);"
OC(Domain_GetMemory)
XB(0x00300000)
NB(0x0080D000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K2, Node_Fetch+0) KEYSTO (K0);"
OC(Node_Fetch+0)
XB(0x00D00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(DOMKEY, Domain_SwapMemory) KEYSFROM (K0);"
OC(Domain_SwapMemory)
XB(0x8030F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(PSB, SB_DestroyNode) KEYSFROM (K2) RCTO(ZZZ);"
OC(SB_DestroyNode)
XB(0x8040D000)
RC(ZZZ)
NB(0x08000000)
cjcc(0x00000000,&_jumpbuf)


         set  0, %i0   /* create for lsfsim  */
         set  0, %i1   /* ignored for lsfsim */
         set  0, %i2   /* ignored for lsfsim */

/*       KALL(DOMKEY, DOMAINREPLACEMEMORY) CHARFROM(%l2*4,4,R) KEYSFROM (K1); */

         OC(Domain_ReplaceMemory)
         XB(0x8C30E000)
         NB(0x00000000)
         PS2(18*4,4)
         cjcc(0,0)

/*  CRASH(STR) */


         .type crash, #function
         .global crash
crash:
         ta    ST_KEYERROR
	 retl
	 nop

/*  FORK */

         .type fork1, #function
         .global fork1	
/* FORK1 LEAVES DOMAIN KEY IN K0  (IE SLOT 15) */
/* fork has no parameters */
fork1:
         save %sp, -SA(MINFRAME+128), %sp
         set  1,%i0
         ba   forkaa
         nop

         .type fork, #function
         .global fork

/*  USES KEY SLOTS 15,14,13 TO DO ITS JOB */
/*  SLOT 6 IS ASSUMED TO BE A DOMCRE, 3 A DOMKEY, 4 A SB */

/*********************************************************/
/*  use regs %l1 - %l5 for scratch.  return code in %i0  */
/*********************************************************/


fork:
         save %sp, -SA(MINFRAME+128), %sp
         set   0,%i0
forkaa:
         ta   ST_FLUSH_WINDOWS  /*  must force to stack */
         !"         KALL(DOMCRE, 0) KEYSFROM(, PSB) RCTO(ZZZ) KEYSTO (K2); /* NEW DOMAIN */"
OC(0)
XB(0x40600400)
RC(ZZZ)
NB(0x0880D000)
cjcc(0x00000000,&_jumpbuf)
 /* NEW DOMAIN */
         tst %o0
         bnz nospace
         nop
         !"         KALL(PSB, SB_CreateNode) RCTO(ZZZ) KEYSTO (K1); /* NEW MEMORY NODE */"
OC(SB_CreateNode)
XB(0x00400000)
RC(ZZZ)
NB(0x0880E000)
cjcc(0x00000000,&_jumpbuf)
 /* NEW MEMORY NODE */
         tst %o0
         bnz nospace
         nop
         !"         KALL(DOMKEY, Domain_Get+3) KEYSTO (K0);"
OC(Domain_Get+3)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K0, Node_Fetch+0) KEYSTO (K0); /* MEMORY (RO) */"
OC(Node_Fetch+0)
XB(0x00F00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)
 /* MEMORY (RO) */
         !"         KALL(K1, Node_Swap+0) KEYSFROM (K0); /* HERE */"
OC(Node_Swap+0)
XB(0x80E0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* HERE */
         !"         KALL(K1, Node_MakeNodeKey) CHARFROM(db5,1) KEYSTO (K1);"
OC(Node_MakeNodeKey)
PS2(db5,1)
XB(0x04E00000)
NB(0x0080E000)
cjcc(0x08000000,&_jumpbuf)

         !"         KALL(K2, Domain_Swap+3) KEYSFROM (K1);"
OC(Domain_Swap+3)
XB(0x80D0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(DOMKEY, Domain_Get+1) KEYSTO (K0);"
OC(Domain_Get+1)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K2, Domain_Swap+1) KEYSFROM (K0);"
OC(Domain_Swap+1)
XB(0x80D0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(DOMKEY, Domain_Get+2) KEYSTO (K0);"
OC(Domain_Get+2)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         !"         KALL(K2, Domain_Swap+2) KEYSFROM (K0); /* COPY MAJOR KEYS */"
OC(Domain_Swap+2)
XB(0x80D0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* COPY MAJOR KEYS */

         !"         KALL(K2, Domain_SPARCCopyCaller) CHARFROM(copyinst,8) RCTO(zzz);"
OC(Domain_SPARCCopyCaller)
PS2(copyinst,8)
XB(0x04D00000)
RC(zzz)
NB(0x08000000)
cjcc(0x08000000,&_jumpbuf)

                  /* make sure that copied domain EntryBlock accepts OC */

         sethi %hi(stacksiz), %l0
         orcc  %l0, %lo(stacksiz), %l0
         bz    onepage3
         nop
           ld [%l0], %l0
           add %l0, 4095, %l0
           srl %l0, 12, %l0
           cmp %l0, 1
           ble  onepage3
           nop
             set  0x100000, %l1   /* source of copy of stack */
             !"             KALL(PSB, SB_CreateNode) RCTO(ZZZ) KEYSTO (K0); /* A NODE */"
OC(SB_CreateNode)
XB(0x00400000)
RC(ZZZ)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)
 /* A NODE */
             tst %o0
             bnz nospace
             nop
             !"             KALL(K0, Node_MakeNodeKey) CHARFROM(db3,1) KEYSTO (K0);"
OC(Node_MakeNodeKey)
PS2(db3,1)
XB(0x04F00000)
NB(0x0080F000)
cjcc(0x08000000,&_jumpbuf)

             !"             KALL(K1, Node_Swap+1) KEYSFROM (K0);"
OC(Node_Swap+1)
XB(0x80E0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)

             set Node_Swap+0,%l2
stackloop3:
             !"             KALL(PSB, SB_CreatePage) RCTO(ZZZ) KEYSTO (K1);"
OC(SB_CreatePage)
XB(0x00400000)
RC(ZZZ)
NB(0x0880E000)
cjcc(0x00000000,&_jumpbuf)

             tst %o0
             bnz nospace
             nop
             !"             KALL(K0, %l2) KEYSFROM (K1);"
OCR(%l2)
XB(0x80F0E000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)


/*          KALL(K1,4096)  CHARFROM(%l1,4096);  */
             OC(4096)
             XB(0x04E00000)
             NB(0x00000000)
             or  %l1, 0,%o3
             set 4096, %o4
             cjcc(0,0)

             inc  %l2
             add  %l1, 4095, %l1
             inc  %l1
             deccc %l0
             bnz  stackloop3
             nop
             ba   startchild
             nop
onepage3:
         !"         KALL(PSB, SB_CreatePage) RCTO(ZZZ) KEYSTO (K0);"
OC(SB_CreatePage)
XB(0x00400000)
RC(ZZZ)
NB(0x0880F000)
cjcc(0x00000000,&_jumpbuf)

         tst %o0
         bnz nospace
         nop
         !"         KALL(K1, Node_Swap+1) KEYSFROM (K0); /* HERE */"
OC(Node_Swap+1)
XB(0x80E0F000)
NB(0x00000000)
cjcc(0x00000000,&_jumpbuf)
 /* HERE */
         !"         KALL(K0,4096) CHARFROM(0x100000,4096);"
OC(4096)
PS2(0x100000,4096)
XB(0x04F00000)
NB(0x00000000)
cjcc(0x08000000,&_jumpbuf)

startchild:
         !"         KALL(K2, Domain_PutSPARCControl) CHARFROM(clonepsw,12);"
OC(Domain_PutSPARCControl)
PS2(clonepsw,12)
XB(0x04D00000)
NB(0x00000000)
cjcc(0x08000000,&_jumpbuf)

         !"         KALL(K2, Domain_MakeBusy) KEYSTO (K0);"
OC(Domain_MakeBusy)
XB(0x00D00000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)

         tst %i0
         bnz  nostart
         nop

         !"         LDEXBL (K0,0);"
OC(0)
XB(0x00F00000)

         !"         FORKJUMP ();"
fj(0x00000000,&_jumpbuf)

nostart:   /* parent returns here */
         !"         KALL(DOMKEY, Domain_GetKey+K2) KEYSTO (K0); /* DOM IN 15 */"
OC(Domain_GetKey+K2)
XB(0x00300000)
NB(0x0080F000)
cjcc(0x00000000,&_jumpbuf)
 /* DOM IN 15 */
         set 1, %i0   /* fake proc id for parent  */
         ret
         restore
childst:  /* child starts here */
         mov %g0, %i0
         ret
         restore      /*  child will window underflow immediately */

nospace:
         set 3, %i0
         ret
         restore

	.type  setjmp, #function
        .global setjmp

setjmp:
	clr	[%o0]
	st 	%sp, [%o0 + 4]
	add	%o7, 8, %o1
	st	%o1, [%o0 + 8]
	st	%fp, [%o0 + 12]
	st	%i7, [%o0 + 16]

	std	%i0, [%o0 + 24]
	std	%i2, [%o0 + 32]
	std	%i4, [%o0 + 40]
	std	%i6, [%o0 + 48]

	std	%l0, [%o0 + 56]
	std	%l2, [%o0 + 64]
	std	%l4, [%o0 + 72]
	std	%l6, [%o0 + 80]

	retl	
	mov	%g0, %o0

	.type longjmp, #function
	.global longjmp
longjmp:
	ta	3

	ldd	[%o0 + 24], %i0
	ldd	[%o0 + 32], %i2
	ldd	[%o0 + 40], %i4
	ldd	[%o0 + 48], %i6

	ldd	[%o0 + 56], %l0
	ldd	[%o0 + 64], %l2
	ldd	[%o0 + 72], %l4
	ldd	[%o0 + 80], %l6

        ld	[%o0 + 4], %sp
	ld	[%o0 + 8], %o7
	ld	[%o0 + 12], %fp
	ld	[%o0 + 16], %i7

	tst	%o1
	bne	1f
	sub	%o7, 8, %o7
	mov	1, %o1
1:	retl
	mov	%o1, %o0


	.seg ".data"
         .weak bootwomb
         .weak stacksiz
clonepsw:
         .word  childst
         .word  childst+4
         .word  0x00000080

copyinst:
         .byte  0xFC    /* copy everything */
         .byte  0x03    /* slot for own domain key */
         .byte  0xEF    /*  Keys 0,1,2 4,5,6,7  */
         .byte  0xF8    /*  8,9,10,11,12 */
         .word  0x00000000   /* unused */

        db5:     .byte 5	
        db3:     .byte 3


