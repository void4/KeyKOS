kc user/sys/bsloadf 0 (,sb,m,sb) (,bsload)
kc user/sys/vcsf 0 (,sb,m,sb) (,prog)

kc test/hello 8 () (%xhellolength)

kc bsload 1 (,test/hello,prog) (%xhellosa)   # base address not starting
kc bsload 0 (,user/sys/uwrapper,prog) (%xuwrappersa)
kc bsload 0 (%x0F200000,user/sys/unixroot/usr/lib/ld.so.1,prog) (%xldsa)

kc prog 17 (,sb) (,progf)

kc sb 0 () (,node)
datakey dk1 %ahello
kc node 16 (,dk1)
datakey dk1 hellosa   # really base address
kc node 17 (,dk1)
datakey dk1 hellolength
kc node 18 (,dk1)
datakey dk1 %x0F200000
kc node 19 (,dk1)
datakey dk1 ldsa
kc node 20 (,dk1)
kc node 36 () (,sensekey)

kc user/sys/factoryc 0 (,sb) (,fb)
kc fb 32+16 (,user/sys/ukeeperf)
kc fb 32+17 (uwrappersa,progf)
kc fb 64 (%x9999)
kc fb 0 (,sensekey)
kc fb 66 () (,hellof)

kc hellof 0 (,sb,m,sb) (,hello)
kc hello 0 (,sik,sok)

