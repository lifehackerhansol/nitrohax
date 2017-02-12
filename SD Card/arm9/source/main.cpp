/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/fifocommon.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>
#include <nds/disc_io.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"

#include "bootsplash.h"
#include "inifile.h"

using namespace std;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause() {
//---------------------------------------------------------------------------------
	iprintf("Press start...\n");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
		swiWaitForVBlank();
	}
	scanKeys();
}

void runFile(string filename) {
	vector<char*> argarray;
	
	if ( strcasecmp (filename.c_str() + filename.size() - 5, ".argv") == 0) {
		FILE *argfile = fopen(filename.c_str(),"rb");
		char str[PATH_MAX], *pstr;
		const char seps[]= "\n\r\t ";

		while( fgets(str, PATH_MAX, argfile) ) {
			// Find comment and end string there
			if( (pstr = strchr(str, '#')) )
				*pstr= '\0';

			// Tokenize arguments
			pstr= strtok(str, seps);

			while( pstr != NULL ) {
				argarray.push_back(strdup(pstr));
				pstr= strtok(NULL, seps);
			}
		}
		fclose(argfile);
		filename = argarray.at(0);
	} else {
		argarray.push_back(strdup(filename.c_str()));
	}

	if ( strcasecmp (filename.c_str() + filename.size() - 4, ".nds") != 0 || argarray.size() == 0 ) {
		iprintf("no nds file specified\n");
	} else {
		iprintf("Running %s with %d parameters\n", argarray[0], argarray.size());
		int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
		iprintf("Start failed. Error %i\n", err);
		doPause();
	}
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}


//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	// REG_SCFG_CLK = 0x80;
	REG_SCFG_CLK = 0x85;

	bool UseNTRSplash = true;
	bool TriggerExit = false;
	std::string	bootstrapPath = "";

	scanKeys();
	int pressed = keysDown();

	if (fatInitDefault()) {
		CIniFile ntrforwarderini( "sd:/_nds/ntr_forwarder.ini" );
		
		if(ntrforwarderini.GetInt("NTR-FORWARDER","NTR_CLOCK",0) == 0) { UseNTRSplash = false; }
		if(ntrforwarderini.GetInt("NTR-FORWARDER","DISABLE_ANIMATION",0) == 0) { if( pressed & KEY_B ) {} else { BootSplashInit(UseNTRSplash); } }
		if(ntrforwarderini.GetInt("NTR-FORWARDER","NTR_CLOCK",0) == 0) {
			REG_SCFG_EXT = 0x83002000;
			REG_SCFG_CLK |= 0x1;
		} else {
			REG_SCFG_EXT = 0x83000000;
			REG_SCFG_CLK = 0x80;
			fifoSendValue32(FIFO_USER_04, 1);
		}

		if(ntrforwarderini.GetInt("NTR-FORWARDER","RESET_SLOT1",0) == 1) {
			fifoSendValue32(FIFO_USER_02, 1);
		}

		fifoSendValue32(FIFO_USER_01, 1);
		fifoWaitValue32(FIFO_USER_03);

		// Only time SCFG should be locked is for compatiblity with NTR retail stuff.
		// So NTR SCFG values (that preserve SD access) are always used when locking.
		// Locking Arm9 SCFG kills SD access. So that will not occur here.
		/* if(ntrforwarderini.GetInt("NTR-FORWARDER","LOCK_ARM7_SCFG_EXT",0) == 1) {
			fifoSendValue32(FIFO_USER_05, 1);
			REG_SCFG_EXT = 0x83000000;
		} else {
			if(ntrforwarderini.GetInt("NTR-FORWARDER","ENABLE_ALL_TWLSCFG",0) == 1) {
				fifoSendValue32(FIFO_USER_06, 1);
				REG_SCFG_EXT = 0x8307F100;
			} else {
				REG_SCFG_EXT = 0x83000000;
			}
		} */
		// Tell Arm7 to apply changes.
		fifoSendValue32(FIFO_USER_07, 1);

		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }
	}

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	std::string filename;
	std::string gamename;

	if (!fatInitDefault()) {
		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	

		iprintf ("fatinitDefault failed!\n");		
			
		doPause();
		
		TriggerExit = true;
	}

	vector<string> extensionList;
	extensionList.push_back(".nds");
	extensionList.push_back(".argv");

	while(1) {

		if(TriggerExit) { 
		do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());
		break;
		}
		
		gamename = "sd:/<<<Start NDS Path                                                                                                                                                                                                                                  End NDS Path>>>";

				CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
				std::string path = gamename;
				path = ReplaceAll( path, "sd:/", "fat:/");
				bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH",path);
				bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );
				filename = bootstrapini.GetString("NDS-BOOTSTRAP", "BOOTSTRAP_PATH","");
				filename = ReplaceAll( filename, "fat:/", "sd:/");
				runFile(filename);
				
				consoleDemoInit();
				iprintf ("Bootstrap not found.\n");
				iprintf ("Running without patches.\n");
				doPause();
			
				runFile (gamename);
				iprintf ("ROM not found.\n");

		while (1) {
			swiWaitForVBlank();
			scanKeys();
			if (!(keysHeld() & KEY_A)) break;
		}
	}

	return 0;
}
