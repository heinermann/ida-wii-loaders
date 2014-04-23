/*
*  IDA Nintendo GameCube RSO Loader Module
*  (C) Copyright 2010 Stephen Simpson
*
*/

#ifndef __REL_H__
#define __REL_H__

//#define DEBUG
#define START  0x80500000

#include "../loader/idaloader.h"

#include <cstdint>
#include <string>


typedef struct {
  void * head;
  void * tail;
} queue_t;

typedef struct {
  void * next;
  void * prev;
} link_t;

typedef struct {
  uint32_t align;
  uint32_t bssAlign;
} module_v2;

typedef struct {
  uint32_t fixSize;
} module_v3;

typedef struct {
  uint32_t id;          // in .rso or .rel, not in .sel

  // in .rso or .rel or .sel
  uint32_t prev;
  uint32_t next;
  uint32_t num_sections;
  uint32_t section_offset;    // points to section_entry*
  uint32_t name_offset;
  uint32_t name_size;
  uint32_t version;
} relhdr_info;

typedef struct {
  relhdr_info info;

  // version 1
  uint32_t bss_size;
  uint32_t rel_offset;
  uint32_t import_offset;
  uint32_t import_size;         // size in bytes

  // Section ids containing functions
  uint8_t prolog_section;
  uint8_t epilog_section;
  uint8_t unresolved_section;
  uint8_t bss_section;

  uint32_t prolog_offset;
  uint32_t epilog_offset;
  uint32_t unresolved_offset;

  // version 2
  uint32_t align;
  uint32_t bss_align;

  // version 3
  uint32_t fix_size;
} relhdr;


typedef struct {
  uint32_t offset;
  uint32_t size;
} section_entry;

typedef struct {
  uint32_t id;      // module id, maps to id in relhdr_info, 0 = base application
  uint32_t offset;
} import_entry;

#define SECTION_EXEC 0x1
#define SECTION_OFF(off) (off&~1)

typedef struct {
  uint16_t offset; // byte offset from previous entry
  uint8_t  type;
  uint8_t  section;
  uint32_t addend;
} rel_entry;


#define R_PPC_NONE            0
#define R_PPC_ADDR32          1     /* S + A */
#define R_PPC_ADDR24          2     /* (S + A) >> 2 */
#define R_PPC_ADDR16          3     /* S + A */
#define R_PPC_ADDR16_LO       4
#define R_PPC_ADDR16_HI       5
#define R_PPC_ADDR16_HA       6
#define R_PPC_ADDR14          7
#define R_PPC_ADDR14_BRTAKEN  8
#define R_PPC_ADDR14_BRNTAKEN 9
#define R_PPC_REL24           10   /* (S + A - P) >> 2 */
#define R_PPC_REL14           11

#define R_DOLPHIN_NOP     201 // C9h current offset += rel.offset
#define R_DOLPHIN_SECTION 202 // CAh current offset = rel.section
#define R_DOLPHIN_END     203 // CBh
#define R_DOLPHIN_MRKREF  204 // CCh


inline void dbg_msg(const char *format, ...)
{
#ifdef DEBUG
  va_list va;
  va_start(va, format);
  int nbytes = vmsg(format, va);
  va_end(va);
#endif
}

inline bool err_msg(const char *format, ...)
{
  va_list va;
  va_start(va, format);
  
  std::string fmt_nl = format;
  fmt_nl += "\n";

  int nbytes = vmsg(fmt_nl.c_str(), va);
  va_end(va);
  return false;
}

#endif