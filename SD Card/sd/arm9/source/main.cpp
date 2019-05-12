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

#include "ndsheaderbanner.h"
#include "nds_loader_arm9.h"

#include "bootsplash.h"
#include "inifile.h"
#include "fileCopy.h"

using namespace std;

int donorSdkVer = 0;

bool gameSoftReset = false;

int mpuregion = 0;
int mpusize = 0;
bool ceCached = true;

bool bootstrapFile = false;

/**
 * Remove trailing slashes from a pathname, if present.
 * @param path Pathname to modify.
 */
void RemoveTrailingSlashes(std::string& path)
{
	while (!path.empty() && path[path.size()-1] == '/') {
		path.resize(path.size()-1);
	}
}

/**
 * Set donor SDK version for a specific game.
 */
void SetDonorSDK(const char* filename) {
	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	grabTID(f_nds_file, game_TID);
	game_TID[4] = 0;
	game_TID[3] = 0;
	fclose(f_nds_file);
	
	donorSdkVer = 0;

	// Check for ROM hacks that need an SDK version.
	static const char sdk2_list[][4] = {
		"AMQ",	// Mario vs. Donkey Kong 2 - March of the Minis
		"AMH",	// Metroid Prime Hunters
		"ASM",	// Super Mario 64 DS
	};
	
	static const char sdk3_list[][4] = {
		"AMC",	// Mario Kart DS
		"EKD",	// Ermii Kart DS (Mario Kart DS hack)
		"A2D",	// New Super Mario Bros.
		"ADA",	// Pokemon Diamond
		"APA",	// Pokemon Pearl
		"ARZ",	// Rockman ZX/MegaMan ZX
		"YZX",	// Rockman ZX Advent/MegaMan ZX Advent
	};
	
	static const char sdk4_list[][4] = {
		"YKW",	// Kirby Super Star Ultra
		"A6C",	// MegaMan Star Force: Dragon
		"A6B",	// MegaMan Star Force: Leo
		"A6A",	// MegaMan Star Force: Pegasus
		"B6Z",	// Rockman Zero Collection/MegaMan Zero Collection
		"YT7",	// SEGA Superstars Tennis
		"AZL",	// Style Savvy
		"BKI",	// The Legend of Zelda: Spirit Tracks
		"B3R",	// Pokemon Ranger: Guardian Signs
	};

	static const char sdk5_list[][4] = {
		"B2D",	// Doctor Who: Evacuation Earth
		"BH2",	// Super Scribblenauts
		"BSD",	// Lufia: Curse of the Sinistrals
		"BXS",	// Sonic Colo(u)rs
		"BOE",	// Inazuma Eleven 3: Sekai heno Chousen! The Ogre
		"BQ8",	// Crafting Mama
		"BK9",	// Kingdom Hearts: Re-Coded
		"BRJ",	// Radiant Historia
		"IRA",	// Pokemon Black Version
		"IRB",	// Pokemon White Version
		"VI2",	// Fire Emblem: Shin Monshou no Nazo Hikari to Kage no Eiyuu
		"BYY",	// Yu-Gi-Oh 5Ds World Championship 2011: Over The Nexus
		"UZP",	// Learn with Pokemon: Typing Adventure
		"B6F",	// LEGO Batman 2: DC Super Heroes
		"IRE",	// Pokemon Black Version 2
		"IRD",	// Pokemon White Version 2
	};

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(sdk2_list) / sizeof(sdk2_list[0]); i++) {
		if (!memcmp(game_TID, sdk2_list[i], 3)) {
			// Found a match.
			donorSdkVer = 2;
			break;
		}
	}
	
	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(sdk3_list) / sizeof(sdk3_list[0]); i++) {
		if (!memcmp(game_TID, sdk3_list[i], 3)) {
			// Found a match.
			donorSdkVer = 3;
			break;
		}
	}

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(sdk4_list) / sizeof(sdk4_list[0]); i++) {
		if (!memcmp(game_TID, sdk4_list[i], 3)) {
			// Found a match.
			donorSdkVer = 4;
			break;
		}
	}

	if (strncmp(game_TID, "V", 1) == 0) {
		donorSdkVer = 5;
	} else {
		// TODO: If the list gets large enough, switch to bsearch().
		for (unsigned int i = 0; i < sizeof(sdk5_list) / sizeof(sdk5_list[0]); i++) {
			if (!memcmp(game_TID, sdk5_list[i], 3)) {
				// Found a match.
				donorSdkVer = 5;
				break;
			}
		}
	}
}

/**
 * Disable soft-reset, in favor of non OS_Reset one, for a specific game.
 */
void SetGameSoftReset(const char* filename) {
	scanKeys();
	if(keysHeld() & KEY_R){
		gameSoftReset = true;
		return;
	}

	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	fseek(f_nds_file, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);
	fread(game_TID, 1, 4, f_nds_file);
	game_TID[4] = 0;
	game_TID[3] = 0;
	fclose(f_nds_file);

	gameSoftReset = false;

	// Check for games that have it's own reset function (OS_Reset not used).
	static const char list[][4] = {
		"NTR",	// Download Play ROMs
		"ASM",	// Super Mario 64 DS
		"SMS",	// Super Mario Star World, and Mario's Holiday
		"AMC",	// Mario Kart DS
		"EKD",	// Ermii Kart DS
		"A2D",	// New Super Mario Bros.
		"ARZ",	// Rockman ZX/MegaMan ZX
		"AKW",	// Kirby Squeak Squad/Mouse Attack
		"YZX",	// Rockman ZX Advent/MegaMan ZX Advent
		"B6Z",	// Rockman Zero Collection/MegaMan Zero Collection
	};

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(list)/sizeof(list[0]); i++) {
		if (memcmp(game_TID, list[i], 3) == 0) {
			// Found a match.
			gameSoftReset = true;
			break;
		}
	}
}

/**
 * Set MPU settings for a specific game.
 */
void SetMPUSettings(const char* filename) {
	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	fseek(f_nds_file, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);
	fread(game_TID, 1, 4, f_nds_file);
	game_TID[4] = 0;
	game_TID[3] = 0;
	fclose(f_nds_file);

	scanKeys();
	int pressed = keysHeld();
	
	if(pressed & KEY_B){
		mpuregion = 1;
	} else if(pressed & KEY_X){
		mpuregion = 2;
	} else if(pressed & KEY_R){
		mpuregion = 3;
	} else {
		mpuregion = 0;
	}
	if(pressed & KEY_RIGHT){
		mpusize = 3145728;
	} else if(pressed & KEY_LEFT){
		mpusize = 1;
	} else {
		mpusize = 0;
	}

	// Check for games that need an MPU size of 3 MB.
	static const char mpu_3MB_list[][4] = {
		"A7A",	// DS Download Station - Vol 1
		"A7B",	// DS Download Station - Vol 2
		"A7C",	// DS Download Station - Vol 3
		"A7D",	// DS Download Station - Vol 4
		"A7E",	// DS Download Station - Vol 5
		"A7F",	// DS Download Station - Vol 6 (EUR)
		"A7G",	// DS Download Station - Vol 6 (USA)
		"A7H",	// DS Download Station - Vol 7
		"A7I",	// DS Download Station - Vol 8
		"A7J",	// DS Download Station - Vol 9
		"A7K",	// DS Download Station - Vol 10
		"A7L",	// DS Download Station - Vol 11
		"A7M",	// DS Download Station - Vol 12
		"A7N",	// DS Download Station - Vol 13
		"A7O",	// DS Download Station - Vol 14
		"A7P",	// DS Download Station - Vol 15
		"A7Q",	// DS Download Station - Vol 16
		"A7R",	// DS Download Station - Vol 17
		"A7S",	// DS Download Station - Vol 18
		"A7T",	// DS Download Station - Vol 19
		"ARZ",	// Rockman ZX/MegaMan ZX
		"YZX",	// Rockman ZX Advent/MegaMan ZX Advent
		"B6Z",	// Rockman Zero Collection/MegaMan Zero Collection
		"A2D",	// New Super Mario Bros.
	};

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(mpu_3MB_list)/sizeof(mpu_3MB_list[0]); i++) {
		if (memcmp(game_TID, mpu_3MB_list[i], 3) == 0) {
			// Found a match.
			mpuregion = 1;
			mpusize = 3145728;
			break;
		}
	}
}

/**
 * Exclude moving nds-bootstrap's cardEngine_arm9 to cached memory region for some games.
 */
void SetSpeedBumpExclude(const char *filename) {
	scanKeys();
	if(keysHeld() & KEY_L){
		ceCached = false;
		return;
	}

	ceCached = true;

	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	fseek(f_nds_file, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);
	fread(game_TID, 1, 4, f_nds_file);
	fclose(f_nds_file);

	static const char list[][5] = {
		"AWRP",	// Advance Wars: Dual Strike (EUR)
		"AVCP",	// Magical Starsign (EUR)
		"YFTP",	// Pokemon Mystery Dungeon: Explorers of Time (EUR)
		"YFYP",	// Pokemon Mystery Dungeon: Explorers of Darkness (EUR)
	};

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(list)/sizeof(list[0]); i++) {
		if (memcmp(game_TID, list[i], 4) == 0) {
			// Found a match.
			ceCached = false;
			break;
		}
	}

	static const char list2[][4] = {
		"AEK",	// Age of Empires: The Age of Kings
		"ALC",	// Animaniacs: Lights, Camera, Action!
		"YAH",	// Assassin's Creed: Altair's Chronicles
		//"ACV",	// Castlevania: Dawn of Sorrow	(fixed on nds-bootstrap side)
		"A5P",	// Harry Potter and the Order of the Phoenix
		"AR2",	// Kirarin * Revolution: Naasan to Issho
		"ARM",	// Mario & Luigi: Partners in Time
		"CLJ",	// Mario & Luigi: Bowser's Inside Story
		"COL",	// Mario & Sonic at the Olympic Winter Games
		"AMQ",	// Mario vs. Donkey Kong 2: March of the Minis
		"B2K",	// Ni no Kuni: Shikkoku no Madoushi
		"C2S",	// Pokemon Mystery Dungeon: Explorers of Sky
		"Y6S",	// Pokemon Mystery Dungeon: Explorers of Sky (Demo)
		"B3R",	// Pokemon Ranger: Guardian Signs
		"APU",	// Puyo Puyo!! 15th Anniversary
		"BYO",	// Puyo Puyo 7
		"YZX",	// Rockman ZX Advent/MegaMan ZX Advent
	    "B6X", // Rockman EXE: Operate Shooting Star
	    "B6Z", // Rockman Zero Collection/MegaMan Zero Collection
		"ARF", // Rune Factory: A Fantasy Harvest Moon
		"AN6", // Rune Factory 2: A Fantasy Harvest Moon
		"AS2", // Spider-Man 2
		"AQ3", // Spider-Man 3
		"CS7", // Summon Night X: Tears Crown
		"AYT", // Tales of Innocence
		"YYK", // Trauma Center: Under the Knife 2
	};

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(list2)/sizeof(list2[0]); i++) {
		if (memcmp(game_TID, list2[i], 3) == 0) {
			// Found a match.
			ceCached = false;
			break;
		}
	}
}

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
	bool HealthandSafety_MSG = false;
	bool UseNTRSplash = true;
	bool SOUND_FREQ = false;
	bool consoleInited = false;

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	scanKeys();
	int pressed = keysHeld();

	if (fatInitDefault()) {
		CIniFile ntrforwarderini( "sd:/_nds/ntr_forwarder.ini" );

		bootstrapFile = ntrforwarderini.GetInt("NTR-FORWARDER", "BOOTSTRAP_FILE", 0);

		HealthandSafety_MSG = ntrforwarderini.GetInt("NTR-FORWARDER","HEALTH&SAFETY_MSG",0);
		if(ntrforwarderini.GetInt("NTR-FORWARDER","NTR_CLOCK",0) == 0) if( pressed & KEY_A ) {} else { UseNTRSplash = false; }
		else if( pressed & KEY_A ) { UseNTRSplash = false; }
		if(ntrforwarderini.GetInt("NTR-FORWARDER","DISABLE_ANIMATION",0) == 0) { if( pressed & KEY_B ) {} else { BootSplashInit(UseNTRSplash, HealthandSafety_MSG, PersonalData->language); } }

		SOUND_FREQ = ntrforwarderini.GetInt("NTR-FORWARDER","SOUND_FREQ",0);
		if(SOUND_FREQ) {
			fifoSendValue32(FIFO_USER_08, 1);
		}

		vector<char*> argarray;

		std::string ndsPath = ntrforwarderini.GetString("NTR-FORWARDER", "NDS_PATH", "");

		std::string romfolder = ndsPath;
		while (!romfolder.empty() && romfolder[romfolder.size()-1] != '/') {
			romfolder.resize(romfolder.size()-1);
		}
		chdir(romfolder.c_str());
		mkdir ("saves", 0777);

		std::string filename = ndsPath;
		const size_t last_slash_idx = filename.find_last_of("/");
		if (std::string::npos != last_slash_idx)
		{
			filename.erase(0, last_slash_idx + 1);
		}

		FILE *f_nds_file = fopen(ndsPath.c_str(), "rb");

		char game_TID[5];
		grabTID(f_nds_file, game_TID);
		game_TID[4] = 0;
		game_TID[3] = 0;
		fclose(f_nds_file);

		std::string savename = ReplaceAll(filename, ".nds", ".sav");
		std::string romFolderNoSlash = romfolder;
		RemoveTrailingSlashes(romFolderNoSlash);
		std::string savepath = romFolderNoSlash+"/saves/"+savename;

		if (getFileSize(savepath.c_str()) == 0 && strcmp(game_TID, "###") != 0) {
			consoleDemoInit();
			iprintf ("Creating save file...\n");

			static const int BUFFER_SIZE = 4096;
			char buffer[BUFFER_SIZE];
			memset(buffer, 0, sizeof(buffer));

			int savesize = 524288;	// 512KB (default size for most games)

			// Set save size to 8KB for the following games
			if (strcmp(game_TID, "ASC") == 0 )	// Sonic Rush
			{
				savesize = 8192;
			}

			// Set save size to 256KB for the following games
			if (strcmp(game_TID, "AMH") == 0 )	// Metroid Prime Hunters
			{
				savesize = 262144;
			}

			// Set save size to 1MB for the following games
			if ( strcmp(game_TID, "AZL") == 0		// Wagamama Fashion: Girls Mode/Style Savvy/Nintendo presents: Style Boutique/Namanui Collection: Girls Style
				|| strcmp(game_TID, "BKI") == 0 )	// The Legend of Zelda: Spirit Tracks
			{
				savesize = 1048576;
			}

			// Set save size to 32MB for the following games
			if (strcmp(game_TID, "UOR") == 0 )	// WarioWare - D.I.Y. (Do It Yourself)
			{
				savesize = 1048576*32;
			}

			FILE *pFile = fopen(savepath.c_str(), "wb");
			if (pFile) {
				for (int i = savesize; i > 0; i -= BUFFER_SIZE) {
					fwrite(buffer, 1, sizeof(buffer), pFile);
				}
				fclose(pFile);
			}
			iprintf ("Done!\n");
		}

		SetDonorSDK(ndsPath.c_str());
		SetGameSoftReset(ndsPath.c_str());
		SetMPUSettings(ndsPath.c_str());
		SetSpeedBumpExclude(ndsPath.c_str());

		CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
		bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", ndsPath);
		bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", savepath);
		bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "");
		bootstrapini.SetInt("NDS-BOOTSTRAP", "DONOR_SDK_VER", donorSdkVer);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "GAME_SOFT_RESET", gameSoftReset);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_REGION", mpuregion);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_SIZE", mpusize);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "CARDENGINE_CACHED", ceCached);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "CONSOLE_MODEL", 2);
		bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );

		scanKeys();
		int held = keysHeld();

		if (strcmp(game_TID, "###") == 0) {
			if (!(held & KEY_A)) {
				argarray.push_back((char*)(bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds"));
				int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
				if (!consoleInited) {
					consoleDemoInit();
					consoleInited = true;
				}
				if (err == 1) iprintf ("hb-bootstrap not found.\n");
			}
		} else {
			argarray.push_back((char*)(bootstrapFile ? "sd:/_nds/nds-bootstrap-nightly.nds" : "sd:/_nds/nds-bootstrap-release.nds"));
			int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
			if (!consoleInited) {
				consoleDemoInit();
				consoleInited = true;
			}
			iprintf("Start failed. Error %i\n", err);
			if (err == 1) iprintf ("Bootstrap not found.\n");
		}

		if (!consoleInited) {
			consoleDemoInit();
			consoleInited = true;
		}
		iprintf ("Running without patches.\n");
		iprintf ("\n");
		doPause();

		argarray.at(0) = (char*)(ndsPath.c_str());
		int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
		consoleClear();
		iprintf("Start failed. Error %i\n", err);
		if (err == 1) iprintf ("ROM not found.\n");
	} else {
		BootSplashInit(UseNTRSplash, HealthandSafety_MSG, PersonalData->language);

		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	

		iprintf ("fatinitDefault failed!\n");
	}

	iprintf ("\n");		
	iprintf ("Press B to return to\n");
	iprintf ("HOME Menu.\n");		

	while (1) {
		scanKeys();
		if (keysHeld() & KEY_B) fifoSendValue32(FIFO_USER_01, 1);	// Tell ARM7 to reboot into 3DS HOME Menu (power-off/sleep mode screen skipped)
		swiWaitForVBlank();
	}

	return 0;
}
