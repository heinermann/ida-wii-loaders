#include "rel_track.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <utility>
#include <algorithm>

rel_track::rel_track(linput_t *p_input)
 : m_valid(false)
 , m_max_filesize( qlsize(p_input) )
 , m_input_file(p_input)
{
  // Read full header
  if (!this->read_header())
  {
    err_msg("REL: Failed to read the header");
    return;
  }

  // Validate header information
  if (!this->validate_header())
  {
    err_msg("REL: Failed simple header validation");
    return;
  }

  // Read sections
  if (!this->read_sections())
  {
    err_msg("REL: Unable to read all sections");
    return;
  }

  m_valid = true;
}

bool rel_track::read_header()
{
  // Read header data from input
  relhdr base_header;
  qlseek(m_input_file, 0, SEEK_SET);
  if (qlread(m_input_file, &base_header, sizeof(base_header)) != sizeof(base_header))
    return err_msg("REL: header is too short or inaccessible");

  // Convert all members from big endian to little endian
  m_id             = swap32(base_header.info.id);
  m_num_sections   = swap32(base_header.info.num_sections);
  m_section_offset = swap32(base_header.info.section_offset);
  m_version        = swap32(base_header.info.version);

  // This data is currently unhandled
  //m_base_header.info.prev           = swap32(base_header.info.prev);
  //m_base_header.info.next           = swap32(base_header.info.next);
  //m_base_header.info.name_offset    = swap32(base_header.info.name_offset); // ignore
  //m_base_header.info.name_size      = swap32(base_header.info.name_size);   // ignore
  //m_base_header.rel_offset          = swap32(base_header.rel_offset);

  m_import_offset = swap32(base_header.import_offset);
  m_import_size   = swap32(base_header.import_size);
  
  m_bss_section = base_header.bss_section;
  m_bss_size    = swap32(base_header.bss_size);
  
  m_prolog_prep.m_offset     = swap32(base_header.prolog_offset);
  m_prolog_prep.m_section_id = base_header.prolog_section;

  m_epilog_prep.m_offset     = swap32(base_header.epilog_offset);
  m_epilog_prep.m_section_id = base_header.epilog_section;

  m_unresolved_prep.m_offset      = swap32(base_header.unresolved_offset);
  m_unresolved_prep.m_section_id  = base_header.unresolved_section;

  // TODO: v2 check
  //m_base_header.align               = swap32(base_header.align);
  //m_base_header.bss_align           = swap32(base_header.bss_align);
  
  // TODO: v3 check
  //m_base_header.fix_size            = swap32(base_header.fix_size);

  return true;
}

bool rel_track::read_sections()
{
  // Read each section
  qlseek(m_input_file, m_section_offset, SEEK_SET);
  for (unsigned i = 0; i < m_num_sections; ++i)
  {
    // read an entry
    section_entry entry;
    if (qlread(m_input_file, &entry, sizeof(entry)) != sizeof(entry))
      return err_msg("REL: Failed to read section %u", i);

    // Swap endianness
    entry.offset  = swap32(entry.offset);
    entry.size    = swap32(entry.size);

    if (entry.offset == 0 && entry.size != 0)   // bss
    {

    }
    else if (entry.offset != 0 && entry.size != 0)  // valid
    {
      // Verify boundary
      if (!verify_section(entry.offset, entry.size))
        return err_msg("REL: Section is out of bounds");
    }
    m_sections.emplace_back(entry);
  }
  return true;
}

bool rel_track::validate_header() const
{
  // Check for absurd amount of sections
  if (m_num_sections > 32 || m_num_sections <= 1)
    return err_msg("REL: Unlikely number of sections (%u)", m_num_sections);

  // Check section boundary
  if (!verify_section(m_section_offset, m_num_sections*sizeof(section_entry)) )
    return err_msg("REL: Section has overlapping or out of bounds offset (%u entries)", m_num_sections);

  // Check version
  if (m_version <= 0 || m_version > 3)
    return err_msg("REL: Unknown version (%u)", m_version);

  return true;
}

bool rel_track::verify_section(uint32_t offset, uint32_t size) const
{
  offset = SECTION_OFF(offset);
  return sizeof(relhdr) <= offset && (offset + size) <= m_max_filesize;
}

bool rel_track::is_good() const
{
  return m_valid;
}

section_entry const * rel_track::get_section(uint entry_id) const
{
  if (entry_id < m_sections.size())
    return &m_sections[entry_id];

  msg("Attempted to retrieve an invalid section (#%u)", entry_id);
  qexit(1);
  return nullptr;
}

ea_t rel_track::section_address(uint8_t section, uint32_t offset) const
{
  return START + SECTION_OFF(this->get_section(section)->offset) + offset;
}

bool rel_track::apply_patches(bool dry_run)
{
  if ( !this->create_sections(dry_run) )
    return err_msg("Creating sections failed");

  if ( !this->apply_relocations(dry_run) )
    return err_msg("Relocations failed");

  // TODO: Create Imports

  // TODO: Assign function names
  if ( !this->apply_names(dry_run) )
    return err_msg("Naming failed");

  return true;
}


bool rel_track::create_sections(bool dry_run)
{
  m_next_section_offset = 0;

  // Create sections
  for (size_t i = 0; i < m_sections.size(); ++i)
  {
    auto & entry = m_sections[i];

    // Skip unused
    if ( entry.offset == 0 && entry.size == 0 )
      continue;

    std::string type = (entry.offset & SECTION_EXEC) ? CLASS_CODE : CLASS_DATA;
    std::string name = (entry.offset & SECTION_EXEC) ? NAME_CODE : NAME_DATA;
    name += std::to_string(static_cast<unsigned long long>(i));

    uint32_t offset = SECTION_OFF(entry.offset);

    // Create the segment
    if ( offset != 0 )  // known segment
    {
      if ( offset < m_next_section_offset )
        return err_msg("Segments are not linear (seg #%u)", i);
      m_next_section_offset = offset + entry.size;

      if (!add_segm(1, START + offset, START + offset + entry.size, name.c_str(), type.c_str()))
        return err_msg("Failed to create segment #%u", i);

      // Set 32-bit addressing
      set_segm_addressing(getseg(START + offset), 1);

      if (!file2base(m_input_file, offset, START + offset, START + offset + entry.size, FILEREG_PATCHABLE))
        return err_msg("Failed to pull data from file (segment #%u)", i);
    }
    else  // .bss section
    {
      offset = m_next_section_offset;
      m_next_section_offset += entry.size;
      entry.offset = offset;

      if (!add_segm(1, START + offset, START + offset + entry.size, NAME_BSS, CLASS_BSS))
        return err_msg("Failed to create BSS segment #%u", i);

      set_segm_addressing(getseg(START + offset), 1);
    }
  }
  return true;
}
bool rel_track::apply_relocations(bool dry_run)
{
  this->init_resolvers(); // initialize user-names

  // Apply relocations
  if (m_import_offset > 0)
  {
    uint32_t count = m_import_size / sizeof(import_entry);
    uint32_t desired_import_size = 0;
    std::map< std::string, std::map<uint32_t, ea_t> > imports_map;
    std::map< std::string, ea_t > imports_module_starts;

    for (unsigned i = 0; i < count; ++i)
    {
      qlseek(m_input_file, m_import_offset + i*sizeof(import_entry), SEEK_SET);

      // Get the entry
      import_entry entry;
      if (qlread(m_input_file, &entry, sizeof(entry)) != sizeof(entry))
        return err_msg("REL: Failed to read relocation data %u", i);
      // Endianness
      entry.offset = swap32(entry.offset);
      entry.id = swap32(entry.id);

      // Seek to relocations
      qlseek(m_input_file, entry.offset, SEEK_SET);
      uint32_t current_section = 0;
      uint32_t current_offset = 0;
      uint32_t value = 0, where = 0, orig = 0;

      // Self-relocations
      if ( entry.id == m_id )
      {
        for (;;)
        {
          // Read operation
          rel_entry rel;
          if (qlread(m_input_file, &rel, sizeof(rel)) != (sizeof(rel)))
            return err_msg("REL: Failed to read relocation operation @0x%08X - error code: %d", qltell(m_input_file), get_qerrno());

          // endianness
          rel.addend = swap32(rel.addend);
          rel.offset = swap16(rel.offset);

          // Kill if it's the end
          if (rel.type == R_DOLPHIN_END)
            break;

          current_offset += rel.offset;
          switch (rel.type)
          {
          case R_DOLPHIN_SECTION:
            current_section = rel.section;
            current_offset  = 0;
            break;
          case R_DOLPHIN_NOP:
            break;
          case R_PPC_ADDR32:
            patch_long( this->section_address(current_section, current_offset), this->section_address(rel.section, rel.addend) );
            break;
          case R_PPC_ADDR16_LO:
            patch_word(this->section_address(current_section, current_offset), this->section_address(rel.section, rel.addend) & 0xFFFF);
            break;
          case R_PPC_ADDR16_HA:
            value = this->section_address(rel.section, rel.addend);
            if ((value & 0x8000) == 0x8000)
              value += 0x00010000;

            patch_word(this->section_address(current_section, current_offset), (value >> 16) & 0xFFFF);
            break;
          case R_PPC_REL24:
            where = this->section_address(current_section, current_offset);
            value = this->section_address(rel.section, rel.addend);
            value -= where;
            orig = static_cast<uint32_t>(get_original_long(where));
            orig &= 0xFC000003;
            orig |= value & 0x03FFFFFC;
            patch_long(where, orig);
            break;
          default:
            msg("REL: RELOC TYPE %u UNSUPPORTED\n", rel.type);
          }

        }
      }
      else // EXTERNALS
      {
        // Retrieve the module name
        std::string imp_module_name = "base";
        auto it_modname = m_module_names.find(entry.id);
        if ( it_modname != m_module_names.end() )
          imp_module_name = it_modname->second;
        else
          imp_module_name = std::string("module") + std::to_string(static_cast<unsigned long long>(entry.id));

        // Read all imports to get the desired size
        for (;;)
        {
          // Read operation
          rel_entry rel;
          if ( qlread(m_input_file, &rel, sizeof(rel)) != (sizeof(rel)))
            return err_msg("REL: Failed to read relocation operation @0x%08X, id %u", qltell(m_input_file), entry.id);

          // endianness
          rel.addend = swap32(rel.addend);
          rel.offset = swap16(rel.offset);

          // Kill if it's the end
          if (rel.type == R_DOLPHIN_END)
            break;

          if ( rel.type != R_DOLPHIN_SECTION && rel.type != R_DOLPHIN_NOP )
          {
            // If the address was not already found, then get the next import location
            ea_t target_offset = START + m_next_section_offset + desired_import_size;
            if ( imports_map[imp_module_name].insert( std::make_pair(rel.addend, target_offset) ).second )
            {
              imports_module_starts.insert( std::make_pair(imp_module_name, target_offset) );
              desired_import_size += 4;
            }
          }

          m_imports[imp_module_name].emplace_back(rel);
        }
      }
    } // for each module
    
    // Now create the import/externals section
    section_entry import_section = { m_next_section_offset, desired_import_size };
    m_next_section_offset += desired_import_size;

    if (!add_segm(1, START + import_section.offset, START + import_section.offset + import_section.size, NAME_EXTERN, CLASS_EXTERN))
      return err_msg("Failed to create XTRN segment");
    set_segm_addressing(getseg(START + import_section.offset), 1);
    
    m_import_section = static_cast<uint8_t>(m_sections.size());
    m_sections.emplace_back(import_section);

    // Add and parse imports
    //ea_t targ_offset = this->section_address(m_import_section);
    for ( auto it = m_imports.begin(); it != m_imports.end(); ++it )
    {
      // Add comment for module
      ea_t target_module_start = imports_module_starts[it->first];
      if ( target_module_start == 0 )
        return err_msg("Failed to locate start of module imports.");
      add_long_cmt( target_module_start, true, "\nImports from %s\n", it->first.c_str() );

      // Iterate relocation opcodes
      uint32_t current_offset = 0, current_section = 0;
      for ( auto e = it->second.begin(); e != it->second.end(); ++e )
      {
        ea_t targ_offset; // this must be initialized for anything that isn't DOLPHIN_SECTION or DOLPHIN_NOP
        
        // If something is actually going to be done with the target
        if ( e->type != R_DOLPHIN_SECTION && e->type != R_DOLPHIN_NOP )
        {
          // Retrieve the target offset for the import
          targ_offset = imports_map[it->first][e->addend];
          if ( targ_offset == 0 )
            return err_msg("Import was not mapped correctly. %s %08X", it->first.c_str(), e->addend);

          // Name the import
          std::ostringstream ss;
          ss << it->first.c_str() << '_' << reinterpret_cast<void*>(e->addend);
          do_name_anyway(targ_offset, ss.str().c_str());
        }

        current_offset += e->offset;
        switch (e->type)
        {
        case R_DOLPHIN_SECTION:
          current_section = e->section;
          current_offset  = 0;
          break;
        case R_DOLPHIN_NOP:
          break;
        case R_PPC_ADDR32:
        {
          patch_long( this->section_address(current_section, current_offset), targ_offset );
          put_long(targ_offset, e->addend);
          break;
        }
        case R_PPC_ADDR16_LO:
        {
          patch_word(this->section_address(current_section, current_offset), targ_offset & 0xFFFF);
          put_long(targ_offset, e->addend);
          break;
        }
        case R_PPC_ADDR16_HA:
        {
          ea_t value = targ_offset;
          if ((value & 0x8000) == 0x8000)
            value += 0x00010000;

          patch_word(this->section_address(current_section, current_offset), (value >> 16) & 0xFFFF);
          put_long(targ_offset, e->addend);
          break;
        }
        case R_PPC_REL24:
        {
          ea_t where = this->section_address(current_section, current_offset);
          ea_t value = targ_offset;
          value -= where;
          uint32_t orig = static_cast<uint32_t>(get_original_long(where));
          orig &= 0xFC000003;
          orig |= value & 0x03FFFFFC;
          patch_long(where, orig);
          break;
        }
        default:
          msg("REL: XTRN RELOC TYPE %u UNSUPPORTED\n", static_cast<unsigned int>(e->type));
        }
      }
    } // for each import
  }
  return true;
}

bool rel_track::apply_names(bool dry_run)
{
  // Obtain addresses
  ea_t epilog_addr = section_address(m_epilog_prep.m_section_id, m_epilog_prep.m_offset);
  ea_t prolog_addr = section_address(m_prolog_prep.m_section_id, m_prolog_prep.m_offset);
  ea_t unresolved_addr = section_address(m_unresolved_prep.m_section_id, m_unresolved_prep.m_offset);

  // Make functions
  auto_make_proc(epilog_addr);
  auto_make_proc(prolog_addr);
  auto_make_proc(unresolved_addr);

  set_name(epilog_addr, "epilog", SN_NOCHECK | SN_PUBLIC);
  set_name(prolog_addr, "prolog", SN_NOCHECK | SN_PUBLIC);
  set_name(unresolved_addr, "unresolved", SN_NOCHECK | SN_PUBLIC);

  // TODO: Make exports

  set_libitem(epilog_addr);
  set_libitem(prolog_addr);
  set_libitem(unresolved_addr);

  return true;
}

void rel_track::init_resolvers()
{
  uint32_t id;
  std::string name, path;
  
  // Retrieve the directory of the current database
  char dir[260] = {};
  if ( !qdirname(dir, sizeof(dir), database_idb) )
    msg("REL: Unable to get directory of idb file.\n");
  path = dir;

  // Load the module names
  m_module_names.clear();
  std::ifstream modid(path + "/module_id.txt");
  while( modid >> std::hex >> id >> name )
    m_module_names[id] = name;

  // Load the function names
  // TODO: load map files matching module names
}
