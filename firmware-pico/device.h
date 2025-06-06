// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "hardware.h"
#include "ports.h"

#define MAXNMODES 16
#define MAXNCOMBIS 16                              // because the COMBI message fits in 32 bytes, one short for each combi

struct modeinfo {
  char name[129];
  float rawl,rawh;
  float pctl,pcth;
  float sil,sih;
  unsigned char prefmap[2];
  char symbol[9];
  int format_count,format_type,format_chars,format_dp;
  int combi_count;                                 // 0 if this is a normal mode
  UC combi_mode[128];                              // the modes and datasets comprising this combi
  UC combi_dataset[128];
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
  int ncombis;                                     // number of combis
  unsigned short validcombis[MAXNCOMBIS];
  int pospid[4];                                   // position PID constants reported by device
  int speedpid[4];                                 // speed PID constants reported by device
  unsigned char modedata[MAXNMODES][128];          // raw mode data
  int modedatalen[MAXNMODES];                      // number of bytes received; 0 if modedata[i] has never been written to
  } devinfo[NPORTS];

extern void device_init(int dn);
extern void device_dump(int dn);
extern int device_getsvar(struct svar*sv,int diff,float*var);
extern void device_dumpmodevar(int port,int mode,int offset,int format);
extern void device_dumpmoderaw(int port,int mode);
extern void device_dumpmodefmt(int port,int mode);
