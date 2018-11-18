timeout 600
ufact javaf test/java 999
#
kc javaf 0 (,sb,m,sb) (,java)
kc java 2 (%aTHREADS_TYPE=native_threads JAVA_HOME=/usr/java1.1)
kc java 0 (%a-classpath classes/samples Hello,sik,sok,test/) (,fdjavar,fdjavab)
#
kc javaf 0 (,sb,m,sb) (,java)
kc java 2 (%aTHREADS_TYPE=native_threads JAVA_HOME=/usr/java1.1)
kc java 42 (%a-classpath classes/samples Hello,sik,sok,test/) (,fdjavar,fdjavab)
#
kc fdjavar 0 (,sb,m,sb) (,java)
kc java 0 (,sik,sok,test/) (,fdjavar,fdjavab)  # this freezedries second time
#
#
#
#
################################################################################
################################################################################
#
#  Now the THAW Test
#
#
#
#
kc fdjavar 0 (,sb,m,sb) (,java)
kc java 0 (,sik,sok,test/)
