console on
# ###############################################################
# Command File to build Pacific.
# All rights Reserved 
# ###############################################################
console off
kc rootnode 3 () (,earlynode)
kc rootnode 4 () (,earlydomainkeys)
kc rootnode 5 () (,firstobjects)
kc rootnode 6 () (,firstobjectsbuilders)
kc rootnode 7 () (,kernelkeys)
kc rootnode 8 () (,kernelmaintenancekeys)
kc rootnode 9 () (,kerneldriverkeys)
kc rootnode 10 () (,admin/bwaitnode)
kc rootnode 11 () (,admin/tarsegnode)

kc earlynode 0 () (,sys/sbt)
kc earlynode 1 () (,admin/primespacebank)
kc earlynode 2 () (,admin/primemeter)
kc earlynode 3 () (,sys/dcc)
kc earlynode 4 () (,admin/console)
kc earlynode 5 () (,admin/primemeternode)
kc earlynode 6 () (,sys/lsfsimcode)
kc earlynode 7 () (,admin/kernelnode)
kc earlynode 8 () (,admin/journalnode)
kc earlynode 9 () (,admin/earlybanks)
kc earlynode 10 () (,admin/earlymeters)

kc admin/earlymeters 0 () (,admin/systemmeternode)
kc admin/systemmeternode 34 () (,admin/systemmeter)
kc admin/earlymeters 1 () (,admin/usermeternode)
kc admin/usermeternode 34 () (,admin/usermeter)

#
# this command file runs on METERSINIT(earlymeters,5) under SYSTEMMETER(earlymeters,0)
#

kc earlydomainkeys 0 () (,admin/dccdcdom)
kc earlydomainkeys 1 () (,admin/dccdom)
kc earlydomainkeys 2 () (,admin/sbdcdom)
kc earlydomainkeys 3 () (,admin/sbdom)
kc earlydomainkeys 4 () (,admin/shivadom)
kc earlydomainkeys 5 () (,admin/emigdom)
kc earlydomainkeys 6 () (,admin/rsyncdom)
kc earlydomainkeys 7 () (,admin/ckptdrvdom)
kc earlydomainkeys 8 () (,admin/wkcdom)

kc firstobjects 0 () (,admin/kidc)
kc firstobjects 1 () (,sys/fcc)
kc firstobjects 2 () (,admin/fc-recall)
kc firstobjects 3 () (,sys/factoryc)
kc firstobjects 4 () (,sys/fsf)
kc firstobjects 5 () (,sys/snodef)
kc firstobjects 6 () (,sys/tdof)
kc firstobjects 7 () (,sys/pcsf)
kc firstobjects 8 () (,admin/initoutseg)
kc firstobjects 9 () (,admin/virtualzeroseg)

kc firstobjectsbuilders 0 () (,builder/kidcdc)
kc firstobjectsbuilders 1 () (,builder/fccdc)
kc firstobjectsbuilders 2 () (,builder/fcdc)
kc firstobjectsbuilders 4 () (,builder/fsf)
kc firstobjectsbuilders 5 () (,builder/snodef)
kc firstobjectsbuilders 6 () (,builder/todf)
kc firstobjectsbuilders 7 () (,builder/pcsf)

kc kernelkeys 0 () (,admin/error)
kc kernelkeys 1 () (,admin/kiwait)
kc kernelkeys 2 () (,sys/discrim)
kc kernelkeys 3 () (,sys/returner)
kc kernelkeys 4 () (,sys/dkc)
kc kernelkeys 5 () (,admin/systimer)
kc kernelkeys 6 () (,admin/calclock)
kc kernelkeys 7 () (,admin/cdapeek)
kc kernelkeys 8 () (,admin/chargeset)
kc kernelkeys 9 () (,admin/devalloc)
kc kernelkeys 10 () (,admin/domaintool)
kc kernelkeys 11 () (,admin/peek)
kc kernelkeys 14 () (,admin/keybits)

kc sys/returner 0 (,rootnode) (,admin/rootnode)

kc kernelmaintenancekeys 0 () (,admin/kerrorlog)
kc kernelmaintenancekeys 1 () (,admin/geterrorlog)

kc kerneldriverkeys 0 () (,admin/uartnode)
kc kerneldriverkeys 1 () (,admin/ethernode)
kc kerneldriverkeys 2 () (,admin/scsinode)
kc kerneldriverkeys 3 () (,admin/atanode)

kc admin/tarsegnode 1 () (,admin/testtarseg)

kc rootnode 16+0 (,sys/)
kc rootnode 16+1 (,builder/)
kc rootnode 16+2 (,admin/)

kc sys/ 0 (%x0000006f) (,user/sys/)
kc sys/returner 0 (,builder/,admin/) (,user/builder/,user/admin/)
#kc sys/returner 0 (,sys/lsfsimcode) (,user/lsfsimcode)
#kt user/lsfsimcode

# temporary error key as keeper for use by cfact
kc sys/returner 0 (,admin/error) (,vdk)

cfact mkeeperf  initdir/mkeeperf 0x0206 h15=admin/console
kc sys/returner 0 (,mkeeperf,mkeeperf.builder) (,sys/mkeeperf,builder/mkeeperf)

cfact datacopyf initdir/datacopy 0x0164
kc sys/returner 0 (,datacopyf,datacopyf.builder) (,sys/datacopyf,builder/datacopyf)

kc admin/journalnode 0 () (,journalpage)
kc journalpage 0 () (,journalpagero)
cfact clockf initdir/clockc 0x112 h0=admin/systimer h1=admin/calclock s2=journalpagero
kc sys/returner 0 (,clockf,clockf.builder) (,sys/clockf,builder/clockf)
kc clockf 0 (,sb,m,sb) (,admin/clock)
kc admin/clock 1003 () (,sys/clock)

# ###################################################### 
kc sys/clock 5 () (%a)
# ######################################################

cfact callsegf initdir/callsegc 0x120d stack=8192
kc sys/returner 0 (,callsegf,callsegf.builder) (,sys/callsegf,builder/callsegf)

cfact vdk2rcf  initdir/vdk2rc 0x20a0d s0=sys/discrim f1=sys/fsf f2=sys/callsegf h15=admin/console
kc sys/returner 0 (,vdk2rcf,vdk2rcf.builder) (,sys/vdk2rcf,builder/vdk2rcf)

cfact vdkf initdir/vdkf 0x50a0d
kc sys/returner 0 (,vdkf,vdkf.builder) (,sys/vdkf,builder/vdkf)

#
# here is a domain keeper that prints out the fault information and
# leaves the domain busted
#
kc sys/vdkf kt+5 (,sb,m,sb) (,,,,k)
kc k 2 (,admin/console) (,vdk)   # type is VDKF_CreateCCK

# These are better choices for production systems
# now would be a good time to set up a vdk for cfact
# kc sys/rcf 1 (,sb,m,sb) (,admin/vdkrc)
# kc sys/vdk2rcf kt+5 (,sb,m,sb) (,,,,k)
# kc k 0 (,admin/vdkrc) (,vdk)  # used by cfact
# kc sys/returner 0 (,vdk) (,admin/vdk)  # here for maintenance
# 
# OR
#
# kc admin/uartnode 1 () (,portb)
# kc sys/vdk2rcf kt+5 (,sb,m,sb) (,,,,k)
# kc k 3 (,portb) (,vdk)  # used by cfact
# kc sys/returner 0 (,vdk) (,admin/vdk) # here for maintenance

kc sys/tdof 1 (,sb,m,sb) (,admin/locallud)

cfact recepf initdir/recepc 0x23e h2=admin/locallud
kc sys/returner 0 (,recepf,recepf.builder) (,admin/localrecepf,builder/localrecepf)

cfact editf initdir/gnedit 0x313 stack=8192 f1=sys/callsegf
kc sys/returner 0 (,editf,editf.builder) (,sys/editf,builder/editf)

cfact siaf initdir/siac 0x126 h1=sys/sbt f2=sys/fsf
kc sys/returner 0 (,siaf,siaf.builder) (,sys/siaf,builder/siaf)

cfact mbwait2f initdir/mbwait2c 0x525 f0=sys/snodef s2=sys/returner f3=sys/fsf s4=sys/discrim h5=sys/clock s6=journalpagero
kc sys/returner 0 (,mbwait2f,mbwait2f.builder) (,sys/mbwait2f,builder/mbwait2f)
kc admin/bwaitnode 1 () (,bwait1)
kc sys/mbwait2f 0 (,sb,m,bwait1) (,admin/mbwait1,admin/mbwait1dom)

cfact waitf initdir/wait 0x325 h0=admin/mbwait1 h2=sys/clock
kc sys/returner 0 (,waitf,waitf.builder) (,sys/waitf,builder/waitf)

cfact forkf initdir/forkfc 0x3d
kc sys/returner 0 (,forkf,forkf.builder) (,sys/forkf,builder/forkf)

cfact bscrf initdir/bscrf 0x1b
kc sys/returner 0 (,bscrf,bscrf.builder) (,sys/bscrf,builder/bscrf)

cfact demo2f initdir/demo2 0x998 h0=sys/clock s1=journalpagero f2=sys/snodef f3=sys/fsf
kc sys/returner 0 (,demo2f,demo2f.builder) (,admin/demo2f,builder/demo2f)

cfact vcsf  initdir/vcsf 0x1b0d s1=admin/virtualzeroseg s2=sys/discrim h15=admin/console
kc sys/returner 0 (,vcsf,vcsf.builder) (,sys/vcsf,builder/vcsf)

cfact bsloadf initdir/bsloadf 0x111
kc sys/returner 0 (,bsloadf,bsloadf.builder) (,sys/bsloadf,builder/bsloadf)

cfact pcskeepf initdir/pcskeepf 0x223 f1=sys/waitf h2=sys/clock
kc sys/returner 0 (,pcskeepf,pcskeepf.builder) (,sys/pcskeepf,builder/pcskeepf)

cfact tssf initdir/tssf 0x90E s1=sys/discrim s2=sys/returner f3=sys/snodef f4=sys/fsf h15=admin/console
kc sys/returner 0 (,tssf,tssf.builder) (,sys/tssf,builder/tssf)

cfact switcherf initdir/switcherf 0x1090D s1=sys/dkc f2=sys/snodef f3=sys/tssf h15=admin/console
kc sys/returner 0 (,switcherf,switcherf.builder) (,sys/switcherf,builder/switcherf)

cfact pcsf initdir/pcs 0x23 f2=sys/tdof f6=sys/fsf f4=sys/pcskeepf
kc sys/returner 0 (,pcsf,pcsf.builder) (,sys/pcsf,builder/pcsf)

cfact lclrecepf initdir/lclrecepf 0x023E f1=sys/tssf
kc sys/returner 0 (,lclrecepf,lclrecepf.builder) (,sys/lclrecepf,builder/lclrecepf)

cfact lclauthf initdir/lclauthf 0x0165 f0=sys/tdof f1=sys/tssf
kc sys/returner 0 (,lclauthf,lclauthf.builder) (,sys/lclauthf,builder/lclauthf)

#kc sys/returner 0 (,initdir/root/) (,sys/unixroot/)
kc initdir/root.zip 3 () (,sys/unixroot)
kc sys/returner 0 (,initdir/ld.so.1) (,sys/ld.so.1)

cfact devttyf initdir/devttyf 0x269 h15=admin/console
kc sys/returner 0 (,devttyf,devttyf.builder) (,sys/devttyf,builder/devttyf)

cfact devconsf initdir/devconsf 0x469 s0=sys/discrim h15=admin/console
kc sys/returner 0 (,devconsf,devconsf.builder) (,sys/devconsf,builder/devconsf)

#cfact ukeeperf initdir/ukeeperf 0x166 h1=sys/unixroot/ f4=sys/fsf f5=sys/waitf h6=sys/clock f7=sys/tdof h14=admin/error h15=admin/console
cfact ukeeperf initdir/ukeeperf 0x166 s1=sys/unixroot f4=sys/fsf f5=sys/waitf h6=sys/clock f7=sys/tdof f8=sys/devconsf h14=admin/error h15=admin/console
kc sys/returner 0 (,ukeeperf,ukeeperf.builder) (,sys/ukeeperf,builder/ukeeperf)
kc sys/returner 0 (,initdir/uwrapper) (,sys/uwrapper)

cfact cswitcherf initdir/cswitcherf 0x16A s1=sys/dkc f2=sys/tdof f3=sys/tssf f4=sys/siaf f5=sys/pcsf f6=sys/mkeeperf h7=sys/clock s8=sys/discrim h15=admin/console
kc sys/returner 0 (,cswitcherf,cswitcherf.builder) (,sys/cswitcherf,builder/cswitcherf)

# add some lud keys to the local lud

#kc sb 0 () (,demoludnode)
#kc demoludnode 16+0 (,sb)
#kc demoludnode 16+1 (,m)
#kc demoludnode 16+2 (,admin/demo2f)

#kc admin/locallud 5 (%x0564656d6f32034e64656d6f322020202020202020202020,demoludnode)   # demo2,demo2

#kc sb 0 () (,systoolludnode)
#kc systoolludnode 16+0 (,sb)
#kc systoolludnode 16+1 (,m)
#kc systoolludnode 16+2 (,sys/pcsf)
#kc systoolludnode 16+3 (,user/)

#kc admin/locallud 5 (%x07737973746f6f6c044e3466743637756a202020202020202020,systoolludnode) # systool,4ft67uj

#kc admin/localrecepf kt+5 (,sb,m,sb) (,,,,k)
#kfork k 1 (,,,admin/console)

#
# at this point we would introduce the scheduler based on usermeter and make
# meters for the users.  schduler will have Primemeter for baseline and 
# systemmeter as overhead
#

kc admin/bwaitnode 2 () (,bwait2)
cfact pschedf initdir/pschedf 0x168  h0=bwait2 h1=admin/systimer s2=journalpagero f3=sys/snodef h15=admin/console
kc user/sys/returner 0 (,pschedf,pschedf.builder) (,sys/pschedf,builder/pschedf)
kc pschedf 0 (,sb,m,sb,admin/usermeternode,admin/systemmeternode) (,admin/pschedadmin,admin/pschednotify,admin/pschedstatus)
# we also  make banks for each user based on the prime bank
# we are running on BANKSINIT.  Later a JoinUser will make a bank for each
# user joined.
#
kc admin/pschedadmin 0 () (,admin/meters/demometer,admin/meters/demochange,admin/meters/demoid)
kc admin/pschedadmin 0 () (,admin/meters/systoolmeter,admin/meters/systoolchange,admin/meters/systoolid)
kc admin/pschedadmin 0 () (,admin/meters/alanmeter,admin/meters/alanchange,admin/meters/alanid)
kc admin/pschedadmin 0 () (,admin/meters/normmeter,admin/meters/normchange,admin/meters/normid)

newbank  demobank  admin/primespacebank
newbank  systoolbank admin/primespacebank
newbank  alanbank admin/primespacebank
newbank  normbank admin/primespacebank

# JOIN DEMO 
kc sys/lclauthf 0 (,sb,m,sb) (,admin/demoauth,admin/demoauthm)
kc admin/demoauthm 7 (%ademo2) # password
kc admin/demoauthm 5 (%x04,admin/demo2f)

kc sb 0 () (,demoludnode)
kc demoludnode 16 (,admin/demoauth)
kc demoludnode 17 (,demobank)
kc demoludnode 18 (,admin/meters/demometer)
kc demoludnode 19 (,demobank)
kc admin/locallud 5 (%aademo2,demoludnode)

# JOIN SYSTOOL 
# for testing, would be the meter contract change request key
kc user/sys/returner 0 (,admin/meters/systoolchange) (,systooluser/systoolmeterchange)
#
kc user/sys/returner 0 (,user/sys/) (,systooluser/sys/)  # 
kc user/sys/returner 0 (,user/admin/) (,systooluser/admin/) # give power keys
kc user/sys/returner 0 (,user/builder/) (,systooluser/builder/)
kc user/sys/returner 0 (,systooluser/) (,admin/systooluser/)

# The Authenticator must be as prompt as the receptionist so it does not use the user meter
kc sys/lclauthf 0 (,sb,m,sb) (,admin/systoolauth,admin/systoolauthm)
# 
#
kc admin/systoolauthm 7 (%a4ft67uj) # password

# The context switcher should run on the users meter. The user meter must run
# long enough for this extended jump to complete.
# the context directory also runs on the user meter

kc sys/tdof 1 (,sb,admin/meters/systoolmeter,sb) (,admin/systoolcontext)
kc sys/cswitcherf kt+5 (,systoolbank,admin/meters/systoolmeter,systoolbank) (,,,,k)  # should use a prompt bank
kc k 0 (,systooluser/,admin/systoolcontext) (,systoolzmk)
kc admin/systoolauthm 5 (%x00,systoolzmk)

kc sb 0 () (,systoolludnode)
kc systoolludnode 16 (,admin/systoolauth)
kc systoolludnode 17 (,systoolbank)
kc systoolludnode 18 (,admin/meters/systoolmeter)
kc systoolludnode 19 (,systoolbank)
kc admin/locallud 5 (%aasystool,systoolludnode)

# JOIN ALAN - use context switcher
# for testing, would be the meter contract change request key
kc user/sys/returner 0 (,admin/meters/alanchange) (,alanuser/alanmeterchange)
#
kc user/sys/returner 0 (,user/sys/) (,alanuser/sys/)  # alan directory for zapper case
kc user/sys/returner 0 (,user/admin/) (,alanuser/admin/) # give power keys
kc user/sys/returner 0 (,alanuser/) (,admin/alanuser/)

kc sys/lclauthf 0 (,sb,m,sb) (,admin/alanauth,admin/alanauthm)
kc admin/alanauthm 7 (%aalan) # password

kc sys/tdof 1 (,sb,admin/meters/alanmeter,sb) (,admin/alancontext)
kc sys/cswitcherf kt+5 (,alanbank,admin/meters/alanmeter,alanbank) (,,,,k)  # should use a prompt bank
kc k 0 (,alanuser/,admin/alancontext) (,alanzmk)
kc admin/alanauthm 5 (%x00,alanzmk)  # connection type 0 - zapper


kc sb 0 () (,alanludnode)
kc alanludnode 16 (,admin/alanauth)
kc alanludnode 17 (,alanbank)
kc alanludnode 18 (,admin/meters/alanmeter)
kc alanludnode 19 (,alanbank)
kc admin/locallud 5 (%axalan,alanludnode)  # name length gets corrected

# JOIN NORM - use context switcher
# for testing, would be the meter contract change request key
kc user/sys/returner 0 (,admin/meters/normchange) (,normuser/normmeterchange)
#
kc user/sys/returner 0 (,user/sys/) (,normuser/sys/)  # norm directory for zapper case
kc user/sys/returner 0 (,user/admin/) (,normuser/admin/) # give power keys
kc user/sys/returner 0 (,normuser/) (,admin/normuser/)

kc sys/lclauthf 0 (,sb,m,sb) (,admin/normauth,admin/normauthm)
kc admin/normauthm 7 (%anorm) # password

kc sys/tdof 1 (,sb,admin/meters/normmeter,sb) (,admin/normcontext)
kc sys/cswitcherf kt+5 (,normbank,admin/meters/normmeter,normbank) (,,,,k)  # should use a prompt bank
kc k 0 (,normuser/,admin/normcontext) (,normzmk)
kc admin/normauthm 5 (%x00,normzmk)  # connection type 0 - zapper


kc sb 0 () (,normludnode)
kc normludnode 16 (,admin/normauth)
kc normludnode 17 (,normbank)
kc normludnode 18 (,admin/meters/normmeter)
kc normludnode 19 (,normbank)
kc admin/locallud 5 (%axnorm,normludnode)  # name length gets corrected


# START local receptionist

kc user/sys/lclrecepf kt+5 (,sb,m,sb) (,,,,k)
kc k 0 (,admin/locallud) (,lclrecep)
kfork lclrecep 0 (,,,admin/console)

#kc admin/uartnode 1 () (,portb)  # can't have this AND gdb style vdk

# START second receptionist for serial port B, now used for GDB stub
#kc user/sys/lclrecepf kt+5 (,sb,m,sb) (,,,,k)
#kc k 0 (,admin/locallud) (,lclrecepserial)
#kfork lclrecepserial 0 (,,,portb)

