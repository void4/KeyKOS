#ifndef _ITEMDEFH_H
#define _ITEMDEFH_H 
/* The following is the maximum length of a file name */
#define maxfilenamelength 50
 
typedef struct {
	unsigned long nodecnt;
	unsigned long plistcnt;
} prim_info_t;
 
typedef struct {
	unsigned int number;
	char filename[maxfilenamelength+1];
	long first;
	unsigned char *lengthplace; /* place to put length of file */
	long firstcda;
} plist_t;

extern prim_info_t *def_initial_items(void);

#endif /* _ITEMDEFH_H */
