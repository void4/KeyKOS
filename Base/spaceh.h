void gspmnfa(NODE *np);
void gspmpfa(CTE *cte);
bool gspcleannodes(void);
   /* Returns TRUE iff cleaned some nodes. */
NODE *gspgnode(void);
CTE *gspgpage(void);
extern unsigned long nodesmarkedforcleaning;
void hash_the_cda(CTE *);
void gspdetpg(CTE *);
