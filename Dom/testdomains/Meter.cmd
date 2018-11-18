kc sb 16 () (,cp) # Make code page
kc cp 4096 (%X033fffff82a0600112bfffff01000000)
kc cp 4096 (%X033fffff82a0600112bfffff)
#Above insterts code: sethi %(-1), %g1; subcc %g1, 1, %g1; bnz *-4; nop
kc domcre 0 (,,sb) (,d) # Make domain to spend time
kc d 35 (,cp) # Install code page in domain.
kc sb 8 () (,m1,m2,m3)  # Get three nodes for three meters.
kc user/sys/dkc 0 (%xffffffbfffff) (,nk) # Counter for each meter
kc m1 19 (,nk)
kc m2 19 (,nk)
kc m3 19 (,nk) # Install counter in each meter.
# kc m1 20 (,nk) # Mark this one green for debugging.
kc m1 34 () (,mk1) # Make 3 meter keys:
kc m2 34 () (,mk2) 
kc m3 34 () (,mk3)
kc m1 17 (,m)
kc m2 17 (,mk1) # Chain meters into stack
kc m3 17 (,mk2)
kc d 33 (,mk3) # Install least meter in domain.
kc d 67 () (,ex) # Acquire resume key (EX) to the domain
kfork ex 0

