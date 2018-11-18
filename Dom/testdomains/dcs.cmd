# A dumb callseg facility
cmdfile test/join.cmd
kc joinf 128 (,sb,m,sb) (,js,,,jr)
kc sb 16 () (,cspage)
kc sb 0 () (,csnode)
kc csnode 33 (%x07) (,csseg)
kc csnode 16+14 (,cspage)
kc domcre 0 (,,sb) (,csdk)
kc csdk 74 (%xe0000000,csseg)
kc csdk 32+2 (,jr)
kc csdk 67 () (,csres)
kc csdk 32+1 (,m)
kc cspage 4096 (%xc0202000)
kfork csres 0
kc js 1 () (%x,dkk0,dkk1,dkk2,dkk3)
