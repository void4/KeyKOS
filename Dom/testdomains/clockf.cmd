kc user/admin/journalnode 0 () (,journalpage)
kc journalpage 0 () (,journalpagero)
#
#kc user/sys/returner 0 (,vdk) (,savevdk)

#kc user/admin/uartnode 1 () (,portb)
#kc user/sys/vdk2rcf kt+5 (,sb,m,sb) (,,,,k)
#kc k 3 (,portb) (,vdk) 

factory clockf test/clockc 0x112 h0=user/admin/systimer h1=user/admin/calclock h15=user/admin/console s2=journalpagero
#
#kc user/sys/returner 0 (,savevdk) (,vdk)
kt clockf
#
kc clockf 42 (,sb,m,sb) (,clock)
#
kc clock 5 () (%a)
