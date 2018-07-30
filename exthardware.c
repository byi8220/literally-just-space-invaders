#pragma once
#include <stdint.h>

#include "exthardware.h"

// READ PORTS:
	// R1
	//   bit 0: coin (0 when active)
	//   bit 1: P2 start
	//   bit 2: P1 start
	//   bit 3: unknown
	//   bit 4: P1 shoot
	//   bit 5: P1 left
	//   bit 6: P1 right
	//   bit 7: unknown
	//
	// R2
	//   bit 0+1: dipswitch life count (0:3, 1:4, 2:5, 3:6)
	//   bit 2: tilt switch
	//   bit 3: dipswitch bonus life at 1:1000, 0:1500
	//   bit 4: P2 shoot
	//   bit 5: P2 left
	//   bit 6: P2 right
	//   bit 7: unknown
	//
	// R3
	//   Shift Register Result

// WRITE PORTS:
	// W2
	//   shift register result offset (bits 0, 1, 2)
	//
	// W3
	//   sound stuff
	//
	// W4
	//   fill shift register
	//
	// W5
	//   sound stuff
	//
	// W6
	//   debug port

uint8_t shift0 = 0;
uint8_t shift1 = 0;
uint8_t shift_offset = 0;

void machine_init() {
	in_port[0] = (0x2) | (0x4) | (0x8);
	in_port[1] = (0x8) ; // Always 1
	in_port[2] = 0;
	in_port[3] = 0;
	in_port[4] = 0;
	in_port[5] = 0;
	in_port[6] = 0;
	in_port[7] = 0;

	out_port[0] = 0;
	out_port[1] = 0;
	out_port[2] = 0;
	out_port[3] = 0;
	out_port[4] = 0;
	out_port[5] = 0;
	out_port[6] = 0; // Watchdog port?
	out_port[7] = 0;
}

uint8_t MachineIN(uint8_t port, uint8_t cpua) {
	uint8_t a = 0;
	switch (port) {
		case(1):
			return in_port[1];
		case(3): {
			uint16_t v = (shift1 << 8) | shift0;
			a = ((v >> (8 - shift_offset)) & 0xff);
			return a;
			}break;
		default:
			return in_port[port];
	}
	return a;
}
uint8_t MachineOUT(uint8_t port, uint8_t value) {
	switch (port) {
		case(1):
			break;
		case(2):
			shift_offset = value & 0x7;
			break;
		case(4):
			shift0 = shift1;
			shift1 = value;
			break;
		default:
			out_port[port] = value;
			break;
		}
	return 0;
}
