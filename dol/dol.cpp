/*
 *  IDA Nintendo GameCube DOL Loader Module
 *  (C) Copyright 2004 by Stefan Esser
 *
 */

#include "../loader/idaloader.h"
#include "dol.h"

/*--------------------------------------------------------------------------
 *
 *   Read the header of the (possible) DOL file into memory. Swap all bytes
 *   because the file is stored as big endian.
 *
 */

int read_header(linput_t *fp, dolhdr *dhdr)
{
  int i;

  // read in dolheader
  qlseek(fp, 0, SEEK_SET);
  if(qlread(fp, dhdr, sizeof(dolhdr)) != sizeof(dolhdr)) return(0);

  // convert header
  for (i=0; i<7; i++) {
    dhdr->offsetText[i] = swap32(dhdr->offsetText[i]);
    dhdr->addressText[i] = swap32(dhdr->addressText[i]);
    dhdr->sizeText[i] = swap32(dhdr->sizeText[i]);
  }
  for (i=0; i<11; i++) {
    dhdr->offsetData[i] = swap32(dhdr->offsetData[i]);
    dhdr->addressData[i] = swap32(dhdr->addressData[i]);
    dhdr->sizeData[i] = swap32(dhdr->sizeData[i]);
  }
  dhdr->entrypoint = swap32(dhdr->entrypoint);
  dhdr->sizeBSS = swap32(dhdr->sizeBSS);
  dhdr->addressBSS = swap32(dhdr->addressBSS);
  return(1);
}

/*--------------------------------------------------------------------------
 *
 *   Check if input file can be a DOL file. Therefore the supposed header
 *   is checked for sanity. If it passes return and fill in the formatname
 *   otherwise return 0
 *
 */

int idaapi accept_file(linput_t *fp, char fileformatname[MAX_FILE_FORMAT_NAME], int n)
{
  int i;

  dolhdr dhdr;
  ulong filelen, valid = 0;

  if(n) return(0);

  // first get the lenght of the file
  
  filelen = qlsize(fp);
  // if to short for a DOL header then this is no DOL
  if (filelen < 0x100) return(0);

  // read DOL header from file
  if (read_header(fp, &dhdr)==0) return(0);
  
  // now perform some sanitychecks
  for (i=0; i<7; i++) {
    
    // DOL segment MAY NOT physically stored in the header
    if (dhdr.offsetText[i]!=0 && dhdr.offsetText[i]<0x100) return(0);
    // end of physical storage must be within file
    if (dhdr.offsetText[i]+dhdr.sizeText[i]>filelen) return(0);
    // we only accept DOLs with segments above 2GB
    if (dhdr.addressText[i] != 0 && !(dhdr.addressText[i] & 0x80000000)) return(0);

    // remember that entrypoint was in a code segment
    if (dhdr.entrypoint >= dhdr.addressText[i] && dhdr.entrypoint < dhdr.addressText[i]+dhdr.sizeText[i]) valid = 1;
  }
  for (i=0; i<11; i++) {

    // DOL segment MAY NOT physically stored in the header
    if (dhdr.offsetData[i]!=0 && dhdr.offsetData[i]<0x100) return(0);
    // end of physical storage must be within file
    if (dhdr.offsetData[i]+dhdr.sizeData[i]>filelen) return(0);
    // we only accept DOLs with segments above 2GB
    if (dhdr.addressData[i] != 0 && !(dhdr.addressData[i] & 0x80000000)) return(0);
  }
  
  // if there is a BSS segment it must be above 2GB, too
  if (dhdr.addressBSS != 0 && !(dhdr.addressBSS & 0x80000000)) return(0);
  
  // if entrypoint is not within a code segment reject this file
  if (!valid) return(0);

  // file has passed all sanity checks and might be a DOL
  qstrncpy(fileformatname, "Nintendo GameCube DOL", MAX_FILE_FORMAT_NAME);
  return(ACCEPT_FIRST | 0xD07);
}



/*--------------------------------------------------------------------------
 *
 *   File was recognised as DOL and user has selected it. Now load it into
 *   the database
 *
 */

void idaapi load_file(linput_t *fp, ushort /*neflag*/, const char * /*fileformatname*/)
{
  dolhdr dhdr;
  uint snum;
  int i;

  // Hello here I am
  msg("---------------------------------------\n");
  msg("Nintendo GameCube DOL Loader Plugin 0.1\n");
  msg("---------------------------------------\n");
  
  // we need PowerPC support otherwise we cannot do much with DOLs
  if ( ph.id != PLFM_PPC )
    set_processor_type("PPC", SETPROC_ALL|SETPROC_FATAL);

  // read DOL header into memory
  if (read_header(fp, &dhdr)==0) qexit(1);
  
  // every journey has a beginning
  inf.beginEA = inf.startIP = dhdr.entrypoint;

  // map selector 1 to 0
  set_selector(1, 0);

  // create all code segments
  for (i=0, snum=1; i<7; i++, snum++) {
    char buf[50];
    
    // 0 == no segment
    if (dhdr.addressText[i] == 0) continue;
    
    // create a name according to segmenttype and number
    sprintf(buf, NAME_CODE "%u", snum);
    
    // add the code segment
    if (!add_segm(1, dhdr.addressText[i], dhdr.addressText[i]+dhdr.sizeText[i]-1, buf, CLASS_CODE)) qexit(1);
    
    // set addressing to 32 bit
    set_segm_addressing(getseg(dhdr.addressText[i]), 1);

    // and get the content from the file
    file2base(fp, dhdr.offsetText[i], dhdr.addressText[i], dhdr.addressText[i]+dhdr.sizeText[i]-1, FILEREG_PATCHABLE);
  }

  // create all data segments
  for (i=0, snum=1; i<11; i++, snum++) {
    char buf[50];

    // 0 == no segment
    if (dhdr.addressData[i] == 0) continue;

    // create a name according to segmenttype and number
    sprintf(buf, NAME_DATA "%u", snum);

    // add the data segment
    if (!add_segm(1, dhdr.addressData[i], dhdr.addressData[i]+dhdr.sizeData[i]-1, buf, CLASS_DATA)) qexit(1);
    
    // set addressing to 32 bit
    set_segm_addressing(getseg(dhdr.addressData[i]), 1);

    // and get the content from the file
    file2base(fp, dhdr.offsetData[i], dhdr.addressData[i], dhdr.addressData[i]+dhdr.sizeData[i]-1, FILEREG_PATCHABLE);
  }

  // is there a BSS defined?
  if (dhdr.addressBSS != NULL) {
    // then add it
    if(!add_segm(1, dhdr.addressBSS, dhdr.addressBSS+dhdr.sizeBSS-1, NAME_BSS, CLASS_BSS)) qexit(1);

    // and set addressing mode to 32 bit
    set_segm_addressing(getseg(dhdr.addressBSS), 1);
  }
 
}

/*--------------------------------------------------------------------------
 *
 *   Loader Module Descriptor Blocks
 *
 */

extern "C" loader_t LDSC = {
  IDP_INTERFACE_VERSION,
  0, /* no loader flags */
  accept_file,
  load_file,
  NULL,
};
