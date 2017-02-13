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
#include <nds/fifocommon.h>
#include <fat.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "nds_loader_arm9.h"

#include "inifile.h"

//---------------------------------------------------------------------------------
void dopause() {
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

int main( int argc, char **argv) {

	if (fatInitDefault()) {
		// consoleDemoInit();
		
		if( access( "sd:/_nds/YSMenu.ini", F_OK ) != -1 ) {
		
			int numr,numw;
			struct VariablesToRead {
				char var1[8];
				u16 var1nl;
				char var2[269];
				u16 var2nl;
				char var3[16];
				u16 var3nl;
				char var4[19];
			} IniFile;

			
			// printf("using YSMenu ini file on SD\n");
			FILE* inifilesd = fopen("sd:/_nds/YSMenu.ini","rb");
			FILE* inifileslot1 = fopen("fat:/TTMenu/YSMenu.ini","wb");
			numr = fread(&IniFile,1,sizeof(IniFile),inifilesd);
			numw = fwrite(&IniFile,1,numr,inifileslot1);
			fclose(inifilesd);
			fclose(inifileslot1);
			
			if( access( "sd:/_nds/twloader/settings.ini", F_OK ) != -1 ) {
				CIniFile twloaderini( "sd:/_nds/twloader/settings.ini" );
				
				if(twloaderini.GetInt("TWL-MODE","SLOT1_KEEPSD",0) == 1) {
					// printf("SD access on\n");
					twloaderini.SetInt("TWL-MODE","SLOT1_KEEPSD",0);	// Set to 0 to lock SD access from forwarders
					twloaderini.SaveIniFile( "sd:/_nds/twloader/settings.ini");
				} else {
					// printf("SD access off\n");
					// Tell arm7 to lock SCFG_EXT
					fifoSendValue32(FIFO_USER_01, 1);
					if (REG_SCFG_EXT == 0x83002000)
						REG_SCFG_EXT = 0x03002000;
					else
						REG_SCFG_EXT = 0x03000000;
				}
			} else {
				// printf("SD access off\n");
				// Tell arm7 to lock SCFG_EXT
				fifoSendValue32(FIFO_USER_01, 1);
				if (REG_SCFG_EXT == 0x83002000)
					REG_SCFG_EXT = 0x03002000;
				else
					REG_SCFG_EXT = 0x03000000;
			}
			
		} else {
			// printf("using normal YSMenu ini file\n");
			int numr,numw;
			struct VariablesToRead {
				u8 filesize[4096];
			} IniFile;

			
			FILE* inifile1 = fopen("fat:/YSMenu_norm/YSMenu.ini","rb");
			FILE* inifile2 = fopen("fat:/TTMenu/YSMenu.ini","wb");
			numr = fread(&IniFile,1,sizeof(IniFile),inifile1);
			numw = fwrite(&IniFile,1,numr,inifile2);
			fclose(inifile1);
			fclose(inifile2);
		}
		// printf("loading YSMenu on flashcard\n");
		chdir("fat:/");
		runNdsFile("fat:/YSMenu.nds", 0, NULL);
	} else {
		consoleDemoInit();
		printf("FAT init failed!\n");
	}

	while(1) {
	swiWaitForVBlank(); }
}
