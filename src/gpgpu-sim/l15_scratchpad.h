// Copyright (c) 2026
// All rights reserved.

#ifndef L15_SCRATCHPAD_H
#define L15_SCRATCHPAD_H

#include <list>
#include <map>
#include <vector>

#include "../abstract_hardware_model.h"
#include "gpu-cache.h"

class shader_core_config;

enum l15_policy_mode_t {
  L15_POLICY_ROW_LOCAL = 0,
  L15_POLICY_BANK_BALANCED = 1,
  L15_POLICY_HYBRID = 2
};

class l15_scratchpad {
 public:
  explicit l15_scratchpad(const shader_core_config *config);

  enum cache_request_status access(new_addr_type addr, bool is_write,
                                   unsigned long long cycle);
  void fill(new_addr_type addr, unsigned long long cycle);
  void cycle(unsigned long long cycle);

  unsigned long long accesses() const { return m_accesses; }
  unsigned long long hits() const { return m_hits; }
  unsigned long long misses() const { return m_misses; }
  unsigned long long reservation_fails() const { return m_reservation_fails; }
  unsigned long long bank_conflicts() const { return m_bank_conflicts; }

 private:
  unsigned line_bank(new_addr_type line_addr) const;
  unsigned policy_bank(new_addr_type line_addr) const;
  new_addr_type line_addr(new_addr_type addr) const;
  bool is_enabled() const { return m_enable && m_num_lines > 0; }
  void touch_line(new_addr_type line_addr);
  void insert_line(new_addr_type line_addr);

 private:
  const shader_core_config *m_config;

  bool m_enable;
  unsigned m_line_size;
  unsigned m_num_lines;
  unsigned m_num_banks;
  unsigned m_policy_mode;

  std::list<new_addr_type> m_lru;
  std::map<new_addr_type, std::list<new_addr_type>::iterator> m_lru_index;
  std::vector<unsigned long long> m_bank_busy_until;

  unsigned long long m_last_cycle;
  unsigned long long m_accesses;
  unsigned long long m_hits;
  unsigned long long m_misses;
  unsigned long long m_reservation_fails;
  unsigned long long m_bank_conflicts;
};

#endif  // L15_SCRATCHPAD_H
