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

#include <maxmod9.h>
#include <nds.h>
#include <stdio.h>
#include <time.h>

#include "bootsplash.h"
#include "gif.h"
#include "errorsplash.h"
#include "tonccpy.h"

#include "soundbank.h"
#include "soundbank_bin.h"

#include "nintendo.h"

#include "ds_gif.h"
#include "dsi_gif.h"
#include "iquedsi_gif.h"

#include "hs_0_gif.h" // Japanese
#include "hs_1_gif.h" // English
#include "hs_2_gif.h" // French
#include "hs_3_gif.h" // German
#include "hs_4_gif.h" // Italian
#include "hs_5_gif.h" // Spanish
#include "hs_6_gif.h" // Chinese
#include "hs_7_gif.h" // Korean

const u8 *hsGifs[8] {
	hs_0_gif,
	hs_1_gif,
	hs_2_gif,
	hs_3_gif,
	hs_4_gif,
	hs_5_gif,
	hs_6_gif,
	hs_7_gif
};

const u32 hsGifSizes[8] {
	hs_0_gif_size,
	hs_1_gif_size,
	hs_2_gif_size,
	hs_3_gif_size,
	hs_4_gif_size,
	hs_5_gif_size,
	hs_6_gif_size,
	hs_7_gif_size
};

const mm_word jingles[2][2] = {
	{SFX_DSBOOT, SFX_DSBOOT_BDAY},
	{SFX_DSIBOOT, SFX_DSIBOOT_BDAY},
};

void BootJingle(bool boostCPU, bool bday) {
	mmInitDefaultMem((mm_addr)soundbank_bin);

	mmLoadEffect(jingles[boostCPU][bday]);

	mm_sound_effect jingle = {
		{ jingles[boostCPU][bday] } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};

	mmEffectEx(&jingle);
}

void SelectSfx(void) {
	mmLoadEffect(SFX_SELECT);

	mm_sound_effect select = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};

	mmEffectEx(&select);
}

void BootSplashInit(bool boostCPU) {
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);

	bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(3, 3);

	bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(7, 3);

	oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);

	// snd();

	BootSplash(boostCPU);
}

void BootSplash(bool boostCPU) {
	if(!boostCPU)
		fifoSendValue32(FIFO_USER_04, 1);

	setBrightness(3, 31);

	bool useTwlCfg = ((isDSiMode() || REG_SCFG_EXT != 0) && (*(u8*)0x02000400 != 0) && (*(u8*)0x02000401 == 0) && (*(u8*)0x02000402 == 0) && (*(u8*)0x02000404 == 0) && (*(u8*)0x02000448 != 0));
	u8 language = (useTwlCfg ? *(u8*)0x02000406 : PersonalData->language);

	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	u8 birthMonth = (useTwlCfg ? *(u8*)0x02000446 : PersonalData->birthMonth);
	u8 birthDay = (useTwlCfg ? *(u8*)0x02000447 : PersonalData->birthDay);
	bool bday = ((Time->tm_mon + 1) == birthMonth) && (Time->tm_mday == birthDay);

	const u8 *data = language == 6 ? iquedsi_gif : (boostCPU ? dsi_gif : ds_gif);
	const u32 size = language == 6 ? iquedsi_gif_size : (boostCPU ? dsi_gif_size : ds_gif_size);
	Gif splash(data, size, true, true);

	Gif healthSafety(hsGifs[language], hsGifSizes[language], false, true);

	// Draw first frame, then wait until the top is done
	healthSafety.displayFrame();
	healthSafety.pause();

	timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(100), Gif::timerHandler);

	if (REG_SCFG_MC != 0x11) {
		u16 *gfx[2];
		for(int i = 0; i < 2; i++) {
			gfx[i] = oamAllocateGfx(&oamMain, SpriteSize_64x32, SpriteColorFormat_Bmp);
			oamSet(&oamMain, i, 67 + (i * 64), 142, 0, 0, SpriteSize_64x32, SpriteColorFormat_16Color, gfx[i], 0, false, false, false, false, false);
		}

		SPRITE_PALETTE[1] = 0x4A52; // Gray for Nintendo logo
		tonccpy(gfx[0], nintendoTiles, nintendoTilesLen / 2);
		tonccpy(gfx[1], nintendoTiles + nintendoTilesLen / 2, nintendoTilesLen / 2);

		oamUpdate(&oamMain);
	}


	// Fade in
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
		setBrightness(3, (24 - i) * 31 / 24);
	}

	// If both will loop forever, show for 3s or until button press
	if(splash.loopForever() && healthSafety.loopForever()) {
		for (int i = 0; i < 60 * 3 && !keysDown(); i++) {
			swiWaitForVBlank();
			scanKeys();

			if (splash.currentFrame() == 24)
				BootJingle(boostCPU, bday);
		}
	} else {
		u16 pressed = 0;
		while (!(splash.finished() && healthSafety.finished()) && !(pressed & KEY_START)) {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();

			if (splash.waitingForInput()) {
				if(healthSafety.paused())
					healthSafety.unpause();
				if(pressed) {
					splash.resume();
					SelectSfx();
				}
			}

			if(healthSafety.waitingForInput()) {
				if(pressed) {
					healthSafety.resume();
					SelectSfx();
				}
			}

			if (splash.currentFrame() == 24)
				BootJingle(boostCPU, bday);
		}
	}

	// Fade out
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
		setBrightness(3, i * 31 / 24);
	}

	timerStop(0);

	if(REG_SCFG_MC == 0x11) { ErrorNoCard(); }

	// Set NTR mode clock speeds. DSi Mode Splash will leave this untouched.
	if(!boostCPU)
		REG_SCFG_CLK = 0x80;
}
