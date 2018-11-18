# build a unix object factory
ufact lwptestf test/lwptest 999
#
# get instance and run normally
#
kc lwptestf 0 (,sb,m,sb) (,lwptest)

kc lwptest 256 (,sik,sok) 
#kc lwptest 0 (,sik,sok)

