#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../micro-ecc/uECC.h"
#include "../sha2/sha2.h"

typedef unsigned char UC;

#define IMAGEBUFSIZE (240*1024)
UC imagebuf[IMAGEBUFSIZE+1];

UC digest[SHA256_DIGEST_SIZE];                   // sha256 digest for signing

UC publickey[64];
UC privatekey[32];
UC signature[64];                                // generated signature

int main(int ac,char**av) {
  char s[1000];
  int image_size;
  FILE*fp;
  int i,v0,v1;
  uECC_Curve c;
  c=uECC_secp256r1();

  assert(uECC_curve_public_key_size(c)==sizeof(publickey));
  assert(uECC_curve_private_key_size(c)==sizeof(privatekey));

  fp=fopen("key.public.bin","rb");
  if(!fp) {
    fprintf(stderr,"Failed to open public key file\n");
    return 16;
    }
  i=fread(publickey,1,sizeof(publickey),fp);
  if(i<sizeof(publickey)) {
    fprintf(stderr,"Error reading public key file\n");
    return 16;
    }
  fclose(fp);
  fp=fopen("key.private.bin","rb");
  if(!fp) {
    fprintf(stderr,"Failed to open private key file\n");
    return 16;
    }
  i=fread(privatekey,1,sizeof(privatekey),fp);
  if(i<sizeof(privatekey)) {
    fprintf(stderr,"Error reading private key file\n");
    return 16;
    }
  fclose(fp);

  printf("   Public:");
  for(i=0;i<sizeof(publickey);i++) printf(" %02X",publickey[i]);
  printf("\n");
  printf("  Private:");
  for(i=0;i<sizeof(privatekey);i++) printf(" %02X",privatekey[i]);
  printf("\n");
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
//  printf("Image size=%d %02x %02x... %02x %02x\n",image_size,imagebuf[0],imagebuf[0],imagebuf[image_size-2],imagebuf[image_size-1]);
//  printf("   SHA256:");
//  for(i=0;i<sizeof(digest);i++) printf(" %02X",digest[i]);
//  printf("\n");
  i=uECC_sign(privatekey,digest,sizeof(digest),signature,c);
  if(i==0) {
    fprintf(stderr,"Failed to generate signature\n");
    return 4;
    }
  printf("Signature:");
  for(i=0;i<sizeof(signature);i++) printf(" %02X",signature[i]);
  printf("\n");
  fp=fopen("signature.bin","wb");
  if(!fp) {
    fprintf(stderr,"Failed to open signature file\n");
    return 16;
    }
  i=fwrite(signature,1,sizeof(signature),fp);
  if(i<sizeof(signature)) {
    fprintf(stderr,"Error writing signature file\n");
    return 16;
    }
  fclose(fp);
  v1=uECC_verify(publickey,digest,sizeof(digest),signature,c);
  printf("  Verify1: %d\n",v1);
  signature[5]^=0x02;                            // flip one bit: this should make the signature test fail
  v0=uECC_verify(publickey,digest,sizeof(digest),signature,c);
  printf("  Verify0: %d\n",v0);
  if(v1==1&&v0==0) {
    printf("Signature test passed\n");
    return 0;
    }
  else {
    printf("Signature test failed\n");
    return 16;
    }
  }
