/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

   jmp_buf jp;

   struct stat st;
 
   extern char **_environ;

main(argc,argv)
   int argc;
   char *argv[];
{
   int i,j;
   char *ptr;
   char **env;
   char buf[1024];
   int fh;
   int rc;
   struct pollfd pfd[3];

   ptr=(char *)getenv("FREEZEDRY");
   if(ptr) {
      if(!strcmp(ptr,"YES")) {
          printf("attempting Freezedry\n");
//          open("./Freezedry.class",0,0);
          freezedry();
          printf("Freezedry return\n");
      }
   }

   if(setjmp(jp)) exit(0);
 
   printf("Hello pagesize=%d statsize=%d NARGS=%d\n",getpagesize(),sizeof(st),argc);


   for(i=0;i<argc;i++) {
       if(argv[i]) printf(" %s",argv[i]);
   }
   printf("\n");

   env=(char **)_environ; 
   while(*env) {
      ptr = *env;
      printf("%s\n",ptr);
      env++;
   }

   printf("Open stackt.cmd\n");
   fh=open("stackt.cmd", O_RDONLY);
   if(fh<0) {
      printf("Open of stackt.cmd failed\n");
   }   
   else {
      read(fh,buf,32);
      printf("'%s'\n",buf);
      close(fh);
   }


//   puts("Type some input ->");
//   gets(buf);
//   puts(buf);

#ifdef xx
   printf("Now write something on /dev/tty\n");

//   alarm(1000);
   fh=open("/dev/tty",O_RDWR,0);
   if(fh < 0) { 
      printf("Open of /dev/tty failed\n");
   }
   else {
      strcpy(buf,"This is a Uart Test\r\n");
      write(fh,buf,strlen(buf));
      write(fh,buf,strlen(buf));
   }

 for(j=0;j<5;j++) {  /* loop 5 times */

   printf("Now poll for input on the tty or console \n");

   pfd[0].fd = fh;
   pfd[0].events = POLLIN;
   pfd[0].revents = 0;
   pfd[1].fd = 0;
   pfd[1].events = POLLIN;
   pfd[1].revents = 0;
   i=poll(pfd,2,10000);

   printf("POLL results i=%d %X %X\n",i,pfd[0].revents,pfd[1].revents);

   if(!i) {
      printf("Poll timeout\n");
   }

   if(pfd[0].revents & POLLIN) {
      char pbuf[256];

      printf("Now read from /dev/tty CR terminated\n");
      i=read(fh,buf,64);
      buf[i]=0;
      sprintf(pbuf,"This is what I read - %s",buf);
      strcat(pbuf,"\r");   /* input CR is changed to NL for unix compat */
      write(fh,pbuf,strlen(pbuf));
   } 
   if(pfd[1].revents & POLLIN) {
      printf("Now read from CONSOLE CR terminated\n");
      gets(buf);
      printf("This is what I read - %s\n",buf);
   } 
 }

   close(fh);
#endif
//   alarm(0);

   printf("Now write something to the file 'testfile'\n");

   fh=open("testfile",O_CREAT | O_RDWR | O_APPEND ,0x81ff);
   if(fh < 0) {
      printf("Failed to create 'testfile'\n");
      exit(0);
   }
   
   for (i=0;i<10;i++) {
     sprintf(buf,"This is a line of text %d\n",i);
     rc=write(fh,buf,strlen(buf));
     if(rc != strlen(buf)) {
         printf("Write failed\n");
         close(fh);
         exit(0);
     }
   }
   close(fh);

  
#ifdef xx
   ptr=(char *)getenv("HOME");
   if(ptr) {
       printf("HOME=%s\n",ptr);
   }
   ptr=(char *)getenv("FREEZEDRY");
   if(ptr) {
       printf("FREEZEDRY=%s\n",ptr);
   } 
#endif
}

trap()
{
   longjmp(jp,1);
}

asm("

        .type freezedry, #function

freezedry:
        ta 3
        set 0x0000ffff, %g1
        set 0, %o0
        ta  8
        retl
        nop
");

