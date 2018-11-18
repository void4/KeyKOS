/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* This program is used to create an ELF object file containing
 * primordial nodes, primordial pages and the control structures for
 * the pages. The pages are either zero filled pre-allocated storage,
 * domain program text and data or flat file data.
 * The resulting .o file will contain three sections and 6 symbols.
 *
 *	Sections:
 *		.prim_nodes
 *		.prim_plist
 *		.prim_pages
 *
 *	Symbols:
 *		prim_nodecnt
 *		prim_node
 *		prim_plistcnt
 *		prim_plist
 *		prim_pagecnt
 *		prim_pages
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <libelf.h>
#include <unistd.h>
#include "kktypes.h"
#include "cvt.h"
#include "itemdefh.h"
#include "item.h"
#include "disknodh.h"

/* Macros */

#define PAGESIZE 4096


/* Structure to record info for a given elf section */

typedef struct scn_info {
	Elf_Scn *scn;		/* section descriptor */
	Elf_Data *data;		/* section data */
	Elf32_Shdr *shdr;	/* section header */
	uint_t ndx; 		/* section index */
	uint_t offset;		/* used by string & symbol sections */
	struct scn_info *link;	/* used by symbol section */
} scninfo_t;


/* Structure to record info for a given elf object file */

typedef struct {
	Elf *elf;			/* elf descriptor */
	Elf32_Ehdr *ehdr;		/* elf header */
	scninfo_t *shstrscn_info;	/* section header str table */
	scninfo_t *symscn_info;		/* symbol table */
	scninfo_t *cntscn_info;		/* section to hold counts */
} elf_info_t;

/* enumeration to list the different file types we are interested in */

typedef enum {
	FT_NOFILE,
	FT_PLAIN,
	FT_EXEC
} filetype_t;


/* Static support functions */

static int readfile(char *, char *, int);
static filetype_t filetype(char *filename);
static void elfload(char *, char *, char *, ulong_t);
static void elfunload(const char *, int, int, int, int, int, int);
static elf_info_t *elf_outfile_init(char *filename);
static void elf_outfile_fini(elf_info_t *einfop);
static scninfo_t *make_strtab(Elf *elf);
static scninfo_t *make_symtab(Elf *elf);
static scninfo_t *make_cntscn(Elf *elf);
static void make_primordial_node_scn(elf_info_t *einfop, char *buf,
	ulong_t count);
static void make_primordial_plist_scn(elf_info_t *einfop, char *buf,
	ulong_t count);
static void make_primordial_page_scn(elf_info_t *einfop, char *buf,
	size_t size);
static void error(char *fmt, ...);
static void set_search_path(char *pathstr);


/* External support functions defined */
int file_countpages(char *filename, int filetype);
char *getfullname(char *);

extern prim_info_t *def_initial_items();

/* External support functions used */

extern prim_info_t *def_initial_items();


/* Static support data */

static char *cmdname;


/* External support data */

extern plist_t plist[];
extern DiskNode_t nodes[];
int verbose;


/* primbuilder [-P search_list] [-o output_file] [-v]
 * search_list is a colon separated list of directories in which to
 * search for domains.
 * output_file is the file in which to write the primordial nodes and
 * pages.
 * -v can be used to print a report of the types of nodes and pages
 * generated.
 */
int
main(int argc, char *argv[])
{
	ulong_t i, total_page_size = 0;
	char *pagebase, *pagep;
	prim_info_t *prim_infop;
	elf_info_t *einfop;
	char *outfile_name = "primordial.o";

	cmdname = argv[0];

	/* Check for domain search path and output file name */
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-P", 2) == 0) {
			/* have a path */
			if (argv[i][2] == '\0')
				set_search_path(argv[i+1]);
			else
				set_search_path(argv[i]+2);
		} else if (strncmp(argv[i], "-o", 2) == 0) {
			if (argv[i][2] == '\0')
				outfile_name = argv[i+1];
			else
				outfile_name = argv[i]+2;
		} else if (strncmp(argv[i], "-v", 2) == 0) 
			verbose = 1;
	}

	/* Populate the plist and node arrays */
	prim_infop = def_initial_items();

	/* Determine how much space we need, grab it and zero it out. */
	for (i = 0; i < prim_infop->plistcnt; i++)
		total_page_size += plist[i].number * PAGESIZE;

	if ((pagebase = (char *)malloc(total_page_size)) == NULL)
		error("main: Memory allocation failure");

	memset(pagebase, 0, total_page_size);	/* zero the pages */

	/* Now loop through and fill the memory allocated */
	pagep = pagebase;
	for (i = 0; i < prim_infop->plistcnt; i++) {
		char *fname = plist[i].filename;
		size_t size = plist[i].number * PAGESIZE;

		switch (plist[i].first) {
		case 0: 
			/* load ELF executable */
			if (fname != NULL && *fname != '\0')
				elfload(fname, (char *)pagep, NULL, 
					size);
			break;

		default:  {
			/* read entire file */
			ulong_t length;

			if (fname != NULL && *fname != '\0') {
				length = readfile(fname, (char *)pagep, 
					size);
				if (plist[i].lengthplace != 0)
					long2b(length, plist[i].lengthplace, 6);
			}
			break;
		}
		} /* end switch */
		pagep += size;
	}

	/* initialize the outfile */
	einfop = elf_outfile_init(outfile_name);

	/* Write the .prim_nodes section  - don't bother with the
	 * first node since it is zero'd out.
	 */
	make_primordial_node_scn(einfop, (char *)(nodes + 1), 
		prim_infop->nodecnt);

	/* Write the .prim_plist section */
	make_primordial_plist_scn(einfop, (char *)plist, 
		prim_infop->plistcnt);

	/* Write the .prim_pages section */
	make_primordial_page_scn(einfop, pagebase, total_page_size);

	elf_outfile_fini(einfop);
	exit(0);
}

/* filetype() attempts to determine the type of a file. It will return
 * FT_NOFILE is the filename is NULL or empty. It will return FT_EXEC
 * if the file is an elf executable. Otherwise it returns FT_PLAIN.
 */
filetype_t
filetype(char *filename)
{
	int fildes;
	Elf *elf;
	Elf32_Ehdr *ehdr;

	if (filename == NULL || *filename == '\0')
		return FT_NOFILE;
	
	if (elf_version(EV_CURRENT) == EV_NONE) 
		error("filetype: Invalid elf version");

	if ((fildes = open(filename, O_RDONLY)) == -1)
		error("filetype: Cannot open file %s", filename);

	elf = elf_begin(fildes, ELF_C_READ, (Elf *)NULL);

	if ((ehdr = elf32_getehdr(elf)) == NULL)
		return FT_PLAIN;

	if (ehdr->e_type == ET_EXEC)
		return FT_EXEC;
	else
		return FT_PLAIN;
}

/*
 * Note: Currently symbuf is not used. At some point we may find it
 * useful to load in the symbol table and string table.
 */

static void
elfload(char *filename, char *codebuf, char *symbuf, ulong_t maxsize)
{
	int fildes;
	Elf *elf;
	Elf32_Phdr *php, *phtable, *phtable_end;
	Elf32_Ehdr *ehdr;
	char *rawptr;
	size_t total_bytes_copied = 0;
	int found_loadable = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) 
		error("elfload: Invalid elf version");

	if ((fildes = open(filename, O_RDONLY)) == -1)
		error("elfload: Cannot open file %s", filename);

	elf = elf_begin(fildes, ELF_C_READ, (Elf *)NULL);

	if ((ehdr = elf32_getehdr(elf)) == NULL)
		error("elfload: Cannot read elf header: %s", filename);

	if (ehdr->e_type != ET_EXEC)
		error("elfload: Not an elf executable: %s", filename);

	if ((phtable = elf32_getphdr(elf)) == NULL)
		error("elfload: missing program header table: %s",
			filename);

	phtable_end = phtable + ehdr->e_phnum;
	php = phtable;
	rawptr = elf_rawfile(elf, 0);

	/* Loops through the segments. We should only find one loadable
	 * segment. If we find more than one, panic since we are
	 * assuming than anything we are ELF loading can be directly
	 * mapped by the kernel. If there are multiple loadable
	 * segments, the mapping will probably fail.
	 */
	while (php < phtable_end) {
		if (php->p_type == PT_LOAD) {
			if (found_loadable++)
				error("elfload: multiple loadable segments in mapped file\n");
			if ((total_bytes_copied + php->p_memsz) > maxsize)
				error("elfload: file larger than "
					"allocated size: %s", filename);

			/* Copy file data */
			memcpy(codebuf, rawptr + php->p_offset, 
				php->p_filesz);
			if (php->p_memsz > php->p_filesz)
				memset(codebuf + php->p_filesz, 0, 
					php->p_memsz - php->p_filesz);
			total_bytes_copied += php->p_memsz;
			codebuf += total_bytes_copied;
		}
		php++;
	}
	/* Zero out remainder of buffer */
	if (total_bytes_copied < maxsize)
		memset(codebuf, 0, maxsize - total_bytes_copied);
	return;
}

/* file_countpages() is used to determine how many pages will be needed
 * to hold the loadable segments from an executable.
 */
int
file_countpages(char *filename, int filetype)
{
	int fildes;
	Elf *elf;
	Elf32_Phdr *php, *phtable, *phtable_end;
	Elf32_Ehdr *ehdr;
	char *rawptr;
	size_t total_bytes = 0;
	int found_loadable = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) 
		error("file_countpages: Invalid elf version");

	if ((fildes = open(filename, O_RDONLY)) == -1)
		error("file_countpages: Cannot open file %s", filename);

	elf = elf_begin(fildes, ELF_C_READ, (Elf *)NULL);

	if (filetype != 0) { /* just return the size of the file */
		int size;

		if ((size = lseek(fildes, 0, SEEK_END)) == -1)
			error("file_countpages: Cannot seek in file: %s\n",
				filename);
		return ((size + (PAGESIZE-1)) / PAGESIZE);
	}

	/* Should be an ELF executable, make sure */
	if ((ehdr = elf32_getehdr(elf)) == NULL ||
		(ehdr->e_type != ET_EXEC) ||
		(phtable = elf32_getphdr(elf)) == NULL) {

		error("file_countpages: expecting ELF executable, didn't get one\n");
	}
	
	/* It's an ELF executable - make sure there's only one
	 * loadable segment (we're using this as a test that it's
	 * mappable).
	 */
	phtable_end = phtable + ehdr->e_phnum;
	php = phtable;
	rawptr = elf_rawfile(elf, 0);

	/* Loops through the segments, counting sizes of LOADable 
	 * segments.
	 */
	while (php < phtable_end) {
		if (php->p_type == PT_LOAD) {
			if (found_loadable++)
				error("file_countpages: multiple loadable segments in mapped file\n");
			total_bytes = php->p_memsz;
		}
		php++;
	}
	return (int)((total_bytes + (PAGESIZE-1)) / PAGESIZE);
}

static uint_t add_str(scninfo_t *si, char *string);
static void add_symbol(scninfo_t *sym_si, char *string, int value, 
	size_t size, Elf_Scn *targ_scn);
static void set_tab_size(scninfo_t *);

static elf_info_t *
elf_outfile_init(char *filename)
{
	elf_info_t *einfop;
	int fd;
	scninfo_t *symstrscn_info;

	if (elf_version(EV_CURRENT) == EV_NONE) 
		error("elf_outfile_init: Invalid elf version");

	if (filename == NULL) 
		error("elf_outfile_init: NULL output file name");

	if ((fd = open(filename, O_RDWR|O_TRUNC|O_CREAT, 0666)) == -1) 
		error("elf_outfile_init: Cannot open file: %s", 
			filename);

	if ((einfop = (elf_info_t *)malloc(sizeof(elf_info_t))) == NULL)
		error("elf_outfile_init: Memory allocation failure");

	if ((einfop->elf = elf_begin(fd, ELF_C_WRITE, (Elf *)0)) == 0) 
		error("elf_outfile_init: Cannot build output file: %s",
			filename);

	einfop->ehdr = elf32_newehdr(einfop->elf);

	/* Mark this as a SPARC object */
	einfop->ehdr->e_machine = EM_SPARC;

	/* Mark this as a relocatable object */
	einfop->ehdr->e_type = ET_REL;

	/* create section header string table section */
	einfop->shstrscn_info = make_strtab(einfop->elf);
	einfop->shstrscn_info->shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".shstrtab");
	einfop->ehdr->e_shstrndx = einfop->shstrscn_info->ndx;

	/* create symbol table string table section */
	symstrscn_info = make_strtab(einfop->elf);
	symstrscn_info->shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".strtab");

	/* create symbol table section */
	einfop->symscn_info = make_symtab(einfop->elf);
	einfop->symscn_info->shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".symtab");
	einfop->symscn_info->link = symstrscn_info;
	einfop->symscn_info->shdr->sh_link = symstrscn_info->ndx;

	/* create .count section - for holding counts */
	einfop->cntscn_info = make_cntscn(einfop->elf);
	einfop->cntscn_info->shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".count");

	return einfop;
}

static void
elf_outfile_fini(elf_info_t *einfop)
{
	/* Update the size of the section header string table and
	 * finish writing out the file.
	 */

	/* set size of section header string table */
	set_tab_size(einfop->shstrscn_info);

	/* set size of symbol table */
	set_tab_size(einfop->symscn_info);

	/* set size of symbol table's string table */
	set_tab_size(einfop->symscn_info->link);

	elf_update(einfop->elf, ELF_C_WRITE);
	elf_end(einfop->elf);
}

#define STRTAB_SIZE 1000

static scninfo_t *
make_strtab(Elf *elf)
{
	scninfo_t *si;

	if ((si = (scninfo_t *)malloc(sizeof(scninfo_t))) == NULL)
		error("make_strtab: Memory allocation failure");

	si->scn = elf_newscn(elf);
	si->shdr = elf32_getshdr(si->scn);
	si->ndx = elf_ndxscn(si->scn);
	si->data = elf_newdata(si->scn);
	if ((si->data->d_buf = (char *)malloc(STRTAB_SIZE)) == NULL)
		error("make_strtab: Memory allocation failure");

	si->data->d_size = STRTAB_SIZE;
	si->data->d_off = 0;
	si->data->d_align = 1;
	si->shdr->sh_type = SHT_STRTAB;
	si->offset = 1; /* first byte should be 0 */
	return si;
}

#define SYMTAB_SIZE 1000

static scninfo_t *
make_symtab(Elf *elf)
{
	scninfo_t *si;

	if ((si = (scninfo_t *)malloc(sizeof(scninfo_t))) == NULL)
		error("make_symtab: Memory allocation failure");

	si->scn = elf_newscn(elf);
	si->shdr = elf32_getshdr(si->scn);
	si->ndx = elf_ndxscn(si->scn);
	si->data = elf_newdata(si->scn);
	if ((si->data->d_buf = (char *) malloc(SYMTAB_SIZE)) == NULL)
		error("make_symtab: Memory allocation failure");

	si->data->d_size = SYMTAB_SIZE;
	si->data->d_off = 0;
	si->data->d_align = 4;
	si->shdr->sh_type = SHT_SYMTAB;
	si->shdr->sh_info = 1; /* point past zero sym */
	si->offset = sizeof(Elf32_Sym); /* first symbol is NULL */
	return si;
}

static scninfo_t *
make_cntscn(Elf *elf)
{
	scninfo_t *si;

	if ((si = (scninfo_t *)malloc(sizeof(scninfo_t))) == NULL)
		error("make_cntscn: Memory allocation failure");

	si->scn = elf_newscn(elf);
	si->shdr = elf32_getshdr(si->scn);
	si->ndx = elf_ndxscn(si->scn);
	si->shdr->sh_flags = SHF_ALLOC + SHF_WRITE;
	si->shdr->sh_type = SHT_PROGBITS;
	return si;
}

static void add_cnt(scninfo_t *si, char *cntname, size_t size, 
	uint_t value, scninfo_t *sym_si);

static void
make_primordial_node_scn(elf_info_t *einfop, char *buf, ulong_t count)
{
	Elf_Scn *scn;		/* section descriptor */
	Elf_Data *data;		/* section data */
	Elf32_Shdr *shdr;	/* section header */
	Elf *elf = einfop->elf;
	size_t size = count * sizeof(DiskNode_t);

	scn = elf_newscn(elf);
	shdr = elf32_getshdr(scn);

	/* create object in the .count scn to hold the node count */
	add_cnt(einfop->cntscn_info, "prim_nodecnt", sizeof(int *), 
		count, einfop->symscn_info);

	/* create a symbol to point to the node data */
	add_symbol(einfop->symscn_info, "prim_nodes", 0, size, scn);

	/* create a data buffer to hold the nodes */
	data = elf_newdata(scn);
	if ((data->d_buf = (char *) malloc(size)) == NULL)
		error("make_primordial_node_scn: "
			"Memory allocation failure");

	memcpy(data->d_buf, buf, size);
	data->d_size = size;
	data->d_off = 0;
	data->d_align = 0x1000;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC + SHF_WRITE;
	shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".prim_nodes");
	return;
}

static void
make_primordial_plist_scn(elf_info_t *einfop, char *buf, ulong_t count)
{
	Elf_Scn *scn;		/* section descriptor */
	Elf_Data *data;		/* section data */
	Elf32_Shdr *shdr;	/* section header */
	Elf *elf = einfop->elf;
	size_t size = count * sizeof(plist_t);

	scn = elf_newscn(elf);
	shdr = elf32_getshdr(scn);

	/* create object in the .count scn to hold the plist count */
	add_cnt(einfop->cntscn_info, "prim_plistcnt", sizeof(int *), 
		count, einfop->symscn_info);

	/* create a symbol to point to the plist data */
	add_symbol(einfop->symscn_info, "prim_plist", 0, size, scn);

	data = elf_newdata(scn);
	if ((data->d_buf = (char *) malloc(size)) == NULL)
		error("make_primordial_plist_scn: "
			"Memory allocation failure");

	memcpy(data->d_buf, buf, size);
	data->d_size = size;
	data->d_off = 0;
	data->d_align = 0x1000;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC + SHF_WRITE;
	shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".prim_plist");
	return;
}

static void
make_primordial_page_scn(elf_info_t *einfop, char *buf, size_t size)
{
	Elf_Scn *scn;		/* section descriptor */
	Elf_Data *data;		/* section data */
	Elf32_Shdr *shdr;	/* section header */
	Elf *elf = einfop->elf;
	ulong_t count = size / PAGESIZE;

	scn = elf_newscn(elf);
	shdr = elf32_getshdr(scn);

	/* create object in the .count scn to hold the page count */
	add_cnt(einfop->cntscn_info, "prim_pagecnt", sizeof(int *), 
		count, einfop->symscn_info);

	/* create a symbol to point to the pages data */
	add_symbol(einfop->symscn_info, "prim_pages", 0, size, scn);

	data = elf_newdata(scn);
	if ((data->d_buf = (char *) malloc(size)) == NULL)
		error("make_primordial_page_scn: "
			"Memory allocation failure");

	memcpy(data->d_buf, buf, size);
	data->d_size = size;
	data->d_off = 0;
	data->d_align = 0x1000;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC + SHF_WRITE;
	shdr->sh_name = 
		add_str(einfop->shstrscn_info, ".prim_pages");
	return;
}

static void
add_cnt(scninfo_t *si, char *cntname, size_t size, uint_t value,
	scninfo_t *sym_si)
{
	Elf_Data *data;

	data = elf_newdata(si->scn);
	if ((data->d_buf = (char *) malloc(size)) == NULL)
		error("add_cnt: " "Memory allocation failure");

	memcpy(data->d_buf, &value, size);
	data->d_size = size;
	data->d_off = 0;
	data->d_align = 4;

	/* create a symbol to point to the node count */
	add_symbol(sym_si, cntname, si->offset, sizeof(int *), si->scn);
	si->offset += data->d_size;
}
static uint_t
add_str(scninfo_t *si, char *string)
{
	uint_t ret;

	if (si->shdr->sh_type != SHT_STRTAB)
		error("add_str: attempt to add a string(%s) to a "
			"non-string section", string);

	if ((si->offset + strlen(string) + 1) > si->data->d_size)
		error("add_str: String table overflow");

	strcpy((char *)si->data->d_buf + si->offset, string);
	ret = si->offset;
	si->offset += (strlen(string) + 1);
	return ret;
}

static void
add_symbol(scninfo_t *sym_si, char *string, int value, size_t size, 
	Elf_Scn *targ_scn)
{
	Elf32_Sym sym;

	if (sym_si->shdr->sh_type != SHT_SYMTAB) 
		error("add_symbol: Attempt to add symbol to a "
			"non-symbol table section");

	if ((sym_si->offset + sizeof(Elf32_Sym)) > sym_si->data->d_size)
		error("add_symbol: Symbol table overflow");

	sym.st_name = add_str(sym_si->link, string);
	sym.st_value = value;
	sym.st_size = size;
	sym.st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT);
	sym.st_other = 0;
	sym.st_shndx = elf_ndxscn(targ_scn);

	memcpy((char *)sym_si->data->d_buf + sym_si->offset, 
		&sym, sizeof(Elf32_Sym));
	sym_si->offset += sizeof(Elf32_Sym);
	return;
}
	
static void
set_tab_size(scninfo_t *si)
{
	si->data->d_size = si->offset;
}

static int
readfile(
char *filename,	/* filename to read */
char *dst,	/* location into which data is read */
int maxsize)	/* maximum size to read */
{
	int fd;
	int len;

	if ((fd = open(filename, O_RDONLY)) == -1)
		error("readfile: Cannot open file: %s", filename);

	if ((len = lseek(fd, 0, SEEK_END)) == -1) {
		close(fd);
		error("readfile: Seek failure: %s", filename);
	}

	if (len > maxsize) 
		error("readfile: File size: 0x%X, larger than space allocated: "
                      "0x%X, File: %s", len, maxsize, filename);

	lseek(fd, 0, SEEK_SET);

	if ((len = read(fd, dst, maxsize)) == -1) {
		close(fd);
		error("readfile: Read failure: %s", filename);
	}
	close(fd);
	return len;
}

static char *search_path = ".";

static void
set_search_path(char *pathstr)
{
	search_path = pathstr;
}

/* If name does not include a '/', try to build a path for it based
 * on the search path. If the file does not exist in any of the
 * directories in the search path, panic.
 */
char *
getfullname(char *name)
{
        char *fname;
        char *cp;

        if (name == NULL | *name == '\0')
                return NULL;

	/* See if the filename has a '/'. If so, don't attempt to
	 * augment it.
	 */
	cp = name;
	while (*cp) {
		if (*cp++ == '/')
			if (access(name, F_OK) == 0) {
				return name;
			} else
				error("getfullname: Cannot find file: %s\n", name);
	}
        /* try the different search directories */
        cp = search_path;
        while (*cp) {
		char *tp = cp;
                char savech;

                while (*tp && *tp != ':') /* get to colon */
                        tp++;
                savech = *tp;
                *tp = '\0';
                fname = (char *)malloc(strlen(name) + strlen(cp) + 2);
                strcpy(fname, cp);
                strcat(fname, "/");
                strcat(fname, name);
                *tp = savech;
                if (access(fname, F_OK) == 0) {
                        return fname;
		}
                free(fname);
                cp = tp + 1;
        }
	error("getfullname: Cannot find file: %s\n", name);
}

static void
error(char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	fprintf(stderr, "%s: ", cmdname);
	(void) vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

