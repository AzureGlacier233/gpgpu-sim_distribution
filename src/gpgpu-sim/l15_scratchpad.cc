// Copyright (c) 2026
// All rights reserved.

#include "l15_scratchpad.h"

#include <assert.h>

#include "shader.h"

l15_scratchpad::l15_scratchpad(const shader_core_config *config) {
  m_config = config;
  m_enable = config->gpgpu_l15_enable;
  m_line_size = config->gpgpu_l15_line_size;
  m_num_lines = 0;
  if (m_line_size > 0) {
    m_num_lines = (config->gpgpu_l15_size_per_cluster_kb * 1024) / m_line_size;
  }
  m_num_banks = config->gpgpu_l15_banks ? config->gpgpu_l15_banks : 1;
  m_policy_mode = config->gpgpu_l15_policy_mode;
  m_bank_busy_until.resize(m_num_banks, 0);

  m_last_cycle = 0;
  m_accesses = 0;
  m_hits = 0;
  m_misses = 0;
  m_reservation_fails = 0;
  m_bank_conflicts = 0;
}

new_addr_type l15_scratchpad::line_addr(new_addr_type addr) const {
  return addr / m_line_size;
}

unsigned l15_scratchpad::policy_bank(new_addr_type line_addr) const {
  const unsigned row_chunk_lines = 64;
  switch (m_policy_mode) {
    case L15_POLICY_ROW_LOCAL:
      return (line_addr / row_chunk_lines) % m_num_banks;
    case L15_POLICY_BANK_BALANCED: {
      new_addr_type x = line_addr ^ (line_addr >> 7) ^ (line_addr >> 13);
      return x % m_num_banks;
    }
    case L15_POLICY_HYBRID: {
      new_addr_type coarse = line_addr / row_chunk_lines;
      new_addr_type fine = line_addr ^ (line_addr >> 9);
      return (coarse ^ fine) % m_num_banks;
    }
    default:
      return line_addr % m_num_banks;
  }
}

unsigned l15_scratchpad::line_bank(new_addr_type line_addr) const {
  return policy_bank(line_addr);
}

void l15_scratchpad::touch_line(new_addr_type line_addr) {
  std::map<new_addr_type, std::list<new_addr_type>::iterator>::iterator it =
      m_lru_index.find(line_addr);
  if (it == m_lru_index.end()) return;
  m_lru.erase(it->second);
  m_lru.push_front(line_addr);
  it->second = m_lru.begin();
}

void l15_scratchpad::insert_line(new_addr_type line_addr) {
  if (m_num_lines == 0) return;

  std::map<new_addr_type, std::list<new_addr_type>::iterator>::iterator it =
      m_lru_index.find(line_addr);
  if (it != m_lru_index.end()) {
    touch_line(line_addr);
    return;
  }

  if (m_lru_index.size() >= m_num_lines) {
    new_addr_type victim = m_lru.back();
    m_lru.pop_back();
    m_lru_index.erase(victim);
  }

  m_lru.push_front(line_addr);
  m_lru_index[line_addr] = m_lru.begin();
}

enum cache_request_status l15_scratchpad::access(new_addr_type addr,
                                                 bool is_write,
                                                 unsigned long long cycle) {
  (void)is_write;
  m_last_cycle = cycle;
  if (!is_enabled()) return MISS;

  m_accesses++;
  new_addr_type laddr = line_addr(addr);
  unsigned bank = line_bank(laddr);
  assert(bank < m_num_banks);

  if (m_bank_busy_until[bank] > cycle) {
    m_reservation_fails++;
    m_bank_conflicts++;
    return RESERVATION_FAIL;
  }

  m_bank_busy_until[bank] = cycle + 1;
  if (m_lru_index.find(laddr) != m_lru_index.end()) {
    m_hits++;
    touch_line(laddr);
    return HIT;
  }

  m_misses++;
  return MISS;
}

void l15_scratchpad::fill(new_addr_type addr, unsigned long long cycle) {
  m_last_cycle = cycle;
  if (!is_enabled()) return;
  insert_line(line_addr(addr));
}

void l15_scratchpad::cycle(unsigned long long cycle) { m_last_cycle = cycle; }
