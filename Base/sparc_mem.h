/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ident "@(#)sparc_mem.h	1.10 28 Aug 1995 14:49:20 %n%"

#define IOMMUSIZE	0x6000		/* 4 pages of iommu space plus 2 pages
					   for I/O */
#define IOMMUPTSIZE	0x4000		/* 4 pages of iommu page table space */
#define IOMMUADDR	0xf4000000	/* starting address of iommu space */
#define ConstantKernelFragment  0xf8000000
#define DIBBASE		0xf4010000	/* Virtual address of firstdib */
extern unsigned long	dib_pages;	/* bus address of the 1st DIB pages */
extern unsigned long	dib_size;	/* space occupied by DIB pages */
extern unsigned long	disk_init_page;	/* bus address of the disk_init pages */
extern unsigned long	first_reqsense_page;/* bus address of the 1st scsi reqsense pages */


#include "memoryh.h"

extern void handle_data_obstacle(void);

extern void dat2inst(void);
extern unsigned short mape_hash(MI p);
void genKMap(void);
extern CTE *page_to_rescind;
extern int Soft;
void rescind_write_access(uint32,unsigned short);
void rescind_read_access (uint32,unsigned short);
mem_result resolve_address(uint32, struct DIB *,
   int  /* 1 iff attempting to write, else 0 */);
void MakeProdRO(CTE*);
void makero(CTE*);
void makekro(CTE*);
void resetkro(CTE*);
void mark_page_clean(CTE*);
