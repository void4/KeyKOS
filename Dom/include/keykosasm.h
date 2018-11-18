/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "trap.h"

#define KT    0x80000000u

#define ORDERCODE_REG           %o0
#define EXITBLK_REG             %o1
#define ENTRYBLK_REG            %o2
#define PASS_STR_REG            %o3
#define PASS_STR_LEN_REG        %o4
#define REC_STR_REG             %o5
#define REC_STR_LEN_REG         %o4
#define REC_STR_MAXLEN_REG      %g1
#define RETCODE_REG             %o0
#define DATA_BYTE_REG           %o1
!
! The following definitions describe the possible JUMP types in the
! exit block JUMP type field (bits 25-24).
!
#define CT_CALL                 0x01000000
#define CT_RETURN               0x02000000
#define CT_FORK                 0x03000000

! to use scaffold change cjcc macro from "ta"  to call cjcc1,nop
 
#define cjcc(a,b) \
      set   CT_CALL, %l7;   \
      or    EXITBLK_REG, %l7, EXITBLK_REG; \
      ta    ST_KEYJUMP       ! CALL
#define rj(a,b) \
     set   CT_RETURN, %l7; \
     or    EXITBLK_REG, %l7, EXITBLK_REG; \
     ta    ST_KEYJUMP       ! CALL
#define fj(a,b) \
     set   CT_FORK, %l7;   \
     or    EXITBLK_REG, %l7, EXITBLK_REG; \
     ta    ST_KEYJUMP       ! CALL
#define PS2(a,b) \
     set   (a), PASS_STR_REG; \
     set   (b), PASS_STR_LEN_REG
#define RS2(a,b) \
     set   (a), REC_STR_REG;      \
     set   (b), REC_STR_MAXLEN_REG 
#define RS3(a,b,c) \
     set   (a), REC_STR_REG; \
     set   (b), REC_STR_MAXLEN_REG
#define RC(a)              ! Return code in R2 
#define OC(a) \
     set   (a), ORDERCODE_REG
#define OCR(a) \
     mov a, ORDERCODE_REG	! a is a register
#define DB(a)              ! Databyte in R3
#define XB(a) \
     set (a), EXITBLK_REG
#define NB(a) \
     set (a), ENTRYBLK_REG
#define KP(a,b,c,d)
#define KRN(a,b,c,d)
