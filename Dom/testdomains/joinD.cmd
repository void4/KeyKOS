kc user/sys/bsloadf 0 (,sb,m,sb) (,bsload)
kc user/sys/vcsf 0 (,sb,m,sb) (,prg)
kc bsload 0 (,test/joinD,prg) (sa)
kc prg 17 (,sb) (,prgfac)
kc user/sys/factoryc 0 (,sb,m) (,fb)
kc fb 17+32 (sa,prgfac)
kc fb 0 (,user/sys/returner) # install is component 0.
kc fb 66 () (,joinDf)

# and now its first deployment as a meter keeper.

kc user/sys/dkc 0 (%x000001000000) (,oneSec)

kc joinDf 8 (,sb,m,sb) (,s,,,r)
kc user/sys/returner 0 (,s) (,user/s) # deliver meter keeper key to other thread.

kc r 16 () (%x,,,,r) # What does the keper get? Expect OC=-1 and string=TOD.
kc r 1 () (,,,msk,r) # Expect meter service key
kt msk
kc msk 3 () (,dk)
kc dk 1 () (%x) # read the meter value
kc msk 16+3 (,oneSec) # Set to one sec

kc r 16 () (%x,,,,r) # What does the keper get? Expect OC=-1 and string=TOD.
