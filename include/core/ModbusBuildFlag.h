#pragma once

// Single source for the DL_ENABLE_MODBUS default (the build itself sets it
// explicitly via platformio.ini's build_flags). Included by every call site
// that branches on the flag — main.cpp and Provisioning.cpp — so the
// fallback used when neither sets it cannot drift between them.
//
// Off by default: the Modbus stack and its RS-485 wiring are the least
// commonly populated part of the board, and excluding it keeps its symbols
// out of a build that never uses it. Override with -DDL_ENABLE_MODBUS=1.
#ifndef DL_ENABLE_MODBUS
#define DL_ENABLE_MODBUS 0
#endif
