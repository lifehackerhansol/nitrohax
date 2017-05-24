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

int main( int argc, char **argv) {

	consoleDemoInit();
	
	if (fatInitDefault()){
		printf("r4idsn.nds started.\n");
	
		if( access( "sd:/_nds/lastsave.ini", F_OK ) != -1 ) {
			FILE * inifilesd, * inifileslot1;
			int numr,numw;
			struct VariablesToRead {
				char var1[11];
				u16 var1nl;
				char var2[270];
				u16 var2nl;
				char var3[14];
				u16 var3nl;
			} IniFile;

		
			inifilesd = fopen("sd:/_nds/lastsave.ini","rb");
			printf("SD lastsave.ini opened.\n");
			inifileslot1 = fopen("fat:/_wfwd/lastsave.ini","wb");
			printf("FAT lastsave.ini opened.\n");
			numr = fread(&IniFile,1,sizeof(IniFile),inifilesd);
			printf("SD lastsave.ini read.\n");
			numw = fwrite(&IniFile,1,numr,inifileslot1);
			printf("Written to FAT lastsave.ini.\n");
			fclose(inifilesd);
			printf("SD lastsave.ini closed.\n");
			fclose(inifileslot1);
			printf("FAT lastsave.ini closed.\n");
			
			if( access( "sd:/_nds/twloader/settings.ini", F_OK ) != -1 ) {
				CIniFile twloaderini( "sd:/_nds/twloader/settings.ini" );
				
				if(twloaderini.GetInt("TWL-MODE","SLOT1_KEEPSD",0) == 1) {
					printf("SD access on\n");
					twloaderini.SetInt("TWL-MODE","SLOT1_KEEPSD",0);	// Set to 0 to lock SD access from forwarders
					twloaderini.SaveIniFile( "sd:/_nds/twloader/settings.ini");
				} else {
					printf("SD access off\n");
					// Tell arm7 to lock SCFG_EXT
					fifoSendValue32(FIFO_USER_01, 1);
					if (REG_SCFG_EXT == 0x83002000)
						REG_SCFG_EXT = 0x03002000;
					else
						REG_SCFG_EXT = 0x03000000;
				}
			} else {
				printf("SD access off\n");
				// Tell arm7 to lock SCFG_EXT
				fifoSendValue32(FIFO_USER_01, 1);
				if (REG_SCFG_EXT == 0x83002000)
					REG_SCFG_EXT = 0x03002000;
				else
					REG_SCFG_EXT = 0x03000000;
			}
			chdir("fat:/");
			printf("Starting Wfwd.nds\n");
			runNdsFile("fat:/Wfwd.nds", 0, NULL);

		} else {
			chdir("fat:/");
			printf("Starting _DSMENU.DAT\n");
			runNdsFile("fat:/_DSMENU.DAT", 0, NULL);
		}
	} else {
		consoleDemoInit();
		printf("FAT init failed!\n");
	}

	while(1) {
	swiWaitForVBlank(); }
}
