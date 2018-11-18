extern CTE *intrn_addr2cte(unsigned long busaddr);
extern CTE *addr2cte(   /* Convert bus address to core table entry */
   unsigned long busaddr);

#if LATER
{  if(busaddr<endmemory && busaddr >= first_user_page)
       return firstcte+((busaddr-first_user_page)/pagesize);
   return intrn_addr2cte(busaddr);}
#endif

