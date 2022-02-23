/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <nds.h>

#include "errorsplash.h"
#include "tonccpy.h"

#include "toperror.h"
#include "suberror.h"

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 24

void ErrorNoCard() {
	setBrightness(3, 31); // white screen

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA (VRAM_A_MAIN_BG_0x06000000);
	vramSetBankC (VRAM_C_SUB_BG_0x06200000);
	int bgMain = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 2);
	int bgSub = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 0, 2);
	u16* bgMapTop = bgGetMapPtr(bgMain);
	u16* bgMapSub = bgGetMapPtr(bgSub);
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH * CONSOLE_SCREEN_HEIGHT; i++) {
		bgMapTop[i] = (u16)i;
		bgMapSub[i] = (u16)i;
	}

	// Set background
	decompress((void*)toperrorTiles, (void*)CHAR_BASE_BLOCK(2), LZ77Vram);
	decompress((void*)suberrorTiles, (void*)CHAR_BASE_BLOCK_SUB(2), LZ77Vram);
	tonccpy(&BG_PALETTE[0], toperrorPal, toperrorPalLen);
	tonccpy(&BG_PALETTE_SUB[0], suberrorPal, suberrorPalLen);

	// Fade in
	for (int i = 0; i < 60; i++) {
		swiWaitForVBlank();
		setBrightness(3, (59 - i) * 31 / 59);
	}

	// Wait for input
	do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());

	// Fade out
	for (int i = 0; i < 60; i++) {
		swiWaitForVBlank();
		setBrightness(3, i * 31 / 59);
	}
}
