/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

main(int argc, char** argv)
{
  int in,out;
  char buf[256];
  int nblocks;
  int i,len;
  struct stat statbuf;


  if(argc != 3 ) {
     fprintf(stderr,"Usage: %s infile outfile\n",argv[0]);
     exit(1);
  }
  
  in=open(argv[1],O_RDONLY);
  if(in < 0) {
     fprintf(stderr,"Cannot open input file %s\n",argv[1]);
     exit(1); 
  }
  out=open(argv[2], O_RDWR | O_CREAT | O_TRUNC,0x1A4);
  if(out < 0) {
     fprintf(stderr,"Cannot open output file %s\n",argv[2]);
     close(in);
     exit(1); 
  }

  if(fstat(in,&statbuf)) {
     fprintf(stderr,"Cannot stat file %s\n",argv[1]);
     close(in);
     close(out);
     exit(1);
  }
  nblocks=statbuf.st_size/256;
  i=1;
  write(out,&i,4);
  i=nblocks*256;
  write(out,&i,4);

  for(i=0;i<nblocks;i++) {
    len=read(in,buf,256);
    if(len != 256) {
       fprintf(stderr,"Error reading file\n");
       close(in);
       close(out);
       exit(1);
    }
    len=write(out,buf,256);
    if(len != 256) {
       fprintf(stderr,"Error writing file\n");
       close(in);
       close(out);
       exit(1);
    }
  } 
  close(in);
  close(out); 
  return 0;
}
