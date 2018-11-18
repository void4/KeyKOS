timeout 6000

factory kvmf test/kkkvm 0x1ffe h5=user/admin/console  # make a generic KVM factory

kt kvmf
kc user/sys/clock 5 () (%a)
kc kvmf 0 (,sb,m,sb) (,kvm)       # get a generic KVM Object

kc kvm 1 (,test/classes/api/)    #  give it a class file
kc kvm 1 (,test/classes/samples/) # give it another class file
kc kvm 5 (,user/admin/console)    # give it a console
kc kvm 6 (%aHello)                # Tell it what application to run
kc kvm 7                          # execute fully
kc user/sys/clock 5 () (%a)
#
#
kc kvm 8 () (,fdkvmf,fdkvmb)      # execute and obtain freezedried version
kc user/sys/clock 5 () (%a)

#
# Now repeatedly run the freeze dried Hello Application
#

kc fdkvmf 0 (,sb,m,sb) (,kvm)     # get an Hello Object
kc kvm 5 (,user/admin/console)    # give it a console
kc kvm 7                          # run it without freezing
kc user/sys/clock 5 () (%a)

#  repeat with new instance

kc fdkvmf 0 (,sb,m,sb) (,kvm)     # get a Hello Object
kc kvm 5 (,user/admin/console)    # give it a console
kc kvm 7                          # run it without freezing
kc user/sys/clock 5 () (%a)

