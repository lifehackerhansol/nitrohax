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

#include <stdio.h>

#include "nds_loader_arm9.h"

int main( int argc, char **argv) {

	//consoleDemoInit();
	if (fatInitDefault()) {
	
//if(*SCFG_EXT>0) {
	    // DSI mode with sd card access
		//iprintf ("REG_SCFG_EXT : %x\n",REG_SCFG_EXT);
		
		// mount the dsi sd card as an additionnal card
		fatMountSimple("dsisd",&__io_dsisd);
		//if (!fatMountSimple("dsisd",&__io_dsisd))
		//{
		//	FILE * inifile1, * inifile2;
		//	int numr,numw;
		//	struct Norm {
		//		char buffer[20000];
		//	} IniFileNorm;

	
		//	inifile1 = fopen("fat:/YSMenu_norm/YSMenu.ini","rb");
		//	inifile2 = fopen("fat:/TTMenu/YSMenu.ini","wb");
		//	numr = fread(&IniFileNorm,1,sizeof(IniFileNorm),inifile1);
		//	numw = fwrite(&IniFileNorm,1,numr,inifile2);
		//	fclose(inifile1);
		//	fclose(inifile2);
	
		//	runNdsFile("fat:/YSMenu.nds", 0, NULL);
		//}
		
		//printf ("dsisd mounted\n");
	//}
	

	FILE * inifilesd, * inifileslot1;
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

	
	inifilesd = fopen("dsisd:/_nds/YSMenu.ini","rb");
	inifileslot1 = fopen("fat:/TTMenu/YSMenu.ini","wb");
	numr = fread(&IniFile,1,sizeof(IniFile),inifilesd);
	numw = fwrite(&IniFile,1,numr,inifileslot1);
	fclose(inifilesd);
	fclose(inifileslot1);
	
	chdir("fat:/");
	
	// Tell Arm7 to lock SCFG_EXT
	fifoSendValue32(FIFO_USER_01, 1);
	REG_SCFG_EXT = 0x03000000;


		runNdsFile("fat:/YSMenu.nds", 0, NULL);
	} else {
		consoleDemoInit();
		printf("FAT init failed!\n");
	}

	while(1) {
	swiWaitForVBlank(); }
}
