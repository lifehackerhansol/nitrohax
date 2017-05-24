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

typedef struct {
	char gameTitle[12];			//!< 12 characters for the game title.
	char gameCode[4];			//!< 4 characters for the game code.
} sNDSHeadertitlecodeonly;

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

	bool HealthandSafety_MSG = false;
	bool UseNTRSplash = true;

	scanKeys();
	int pressed = keysDown();

	if (fatInitDefault()) {
		CIniFile ntrforwarderini( "sd:/_nds/ntr_forwarder.ini" );
		
		if(ntrforwarderini.GetInt("NTR-FORWARDER","HEALTH&SAFETY_MSG",0) == 1) { HealthandSafety_MSG = true; }
		if(ntrforwarderini.GetInt("NTR-FORWARDER","NTR_CLOCK",0) == 0) if( pressed & KEY_A ) {} else { UseNTRSplash = false; }
		else if( pressed & KEY_A ) { UseNTRSplash = false; }
		if(ntrforwarderini.GetInt("NTR-FORWARDER","DISABLE_ANIMATION",0) == 0) { if( pressed & KEY_B ) {} else { BootSplashInit(UseNTRSplash, HealthandSafety_MSG, PersonalData->language); } }

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
	std::string savename;

	if (!fatInitDefault()) {
		BootSplashInit(UseNTRSplash, HealthandSafety_MSG, PersonalData->language);
		
		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	

		iprintf ("fatinitDefault failed!\n");		
			
		stop();
	}

	while(1) {

		gamename = "sd:/<<<Start NDS Path                                                                                                                                                                                                                            End NDS Path>>>";
		savename = ReplaceAll(gamename, ".nds", ".sav");
		
		if (access(savename.c_str(), F_OK)) {
			FILE *f_nds_file = fopen(gamename.c_str(), "rb");

			char game_TID[5];
			fseek(f_nds_file, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);
			fread(game_TID, 1, 4, f_nds_file);
			game_TID[4] = 0;
			fclose(f_nds_file);

			if (strcmp(game_TID, "####") != 0) {	// Create save if game isn't homebrew
				consoleDemoInit();
				iprintf ("Creating save file...\n");

				static const int BUFFER_SIZE = 4096;
				char buffer[BUFFER_SIZE];
				memset(buffer, 0, sizeof(buffer));

				int savesize = 524288;	// 512KB (default size for most games)

				// Set save size to 1MB for the following games
				if (strcmp(game_TID, "AZLJ") == 0 ||	// Wagamama Fashion: Girls Mode
					strcmp(game_TID, "AZLE") == 0 ||	// Style Savvy
					strcmp(game_TID, "AZLP") == 0 ||	// Nintendo presents: Style Boutique
					strcmp(game_TID, "AZLK") == 0 )	// Namanui Collection: Girls Style
				{
					savesize = 1048576;
				}

				// Set save size to 32MB for the following games
				if (strcmp(game_TID, "UORE") == 0 ||	// WarioWare - D.I.Y.
					strcmp(game_TID, "UORP") == 0 )	// WarioWare - Do It Yourself
				{
					savesize = 1048576*32;
				}

				FILE *pFile = fopen(savename.c_str(), "wb");
				if (pFile) {
					for (int i = savesize; i > 0; i -= BUFFER_SIZE) {
						fwrite(buffer, 1, sizeof(buffer), pFile);
					}
					fclose(pFile);
				}
				iprintf ("Done!\n");
			}

		}

		CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
		bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", gamename);
		bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", savename);
		scanKeys();
		if (keysHeld() & KEY_Y) bootstrapini.SetString("NDS-BOOTSTRAP", "ARM7_DONOR_PATH", gamename);
		bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );
		filename = bootstrapini.GetString("NDS-BOOTSTRAP", "BOOTSTRAP_PATH","");
		runFile(filename);
		
		consoleDemoInit();
		iprintf ("Bootstrap not found.\n");
		iprintf ("Running without patches.\n");
		iprintf ("\n");
		iprintf ("If this is a retail ROM,\n");
		iprintf ("it will not run.\n");
		iprintf ("\n");
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
