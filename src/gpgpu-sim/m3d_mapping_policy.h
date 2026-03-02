// Copyright (c) 2026
// All rights reserved.

#ifndef M3D_MAPPING_POLICY_H
#define M3D_MAPPING_POLICY_H

#include <string>
#include <vector>

#include "../abstract_hardware_model.h"
#include "addrdec.h"

class memory_config;

enum m3d_mc_map_mode_t {
  M3D_MC_MAP_ROW_LOCAL = 0,
  M3D_MC_MAP_BANK_BALANCED = 1,
  M3D_MC_MAP_HYBRID = 2,
  M3D_MC_MAP_REGION_TABLE = 3
};

struct m3d_mapping_result_t {
  unsigned bank;
  unsigned row;
  unsigned col;
  unsigned tier_tag;
  unsigned policy_id;
};

class m3d_mapping_policy {
 public:
  explicit m3d_mapping_policy(const memory_config *config);

  bool enabled() const { return m_enabled; }
  m3d_mapping_result_t apply(new_addr_type addr, const addrdec_t &base,
                             unsigned num_banks) const;

 private:
  struct region_policy_t {
    new_addr_type start;
    new_addr_type end;
    unsigned policy_id;
  };

  void load_region_table(const char *path);
  unsigned resolve_policy_id(new_addr_type addr) const;
  unsigned map_bank(new_addr_type line_addr, unsigned policy_id,
                    unsigned num_banks) const;
  unsigned map_tier_tag(new_addr_type line_addr, unsigned policy_id) const;

 private:
  const memory_config *m_config;
  bool m_enabled;
  unsigned m_mode;
  unsigned m_default_policy;
  std::vector<region_policy_t> m_regions;
};

#endif  // M3D_MAPPING_POLICY_H
