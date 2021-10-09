/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2017
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

#include "file_browse.h"
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <nds.h>
#include "ndsheaderbanner.h"

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

using namespace std;

struct DirEntry {
	DirEntry(string name, bool isDirectory, bool secondaryDonor) : name(name), isDirectory(isDirectory), secondaryDonor(secondaryDonor) {}
	DirEntry() {}

	string name;
	bool isDirectory;
	bool secondaryDonor;
} ;

bool nameEndsWith(const string_view name, const vector<string_view> extensionList) {
	if (name.size() == 0)
		return false;

	if (extensionList.size() == 0)
		return true;

	if (name.substr(0, 2) == "._")
		return false; // Don't show macOS's index files

	for (const std::string_view &ext : extensionList) {
		if(name.length() > ext.length() && strcasecmp(name.substr(name.length() - ext.length()).data(), ext.data()) == 0)
			return true;
	}
	return false;
}

bool dirEntryPredicate (const DirEntry& lhs, const DirEntry& rhs) {

	if (!lhs.isDirectory && rhs.isDirectory) {
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory) {
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void getDirectoryContents (vector<DirEntry>& dirContents, const vector<string_view> extensionList = {}) {
	dirContents.clear();

	DIR *pdir = opendir (".");

	if (pdir == NULL) {
		iprintf ("Unable to open the directory.\n");
	} else {

		while(true) {
			dirent *pent = readdir(pdir);
			if (pent == nullptr)
				break;

			if (strcmp(pent->d_name, ".") != 0 && strcmp(pent->d_name, "_nds") != 0 && strcmp(pent->d_name, "saves") != 0) {
				if (pent->d_type == DT_DIR) {
					dirContents.emplace_back(pent->d_name, pent->d_type == DT_DIR, false);
				} else if (nameEndsWith(pent->d_name, extensionList)) {
					u32 arm7size = 0;

					FILE* file = fopen(pent->d_name, "rb");
					fseek(file, 0x3C, SEEK_SET);
					fread(&arm7size, sizeof(u32), 1, file);
					bool dsiBinariesFound = checkDsiBinaries(file);
					fclose(file);

					bool showFile = false;
					bool secondaryDonor = false;
					if (requiresDonorRom == 53) {
						showFile =
							// DSi-Enhanced
							(((arm7size==0x28F84
						  || arm7size==0x2909C
						  || arm7size==0x2914C
						  || arm7size==0x29164
						  || arm7size==0x29EE8
						  || arm7size==0x2A2EC
						  || arm7size==0x2A318
						  || arm7size==0x2AF18
						  || arm7size==0x2B184
						  || arm7size==0x2B24C
						  || arm7size==0x2C5B4) && dsiBinariesFound)
							// DSi-Exclusive/DSiWare
						  || arm7size==0x25664
						  || arm7size==0x257DC
						  || arm7size==0x25860
						  || arm7size==0x26CC8
						  || arm7size==0x26D10
						  || arm7size==0x26D50
						  || arm7size==0x26DF4);

						secondaryDonor =
						    (arm7size==0x25664
						  || arm7size==0x257DC
						  || arm7size==0x25860
						  || arm7size==0x26CC8
						  || arm7size==0x26D10
						  || arm7size==0x26D50
						  || arm7size==0x26DF4);
					} else if (requiresDonorRom == 2) { // SDK2
						showFile = (arm7size==0x26F24 || arm7size==0x26F28);
					}

					if (showFile) {
						dirContents.emplace_back(pent->d_name, pent->d_type == DT_DIR, secondaryDonor);
					}
				}
			}

		}

		closedir(pdir);
	}

	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

void showDirectoryContents (const vector<DirEntry>& dirContents, int startRow) {
	char path[PATH_MAX];


	getcwd(path, PATH_MAX);

	// Clear the screen
	iprintf ("\x1b[2J");

	// Print the path
	if (strlen(path) < SCREEN_COLS) {
		iprintf ("%s", path);
	} else {
		iprintf ("%s", path + strlen(path) - SCREEN_COLS);
	}

	// Move to 2nd row
	iprintf ("\x1b[1;0H");
	// Print line of dashes
	iprintf ("--------------------------------");

	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);
		char entryName[SCREEN_COLS + 1];

		// Set row
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);

		if (entry->isDirectory) {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 3] = '\0';
			iprintf (" [%s]", entryName);
		} else {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 1] = '\0';
			iprintf (" %s", entryName);
		}
	}
}

void browseForFile (const vector<string_view> extensionList, string* donorRomPath1, string* donorRomPath2) {
	int pressed = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	vector<DirEntry> dirContents;

	getDirectoryContents (dirContents, extensionList);
	showDirectoryContents (dirContents, screenOffset);

	while (true) {
		// Clear old cursors
		for (int i = ENTRIES_START_ROW; i < ENTRIES_PER_SCREEN + ENTRIES_START_ROW; i++) {
			iprintf ("\x1b[%d;0H ", i);
		}
		// Show cursor
		iprintf ("\x1b[%d;0H*", fileOffset - screenOffset + ENTRIES_START_ROW);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!pressed);

		if (pressed & KEY_UP) 		fileOffset -= 1;
		if (pressed & KEY_DOWN) 	fileOffset += 1;
		if (pressed & KEY_LEFT) 	fileOffset -= ENTRY_PAGE_LENGTH;
		if (pressed & KEY_RIGHT)	fileOffset += ENTRY_PAGE_LENGTH;

		if (fileOffset < 0) 	fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
		if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list

		// Scroll screen if needed
		if (fileOffset < screenOffset) 	{
			screenOffset = fileOffset;
			showDirectoryContents (dirContents, screenOffset);
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
			showDirectoryContents (dirContents, screenOffset);
		}

		if (pressed & KEY_A) {
			DirEntry* entry = &dirContents.at(fileOffset);
			if (entry->isDirectory) {
				iprintf("Entering directory\n");
				// Enter selected directory
				chdir (entry->name.c_str());
				getDirectoryContents (dirContents, extensionList);
				screenOffset = 0;
				fileOffset = 0;
				showDirectoryContents (dirContents, screenOffset);
			} else {
				// Clear the screen
				iprintf ("\x1b[2J");
				// Return the chosen file
				char path[PATH_MAX];
				getcwd(path, PATH_MAX);
				char donorPath[PATH_MAX];
				sprintf(donorPath, "%s%s", path, entry->name.c_str());

				if (entry->secondaryDonor) {
					*donorRomPath2 = donorPath;
				} else {
					*donorRomPath1 = donorPath;
				}
				return;
			}
		}

		if (pressed & KEY_B) {
			// Go up a directory
			chdir ("..");
			getDirectoryContents (dirContents, extensionList);
			screenOffset = 0;
			fileOffset = 0;
			showDirectoryContents (dirContents, screenOffset);
		}
	}
}
