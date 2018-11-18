#
kc clockf 42 (,sb,m,sb) (sa,clockvcs)
kt clockvcs

kc user/sys/factoryc 0 (,sb,m) (,fb)
kc fb 17+32 (sa,clockvcs)
kc fb 64 (%x00000112)
kc fb 0+128 (,systimer)
kc fb 1+128 (,calclock)
kc fb 15+128 (,console)
kc fb 2 (,journalpagero)
kc fb 66 () (,clocktf)
#
kt clocktf
#
kc clocktf 0 (,sb,m,sb) (,clockt)
# 
kc clockt 5 () (%a)
