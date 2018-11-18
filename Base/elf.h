/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _ELF_H
#define _ELF_H

#define	STT_NOTYPE	0		/* TYPE */
#define	STT_OBJECT	1
#define	STT_FUNC	2
#define	STT_SECTION	3
#define	STT_FILE	4
#define	STT_NUM		5

#define	STT_LOPROC	13		/* processor specific range */
#define	STT_HIPROC	15

/*
 *	The macros compose and decompose values for S.st_info
 *
 *	bind = ELF32_ST_BIND(S.st_info)
 *	type = ELF32_ST_TYPE(S.st_info)
 *	S.st_info = ELF32_ST_INFO(bind, type)
 */

#define	ELF32_ST_BIND(info)		((info) >> 4)
#define	ELF32_ST_TYPE(info)		((info) & 0xf)
#define	ELF32_ST_INFO(bind, type)	(((bind)<<4)+((type)&0xf))
#define	STB_LOCAL	0		/* BIND */
#define	STB_GLOBAL	1
#define	STB_WEAK	2
#define	STB_NUM		3

#define	STB_LOPROC	13		/* processor specific range */
#define	STB_HIPROC	15

#define	SHT_NULL	0		/* sh_type */
#define	SHT_PROGBITS	1
#define	SHT_SYMTAB	2
#define	SHT_STRTAB	3
#define	SHT_RELA	4
#define	SHT_HASH	5
#define	SHT_DYNAMIC	6
#define	SHT_NOTE	7
#define	SHT_NOBITS	8
#define	SHT_REL		9
#define	SHT_SHLIB	10
#define	SHT_DYNSYM	11
#define	SHT_NUM		12
#define	SHT_LOUSER	0x80000000
#define	SHT_HIUSER	0xffffffff

#define	SHT_LOPROC	0x70000000	/* processor specific range */
#define	SHT_HIPROC	0x7fffffff

#define	SHF_WRITE	0x1		/* sh_flags */
#define	SHF_ALLOC	0x2
#define	SHF_EXECINSTR	0x4
#define	SHF_MASKPROC	0xf0000000	/* processor specific values */

#define	SHN_UNDEF	0		/* special section numbers */
#define	SHN_LORESERVE	0xff00
#define	SHN_ABS		0xfff1
#define	SHN_COMMON	0xfff2
#define	SHN_HIRESERVE	0xffff

#define	SHN_LOPROC	0xff00		/* processor specific range */
#define	SHN_HIPROC	0xff1f

#define	ELFMAG		"\177ELF"

typedef unsigned long	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned long	Elf32_Off;
typedef long		Elf32_Sword;
typedef unsigned long	Elf32_Word;

/*
 *	ELF header
 */

#define	EI_NIDENT	16

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* ident bytes */
	Elf32_Half	e_type;			/* file type */
	Elf32_Half	e_machine;		/* target machine */
	Elf32_Word	e_version;		/* file version */
	Elf32_Addr	e_entry;		/* start address */
	Elf32_Off	e_phoff;		/* phdr file offset */
	Elf32_Off	e_shoff;		/* shdr file offset */
	Elf32_Word	e_flags;		/* file flags */
	Elf32_Half	e_ehsize;		/* sizeof ehdr */
	Elf32_Half	e_phentsize;		/* sizeof phdr */
	Elf32_Half	e_phnum;		/* number phdrs */
	Elf32_Half	e_shentsize;		/* sizeof shdr */
	Elf32_Half	e_shnum;		/* number shdrs */
	Elf32_Half	e_shstrndx;		/* shdr string index */
} Elf32_Ehdr;

/*
 *	Section header
 */

typedef struct {
	Elf32_Word	sh_name;	/* section name */
	Elf32_Word	sh_type;	/* SHT_... */
	Elf32_Word	sh_flags;	/* SHF_... */
	Elf32_Addr	sh_addr;	/* virtual address */
	Elf32_Off	sh_offset;	/* file offset */
	Elf32_Word	sh_size;	/* section size */
	Elf32_Word	sh_link;	/* misc info */
	Elf32_Word	sh_info;	/* misc info */
	Elf32_Word	sh_addralign;	/* memory alignment */
	Elf32_Word	sh_entsize;	/* entry size if table */
} Elf32_Shdr;

/*
 *	Symbol table
 */

typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	unsigned char	st_info;	/* bind, type: ELF_32_ST_... */
	unsigned char	st_other;
	Elf32_Half	st_shndx;	/* SHN_... */
} Elf32_Sym;

#endif /* _ELF_H */
