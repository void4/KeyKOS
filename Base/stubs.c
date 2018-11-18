
#include "keyh.h"
/* Temporary stubs file */
// xmem(){crash("xmem not implemented yet");}
// xmemb(){crash("xmemb not implemented yet");}
// scASMidta(){crash("scASMidta not implemented yet");}
// scASMidta2(){crash("scASMidta2 not implemented yet");}
// scASMdtao(){crash("scASMdtao not implemented yet");}
#if defined(diskless_kernel)
gspcleannodes(){crash("gspcleannodes shouldn't be called in a diskless kernel");}
#endif
