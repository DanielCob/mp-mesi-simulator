#ifndef DEBUG_DEBUG_H
#define DEBUG_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "isa.h"
#include "bus.h"
#include "cache.h"
#include "memory.h"
#include "pe.h"

#ifdef __cplusplus
extern "C" {
#endif

void dbg_init(void);
void dbg_start_cli(void);
void dbg_shutdown(void);

bool dbg_enabled(void);

// Hooks (cheap no-ops when disabled)
void dbg_before_instruction(int pe_id, uint64_t pc, const Instruction* insn);
void dbg_on_bus_event(int type, uint64_t addr, int src_pe, int invalidations);
void dbg_on_mesi_transition(int pe_id, uint64_t addr, MESI_State from, MESI_State to);

// Provide simulator context to debugger for info/stats commands
// Provide simulator context to debugger for info/stats commands
// Pass pointers to bus, caches, PEs, and memory (not owned by debugger)
void dbg_register_context(Bus* bus, Cache* caches, int num_pes, PE* pes, Memory* mem);

#ifdef __cplusplus
}
#endif

#endif // DEBUG_DEBUG_H
