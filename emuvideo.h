#pragma once
#include "SDL2-2.0.8\include\SDL.h"
void machine_init();
int initVideo(CPU *cpu);
uint8_t draw_frame(CPU *cpu);