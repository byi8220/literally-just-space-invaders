#include <Windows.h>
#include "core.h"
#include "SDL2-2.0.8\include\SDL.h"
#include "emuvideo.h"



#define VIDEO_RAM_START 0x2400
#define VIDEO_X 256
#define VIDEO_Y 224
#define DRAW_TOP 2
#define DRAW_BOT 1

char *videoMem;
char draw_half; // Space invaders draws half screen at a time (60 / 2 draws == 30 effective FPS)
uint8_t toggle = 2;

SDL_Window* gWindow = NULL;
SDL_Renderer *gRender = NULL;
SDL_Surface *gSurface = NULL;


int initVideo(CPU *cpu) {
	HMODULE libHandle = LoadLibrary(TEXT("SDL2.dll"));
	if (libHandle == NULL) {
		printf("Could not load SDL2.dll\n");
		exit(1);
	}
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		printf("SDL init failed: %s\n", SDL_GetError());
		exit(1);
	}
	gWindow = SDL_CreateWindow("Speic Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, VIDEO_Y, VIDEO_X, SDL_WINDOW_SHOWN);
	gRender = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
	draw_half = DRAW_TOP;
	return 0;
}

uint8_t draw_frame(CPU *cpu) {

	SDL_SetRenderDrawColor(gRender, 0, 0, 0, 0xFF);
	SDL_RenderClear(gRender);
	SDL_SetRenderDrawColor(gRender, 0xFF, 0xFF, 0xFF, 0xFF);
	uint8_t *cb = &cpu->memory[VIDEO_RAM_START];
	for (int row = 0; row < VIDEO_Y; row++) {
		for (int col = 0; col < VIDEO_X; col += 8) {
			for (int j = 0; j < 8; j++) {
				if ((*cb) & (1 << j)) {
					int dX = row;
					int dY = col + j;
					SDL_RenderDrawPoint(gRender, dX, VIDEO_X - dY);
				}
			}
			cb++;
		}

	}
	SDL_RenderPresent(gRender);
	toggle = (toggle == DRAW_TOP) ? DRAW_BOT : DRAW_TOP; // Which Interrupt to call?
	return toggle; // Return interrupt code
}