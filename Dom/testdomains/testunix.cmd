# build a unix object factory
kc sb 0 () (,,foozipronc)  # put a dk0 here first
getfile foo.zip foozip
kc foozip 3 () (,foozipronc)  # if file transfer was successful now a rono segment
ufact hellof test/hello 999 s1=foozipronc
#
# get instance and run normally
#
kc user/admin/uartnode 1 () (,portb)
kc user/sys/devttyf kt+5 (,sb,m,sb) (,,,,k)
kc k 0 (,portb) (,devtty)

kc hellof 0 (,sb,m,sb) (,hello)
kc hello 2 (%aHOME=/home/alan PRINTER=/dev/barkus)
kc hello 4 (%atty,devtty)
kc hello 3 (,sb,,sb) (,fenvr)
#kc hello 256 (%a-classfiles alpha:beta,sik,sok,test/) (,k0)
kc hello 0 (%a-classfiles alpha:beta,sik,sok,test/) (,k0)


kc fenvr 0 (,sb,m,sb) (,hello)
kc hello 4 (%atty,devtty)
kc hello 0 (%atest of frozen environment,sik,sok,test/) (,k0)

#
#  get and instance and run with freezedry request 
#
#kc hellof 0 (,sb,m,sb) (,hello)
kc fenvr 0 (,sb,m,sb) (,hello)
kc hello 42 (%athis is a freezedry test,sik,sok,test/) (,fdr,fdb)
#
#  now get instance of new preloaded unix object and run (note args from freeze)
#
kc fdr 0 (,sb,m,sb) (,hello)
kc hello 4 (%atty,devtty)
kc hello 0 (,sik,sok,test/)
