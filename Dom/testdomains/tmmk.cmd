timeout 600

kc user/admin/uartnode 1 () (,portb)
kc user/sys/tssf kt+5 (,sb,m,sb) (,,,,k)
kc k 2 (,portb) (,usik,usok,ucck)

kc user/sys/tssf kt+5 (,sb,m,sb) (,,,,k)
kc k kt+5 (,usik,usok,ucck) (,,,,k)
kc user/sys/dkc 0 (%x000000000001) (,dk1)
kc k 4 (%x1b,dk1) (,tmmk,sik3,sok3,cck3)

cfact switcherf test/switcher 888 h1=user/sys/dkc h15=user/admin/console
kc switcherf kt+5 (,sb,m,sb) (,,,,k)

kfork k 0 (,sik3,sok3,cck3,tmmk)

#kc user/sys/dkc 0 (%x000000000002) (,dk2)
#kc tmmk 0 (,dk2) (,sika,soka,ccka)
#kt ccka
# kc soka 0 (%ahello) (,,,,soka)
