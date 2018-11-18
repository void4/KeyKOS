int Strcmp(const char* s1, const char* s2){
  while (*s1 == *s2++) if (!*s1++) return (0);
  return (*s1 - *--s2);}

char* Strncpy(char* s1, const char* s2, int n){
  char* os1 = s1; n++;
  while (--n && (*s1++ = *s2++));
  if (n) while (--n) *s1++ = 0;
  return (os1);}

char* Strcpy(char *s1, const char *s2){ // bad routine!
  char* rp = s1;
  while(*s1++ = *s2++);
  return(rp);}

char* Strcat(char* s1, const char* s2){ // bad routine!
  char* s = s1;
  while (*s1) ++s1;
  Strcpy(s1, s2);
  return (s);}
