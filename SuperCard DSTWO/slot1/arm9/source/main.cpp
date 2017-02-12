/*---------------------------------------------------------------------------------

	DSTwo Auto-Boot ROM path setter
	by Robz8

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>
#include <nds/disc_io.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"

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
int main(void) {
//---------------------------------------------------------------------------------

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	//int pathLen;
	//std::string filename;

	//iconTitleInit();

	// Subscreen as a console
	videoSetModeSub(MODE_0_2D);
	vramSetBankH(VRAM_H_SUB_BG);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	
	
	unsigned int * SCFG_EXT=	(unsigned int*)0x4004008;
	
	//unsigned int * SCFG_ROM=	(unsigned int*)0x4004000;		
	//unsigned int * SCFG_CLK=	(unsigned int*)0x4004004;
	//unsigned int * SCFG_MC=		(unsigned int*)0x4004010;
	//unsigned int * SCFG_ROM_ARM7_COPY=	(unsigned int*)0x2370000;
	//unsigned int * SCFG_EXT_ARM7_COPY=  (unsigned int*)0x2370008;
	//
	//if(*SCFG_EXT>0) {
	//	iprintf ("DSI SCFG_ROM ARM9 : %x\n\n",*SCFG_ROM);			
	//	iprintf ("DSI SCFG_CLK ARM9 : %x\n\n",*SCFG_CLK);
	//	iprintf ("DSI SCFG_EXT ARM9 : %x\n\n",*SCFG_EXT);			
	//	iprintf ("DSI SCFG_MC ARM9 : %x\n\n",*SCFG_MC);
	//	
	//	iprintf ("DSI SCFG_ROM ARM7 : %x\n\n",*SCFG_ROM_ARM7_COPY);
	//	iprintf ("DSI SCFG_EXT ARM7 : %x\n\n",*SCFG_EXT_ARM7_COPY);
	//	
	//	//test RAM
	//	unsigned int * TEST32RAM=	(unsigned int*)0x20004000;		
	//	*TEST32RAM = 0x55;
	//	iprintf ("FCRAM ACCESS : %x\n\n",*TEST32RAM);
	//}
	

	//if (!fatInitDefault()) {
		//iprintf ("fatinitDefault failed!\n");		
			
		//stop();
	//}
	
	if(*SCFG_EXT>0) {
	    // DSI mode with sd card access
		iprintf ("SCFG_EXT : %x\n",*SCFG_EXT);
		
		// mount the dsi sd card as an additionnal card
		fatMountSimple("dsisd",&__io_dsisd);
		
		printf ("dsisd mounted");
	}
			
	
	//doPause();

	FILE* inifilesd;
	struct VariablesToRead {
	char var1[10];
	u16 var1nl;
	char var2[273];
	u16 var2nl;
	char var3[9];
	u16 var3nl;
	} IniFileSD;
	
	FILE* inifileslot1;
	struct VariablesToSave {
	char var1[10];
	u16 var1nl;
	char var2[273];
	u16 var2nl;
	char var3[9];
	u16 var3nl;
	} IniFileSlot1;

	
	inifilesd = fopen("dsisd:/_dstwofwd/autoboot.ini","rb");
	inifileslot1 = fopen("fat:/_dstwo/autoboot.ini","wb");
	fwrite(&IniFileSD,1,sizeof(IniFileSlot1),inifilesd);
	fclose(inifilesd);
	fclose(inifileslot1);
	
	// Sets SCFG_EXT to zero. Arm7 sets bit31 to 1. This results in SCFG getting locked out again.
	// So this will help fix compatibility issues with games that have issue with the new patch.
	*SCFG_EXT=0x0;
	
	int err = runNdsFile ("fat:/_dstwo/autoboot.nds",0,0);
	iprintf ("Failed to start autoboot.nds", err);
 
	while(1) {
		
	
	}

	return 0;
}
