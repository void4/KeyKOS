timeout 600
ufact kvmf test/kvm 999
#
kc kvmf 0 (,sb,m,sb) (,kvm)
kc kvm 0 (%a-classpath classes/samples Hello,sik,sok,test/)
#
kc kvmf 0 (,sb,m,sb) (,kvm)
kc kvm 42 (%a-classpath classes/samples Hello,sik,sok,test/) (,fdkvmr,fdkvmb)
#
kc fdkvmr 0 (,sb,m,sb) (,kvm)
kc kvm 0 (,sik,sok,test/)

