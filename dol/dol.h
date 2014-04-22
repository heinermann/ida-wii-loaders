/*
 *  IDA Nintendo GameCube DOL Loader Module
 *  (C) Copyright 2004 by Stefan Esser
 *
 */

#ifndef __DOL_H__
#define __DOL_H__

/* Header Size = 100h bytes 

    0000-001B  Text[0..6] sections File Positions
    001C-0047  Data[0..10] sections File Positions 
    0048-0063  Text[0..6] sections Mem Address
    0064-008F  Data[0..10] sections Mem Address
    0090-00AB  Text[0..6] sections Sizes
    00AC-00D7  Data[0..10] sections Sizes
         00D8  BSS Mem address
         00DC  BSS Size
         00E0  Entry Point
    
    0100-....  Start of sections datas (body)
*/

typedef struct {
  unsigned int offsetText[7];
  unsigned int offsetData[11];
  unsigned int addressText[7];
  unsigned int addressData[11];
  unsigned int sizeText[7];
  unsigned int sizeData[11];
  unsigned int addressBSS;
  unsigned int sizeBSS;
  unsigned int entrypoint;
} dolhdr;

#endif