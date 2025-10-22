#define _GNU_SOURCE
#include "debug.h"
#include "log.h"
#include "isa.h"
#include "bus.h"
#include "memory.h"
#include "pe.h"
#include "bus_stats.h"
#include "cache_stats.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

typedef struct {
    bool enabled;
    bool paused;            // global pause
    int step_global;        // remaining global steps
    int step_pe[16];        // per-PE steps (adjust if needed)
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    pthread_t       cli_thr;
    bool cli_running;
} Debugger;

static Debugger G = {
    .enabled = false, .paused = false,
    .step_global = 0, .step_pe = {0},
    .mtx = PTHREAD_MUTEX_INITIALIZER,
    .cv  = PTHREAD_COND_INITIALIZER,
    .cli_running = false
};

bool dbg_enabled(void) { return G.enabled; }

// Simulator context (shallow pointers, not owned)
static Bus*     G_bus  = NULL;
static Cache*   G_caches = NULL;
static int      G_num_pes = 0;
static PE*  G_pes = NULL;
static Memory*  G_mem  = NULL;

// Simple PC breakpoints: store up to N per PE
#define MAX_BP 64
typedef struct { bool used; int pe; uint64_t pc; } PcBp;
static PcBp G_bp[MAX_BP];

void dbg_register_context(Bus* bus, Cache* caches, int num_pes, PE* pes, Memory* mem) {
    G_bus = bus;
    G_caches = caches;
    G_num_pes = num_pes;
    G_pes = pes;
    G_mem = mem;
}

static int bp_add_pc(int pe, uint64_t pc) {
    for (int i = 0; i < MAX_BP; i++) if (!G_bp[i].used) {
        G_bp[i].used = true; G_bp[i].pe = pe; G_bp[i].pc = pc; return i;
    }
    return -1;
}
static void bp_list(void) {
    for (int i = 0; i < MAX_BP; i++) if (G_bp[i].used) {
        printf("  #%d pc PE%d@%lu\n", i, G_bp[i].pe, (unsigned long)G_bp[i].pc);
    }
    fflush(stdout);
}
static bool bp_match(int pe, uint64_t pc) {
    for (int i = 0; i < MAX_BP; i++) if (G_bp[i].used && G_bp[i].pe == pe && G_bp[i].pc == pc) return true;
    return false;
}
static void bp_delete(int id) { if (id>=0 && id<MAX_BP) G_bp[id].used=false; }

static void print_help(void) {
    const char* B = log_color_bold();
    const char* RESET = log_color_reset();
    printf("%sDebugger commands%s\n", B, RESET);
    printf("  help                 - show this help\n");
    printf("  pause                - pause all PEs\n");
    printf("  cont                 - continue execution\n");
    printf("  step [n]             - step n instructions globally (default 1)\n");
    printf("  steppe <pe> [n]      - step n instructions only for PE\n");
    printf("  cache <pe>           - dump PE cache (valid lines, hex tags/addrs)\n");
    printf("  regs <pe|all>        - show register file of one or all PEs\n");
    printf("  memline <addr>       - show one memory block (BLOCK_SIZE doubles)\n");
    printf("  stats bus            - show bus statistics\n");
    printf("  break pc <pe>@<pc>   - set PC breakpoint\n");
    printf("  breaks               - list breakpoints\n");
    printf("  delete <id>          - delete breakpoint\n");
    fflush(stdout);
}

static void dump_cache_pe(int pe) {
    if (!(pe>=0 && pe<G_num_pes && G_caches)) { LOGW("[DBG] usage: cache <pe>"); return; }
    Cache* c = &G_caches[pe];
    pthread_mutex_lock(&c->mutex);
    printf("[DBG] Cache dump PE%d: SETS=%d WAYS=%d (only valid lines)\n", pe, SETS, WAYS);
    for (int set = 0; set < SETS; set++) {
        for (int way = 0; way < WAYS; way++) {
            CacheLine* cl = &c->sets[set].lines[way];
            if (!cl->valid) continue;
            char st = '?';
            switch (cl->state) { case M: st='M'; break; case E: st='E'; break; case S: st='S'; break; case I: st='I'; break; }
            unsigned long idx = cl->tag * SETS + (unsigned long)set;
            unsigned long base_addr = idx * BLOCK_SIZE;
            printf("  set=%2d way=%d state=%c tag=0x%lX base_addr=0x%lX data=[", set, way, st, cl->tag, base_addr);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                printf(i == 0 ? "%.6f" : ", %.6f", cl->data[i]);
            }
            printf("]\n");
        }
    }
    pthread_mutex_unlock(&c->mutex);
    fflush(stdout);
}

static void trim(char* s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
    size_t i = 0; while (isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s+i, n - i + 1);
}

static void* cli_main(void* arg) {
    (void)arg;
    char line[256];
    LOGI("[DBG] CLI ready. Type 'help'.");
    while (G.cli_running && fgets(line, sizeof(line), stdin)) {
        trim(line);
        if (line[0] == 0) continue;
        pthread_mutex_lock(&G.mtx);
        if (strcmp(line, "help") == 0) {
            pthread_mutex_unlock(&G.mtx);
            print_help();
            continue;
        } else if (strcmp(line, "pause") == 0) {
            G.paused = true;
            LOGI("[DBG] paused");
        } else if (strcmp(line, "cont") == 0 || strcmp(line, "c") == 0) {
            G.paused = false;
            G.step_global = 0;
            memset(G.step_pe, 0, sizeof(G.step_pe));
            pthread_cond_broadcast(&G.cv);
            LOGI("[DBG] continue");
        } else if (strncmp(line, "steppe ", 7) == 0) {
            int pe = -1, n = 1;
            sscanf(line+7, "%d %d", &pe, &n);
            if (pe >= 0 && pe < (int)(sizeof(G.step_pe)/sizeof(G.step_pe[0]))) {
                G.paused = true;
                G.step_pe[pe] += (n > 0 ? n : 1);
                pthread_cond_broadcast(&G.cv);
                LOGI("[DBG] steppe pe=%d n=%d", pe, n);
            } else {
                LOGW("[DBG] invalid PE id");
            }
        } else if (strncmp(line, "step", 4) == 0) {
            int n = 1;
            (void)sscanf(line+4, "%d", &n);
            G.paused = true;
            G.step_global += (n > 0 ? n : 1);
            pthread_cond_broadcast(&G.cv);
            LOGI("[DBG] step n=%d", n);
        } else if (strncmp(line, "cache", 5) == 0) {
            int pe=-1; sscanf(line+5, "%d", &pe);
            dump_cache_pe(pe);
        } else if (strncmp(line, "stats bus", 9) == 0) {
            if (G_bus) bus_stats_print(&G_bus->stats);
        } else if (strncmp(line, "regs", 4) == 0) {
            char arg[16] = {0};
            if (sscanf(line+4, "%15s", arg) == 1 && strcmp(arg, "all") == 0) {
                if (G_pes && G_num_pes > 0) {
                    for (int i = 0; i < G_num_pes; i++) reg_print(&G_pes[i].rf, i);
                    fflush(stdout);
                }
            } else {
                int pe=-1; sscanf(line+4, "%d", &pe);
                if (pe>=0 && pe<G_num_pes && G_pes) {
                    reg_print(&G_pes[pe].rf, pe);
                    fflush(stdout);
                } else LOGW("[DBG] usage: regs <pe|all>");
            }
        } else if (strncmp(line, "memline", 7) == 0) {
            int addr=-1; sscanf(line+7, "%d", &addr);
            if (addr>=0 && G_mem) {
                int base = ALIGN_DOWN(addr);
                double block[BLOCK_SIZE];
                pthread_mutex_lock(&G_mem->mutex);
                for (int i = 0; i < BLOCK_SIZE; i++) block[i] = G_mem->data[base + i];
                pthread_mutex_unlock(&G_mem->mutex);
                printf("[DBG] Memory line @ base=0x%X (addr 0x%X..0x%X):\n", base, base, base+BLOCK_SIZE-1);
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    printf("  [%d] %f\n", base + i, block[i]);
                }
                fflush(stdout);
            } else LOGW("[DBG] usage: memline <addr>");
        } else if (strncmp(line, "break pc", 8) == 0) {
            int pe=-1; unsigned long pc=0; if (sscanf(line+8, "%d@%lu", &pe, &pc)==2) {
                int id = bp_add_pc(pe, pc);
                if (id>=0) LOGI("[DBG] break #%d set at PE%d@%lu", id, pe, pc);
                else LOGW("[DBG] no space for new breakpoint");
            } else LOGW("[DBG] usage: break pc <pe>@<pc>");
        } else if (strncmp(line, "breaks", 6) == 0) {
            bp_list();
        } else if (strncmp(line, "delete", 6) == 0) {
            int id=-1; if (sscanf(line+6, "%d", &id)==1) { bp_delete(id); LOGI("[DBG] deleted #%d", id);} else LOGW("[DBG] usage: delete <id>");
        } else if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
            G.paused = false;
            G.cli_running = false;
            pthread_cond_broadcast(&G.cv);
            LOGI("[DBG] quit");
            pthread_mutex_unlock(&G.mtx);
            break;
        } else {
            LOGW("[DBG] unknown command: %s", line);
        }
        pthread_mutex_unlock(&G.mtx);
    }
    LOGI("[DBG] CLI closed");
    return NULL;
}

void dbg_init(void) {
    const char* env = getenv("SIM_DEBUG");
    if (env && (strcmp(env, "1")==0 || strcasecmp(env, "true")==0 || strcasecmp(env, "yes")==0)) {
        G.enabled = true;
        G.paused = true; // start paused in debug mode
    }
}

void dbg_start_cli(void) {
    if (!G.enabled || G.cli_running) return;
    G.cli_running = true;
    pthread_create(&G.cli_thr, NULL, cli_main, NULL);
}

void dbg_shutdown(void) {
    if (!G.enabled) return;
    pthread_mutex_lock(&G.mtx);
    G.cli_running = false;
    pthread_mutex_unlock(&G.mtx);
}

static bool should_block_for_pe(int pe_id) {
    if (!G.enabled) return false;
    if (!G.paused) return false;
    if (G.step_pe[pe_id] > 0) return false;
    if (G.step_global > 0) return false;
    return true;
}

static void consume_step(int pe_id) {
    if (G.step_pe[pe_id] > 0) {
        G.step_pe[pe_id]--;
    } else if (G.step_global > 0) {
        G.step_global--;
    }
}

void dbg_before_instruction(int pe_id, uint64_t pc, const Instruction* insn) {
    (void)insn;
    if (!G.enabled) return;
    pthread_mutex_lock(&G.mtx);
    static bool announced_ready = false;
    if (!announced_ready) {
        LOGI("[DBG] Debugger ready (PE threads running). Type 'help'.");
        announced_ready = true;
    }
    // break on pc
    if (bp_match(pe_id, pc)) {
        G.paused = true;
        LOGI("[DBG] hit breakpoint at PE%d@%lu", pe_id, (unsigned long)pc);
    }
    while (should_block_for_pe(pe_id)) {
        pthread_cond_wait(&G.cv, &G.mtx);
    }
    if (G.paused && (G.step_pe[pe_id] > 0 || G.step_global > 0)) {
        consume_step(pe_id);
    }
    pthread_mutex_unlock(&G.mtx);
}

void dbg_on_bus_event(int type, uint64_t addr, int src_pe, int invalidations) {
    (void)type; (void)addr; (void)src_pe; (void)invalidations;
}

void dbg_on_mesi_transition(int pe_id, uint64_t addr, MESI_State from, MESI_State to) {
    (void)pe_id; (void)addr; (void)from; (void)to;
}
