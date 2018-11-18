/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*********************************************************************************
   Definitions of RootNode structure
*********************************************************************************/

#ifndef _H_rootnode
#define _h_rootnode


#define ROOTSYS             0
#define ROOTBUILDER         1
#define ROOTADMIN           2
#define ROOTEARLYNODE       3
#define ROOTEARLYDOMAIN     4
#define ROOTFIRSTOBJ        5
#define ROOTFIRSTOBJDOMAIN  6
#define ROOTKERNELKEYS      7
#define ROOTKERNELMKEYS     8
#define ROOTKERNELDNODE     9
#define ROOTBWAITNODE      10
#define ROOTTARSEGNODE     11

#define EARLYSBT            0
#define EARLYPRIMESB        1
#define EARLYPRIMEMETER     2
#define EARLYDCC            3
#define EARLYCONSOLE        4
#define EARLYPRIMEMETERNODE 5
#define EARLYLSFSIMCODE     6
#define EARLYKERNELNODE     7
#define EARLYJOURNALNODE    8
#define EARLYBANKS          9
#define EARLYMETERS        10
#define EARLYPRIMERANGE    11
/* TEMP TEMP TEMP */
#define EARLYWOMBFACIL     14
#define EARLYPRIVNODE      15

#define METERSSYSTEM        0
#define METERSUSER          1
#define METERSBASIC         2
#define METERSAUX           3
#define METERSFACT          4
#define METERSINIT          5

#define BANKSAUX            0
#define BANKSFACT           1
#define BANKSINIT           2

#define EARLYDOMDCCDC       0
#define EARLYDOMDCC         1
#define EARLYDOMSBT         2
#define EARLYDOMSB          3
#define EARLYDOMSHIVA       4
#define EARLYDOMEMIG        5
#define EARLYDOMRSYNC       6
#define EARLYDOMCKPTDVR     7
#define EARLYDOMWKC         8

#define FIRSTKIDC           0
#define FIRSTFCC            1
/* FIRSTFC has recall builder rights */
#define FIRSTFC             2
#define FIRSTFCNORECALL     3
#define FIRSTFSF            4
#define FIRSTSNODEF         5
#define FIRSTTDOF           6
#define FIRSTPCSF           7
#define FIRSTOUTSEG         8
#define FIRSTVIRTUALZERO    9

#define FIRSTKIDCDC         0
#define FIRSTFCCDC          1
#define FIRSTFCDC           2

#define FIRSTFSFBUILD       4
#define FIRSTSNODEFBUILD    5
#define FIRSTTDOFBUILD      6
#define FIRSTPCSFBUILD      7

#define KERNELERROR         0
#define KERNELKIWAIT        1
#define KERNELDISCRIM       2
#define KERNELRETURNER      3
#define KERNELDKC           4
#define KERNELSYSTIMER      5
#define KERNELCALCLOCK      6
#define KERNELPEEK          7
#define KERNELCHARGESET     8
#define KERNELDEVALLOC      9
#define KERNELDOMAINTOOL   10

#define KERNELMKERRORLOG    0
#define KERNELMGETERRORLOG  1
#define KERNELMCDUMP        2
#define KERNELMCKERN1       3
#define KERNELMCKERN2       4

#define KERNELDUARTNODE     0
#define KERNELDETHERNODE    1
#define KERNELDSCSINODE     2
#define KERNELDATANODE      3

#define BWAIT1              0
#define BWAIT2              1

#define TARINIT             0
#define TARTEST             1

#endif
