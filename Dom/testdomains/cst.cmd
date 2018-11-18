# A command file to test callseg.
kc sb 0 () (,n)
cmdfile test/join.cmd
kc user/sys/dkc 0 (%x00000000e013) (,fk)
kc n 16+15 (,fk)
kc joinf 0 (,sb,m,sb) (,js,,,jr)
kc n 16+14 (,jr)
kc user/sys/callsegf 0 (,sb,m,sb) (,cs)
kc joinf 0 (,sb,m,sb) (,jsx,,,jrx)
kc n 33 () (,sg)
kc cs 1 (,sg)
kfork cs 3 (%x0000000010000123)
kc js 1 () (%x,k0,,,k3)
kc user/sys/discrim 0 (,k3)
kt k0
