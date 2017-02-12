/*-----------------------------------------------------------------

 Copyright (C) 2010  Dave "WinterMute" Murphy

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
#include <fat.h>
#include <limits.h>

#include <stdio.h>
#include <stdarg.h>

#include <nds/fifocommon.h>

#include "nds_loader_arm9.h"
#include "inifile.h"
#include "bootsplash.h"
#include "errorsplash.h"

using namespace std;

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
	} else {
		int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
	}
}

int main(int argc, char **argv) {

	REG_SCFG_CLK = 0x85;
	REG_SCFG_EXT = 0x83000000; // NAND/SD Access
	
	bool UseNTRSplash = true;
	bool TriggerExit = false;

	scanKeys();
	int pressed = keysDown();

	if (fatInitDefault()) {

		CIniFile bootstrapini( "sd:/nds/ntr_forwarder.ini" );

		if(bootstrapini.GetInt("NTRFORWARDER","NTRCLOCK",0) == 0) { UseNTRSplash = false; }

		if(bootstrapini.GetInt("NTRFORWARDER","DISABLEANIMATION",0) == 1) {
			if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); } else { if( UseNTRSplash == true ) { REG_SCFG_CLK = 0x80; } }
		} else {
			if( pressed & KEY_B ) { if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); } } else { BootSplashInit(UseNTRSplash); }
		}

		if(bootstrapini.GetInt("NTRFORWARDER","NTRCLOCK",0) == 1) {
			REG_SCFG_CLK = 0x80;
			fifoSendValue32(FIFO_USER_04, 1);
		}

		fifoSendValue32(FIFO_USER_01, 1);
		fifoWaitValue32(FIFO_USER_03);
		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }

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
	
		inifile = fopen("sd:/_dsttfwd/YSMenu.ini","wb");
		fwrite(&IniFile,1,sizeof(IniFile),inifile);
		fclose(inifile);

		runFile("sd:/_dsttfwd/loadcard.nds");
	} else {
		if ( pressed & KEY_B ) { if(REG_SCFG_MC == 0x11) { BootSplashInit(UseNTRSplash); } } else { BootSplashInit(UseNTRSplash); }
	}
	
	while(1) { 
		swiWaitForVBlank(); 
	}
	return 0;
}

