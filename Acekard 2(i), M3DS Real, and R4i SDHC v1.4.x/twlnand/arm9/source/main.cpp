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
#include <fat.h>
#include <nds/fifocommon.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <list>

#include "inifile.h"
#include "bootsplash.h"
#include "nds_card.h"
#include "launch_engine.h"
#include "crc.h"
#include "version.h" 

int main(int argc, const char* argv[]) {
	
	// NTR Mode/Splash used by default
	bool UseNTRSplash = true;

	REG_SCFG_CLK = 0x85;

	swiWaitForVBlank();

	// If slot is powered off, tell Arm7 slot power on is required.
	if(REG_SCFG_MC == 0x11) { fifoSendValue32(FIFO_USER_02, 1); }
	if(REG_SCFG_MC == 0x10) { fifoSendValue32(FIFO_USER_02, 1); }

	dsi_forceTouchDsmode();

	u32 ndsHeader[0x80];
	char gameid[4];
	uint32_t headerCRC;
	
	scanKeys();
	int pressed = keysDown();

	if (fatInitDefault()) {
		CIniFile ntrlauncher_config( "sd:/_nds/ntr_forwarder.ini" );
		
		if(ntrlauncher_config.GetInt("NTR-FORWARDER","NTR_CLOCK",0) == 0) { UseNTRSplash = false; }

		if(ntrlauncher_config.GetInt("NTR-FORWARDER","DISABLE_ANIMATION",0) == 0) {
			if( pressed & KEY_B ) { if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); } } else { BootSplashInit(UseNTRSplash); }
		} else {
			if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); }
		}

		if( UseNTRSplash == true ) {
			fifoSendValue32(FIFO_USER_04, 1);
			REG_SCFG_CLK = 0x80;
			swiWaitForVBlank();
		}

		if(ntrlauncher_config.GetInt("NTR-FORWARDER","RESET_SLOT1",0) == 1) {
			fifoSendValue32(FIFO_USER_02, 1);
			fifoSendValue32(FIFO_USER_06, 1);
		}

	} else {
		if ( pressed & KEY_B ) { if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); } } else { BootSplashInit(UseNTRSplash); }
	}

	// Tell Arm7 it's ready for card reset (if card reset is nessecery)
	fifoSendValue32(FIFO_USER_01, 1);
	// Waits for Arm7 to finish card reset (if nessecery)
	fifoWaitValue32(FIFO_USER_03);


	// Wait for card to stablize before continuing
	for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }

	sysSetCardOwner (BUS_OWNER_ARM9);

	getHeader (ndsHeader);

	for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }
	
	memcpy (gameid, ((const char*)ndsHeader) + 12, 4);
	headerCRC = crc32((const char*)ndsHeader, sizeof(ndsHeader));
	
		FILE* inifile;
		struct VariablesToSave {
			char var1[8];
			u16 var1nl;
			char var2[269];
			u16 var2nl;
			char var3[16];
			u16 var3nl;
			char var4[19];
		} IniFile;

		strcpy(IniFile.var1,"[YSMENU]");
		IniFile.var1nl = 0x0A0D;
		strcpy(IniFile.var2,"AUTO_BOOT=/<<<Start NDS Path                                                                                                                                                                                                                                  End NDS Path>>>");
		IniFile.var2nl = 0x0A0D;
		strcpy(IniFile.var3,"DEFAULT_DMA=true");	// For saving
		IniFile.var3nl = 0x0A0D;
		strcpy(IniFile.var4,"DEFAULT_RESET=false");	// For soft-reset
	
		inifile = fopen("sd:/_nds/YSMenu.ini","wb");
		fwrite(&IniFile,1,sizeof(IniFile),inifile);
		fclose(inifile);

	while(1) {
		if(REG_SCFG_MC == 0x11) { 
		break; } else {
			runLaunchEngine ();
		}
	}
	return 0;
}

