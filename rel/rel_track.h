#ifndef __REL_TRACK_H__
#define __REL_TRACK_H__

#include "rel.h"
#include <vector>
#include <map>

#define BASENAME "_BASE_"

struct fxn_naming_entry
{
  uint32_t m_offset;
  uint8_t  m_section_id;
};

class rel_track
{
public:
  rel_track();
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

  // Initializes the name and module resolvers
  void init_resolvers();

  uint32_t get_external_offset(std::string const &modulename, uint32_t offset, uint8_t section) const;

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

  uint8_t m_bss_section_ign;
  uint32_t m_bss_size;

  uint32_t m_rel_offset;
  //

  bool m_valid;
  uint32_t m_max_filesize;
  linput_t * m_input_file;

  uint32_t m_next_section_offset;
  uint8_t m_import_section;
  uint8_t m_internal_bss_section;
  std::map<std::string, std::vector<rel_entry> > m_imports;

  std::vector<section_entry> m_sections;

  std::map<uint32_t,std::string> m_module_names;
  std::map<uint32_t, std::map<uint32_t,std::string> > m_function_names;

  std::map<std::string, rel_track> m_external_modules;

  friend int idaapi enum_modules_cb(char const * file, rel_track * owner);
};

#endif // #ifndef __REL_TRACK_H__
