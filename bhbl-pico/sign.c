#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "../micro-ecc/uECC.h"
#include "../sha2/sha2.h"

typedef unsigned char UC;

#define IMAGEBUFSIZE (240*1024)
UC imagebuf[IMAGEBUFSIZE+1];

UC digest[SHA256_DIGEST_SIZE];                   // sha256 digest for signing
//UC signature[XXX];                   // generated signature

int main(int ac,char**av) {
  char s[1000];
  int image_size;
  FILE*fp;
  int i;

  if(ac<2) {
    fprintf(stderr,"Usage: %s <image file>\n",av[0]);
    return 4;
    }
  fp=fopen(av[1],"rb");
  if(!fp) {
    fprintf(stderr,"Failed to open image file %s\n",av[1]);
    return 16;
    }
  image_size=fread(imagebuf,1,IMAGEBUFSIZE+1,fp);
  if(image_size<1) {
    fprintf(stderr,"Error reading image file %s\n",av[1]);
    return 16;
    }
  if(image_size>IMAGEBUFSIZE) {
    fprintf(stderr,"Image file %s is too large (maximum %d bytes)\n",av[2],IMAGEBUFSIZE);
    return 16;
    }
  sha256(imagebuf,image_size,digest);
  printf("SHA256:");
  for(i=0;i<sizeof(digest);i++) printf(" %02X",digest[i]);
  printf("\n");
  return 16;
  }


