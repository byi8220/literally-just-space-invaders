#pragma once
#include <ctype.h>
#include <stdint.h>

typedef struct CondFlags{
    uint8_t z : 1;
    uint8_t s : 1;
    uint8_t p : 1;
    uint8_t cy : 1;
    uint8_t ac : 1;
    uint8_t pad : 2;
} CondFlags;

typedef struct CPU_state{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    uint8_t *memory;
    CondFlags flags;
    uint8_t int_enable : 1;
} CPU;


void unimplemented(CPU *cpu);
void emulate(CPU *cpu);

unsigned char *regptr(CPU *cpu, uint8_t regcode);

void sub40(CPU *cpu, uint8_t top, uint8_t bot, uint8_t *opcode);
void rtype(CPU *cpu, uint8_t *dest, uint8_t *src, uint8_t top, uint8_t bot);
void control(CPU *cpu, uint8_t top, uint8_t bot, uint8_t *opcode);

uint8_t parity(uint16_t word);
uint8_t parity_limit(uint16_t word, int limit);



void generateInterrupt(CPU *cpu, int intcode);


extern void machine_init();
extern uint8_t MachineIN(uint8_t port, uint8_t cpua);
extern uint8_t MachineOUT(uint8_t port, uint8_t value);