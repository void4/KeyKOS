kc user/admin/journalnode 0 () (,journalpage)
kc journalpage 0 () (,journalpagero)

kc user/sys/bsloadf 0 (,sb,m,sb) (,bsload)
kc user/sys/vcsf 0 (,sb,m,sb) (,seg)
kc bsload 0 (,test/clockc,seg) (sa)
kc seg 17 (,sb) (,progfact)
#
kc user/sys/factoryc 0 (,sb,m) (,fb)
kc fb 17+32 (sa,progfact)
kc fb 64 (%x00000112)
kc fb 0+128 (,user/admin/systimer)
kc fb 1+128 (,user/admin/calclock)
kc fb 15+128 (,user/admin/console)
kc fb 2 (,journalpagero)
kc fb 66 () (,clockf)
#
kt clockf
#
kc clockf 0 (,sb,m,sb) (,clock)
# 
kc clock 5 () (%a)
