#ifndef __REL_TRACK_H__
#define __REL_TRACK_H__

#include "rel.h"
#include <vector>
#include <map>

struct fxn_naming_entry
{
  uint32_t m_offset;
  uint8_t  m_section_id;
};

class rel_track
{
public:
  rel_track(linput_t *p_input);

  bool is_good() const;

  section_entry const * get_section(uint entry_id) const;
  ea_t section_address(uint8_t section, uint32_t offset = 0) const;

  bool apply_patches(bool dry_run = false);
private:
  bool read_header();
  bool read_sections();
  bool verify_section(uint32_t offset, uint32_t size) const;

  bool validate_header() const;

  bool create_sections(bool dry_run = false);
  bool apply_relocations(bool dry_run = false);
  bool apply_names(bool dry_run = false);

  //
  uint32_t m_id;
  uint32_t m_version;

  uint32_t m_num_sections;
  uint32_t m_section_offset;

  fxn_naming_entry m_prolog_prep;
  fxn_naming_entry m_epilog_prep;
  fxn_naming_entry m_unresolved_prep;

  uint32_t m_import_offset;
  uint32_t m_import_size;

  uint32_t m_bss_section;
  uint32_t m_bss_size;
  //

  bool m_valid;
  uint32_t m_max_filesize;
  linput_t * m_input_file;

  uint32_t m_next_section_offset;
  uint8_t m_import_section;
  std::map<std::string, std::vector<rel_entry> > m_imports;

  std::vector<section_entry> m_sections;


};

#endif // #ifndef __REL_TRACK_H__
