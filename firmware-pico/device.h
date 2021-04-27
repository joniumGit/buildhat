#pragma once

#include "hardware.h"

#define MAXNMODES 16

struct modeinfo {
  char name[129];
  float rawl,rawh;
  float pctl,pcth;
  float sil,sih;
  unsigned char prefmap[2];
  char symbol[9];
  int format_count,format_type,format_chars,format_dp;
  };

extern struct devinfo {
  int connected;                                   // following data are valid only when connected==1
  int signature;                                   // for passive IDs
  int id;                                          // 1..11 for passive IDs; 0 for active ID
  int buttonpressed;                               // button pressed flag for ID 5
  int type;                                        // from TYPE message for active IDs; -1 if unknown
  int nmodes;                                      // number of modes
  int nview;                                       // number of modes in view
  int baud;                                        // fast baud rate
  unsigned int hwver,swver;                        // hardware and software versions
  struct modeinfo modes[MAXNMODES];
  int pospid[4];                                   // position PID constants reported by device
  int speedpid[4];                                 // speed PID constants reported by device
  unsigned char modedata[MAXNMODES][128];
  int modedatavalid[MAXNMODES];                    // has modedata[i] ever been written to?
  } devinfo[NPORTS];

extern void device_init(int dn);
extern void device_dump(int dn);
