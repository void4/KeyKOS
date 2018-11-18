 
/* Describe kernel page (CDA 1) */
/* This header file is like kernelp.h except for using uint64 */
/* in place of LLI. The header files should be unified */
/* after conversion. */

struct KernelPage {
   uint64 KP_LastCheckPointTOD;     /* System timer at last checkpoint */
   uint64 KP_RestartCheckPointTOD;  /* System timer of the checkpoint */
                                 /* used for the most recent restart */
   uint64 KP_RestartTOD;            /* System timer at last restart */
   uint64 KP_LastSetTOD;            /* Last time settime was done */
   uint64 KP_system_time;           /* Current system timer */
};
