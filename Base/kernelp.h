/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
/* Describe kernel page (CDA 1) */
 
struct KernelPage {
   uint64 KP_LastCheckPointTOD;     /* System timer at last checkpoint */
   uint64 KP_RestartCheckPointTOD;  /* System timer of the checkpoint */
                                 /* used for the most recent restart */
   uint64 KP_RestartTOD;            /* System timer at last restart */
   uint64 KP_LastSetTOD;            /* Last time settime was done */
   uint64 KP_system_time;           /* Current system timer */
};
