/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*****************************************************************************
  ocrc.h

  Define standard order codes and return codes

  This is drawn from the online manual (<p3,keyconv>)
  but is here for the convenience of progrmmers.  The individual header
  files will no longer contain the Alleged key types of the object.
******************************************************************************/
#ifndef _H_ocrc
#define _H_ocrc

#include "keykos.h"
/*****************************************************************************
  Standard ordercodes
******************************************************************************/
/* DESTROY_OC is a standard self destruction request                         */
/* EXTEND_OC  signals the continuation of extended jump protocol             */
/* NODESTROY_OC requests a diminished rights key that ignores DESTROY_OC     */

#define DESTROY_OC               KT+4
#define EXTEND_OC                KT+5
#define NODESTROY_OC             KT+6

/*****************************************************************************
  Standard returncodes
******************************************************************************/
/* OK_RC is the standard no error return code                                */
/* NONPROMPTSB_RC  indicates that the PSB is not official                    */
/* NOSPACE_RC  indicates that some SB did not provide space                  */
/* DATAKEY_RC  The kernel recognized a dead key                              */
/* INVALIDOC_RC  The ordercode is invalid for this key                       */
/* FORMATERROR_RC The offered string format is invalid                       */
/* EXTEND_RC    signals the continuation of extended jump protocol           */
/* NONODES_RC   a segment keeper can get no nodes from its bank              */
/* NOPAGES_RC   a segment keeper can get no pages from its bank              */
/* NOSTORE_RC   a store attempt on a RO segment                              */
/* NOPARENT_RC  parent segment has been deleted                              */

#define OK_RC                    0
#define NONPROMPTSB_RC           1
#define NOSPACE_RC               2
#define DATAKEY_RC               KT+1
#define INVALIDOC_RC             KT+2
#define FORMATERROR_RC           KT+3
#define EXTEND_RC                KT+5
#define NONODES_RC               KT+6
#define NOPAGES_RC               KT+7
#define NOSTORE_RC               KT+8
#define NOPARENT_RC              KT+9


#endif
