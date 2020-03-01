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

#include "inifile.h"
#include "fileCopy.h"

#include "donorMap.h"
#include "speedBumpExcludeMap.h"

using namespace std;

static bool boostCpu = false;
static bool boostVram = false;
static int dsiMode = 0;

static bool gameSoftReset = false;

static int mpuregion = 0;
static int mpusize = 0;
static bool ceCached = true;

static bool bootstrapFile = false;

static bool consoleInited = false;

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
int SetDonorSDK(const char* filename) {
	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	grabTID(f_nds_file, game_TID);
	fclose(f_nds_file);
	
	for (auto i : donorMap) {
		if (i.first == 5 && game_TID[0] == 'V')
			return 5;

		if (i.second.find(game_TID) != i.second.cend())
			return i.first;
	}

	return 0;
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
void SetSpeedBumpInclude(const char *filename) {
	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	fseek(f_nds_file, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);
	fread(game_TID, 1, 4, f_nds_file);
	fclose(f_nds_file);

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(sbeList)/sizeof(sbeList[0]); i++) {
		if (memcmp(game_TID, sbeList[i], 4) == 0) {
			// Found a match.
			ceCached = false;
			break;
		}
	}

	// TODO: If the list gets large enough, switch to bsearch().
	for (unsigned int i = 0; i < sizeof(sbeList2)/sizeof(sbeList2[0]); i++) {
		if (memcmp(game_TID, sbeList2[i], 3) == 0) {
			// Found match
			ceCached = false;
			break;
		}
	}

	scanKeys();
	if(keysHeld() & KEY_L){
		ceCached = !ceCached;
	}
}

/**
 * Fix AP for some games.
 */
std::string setApFix(const char *filename) {
	bool useTwlmPath = (access("sd:/_nds/TWiLightMenu/apfix", F_OK) == 0);

	bool ipsFound = false;
	char ipsPath[256];
	snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/%s/apfix/%s.ips", (useTwlmPath ? "TWiLightMenu" : "ntr-forwarder"), filename);
	ipsFound = (access(ipsPath, F_OK) == 0);

	if (!ipsFound) {
		FILE *f_nds_file = fopen(filename, "rb");

		char game_TID[5];
		u16 headerCRC16 = 0;
		fseek(f_nds_file, offsetof(sNDSHeader2, gameCode), SEEK_SET);
		fread(game_TID, 1, 4, f_nds_file);
		fseek(f_nds_file, offsetof(sNDSHeader2, headerCRC16), SEEK_SET);
		fread(&headerCRC16, sizeof(u16), 1, f_nds_file);
		fclose(f_nds_file);
		game_TID[4] = 0;

		snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/%s/apfix/%s-%X.ips", (useTwlmPath ? "TWiLightMenu" : "ntr-forwarder"), game_TID, headerCRC16);
		ipsFound = (access(ipsPath, F_OK) == 0);
	}

	if (ipsFound) {
		return ipsPath;
	}

	return "";
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

	defaultExceptionHandler();

	if (fatInitDefault()) {
		CIniFile ntrforwarderini( "sd:/_nds/ntr_forwarder.ini" );

		bootstrapFile = ntrforwarderini.GetInt("NTR-FORWARDER", "BOOTSTRAP_FILE", 0);

		std::string ndsPath = argv[1];

		std::string romfolder = ndsPath;
		while (!romfolder.empty() && romfolder[romfolder.size()-1] != '/') {
			romfolder.resize(romfolder.size()-1);
		}
		chdir(romfolder.c_str());

		std::string filename = ndsPath;
		const size_t last_slash_idx = filename.find_last_of("/");
		if (std::string::npos != last_slash_idx)
		{
			filename.erase(0, last_slash_idx + 1);
		}

		FILE *f_nds_file = fopen(filename.c_str(), "rb");
		int isHomebrew = checkIfHomebrew(f_nds_file);

		if (isHomebrew == 0) {
			mkdir ("saves", 0777);
		}

		char game_TID[5];
		grabTID(f_nds_file, game_TID);
		game_TID[4] = 0;
		fclose(f_nds_file);

		vector<char*> argarray;

		if (isHomebrew == 2) {
			argarray.push_back(argv[1]);
			int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
			if (!consoleInited) {
				consoleDemoInit();
				consoleInited = true;
			}
			iprintf("Start failed. Error %i\n", err);
			if (err == 1) iprintf ("ROM not found.\n");
		} else {
			// Delete cheat data
			remove("sd:/_nds/nds-bootstrap/cheatData.bin");
			remove("sd:/_nds/nds-bootstrap/wideCheatData.bin");

			std::string savename = ReplaceAll(filename, ".nds", ".sav");
			std::string romFolderNoSlash = romfolder;
			RemoveTrailingSlashes(romFolderNoSlash);
			std::string savepath = romFolderNoSlash+"/saves/"+savename;

			if (getFileSize(savepath.c_str()) == 0 && isHomebrew == 0) {
				consoleDemoInit();
				iprintf ("Creating save file...\n");
				iprintf ("\n");
				iprintf ("If this takes a while,\n");
				iprintf ("press HOME, and press B.\n");
				iprintf ("\n");

				static const int BUFFER_SIZE = 4096;
				char buffer[BUFFER_SIZE];
				memset(buffer, 0, sizeof(buffer));

				int savesize = 524288;	// 512KB (default size for most games)

				// Set save size to 8KB for the following games
				if (strncmp(game_TID, "ASC", 3) == 0 )	// Sonic Rush
				{
					savesize = 8192;
				}

				// Set save size to 256KB for the following games
				if (strncmp(game_TID, "AMH", 3) == 0 )	// Metroid Prime Hunters
				{
					savesize = 262144;
				}

				// Set save size to 1MB for the following games
				if (strncmp(game_TID, "AZL", 3) == 0	// Wagamama Fashion: Girls Mode/Style Savvy/Nintendo presents: Style Boutique/Namanui Collection: Girls Style
				 || strncmp(game_TID, "C6P", 3) == 0	// Picross 3D
				 || strncmp(game_TID, "BKI", 3) == 0)	// The Legend of Zelda: Spirit Tracks
				{
					savesize = 1048576;
				}

				// Set save size to 8MB for the following games
				if (strncmp(game_TID, "VAA", 3) == 0	// Art Academy
				 || strncmp(game_TID, "UXB", 3) == 0)	// Jam with the Band
				{
					savesize = 1048576*8;
				}

				// Set save size to 32MB for the following games
				if (strncmp(game_TID, "UOR", 3) == 0)	// WarioWare - D.I.Y. (Do It Yourself)
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

			int donorSdkVer = 0;

			if (isHomebrew == 0) {
				donorSdkVer = SetDonorSDK(ndsPath.c_str());
				SetGameSoftReset(ndsPath.c_str());
				SetMPUSettings(ndsPath.c_str());
				SetSpeedBumpInclude(ndsPath.c_str());
			}

			CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
			// Read
			boostCpu = bootstrapini.GetInt("NDS-BOOTSTRAP", "BOOST_CPU", boostCpu);
			boostVram = bootstrapini.GetInt("NDS-BOOTSTRAP", "BOOST_VRAM", boostVram);
			dsiMode = bootstrapini.GetInt("NDS-BOOTSTRAP", "DSI_MODE", dsiMode);
			if (dsiMode < 0) dsiMode = 0;
			else if (dsiMode > 2) dsiMode = 2;

			// Write
			bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", ndsPath);
			bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", savepath);
			if (isHomebrew == 0) {
				bootstrapini.SetString("NDS-BOOTSTRAP", "AP_FIX_PATH", setApFix(filename.c_str()));
			}
			bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "");
			bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", boostCpu);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", boostVram);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", dsiMode);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "DONOR_SDK_VER", donorSdkVer);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "GAME_SOFT_RESET", gameSoftReset);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_REGION", mpuregion);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_SIZE", mpusize);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "CARDENGINE_CACHED", ceCached);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "CONSOLE_MODEL", 2);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", -1);
			bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );

			if (isHomebrew == 1) {
				argarray.push_back((char*)(bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds"));
				int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
				if (!consoleInited) {
					consoleDemoInit();
					consoleInited = true;
				}
				iprintf("Start failed. Error %i\n", err);
				if (err == 1) iprintf ("nds-bootstrap (hb) not found.\n");
			} else {
				argarray.push_back((char*)(bootstrapFile ? "sd:/_nds/nds-bootstrap-nightly.nds" : "sd:/_nds/nds-bootstrap-release.nds"));
				int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
				if (!consoleInited) {
					consoleDemoInit();
					consoleInited = true;
				}
				iprintf("Start failed. Error %i\n", err);
				if (err == 1) iprintf ("nds-bootstrap not found.\n");
			}
		}
	} else {
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
