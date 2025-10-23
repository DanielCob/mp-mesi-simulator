#ifndef HANDLERS_H
#define HANDLERS_H

#include "bus.h"

// Register handlers on the bus
void bus_register_handlers(Bus* bus);

// Individual handlers (not public outside the bus)
void handle_busrd(Bus* bus, int addr, int src_pe);
void handle_busrdx(Bus* bus, int addr, int src_pe);
void handle_busupgr(Bus* bus, int addr, int src_pe);
void handle_buswb(Bus* bus, int addr, int src_pe);

#endif