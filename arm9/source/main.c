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
#include <nds/fifocommon.h>

#include <stdio.h>
#include <fat.h>
#include <string.h>
#include <malloc.h>

#include "cheat_engine.h"
#include "version.h"

static inline void ensure (bool condition, const char* errorMsg) {
	if (false == condition) {
		consoleDemoInit();
		iprintf (errorMsg);
		while(1) swiWaitForVBlank();
	}

	return;
}

//---------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
	bool TWLBoostCPU = false;
	bool BoostVRAM = false;

	// If slot is powered off, tell Arm7 slot power on is required.
	if(REG_SCFG_MC == 0x11) { fifoSendValue32(FIFO_USER_02, 1); }
	if(REG_SCFG_MC == 0x10) { fifoSendValue32(FIFO_USER_02, 1); }

	if(REG_SCFG_MC == 0x11) {
		consoleDemoInit();
		iprintf ("No cartridge detected!\nPlease insert a cartridge to continue!");
		do { swiWaitForVBlank(); } while (REG_SCFG_MC == 0x11);
		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }
	}

	// Tell Arm7 it's ready for card reset (if card reset is nessecery)
	fifoSendValue32(FIFO_USER_01, 1);
	// Waits for Arm7 to finish card reset (if nessecery)
	fifoWaitValue32(FIFO_USER_03);

	sysSetCardOwner (BUS_OWNER_ARM9);

	// Delay half a second for the DS card to stabilise
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }

	if(TWLBoostCPU == true) { BoostVRAM = true; } else { BoostVRAM = false; }

	while(1) {
		if(REG_SCFG_MC == 0x11) {
			break;
		} else {
			runCheatEngine (BoostVRAM);
		}
	}
	return 0;
}
