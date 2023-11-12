/*
		cheat.cpp
		Portions copyright (C) 2008 Normmatt, www.normmatt.com, Smiths (www.emuholic.com)
		Portions copyright (C) 2008 bliss (bliss@hanirc.org)
		Copyright (C) 2009 yellow wood goblin

		This program is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		This program is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cheat.h"
#include "ndsheaderbanner.h"
#include "tonccpy.h"

#include <nds/arm9/dldi.h>
#include <algorithm>

#define ENTRIES_PER_SCREEN 20

CheatCodelist::~CheatCodelist(void) {}

inline u32 gamecode(const char *aGameCode)
{
	u32 gameCode;
	tonccpy(&gameCode, aGameCode, sizeof(gameCode));
	return gameCode;
}

#define CRCPOLY 0xedb88320
static u32 crc32(const u8* p,size_t len)
{
	u32 crc=-1;
	while(len--)
	{
		crc^=*p++;
		for(int ii=0;ii<8;++ii) crc=(crc>>1)^((crc&1)?CRCPOLY:0);
	}
	return crc;
}

bool CheatCodelist::parse(const std::string& aFileName)
{
	bool res=false;
	u32 romcrc32,gamecode;
	if(romData(aFileName,gamecode,romcrc32))
	{
		// First try ntr-forwarder folder
		FILE* dat=fopen("sd:/_nds/ntr-forwarder/usrcheat.dat","rb");
		// If that fails, try TWiLight's file
		if(!dat)
			dat=fopen("sd:/_nds/TWiLightMenu/extras/usrcheat.dat","rb");

		if(dat)
		{
			res=parseInternal(dat,gamecode,romcrc32);
			fclose(dat);
		}
	}
	return res;
}

bool CheatCodelist::searchCheatData(FILE* aDat,u32 gamecode,u32 crc32,long& aPos,size_t& aSize)
{
	aPos=0;
	aSize=0;
	const char* KHeader="R4 CheatCode";
	char header[12];
	fread(header,12,1,aDat);
	if(strncmp(KHeader,header,12)) return false;

	sDatIndex idx,nidx;

	fseek(aDat,0,SEEK_END);
	long fileSize=ftell(aDat);

	fseek(aDat,0x100,SEEK_SET);
	fread(&nidx,sizeof(nidx),1,aDat);

	bool done=false;

	while(!done)
	{
		tonccpy(&idx,&nidx,sizeof(idx));
		fread(&nidx,sizeof(nidx),1,aDat);
		if(gamecode==idx._gameCode&&crc32==idx._crc32)
		{
			aSize=((nidx._offset)?nidx._offset:fileSize)-idx._offset;
			aPos=idx._offset;
			done=true;
		}
		if(!nidx._offset) done=true;
	}
	return (aPos&&aSize);
}

bool CheatCodelist::parseInternal(FILE* aDat,u32 gamecode,u32 crc32)
{
	// dbg_printf("%x, %x\n",gamecode,crc32);

	_data.clear();

	long dataPos; size_t dataSize;
	if(!searchCheatData(aDat,gamecode,crc32,dataPos,dataSize)) return false;
	fseek(aDat,dataPos,SEEK_SET);

	// dbg_printf("record found: %d\n",dataSize);

	char* buffer=(char*)malloc(dataSize);
	if(!buffer) return false;
	fread(buffer,dataSize,1,aDat);
	char* gameTitle=buffer;

	u32* ccode=(u32*)(((u32)gameTitle+strlen(gameTitle)+4)&~3);
	u32 cheatCount=*ccode;
	cheatCount&=0x0fffffff;
	ccode+=9;

	u32 cc=0;
	while(cc<cheatCount)
	{
		u32 folderCount=1;
		char* folderName=NULL;
		char* folderNote=NULL;
		u32 flagItem=0;
		if((*ccode>>28)&1)
		{
			flagItem|=cParsedItem::EInFolder;
			if((*ccode>>24)==0x11) flagItem|=cParsedItem::EOne;
			folderCount=*ccode&0x00ffffff;
			folderName=(char*)((u32)ccode+4);
			folderNote=(char*)((u32)folderName+strlen(folderName)+1);
			_data.push_back(cParsedItem(folderName,folderNote,cParsedItem::EFolder));
			cc++;
			ccode=(u32*)(((u32)folderName+strlen(folderName)+1+strlen(folderNote)+1+3)&~3);
		}

		u32 selectValue=cParsedItem::ESelected;
		for(size_t ii=0;ii<folderCount;++ii)
		{
			char* cheatName=(char*)((u32)ccode+4);
			char* cheatNote=(char*)((u32)cheatName+strlen(cheatName)+1);
			u32* cheatData=(u32*)(((u32)cheatNote+strlen(cheatNote)+1+3)&~3);
			u32 cheatDataLen=*cheatData++;

			if(cheatDataLen)
			{
				_data.push_back(cParsedItem(cheatName,cheatNote,flagItem|((*ccode&0xff000000)?selectValue:0),dataPos+(((char*)ccode+3)-buffer)));
				if((*ccode&0xff000000)&&(flagItem&cParsedItem::EOne)) selectValue=0;
				_data.back()._cheat.resize(cheatDataLen);
				tonccpy(_data.back()._cheat.data(),cheatData,cheatDataLen*4);
			}
			cc++;
			ccode=(u32*)((u32)ccode+(((*ccode&0x00ffffff)+1)*4));
		}
	}
	free(buffer);
	generateList();
	return true;
}

void CheatCodelist::generateList(void)
{
	_indexes.clear();
	// _List.removeAllRows();

	std::vector<cParsedItem>::iterator itr=_data.begin();
	while(itr!=_data.end())
	{
		std::vector<std::string> row;
		row.push_back("");
		row.push_back((*itr)._title);
		// _List.insertRow(_List.getRowCount(),row);
		_indexes.push_back(itr-_data.begin());
		u32 flags=(*itr)._flags;
		++itr;
		if((flags&cParsedItem::EFolder)&&(flags&cParsedItem::EOpen)==0)
		{
			while(((*itr)._flags&cParsedItem::EInFolder)&&itr!=_data.end()) ++itr;
		}
	}
}

void CheatCodelist::deselectFolder(size_t anIndex)
{
	std::vector<cParsedItem>::iterator itr=_data.begin()+anIndex;
	while(--itr>=_data.begin())
	{
		if((*itr)._flags&cParsedItem::EFolder)
		{
			++itr;
			break;
		}
	}
	while(((*itr)._flags&cParsedItem::EInFolder)&&itr!=_data.end())
	{
		(*itr)._flags&=~cParsedItem::ESelected;
		++itr;
	}
}

bool CheatCodelist::romData(const std::string& aFileName,u32& aGameCode,u32& aCrc32)
{
	bool res=false;
	FILE* rom=fopen(aFileName.c_str(),"rb");
	if(rom)
	{
		u8 header[512];
		if(1==fread(header,sizeof(header),1,rom))
		{
			aCrc32=crc32(header,sizeof(header));
			aGameCode=gamecode((const char*)(header+12));
			res=true;
		}
		fclose(rom);
	}
	return res;
}

std::vector<u32> CheatCodelist::getCheats()
{
	std::vector<u32> cheats;
	for(uint i=0;i<_data.size();i++)
	{
		if(_data[i]._flags&cParsedItem::ESelected)
		{
			cheats.insert(cheats.end(),_data[i]._cheat.begin(),_data[i]._cheat.end());
		}
	}
	return cheats;
}

void CheatCodelist::drawCheatList(std::vector<CheatCodelist::cParsedItem *>& list, uint curPos, uint screenPos, uint scrollPos) {
	consoleClear();

	// Print Cheats at the top
	iprintf("\x1B[47m\t\t\t\t Cheats\n");

	// Print the list
	for(uint i=0;i<20 && i<list.size();i++) {
		if(list[screenPos+i]->_flags&cParsedItem::EFolder) {
			iprintf("\x1B[%om", screenPos+i == curPos ? 043 : 033);
			iprintf("%s\n", list[screenPos+i]->_title.substr((screenPos+i == curPos) ? scrollPos : 0, 30).c_str());
		} else {
			iprintf("\x1B[%om", (list[screenPos+i]->_flags&cParsedItem::ESelected ? 042 : 047) - (screenPos+i == curPos ? 0 : 010));
			iprintf("%s\n", list[screenPos+i]->_title.substr((screenPos+i == curPos) ? scrollPos : 0, 30).c_str());
		}
	}

	// Print bottom text
	iprintf("\x1B[47m");
	if(list[curPos]->_flags&cParsedItem::EFolder)
		iprintf("\x1B[22;0H<A> Open     ");
	else if(list[curPos]->_flags&cParsedItem::ESelected)
		iprintf("\x1B[22;0H<A> Deselect ");
	else
		iprintf("\x1B[22;0H<A> Select   ");
	iprintf("<X> Save <B> Cancel<L>Deselect all ");
	if(list[curPos]->_comment != "")
		iprintf("<Y> Info");
}

void CheatCodelist::selectCheats(std::string filename)
{
	int pressed = 0;
	int held = 0;

	consoleClear();

	iprintf("\t\t\t\t Cheats\n");
	iprintf("\n\t\t\t  Loading...\n");
	
	parse(filename);

	bool cheatsFound = true;
	// If no cheats are found
	if(_data.size() == 0) {
		cheatsFound = false;
		consoleClear();
		iprintf("\t\t\t\t Cheats\n");
		iprintf("\n\t\t  No cheats found.\n");

		do {
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while(!(pressed & KEY_B));
	}

	// Make list of all cheats not in folders and folders
	std::vector<CheatCodelist::cParsedItem *> currentList;
	for(uint i=0;i<_data.size();i++) {
		if(!(_data[i]._flags&cParsedItem::EInFolder)) {
			currentList.push_back(&_data[i]);
		}
	}

	int mainListCurPos = -1, mainListScreenPos = -1,
		cheatWnd_cursorPosition = 0, cheatWnd_screenPosition = 0,
		cheatWnd_scrollPosition = 0, cheatWnd_scrollTimer = 0,
		cheatWnd_scrollDirection = 1;

	while(cheatsFound) {
		// Scroll screen if needed
		if(cheatWnd_cursorPosition < cheatWnd_screenPosition) {
			cheatWnd_screenPosition = cheatWnd_cursorPosition;
		} else if(cheatWnd_cursorPosition > cheatWnd_screenPosition + ENTRIES_PER_SCREEN - 1) {
			cheatWnd_screenPosition = cheatWnd_cursorPosition - ENTRIES_PER_SCREEN + 1;
		}

		drawCheatList(currentList, cheatWnd_cursorPosition, cheatWnd_screenPosition, cheatWnd_scrollPosition);

		do {
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();

			if(currentList[cheatWnd_cursorPosition]->_title.length() > 30u) {
				if(cheatWnd_scrollTimer > 0) {
					cheatWnd_scrollTimer--;
				} else {
					if((cheatWnd_scrollDirection == 1 && cheatWnd_scrollPosition < (int)currentList[cheatWnd_cursorPosition]->_title.length() - 30)
					|| (cheatWnd_scrollDirection == -1 && cheatWnd_scrollPosition > 0)) {
						cheatWnd_scrollPosition += cheatWnd_scrollDirection;
						cheatWnd_scrollTimer = 6;
					} else {
						cheatWnd_scrollDirection *= -1;
						cheatWnd_scrollTimer = 120;
					}

					drawCheatList(currentList, cheatWnd_cursorPosition, cheatWnd_screenPosition, cheatWnd_scrollPosition);
				}
			}
			swiWaitForVBlank();
		} while(!pressed && !held);

		if(held & KEY_UP) {
			if(cheatWnd_cursorPosition>0) {
				cheatWnd_cursorPosition--;
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			}
		} else if(held & KEY_DOWN) {
			if(cheatWnd_cursorPosition<((int)currentList.size()-1)) {
				cheatWnd_cursorPosition++;
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			}
		} else if(held & KEY_LEFT) {
			if(cheatWnd_cursorPosition != 0) {
				cheatWnd_cursorPosition -= (cheatWnd_cursorPosition > 8 ? 8 : cheatWnd_cursorPosition);
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			}
		} else if(held & KEY_RIGHT) {
			if(cheatWnd_cursorPosition != (int)currentList.size() - 1) {
				cheatWnd_cursorPosition += (cheatWnd_cursorPosition < (int)(currentList.size()-8) ? 8 : currentList.size()-cheatWnd_cursorPosition-1);
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			}
		} else if(pressed & KEY_A) {
			if(currentList[cheatWnd_cursorPosition]->_flags&cParsedItem::EFolder) {
				uint i = std::distance(&_data[0], currentList[cheatWnd_cursorPosition]) + 1;
				currentList.clear();
				for(; !(_data[i]._flags & cParsedItem::EFolder) && i < _data.size(); i++) {
					currentList.push_back(&_data[i]);
				}
				mainListCurPos = cheatWnd_cursorPosition;
				mainListScreenPos = cheatWnd_screenPosition;
				cheatWnd_cursorPosition = 0;
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			} else {
				cParsedItem &cheat = *currentList[cheatWnd_cursorPosition];
				bool select = !(cheat._flags & cParsedItem::ESelected);
				if(cheat._flags & cParsedItem::EOne)
					deselectFolder(std::distance(&_data[0], currentList[cheatWnd_cursorPosition]));
				if(select || !(cheat._flags & cParsedItem::EOne))
					cheat._flags ^= cParsedItem::ESelected;
			}
		} else if(pressed & KEY_B) {
			if(mainListCurPos != -1) {
				currentList.clear();
				for(uint i=0;i<_data.size();i++) {
					if(!(_data[i]._flags&cParsedItem::EInFolder)) {
						currentList.push_back(&_data[i]);
					}
				}
				cheatWnd_cursorPosition = mainListCurPos;
				cheatWnd_screenPosition = mainListScreenPos;
				mainListCurPos = -1;
				cheatWnd_scrollTimer = 60;
				cheatWnd_scrollPosition = 0;
			} else {
				break;
			}
		} else if(pressed & KEY_X) {
			consoleClear();
			iprintf("\t\t\t\t Cheats\n");
			iprintf("\n\t\t\t  Saving...\n");
;
			onGenerate();
			break;
		} else if(pressed & KEY_Y) {
			if(currentList[cheatWnd_cursorPosition]->_comment != "") {
				consoleClear();
				iprintf("\t\t\t\t Cheats\n");

				std::string _topText = "";
				std::string _topTextStr(currentList[cheatWnd_cursorPosition]->_comment);
				std::vector<std::string> words;
				std::size_t pos;

				// Process comment to stay within the box
				while((pos = _topTextStr.find(' ')) != std::string::npos) {
					words.push_back(_topTextStr.substr(0, pos));
					_topTextStr = _topTextStr.substr(pos + 1);
				}
				if(_topTextStr.size())
					words.push_back(_topTextStr);
				std::string temp;
				for(auto word : words) {
					// Split word if the word is too long for a line
					int width = word.length() * 8;
					if(width > 240) {
						if(temp.length())
							_topText += temp + '\n';
						for(int i = 0; i < width/240; i++) {
							word.insert((float)((i + 1) * word.length()) / ((width/240) + 1), "\n");
						}
						_topText += word + '\n';
						continue;
					}

					width = (temp.length() + 1 + word.length()) * 8;
					if(width > 240) {
						_topText += temp + '\n';
						temp = word;
					} else {
						temp += " " + word;
					}
				}
				if(temp.size())
					_topText += temp;
				
				// Print comment
				printf("\n%s\n", _topText.c_str() + 1); // + 1 to ignore space at start

				// Print 'Back' text
				printf("\n<B> Back\n");

				do {
					scanKeys();
					pressed = keysDown();
					swiWaitForVBlank();
				} while(!(pressed & KEY_B));
			}
		} else if(pressed & KEY_L) {
			// Delect all in the actual data so it doesn't just get the folder
			for(auto itr = _data.begin(); itr != _data.end(); itr++) {
				(*itr)._flags &= ~cParsedItem::ESelected;
			}
			// Also deselect them in the current list so that it updates the display
			for(auto itr = currentList.begin(); itr != currentList.end(); itr++) {
				(*itr)->_flags &= ~cParsedItem::ESelected;
			}
		}
	}
}

static void updateDB(u8 value,u32 offset,FILE* db)
{
	u8 oldvalue;
	if(!db) return;
	if(!offset) return;
	if(fseek(db,offset,SEEK_SET)) return;
	if(fread(&oldvalue,sizeof(oldvalue),1,db)!=1) return;
	if(oldvalue!=value)
	{
		if(fseek(db,offset,SEEK_SET)) return;
		fwrite(&value,sizeof(value),1,db);
	}
}

void CheatCodelist::onGenerate(void)
{
	FILE* db;
	// Use ntr-forwarder folder if exists
	if(access("sd:/_nds/ntr-forwarder/usrcheat.dat", F_OK) == 0)
		db=fopen("sd:/_nds/ntr-forwarder/usrcheat.dat","r+b");
	else // use TWiLight's file
		db=fopen("sd:/_nds/TWiLightMenu/extras/usrcheat.dat","r+b");

	if(db)
	{
		std::vector<cParsedItem>::iterator itr=_data.begin();
		while(itr!=_data.end())
		{
			updateDB(((*itr)._flags&cParsedItem::ESelected)?1:0,(*itr)._offset,db);
			++itr;
		}
		fclose(db);
	}
}

void CheatCodelist::writeCheatsToFile(const char *path) {
	FILE *file = fopen(path, "wb");
	if(file) {
		std::vector<u32> cheats(getCheats());
		fwrite(cheats.data(),4,cheats.size(),file);
		fwrite("\0\0\0\xCF",4,1,file);
		fclose(file);
	}
}