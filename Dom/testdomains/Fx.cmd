kc sb 16 () (,cp)
# cp is our code page
kc cp 4096 (%x1D24000081E80000)
# commands sethi 0x90000000 %o6; restore in cp.
kc cp 4096 (%x91D02044)
# ta 0x44
kc domcre 0 (,,sb) (,dk)
# dk is a domain key
kc dk 205 (%x0000000003130414)
# set g1 to 03130414
kc dk 51 (,cp)
# cp is its address space
kc dk 49 (,m)
# Give it a meter
kc user/wombfacil 2 () (,res_node)
kc res_node 2 () (,error_key)
kc dk 50 (,error_key)
# Its keeper is now the crash key
kc dk 66 () (,exit)
# Get fault exit so we won't need entry block.
kc exit 0
# It should have tried to fetch window from
# invalid space.
