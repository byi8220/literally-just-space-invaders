#pragma once

uint8_t in_port[8];
uint8_t out_port[8];

uint8_t MachineIN(uint8_t port, uint8_t cpua);
uint8_t MachineOUT(uint8_t port, uint8_t value);
