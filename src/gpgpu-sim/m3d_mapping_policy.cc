// Copyright (c) 2026
// All rights reserved.

#include "m3d_mapping_policy.h"

#include <stdio.h>
#include <fstream>
#include <sstream>

#include "gpu-sim.h"

m3d_mapping_policy::m3d_mapping_policy(const memory_config *config) {
  m_config = config;
  m_enabled = config->gpgpu_m3d_mc_map_enable;
  m_mode = config->gpgpu_m3d_mc_map_mode;
  m_default_policy = M3D_MC_MAP_ROW_LOCAL;

  if (m_enabled && config->gpgpu_m3d_mc_policy_table_file &&
      config->gpgpu_m3d_mc_policy_table_file[0] != '\0') {
    load_region_table(config->gpgpu_m3d_mc_policy_table_file);
  }
}

void m3d_mapping_policy::load_region_table(const char *path) {
  std::ifstream in(path);
  if (!in.good()) {
    fprintf(stdout,
            "GPGPU-Sim m3d: unable to open policy table file '%s', fallback to "
            "global mode\n",
            path);
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string s0, s1, s2;
    if (!std::getline(ss, s0, ',')) continue;
    if (!std::getline(ss, s1, ',')) continue;
    if (!std::getline(ss, s2, ',')) continue;

    region_policy_t entry;
    std::stringstream(s0) >> std::hex >> entry.start;
    std::stringstream(s1) >> std::hex >> entry.end;
    std::stringstream(s2) >> entry.policy_id;
    if (entry.start <= entry.end) m_regions.push_back(entry);
  }
}

unsigned m3d_mapping_policy::resolve_policy_id(new_addr_type addr) const {
  if (m_mode != M3D_MC_MAP_REGION_TABLE) return m_mode;

  for (size_t i = 0; i < m_regions.size(); ++i) {
    if (addr >= m_regions[i].start && addr <= m_regions[i].end) {
      return m_regions[i].policy_id;
    }
  }
  return m_default_policy;
}

unsigned m3d_mapping_policy::map_bank(new_addr_type line_addr,
                                      unsigned policy_id,
                                      unsigned num_banks) const {
  if (num_banks == 0) return 0;
  const unsigned row_chunk_lines = 64;
  switch (policy_id) {
    case M3D_MC_MAP_ROW_LOCAL:
      return (line_addr / row_chunk_lines) % num_banks;
    case M3D_MC_MAP_BANK_BALANCED: {
      new_addr_type x = line_addr ^ (line_addr >> 7) ^ (line_addr >> 13);
      return x % num_banks;
    }
    case M3D_MC_MAP_HYBRID: {
      new_addr_type coarse = line_addr / row_chunk_lines;
      new_addr_type fine = line_addr ^ (line_addr >> 9);
      return (coarse ^ fine) % num_banks;
    }
    default:
      return line_addr % num_banks;
  }
}

unsigned m3d_mapping_policy::map_tier_tag(new_addr_type line_addr,
                                          unsigned policy_id) const {
  switch (policy_id) {
    case M3D_MC_MAP_ROW_LOCAL:
      return (line_addr >> 6) & 0x3;
    case M3D_MC_MAP_BANK_BALANCED:
      return (line_addr ^ (line_addr >> 5)) & 0x3;
    case M3D_MC_MAP_HYBRID:
      return ((line_addr >> 6) ^ (line_addr >> 3)) & 0x3;
    default:
      return 0;
  }
}

m3d_mapping_result_t m3d_mapping_policy::apply(new_addr_type addr,
                                               const addrdec_t &base,
                                               unsigned num_banks) const {
  m3d_mapping_result_t ret;
  ret.bank = base.bk;
  ret.row = base.row;
  ret.col = base.col;
  ret.tier_tag = 0;
  ret.policy_id = resolve_policy_id(addr);

  if (!m_enabled) return ret;

  const new_addr_type line_addr = addr >> 7;  // aligned to 128B granularity
  ret.bank = map_bank(line_addr, ret.policy_id, num_banks);
  ret.tier_tag = map_tier_tag(line_addr, ret.policy_id);
  return ret;
}
