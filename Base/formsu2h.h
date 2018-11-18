/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

void formmsg(const char *);
   /* Called with a (NUL terminated) error message to be printed. */

/* Routine to write a page to disk.
   On the first call, writes to page 0.
   Each subsequent call writes to the next page on disk. */
enum WriteType {
   WRDATA,    /* Write the page of data at buf. */
   ZERO,    /* Write a page of zeroes. */
   NODATA   /* Page need not be initialized. */
};
int formwrt(
   enum WriteType code,
   const void *buf);
      /* if code == DATA, buf is pointer to a page of data to write */
   /* Return value is nonzero if permanent error, do not continue. */

/* Routine to finish up disk I/O (flush any buffers). */
void formclosedev(void);
