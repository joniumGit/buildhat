// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../micro-ecc/uECC.h"

typedef unsigned char UC;

UC publickey[64];
UC privatekey[32];

int main(int ac,char**av) {
  FILE*fp;
  int i;
  uECC_Curve c;
  c=uECC_secp256r1();

  assert(uECC_curve_public_key_size(c)==sizeof(publickey));
  assert(uECC_curve_private_key_size(c)==sizeof(privatekey));
  printf("****** KEY GENERATOR FOR TESTING PURPOSES ONLY ******\n");
  i=uECC_make_key(publickey,privatekey,c);
  if(i==0) {
    fprintf(stderr,"Failed to make key pair\n");
    return 4;
    }
  printf(" Public:");
  for(i=0;i<sizeof(publickey);i++) printf(" %02X",publickey[i]);
  printf("\n");
  printf("Private:");
  for(i=0;i<sizeof(privatekey);i++) printf(" %02X",privatekey[i]);
  printf("\n");
  fp=fopen("key.public.bin","wb");
  if(!fp) {
    fprintf(stderr,"Failed to open public key file\n");
    return 16;
    }
  i=fwrite(publickey,1,sizeof(publickey),fp);
  if(i<sizeof(publickey)) {
    fprintf(stderr,"Error writing public key file %s\n",av[1]);
    return 16;
    }
  fclose(fp);
  fp=fopen("key.public.h","w");
  if(!fp) {
    fprintf(stderr,"Failed to open public key header file\n");
    return 16;
    }
  fprintf(fp,"static unsigned char publickey[]={");
  for(i=0;i<sizeof(publickey);i++) {
    if(i) fprintf(fp,",");
    fprintf(fp,"0x%02X",publickey[i]);
    }
  fprintf(fp,"};\n");
  fclose(fp);
  fp=fopen("key.private.bin","wb");
  if(!fp) {
    fprintf(stderr,"Failed to open private key file\n");
    return 16;
    }
  i=fwrite(privatekey,1,sizeof(privatekey),fp);
  if(i<sizeof(privatekey)) {
    fprintf(stderr,"Error writing private key file %s\n",av[1]);
    return 16;
    }
  fclose(fp);
  return 0;
  }
