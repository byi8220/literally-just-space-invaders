#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Windows.h>
#include "SDL2-2.0.8\include\SDL.h"

#include "core.h"
#include "emuvideo.h"
#include "exthardware.h"

#define RST0ADDR 0x0000
#define RST1ADDR 0x0008
#define RST2ADDR 0x0010
#define RST3ADDR 0x0018
#define RST4ADDR 0x0020
#define RST5ADDR 0x0028
#define RST6ADDR 0x0030
#define RST7ADDR 0x0038

#define DRAW_FRAME_TIME (16.67 / 2)
#define EMULATION_BREAKPOINT 0xFFFF

extern int initVideo(CPU *cpu);
extern uint8_t draw_frame(CPU *cpu);

uint32_t lastOpTime = 0;
uint32_t lastDrawTime = 0;
uint32_t currTime = 0;
// Timing cycles
unsigned char cycles8080[] = {
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x00..0x0f
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x10..0x1f
	4, 10, 16, 5, 5, 5, 7, 4, 4, 10, 16, 5, 5, 5, 7, 4, //etc
	4, 10, 13, 5, 10, 10, 10, 4, 4, 10, 13, 5, 5, 5, 7, 4,

	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, //0x40..0x4f
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,

	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, //0x80..8x4f
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,

	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, //0xc0..0xcf
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11,
	11, 10, 10, 18, 17, 11, 7, 11, 11, 5, 10, 5, 17, 17, 7, 11,
	11, 10, 10, 4, 17, 11, 7, 11, 11, 5, 10, 4, 17, 17, 7, 11,
};

int cycles = 0;

int main(int argc, char **argv){
    if(argc < 2){
        printf("Usage: core.exe <filename> [offset]\n");
        exit(1);
    }
    int offset;
    if(argc < 3 ){
        offset = 0;
    }else{
        offset = strtol(argv[2], NULL, 0); 
    }
    FILE *f = fopen(argv[1], "rb");
    if(f == NULL){
        printf("Could not open file %s\n", argv[1]);
        exit(1);
    }

    int powerOn = 1;
	// Create CPU
	machine_init();
    CondFlags flags = {
        0,  // z
        0,  // s
        0,  // p
        0,  // cy
        0,  // ac
        0};
    CPU *cpu = malloc(sizeof(cpu));
    cpu->a = 0;
    cpu->b = 0;
    cpu->c = 0;
    cpu->d = 0;
    cpu->e = 0;
    cpu->h = 0;
    cpu->l = 0;
    cpu->int_enable = 0;
    cpu->memory = malloc(0x10000);
    cpu->pc = offset;
    cpu->sp = 0;
    cpu->flags = flags;

	// Load ROM
    fseek(f, 0L, SEEK_END);
    uint16_t filesize = ftell(f);
    fseek(f,0L, SEEK_SET);
    fread(cpu->memory, filesize, 1, f);
    fclose(f);	

	initVideo(cpu);



    while(powerOn){

		currTime = SDL_GetTicks();
		if (SDL_TICKS_PASSED(currTime, lastOpTime) && cycles > 2000) {
			cycles = 0;
			continue;
		}
        // Fetch Interrupt
        uint8_t *opcode = &cpu->memory[cpu->pc];
		cycles += cycles8080[*opcode];
		// In/Out opcodes handled externally
        if (*opcode == 0xdb) {    
            uint8_t port = opcode[1];    
            cpu->a = MachineIN(port, cpu->a);    
            cpu->pc += 2; //Since emulate isn't being called pc has to be manually advanced    
        }else if (*opcode == 0xd3){    
            uint8_t port = opcode[1];    
            MachineOUT(port, cpu->a);    
            cpu->pc += 2;    //Since emulate isn't being called pc has to be manually advanced    
        }else{
            emulate(cpu);
        }

		lastOpTime = currTime;


        // Space Invaders Specific Input Hardware
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            // Input Interrupts here
            switch (evt.type) {
                case(SDL_KEYDOWN):
                    switch (evt.key.keysym.sym) {
                        case(SDLK_c):
                            in_port[1] = in_port[1] | 0x1;
                            break;
                        case(SDLK_a):
                            in_port[1] = in_port[1] | (1 << 5);
                            break;
                        case(SDLK_d):
                            in_port[1] = in_port[1] | (1 << 6);
                            break;
                        case(SDLK_SPACE):
                            in_port[1] = in_port[1] | (1 << 4);
                            break;
                        case(SDLK_j):
                            in_port[1] = in_port[1] | (1 << 2);
                            break;
                        case(SDLK_ESCAPE):
                            exit(0);
                            break;
                        default:
                            break;
                        }
                    break;
                case(SDL_KEYUP):
                        switch (evt.key.keysym.sym) {
                        case(SDLK_c):
                            in_port[1] = in_port[1] & ~0x1;
                            break;
                        case(SDLK_a):
                            in_port[1] = in_port[1] & ~(1 << 5);
                            break;
                        case(SDLK_d):
                            in_port[1] = in_port[1] & ~(1 << 6);
                            break;
                        case(SDLK_SPACE):
                            in_port[1] = in_port[1] & ~(1 << 4);
                            break;
                        case(SDLK_j):
                            in_port[1] = in_port[1] & ~(1 << 2);
                            break;
                        default:
                            break;
                        }
                        break;
                default:
                    break;
                    }
                }

            // 60 Hz Redraws
            if (currTime - lastDrawTime > (DRAW_FRAME_TIME )) {
                lastDrawTime = currTime;
                if (cpu->int_enable) {
                    uint8_t toggle = draw_frame(cpu); // Drawing generates an interrupt based on which half it draws
                    generateInterrupt(cpu, toggle); // Interrupt
                }
            }
    }

	return 0;
}

void unimplemented(CPU *cpu){
    
    printf("Unimplemented Instruction %02x at memory address 0x%04x\n", cpu->memory[cpu->pc - 1], cpu->pc - 1);
	getchar();
    exit(1);
}

// TODO: Implement AC flag

int step = 0;

void emulate(CPU *cpu) {

	// Fetch opcode
	unsigned char *opcode = &cpu->memory[cpu->pc];


	// Debugging breakpoint
    // Probably should have just called these based on defines but C preprocessor scares me
/*
	if (cpu->pc == EMULATION_BREAKPOINT) {
		step = 1;
	}
	if (step){
		printf("Instruction: %02x At memory location %04x [%02x %02x]:\n", opcode[0], cpu->pc, opcode[1], opcode[2]);
		// Debug Info
		printf("\tC=%d,P=%d,S=%d,Z=%d\n", cpu->flags.cy, cpu->flags.p,
			cpu->flags.s, cpu->flags.z);
		printf("\tA $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP %04x PC: 0x%04x INT:%01x\n",

			cpu->a, cpu->b, cpu->c, cpu->d,
			cpu->e, cpu->h, cpu->l, cpu->sp, cpu->pc, cpu->int_enable);
		getchar();
	}
*/
	
	uint8_t top = (*opcode) >> 4; // upper half of byte
    uint8_t bot = (*opcode) & 0x0F; // lower half of byte
    unsigned char *src;// Pointer to source register for moves and register arithmetic

    cpu->pc++;
    // See: http://pastraiser.com/cpu/i8080/i8080_opcodes.html
    // For opcode organization
	if(top <= 0x3){
        sub40(cpu, top, bot, opcode);
    }

    else if(0x4 <= top && top <= 0x7){
        if(*opcode == 0x76){
            // HLT
            unimplemented(cpu);
        }else{
			// Instructions 0x40 to 0x7F have common format [OP | DST | SRC]
            src = regptr(cpu, bot & 0x07);
            unsigned char *dest = regptr(cpu, (*opcode & 0x3F) >> 3);
            rtype(cpu, dest, src, top, bot);
        }
    }
    
    else if(0x8 <= top && top <= 0xB){
        src = regptr(cpu, bot & 0x07);
        rtype(cpu, &cpu->a, src, top, bot);
    }
    
    else if(0xC <= top && top <= 0xF){
        control(cpu, top, bot, opcode);
    }
}


// Opcode Implementation
// The manual does a better job of documentation than any walls of text I could write
// 8080 Manual: http://altairclone.com/downloads/manuals/8080%20Programmers%20Manual.pdf

// Handle instructions 0x00 to 0x3F
void sub40(CPU *cpu, uint8_t top, uint8_t bot, uint8_t *opcode){
    unsigned char *src = regptr(cpu, (*opcode & 0x3F) >> 3);
    uint16_t result;
    // Single Reg Increments/Move ops
    if(bot == 0x4 || bot == 0xC){
        // INR
        result = *src+1;
        cpu->flags.z = ((result & 0xff) == 0);    
        cpu->flags.s = ((result & 0x80) != 0);    
        cpu->flags.p = parity(result&0xff);    
        *src = result & 0xff;    
    }else if(bot == 0x5 || bot == 0xD){
        // DCR
        result = *src-1;
        cpu->flags.z = ((result & 0xff) == 0);    
        cpu->flags.s = ((result & 0x80) != 0);    
        cpu->flags.p = parity(result&0xff);    
        *src = result & 0xff;    
    }else if(bot == 0x6 || bot == 0xE){
        // MVI
        *src = opcode[1];
        cpu->pc += 1;
    }
    //
    // Double Register Ops
    //
    else if(bot == 0x1){
        // LXI
        if(top == 0x0){
			// 01
			// LXI B d16
            cpu->b = opcode[2];
            cpu->c = opcode[1];
        }else if(top == 0x1){
			// 11
			// LXI D d16
            cpu->d = opcode[2];
            cpu->e = opcode[1];
        }else if(top == 0x2){
			// 21
			// LXI H d16
            cpu->h = opcode[2];
            cpu->l = opcode[1];
        }else if(top == 0x3){
			// 31
			// LXI SP d16
            uint16_t x = (opcode[2] << 8) | (opcode[1]);
            cpu->sp = x;
        }
        cpu->pc += 2;
    }else if(bot == 0x2){
        // STAX/SHLD/STA
        uint16_t addr;
        if(top == 0x0){
			// 02
			// STAX B
            addr = (cpu->b << 8) | (cpu->c);
            cpu->memory[addr] = cpu->a;
        }else if(top == 0x1){
			// 12
			// STAX D
            addr = (cpu->d << 8) | (cpu->e);
            cpu->memory[addr] = cpu->a;
        }else if(top == 0x2){
			// 22
			// SHLD [a16]
            addr = (opcode[2] << 8) | (opcode[1]);
            cpu->memory[addr] = cpu->l;
            cpu->memory[addr+1] = cpu->h;
            cpu->pc +=2;
        }else if(top == 0x3){
			// 32
			// STA [a16]
            addr = (opcode[2] << 8) | (opcode[1]);
            cpu->memory[addr] = cpu->a;
            cpu->pc +=2;
        }
    }else if(bot == 0x3){
        // INX
        uint16_t x;
        if(top == 0x0){
			// 03
			// INX B
            x = ((cpu->b) << 8) | (cpu->c);
            x++;
            cpu->b = (x >> 8);
            cpu->c = (x & 0xFF);
        }else if(top == 0x1){
			// 13
			// INX D
            x = ((cpu->d) << 8) | (cpu->e);
            x++;
            cpu->d = (x >> 8);
            cpu->e = (x & 0xFF);
        }else if(top == 0x2){
			// 23
			// INX H
            x = ((cpu->h) << 8) | (cpu->l);
            x++;
            cpu->h = (x >> 8);
            cpu->l = (x & 0xFF);
        }else if(top == 0x3){
			// 33
			// INX SP
            cpu->sp++;
        }
    }else if(bot == 0x7){
        // Rotates/DAA
        uint8_t bit;
        if(top == 0x0){
			// 07
            //RLC
            cpu->flags.cy = ((cpu->a & 0x80) != 0);
            cpu->a = cpu->a << 1;
            cpu->a += (cpu->flags.cy);
        }else if(top == 0x1){
			// 17
            //RAL
            bit = cpu->flags.cy & 1;
            cpu->flags.cy = ((cpu->a & 0x80) != 0);
            cpu->a = cpu->a << 1;
            cpu->a += bit;
        }else if(top == 0x2){
			// 27
            // DAA
			// No AC implemented
			if ((cpu->a & 0xF) > 0x9) {
				cpu->a += 6;
			}
			if ((cpu->a & 0xf0) > 0x90)
			{
				uint16_t result = (uint16_t)cpu->a + 0x60;
				cpu->a = result & 0xff;
				cpu->flags.z = ((result & 0xff) == 0);
				cpu->flags.s = ((result & 0x80) != 0);
				cpu->flags.cy = (result > 0xff);
				cpu->flags.p = parity(result & 0xff);
			}
        }else if(top == 0x3){
			// 37
            //STC
            cpu->flags.cy = 1;
        }
    }else if(bot == 0x9){
		// 09 19 29 39
        // DAD
        uint32_t hl = ((cpu->h) << 8) | (cpu->l);
		uint16_t x = 0;
        if(top == 0x0){
			// DAD B
            x = ((cpu->b) << 8) | (cpu->c);
        }else if(top == 0x1){
			// DAD D
            x = ((cpu->d) << 8) | (cpu->e);
        }else if(top == 0x2){
			// DAD H
            x = ((cpu->h) << 8) | (cpu->l);
        }else if(top == 0x3){
			// DAD SP
            x = cpu->sp;
        }
        hl = hl + x;
        cpu->h = (hl >> 8);
        cpu->l = (hl & 0xFF);
        cpu->flags.cy = (hl > 0xFFFF);
    }else if(bot == 0xA){
        // LDAX/LHLD/LDA
        uint16_t addr;
        if(top == 0x0){
			// 0A
			// LDAX B
            addr = ((cpu->b) << 8) | (cpu->c);
            cpu->a = cpu->memory[addr];
        }else if(top == 0x1){
			// 1A
			// LDAX D
            addr = ((cpu->d) << 8) | (cpu->e);
            cpu->a = cpu->memory[addr];
        }else if(top == 0x2){
			// 2A
			// LHLD [a16]
            addr = (opcode[2] << 8) | (opcode[1]);
            cpu->l = cpu->memory[addr];
            cpu->h = cpu->memory[addr+1];
            cpu->pc += 2;
        }else if(top == 0x3){
			// 3A
			// LDA [a16]
            addr = (opcode[2] << 8) | (opcode[1]);
            cpu->a = cpu->memory[addr];
            cpu->pc += 2;
        }
    }else if(bot == 0xB){
        // DCX
        uint16_t x;
        if(top == 0x0){
			// 0B
			// DCX B
            x = ((cpu->b) << 8) | (cpu->c);
            x--;
            cpu->b = (x >> 8);
            cpu->c = (x & 0xFF);
        }else if(top == 0x1){
			// 1B
			// DCX D
            x = ((cpu->d) << 8) | (cpu->e);
            x--;
            cpu->d = (x >> 8);
            cpu->e = (x & 0xFF);
        }else if(top == 0x2){
			// 2B
			// DCX H
            x = ((cpu->h) << 8) | (cpu->l);
            x--;
            cpu->h = (x >> 8);
            cpu->l = (x & 0xFF);
        }else if(top == 0x3){
			// 3B
			// DCX SP
            cpu->sp--;
        }
    }else if(bot == 0xF){
        // Rotates 
        uint8_t bit;
        if(top == 0x0){
			// 0F
			// RRC
            bit = cpu->a & 0x1; //LSB of a
            cpu->flags.cy = bit; // carry = LSB
            cpu->a = cpu->a >> 1; // Rshift
            cpu->a |= (bit << 7); // move LSB to MSB
        }else if(top == 0x1){
			// 1F
			// RAR
            bit = cpu->flags.cy & 1;
            cpu->flags.cy = cpu->a & 1;
            cpu->a = cpu->a >> 1;
            cpu->a |= (bit << 7);
        }else if(top == 0x2){
			// 2F
			// CMA
            cpu->a = ~(cpu->a);
        }else if(top == 0x3){
			// 3F
			// CMC
            cpu->flags.cy = (cpu->flags.cy == 0);
        }
    }
}

// Instructions 0x40 to 0xBF
// 2 Register Arithmetic and Logic
void rtype(CPU *cpu, uint8_t *dest, uint8_t *src, uint8_t top, uint8_t bot){
    if(top < 0x8){
        // Move
        *dest = *src;
    }else{
        uint16_t result = cpu->a;
        if(top == 0x8){
            if(bot < 0x8){
                // ADD
                result += *src;
            }else{
                // ADC
                result += *src + cpu->flags.cy;
            }
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0x9){
            if(bot < 0x8){
                // SUB
                result -= *src;
            }else{
                // SBB
                result = result - *src - cpu->flags.cy;
            }
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;   
        }else if(top == 0xA){
            if(bot < 0x8){
                // ANA
                result = result & *src;
            }else{
                // XRA
                result = result ^ *src;
            }
            cpu->flags.z = (result == 0);    
            cpu->flags.s = (0x80 == (result & 0x80));    
            cpu->flags.p = parity(result);   
            cpu->flags.cy = 0;
            cpu->a = result & 0xff; 
        }else if(top == 0xB){
            if(bot < 0x8){
                // ORA
                result = result | *src;
                cpu->flags.z = (result == 0);    
                cpu->flags.s = (0x80 == (result & 0x80));    
                cpu->flags.p = parity(result);   
                cpu->flags.cy = 0;
                cpu->a = result & 0xff; 
            }else{
                // CMP
                result = cpu->a - *src;
                cpu->flags.z = ((result & 0xff) == 0);    
                cpu->flags.s = ((result & 0x80) != 0);    
                cpu->flags.cy = (result > 0xff);    
                cpu->flags.p = parity(result&0xff);    
            }
        }
    }
}

// Handle Instructions from 0xC0 to 0xFF
// Control Flow and immediate arithmetic/logic
void control(CPU *cpu, uint8_t top, uint8_t bot, uint8_t *opcode){
    if(bot == 0x0){
        // RNZ/RNC/RPO/RP
        if(top == 0xC){
			// C0
			// RNZ
            if(cpu->flags.z == 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            } 
        }else if(top == 0xD){
			// D0
			// RNC
            if(cpu->flags.cy == 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// E0
			// RPO
            if(cpu->flags.p == 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// F0
			// RP
            if(cpu->flags.s == 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            }             
        }
    }else if(bot == 0x1){
        // POP
        if(top == 0xC){
			// C1
			// POP B
            cpu->c = cpu->memory[cpu->sp];
            cpu->b = cpu->memory[cpu->sp + 1];
            cpu->sp += 2;
        }else if(top == 0xD){
			// D1
			// POP D
            cpu->e = cpu->memory[cpu->sp];
            cpu->d = cpu->memory[cpu->sp + 1];
            cpu->sp += 2;
        }else if(top == 0xE){
			// E1
			// POP H
            cpu->l = cpu->memory[cpu->sp];
            cpu->h = cpu->memory[cpu->sp + 1];            
            cpu->sp += 2;
        }else if(top == 0xF){
			// F1
            // POP PSW
            uint8_t psw = cpu->memory[cpu->sp];
            cpu->flags.z  = (0x01 == (psw & 0x01));    
            cpu->flags.s  = (0x02 == (psw & 0x02));    
            cpu->flags.p  = (0x04 == (psw & 0x04));    
            cpu->flags.cy = (0x05 == (psw & 0x08));    
            cpu->flags.ac = (0x10 == (psw & 0x10)); 

            cpu->a = cpu->memory[cpu->sp + 1];

            cpu->sp += 2;
        }
    }else if(bot == 0x2){
        // JNZ/JNC/JPO/JP
        if(top == 0xC){
			// C2
			// JNZ [a16]
            if(cpu->flags.z == 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xD){
			//D2
			// JNC [a16]
            if(cpu->flags.cy == 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// E2
			// JPO [a16]
            if(cpu->flags.p == 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// F2
			// JP [a16]
            if(cpu->flags.s == 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }
    }else if(bot == 0x3){
        // JMP/OUT/XTHL/DI
        if(top == 0xC){
			// C3
			// JMP [a16]
            cpu->pc = (opcode[2] << 8) | opcode[1];
        }else if(top == 0xD){
            // Never reached
            cpu->pc += 1;
        }else if(top == 0xE){
			// E3
			// XTHL
            uint8_t tmp;
            tmp = cpu->l;
            cpu->l = cpu->memory[cpu->sp];
            cpu->memory[cpu->sp] = tmp;
            tmp = cpu->h;
            cpu->h = cpu->memory[cpu->sp + 1];
            cpu->memory[cpu->sp + 1] = tmp;
        }else if(top == 0xF){
			// F3
			// DI
            cpu->int_enable = 0;
        }
    }else if(bot == 0x4){
        // CNZ/CNC/CPO/CP
        if(top == 0xC){
			// C4
			// CNZ [a16]
            if(cpu->flags.z == 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xD){
			// D4
			// CNC [a16]
            if(cpu->flags.cy == 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// E4
			// CPO [a16]
            if(cpu->flags.p == 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// F4
			// CP [a16]
            if(cpu->flags.s == 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }
    }else if(bot == 0x5){
        // PUSH
        if(top == 0xC){
			// C5
			// PUSH B
            cpu->memory[cpu->sp-1] = cpu->b;
            cpu->memory[cpu->sp-2] = cpu->c;
            cpu->sp -= 2;
        }else if(top == 0xD){
			// D5
			// PUSH D
            cpu->memory[cpu->sp-1] = cpu->d;
            cpu->memory[cpu->sp-2] = cpu->e;
            cpu->sp -= 2;
        }else if(top == 0xE){
			// E5
			// PUSH H
            cpu->memory[cpu->sp-1] = cpu->h;
            cpu->memory[cpu->sp-2] = cpu->l;
            cpu->sp -= 2;
        }else if(top == 0xF){
			// F5
			// PUSH PSW
            // Pushes A then PSW
            cpu->memory[cpu->sp-1] = cpu->a;            
            uint8_t psw = (
                cpu->flags.z |
                cpu->flags.s << 1 |
                cpu->flags.p << 2 |
                cpu->flags.cy << 3 |
                cpu->flags.ac << 4
            );
            cpu->memory[cpu->sp-2] = psw;
            cpu->sp -= 2;
        }
    }else if(bot == 0x6){
		// Immediate Add/Sub/And/Or
        // ADI/SUI/ANI/ORI
        uint16_t result = opcode[1];
        if(top == 0xC){
			// C6
			// ADI d8
            result = cpu->a + result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xD){
			// D6
			// SUI d8
            result = cpu->a - result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xE){
			// E6
			// ANI d8
            result = cpu->a & result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = 0;    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xF){
			// F6
			// ORI d8
            result = cpu->a | result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = 0;    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }
        cpu->pc += 1;
    }else if(bot == 0x7){
        // RST 0/2/4/6
        // Equivalents to CALL [Constant Addr]
		// 1 Byte Instrs so return is prefetched instr
        if(top == 0xC){
			// C7
			// RST 0
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST0ADDR;
        }else if(top == 0xD){
			// D7
			// RST 2
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST2ADDR;
        }else if(top == 0xE){
			// E7
			// RST 4
			uint16_t ret = cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST4ADDR;
        }else if(top == 0xF){
			// F7
			// RST 6
			uint16_t ret = cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST6ADDR;
        }
    }else if(bot == 0x8){
        // RZ/RC/RPE/RM
        if(top == 0xC){
			// C8
			// RZ
            if(cpu->flags.z != 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            } 
        }else if(top == 0xD){
			// D8
			// RC
            if(cpu->flags.cy != 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// E8
			// RPE
            if(cpu->flags.p != 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// F8
			// RM
            if(cpu->flags.s != 0){
                cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
                cpu->sp += 2;   
            }else{
                //cpu->pc += 2;
            }             
        }
    }else if(bot == 0x9){
        // RET/*RET/PCHL/SPHL
        if(top == 0xC || top == 0xD){
			// C9 D9
            //RET
            cpu->pc = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp+1] << 8);    
            cpu->sp += 2;    
        }else if(top == 0xE){
			// E9
			// PCHL
            cpu->pc = (cpu->h << 8) | (cpu->l);
        }else if(top == 0xF){
			// F9
			// SPHL
            cpu->sp = (cpu->h << 8) | (cpu->l);
        }
    }else if(bot == 0xA){
        // JZ/JC/JPE/JM
        if(top == 0xC){
			// CA
			// JZ [a16]
            if(cpu->flags.z != 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xD){
			// DA
			// JC [a16]
            if(cpu->flags.cy != 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// EA
			// JPE [a16]
            if(cpu->flags.p != 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// FA
			// JM [a16]
            if(cpu->flags.s != 0){
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }
    }else if(bot == 0xB){
        // *JMP/IN/XCHG/EI
        if(top == 0xC){
			// CB
			// JMP [a16]
            cpu->pc = (opcode[2] << 8) | opcode[1];
        }else if(top == 0xD){
            // Never reached
            cpu->pc += 1;
        }else if(top == 0xE){
			// EB
			// XCHG
            uint8_t tmp;
            tmp = cpu->l;
            cpu->l = cpu->e;
            cpu->e = tmp;
            tmp = cpu->h;
            cpu->h = cpu->d;
            cpu->d = tmp;
        }else if(top == 0xF){
			// FB
			// EI
            cpu->int_enable = 1;
        }
    }else if(bot == 0xC){
        // CZ/CC/CPE/CM
        if(top == 0xC){
			// CC
			// CZ [a16]
            if(cpu->flags.z != 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xD){
			// DC
			// CC [a16]
            if(cpu->flags.cy != 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            } 
        }else if(top == 0xE){
			// EC
			// CPE [a16]
            if(cpu->flags.p != 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }else if(top == 0xF){
			// FC
			// CM [a16]
            if(cpu->flags.s != 0){
                uint16_t ret= cpu->pc + 2;
                cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
                cpu->memory[cpu->sp - 2] = (ret & 0xFF);
                cpu->sp -= 2;
                cpu->pc = (opcode[2] << 8) | opcode[1];
            }else{
                cpu->pc += 2;
            }             
        }
    }else if(bot == 0xD){
        // CALL/*CALL
        // All four cases are CALL instructions
		// CD DD ED FD	
		// CALL [a16]
        uint16_t ret= cpu->pc + 2;
        cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
        cpu->memory[cpu->sp - 2] = (ret & 0xFF);
        cpu->sp -= 2;
        cpu->pc = (opcode[2] << 8) | opcode[1];
    }else if(bot == 0xE){
        // ACI/SBI/XRI/CPI
        uint16_t result = opcode[1];
        if(top == 0xC){
			// CE
			// ACI d8
            result = cpu->a + result + cpu->flags.cy;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xD){
			// DE
			// SBI d8
            result = cpu->a - result - cpu->flags.cy;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xE){
			// EE
			// XRI d8
            result = cpu->a ^ result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = 0;    
            cpu->flags.p = parity(result&0xff);    
            cpu->a = result & 0xff;    
        }else if(top == 0xF){
			// FE
			// CPI d8
            result = cpu->a - result;
            cpu->flags.z = ((result & 0xff) == 0);    
            cpu->flags.s = ((result & 0x80) != 0);    
            cpu->flags.cy = (result > 0xff);    
            cpu->flags.p = parity(result&0xff);    
        }
        cpu->pc += 1;        
    
	}else if(bot == 0xF){
        // RST 1/3/5/7
        // Equivalents to CALL [Constant Addr]
		// 1 byte long instr so return is already fetched
        if(top == 0xC){
			// CF
			// RST 1
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST1ADDR;
        }else if(top == 0xD){
			// DF
			// RST 3
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST4ADDR;
        }else if(top == 0xE){
			// EF
			// RST 5
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST5ADDR;
        }else if(top == 0xF){
			// FF
			// RST 7
            uint16_t ret= cpu->pc;
            cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
            cpu->memory[cpu->sp - 2] = (ret & 0xFF);
            cpu->sp -= 2;
            cpu->pc = RST7ADDR;
        }
    }
}

// Return a pointer to the data that is to be manipulated by the caller
inline unsigned char *regptr(CPU *cpu, uint8_t regcode){
    switch(regcode){
        case(0): return &cpu->b;
        case(1): return &cpu->c;
        case(2): return &cpu->d;
        case(3): return &cpu->e;
        case(4): return &cpu->h;
        case(5): return &cpu->l;
		case(6):
			{
			uint16_t memloc = cpu->h;
			memloc = memloc << 8;
			memloc |= cpu->l;
			return &cpu->memory[memloc];
			}
            break;
        case(7): return &cpu->a;
        default:
            unimplemented(cpu);
            return 0;
            break;
    }
}


// Parity = does it have even or odd number of 1's
uint8_t parity(uint16_t word){
    return parity_limit(word, 8);
}

// Check the first [limit] bits
uint8_t parity_limit(uint16_t word, int limit){
	int p = 0;
	word = (word & ((1<<limit)-1));
	for (int i=0; i<limit; i++){
		if (word & 0x1) 
			p++;
		word = word >> 1;
	}
	return (0 == (p & 0x1));
}


inline void generateInterrupt(CPU *cpu, int intcode){
    // Push interrupt to stack
	// Equivalent to RST [intcode]
    uint16_t ret = cpu->pc;
    cpu->memory[cpu->sp - 1] = (ret >> 8) & 0xFF;
    cpu->memory[cpu->sp - 2] = (ret & 0xFF);
    cpu->sp -= 2;
    cpu->pc = 8 * intcode;
	cpu->int_enable = 0;
}