timeout 600

kc user/admin/uartnode 1 () (,portb)
kc user/sys/tssf kt+5 (,sb,m,sb) (,,,,k)
kc k 2 (,portb) (,usik,usok,ucck)

space sb
kc user/sys/switcherf kt+5 (,sb,m,sb) (,,,,k)
kc k 0 (,usik,usok,ucck) (,switcher)

kc switcher 0 () (%a,sika,soka,ccka)

#kc soka 0 (%ahello) (,,,,soka)

kc switcher kt+4
space sb
