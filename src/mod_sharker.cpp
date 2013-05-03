// mod_sharker.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "mod_sharker.h"
#include "patch_lib.h"

static char* const LOG_FILE_NAME = "mod_sharker.log";
static LPCTSTR const INI_FILE_NAME = L".\\mod_sharker.ini";
static LPCTSTR const INI_HEADER = L"Options";
static LPCTSTR const INI_LOADINI = L"LoadINI";
static LPCTSTR const INI_LOADFOLDER = L"LoadFolder";

static LPCTSTR const LOG_BEGIN = L"\ufeffmod_sharker version 1.2 by Plonecakes\nLog file begin\r\n";

std::map<std::wstring, std::vector<MemorySegment*>> Backups;
std::vector<SectionInfo*> Sections;

void LoadHooks(HMODULE hModule) {
	// Find client memory bounds.
	if (!GetSectionAddresses(GetModuleHandle(NULL), &Sections))
		throw "GetSectionAddresses";

	// Begin log
	BeginLog();

	// Load patches from the main INI.
	LoadINI(INI_FILE_NAME);

	// Load referenced INIs
	WCHAR loads[400], *token, *next_token;
	GetPrivateProfileString(INI_HEADER, INI_LOADINI, NULL, loads, sizeof(loads), INI_FILE_NAME);
	token = wcstok_s(loads, L",", &next_token);
	while(token != NULL) {
		LoadINI(token);

		// Next INI.
		token = wcstok_s(NULL, L",", &next_token);
	}

	// Load referenced directories of INIs
	GetPrivateProfileString(INI_HEADER, INI_LOADFOLDER, NULL, loads, sizeof(loads), INI_FILE_NAME);
	token = wcstok_s(loads, L",", &next_token);
	//if(token == NULL && loads[0] != L'\0') token = loads;
	while(token != NULL) {
		WCHAR folder[200];
		WIN32_FIND_DATA file;
		HANDLE hFolder;
		DWORD error;
		// Log...
		LogMessage(L"Loading patches from folder %s...", token);

		// Create search string.
		swprintf(folder, sizeof(folder), L"%s\\*.ini", token);

		// Iterate through files.
		hFolder = FindFirstFile(folder, &file);
		while(hFolder) {
			LoadINI(file.cFileName, token);
			FindNextFile(hFolder, &file);
		}

		if((error = GetLastError()) == ERROR_INVALID_HANDLE) {
			LogMessage(L"Directory does not exist");
		}
		else if(error != ERROR_NO_MORE_FILES) {
			LogMessage(L"Error when iterating directory %s: code %i", token, error);
		}
		FindClose(hFolder);

		// Next folder.
		token = wcstok_s(NULL, L",", &next_token);
	}
}

void LoadINI(LPCTSTR filename, LPCTSTR folder) {
	LPTSTR fullname;
	// If this is not a full path...
	bool iscur = wcsncmp(filename, L".\\", 2) == 0;
	if(folder != NULL) {
		if(!wcsncmp(folder, L".\\", 2) && filename[1] != L':') {
			int len = wcslen(folder), len2 = wcslen(filename);
			fullname = (LPTSTR)malloc((len + len2 + 4) * sizeof(WCHAR));
			wcsncpy_s(fullname, 3, L".\\", 2);
			wcsncpy_s(fullname + 2, len + 1, folder, len);
			fullname[2 + len] = L'\\';
			wcsncpy_s(fullname + 3 + len, len2 + 1, filename, len2);
		}
		else {
			int len = wcslen(folder), len2 = wcslen(filename);
			fullname = (LPTSTR)malloc((len + wcslen(filename) + 2) * sizeof(WCHAR));
			wcsncpy_s(fullname, len + 1, folder, len);
			fullname[len] = L'\\';
			wcsncpy_s(fullname + 1 + len, len2 + 1, filename, len2);
		}
	}
	else if(!iscur && filename[1] != L':') {
		// We must add the current directory prefix.
		int len = wcslen(filename);
		fullname = (LPTSTR)malloc((len + 3) * sizeof(WCHAR));
		wcsncpy_s(fullname, 3, L".\\", 2);
		wcsncpy_s(fullname + 2, len + 1, filename, len);
	}
	else {
		int len = wcslen(filename);
		fullname = (LPTSTR)malloc((len + 1) * sizeof(WCHAR));
		wcsncpy_s(fullname, len + 1, filename, len);
	}
	LogMessage(L"Loading patches from file %s", (iscur ? filename + 2 : filename));

	// Retrieve the patches in this file (these are section names).
	WCHAR name_buffer[3000], *nbptr;
	int len = 0;
	len = GetPrivateProfileSectionNames(name_buffer, sizeof(name_buffer), fullname);
	nbptr = name_buffer;
	while((len = wcslen(nbptr)) > 0) {
		// Skip options section.
		// Check if patch is enabled or missing from options.
		if(wcscmp(nbptr, INI_HEADER) != 0 && GetPrivateProfileInt(INI_HEADER, nbptr, 1, INI_FILE_NAME) == 1) {
			// That is so. Apply patches.
			int patch_id = 1, length, rlength, section = 0, j;
			WCHAR key_name[20], section_buffer[30], search_buffer[2000], replace_buffer[2000];
			signed short search_binary[1000], replace_binary[1000];
			for(; true; ++patch_id) {
				swprintf(key_name, sizeof(key_name), L"Section%i", patch_id);
				if(GetPrivateProfileString(nbptr, key_name, NULL, section_buffer, sizeof(section_buffer), fullname)) {
					// Searching a custom section for the patch. By default it searches the code section.
					// First, check pseudonyms.
					if(wcscmp(section_buffer, L"code") == 0) section = 0;
					else if(wcscmp(section_buffer, L"data") == 0 || wcscmp(section_buffer, L"resource") == 0
						|| wcscmp(section_buffer, L"rsc") == 0 || wcscmp(section_buffer, L"resources") == 0) section = 1;
					else {
						// Check given section name.
						std::vector<SectionInfo*>::iterator i;
						for(i = Sections.begin(), j = 0; i != Sections.end(); ++i, ++j) {
							if(wcscmp(section_buffer, (*i)->name)) {
								section = j;
								break;
							}
						}

						// Last hope is if it is numerical.
						if(i == Sections.end()) {
							section = _wtoi(section_buffer);
						}
					}
				}
				swprintf(key_name, sizeof(key_name), L"Search%i", patch_id);
				if(GetPrivateProfileString(nbptr, key_name, NULL, search_buffer, sizeof(search_buffer), fullname)) {
					// There is a patch of this ID. We must parse it and the replacement then patch the client's memory.
					if((length = ParseHex(search_buffer, search_binary)) < 0) {
						LogMessage(L"Error parsing search code for %s.%s at index %i.", nbptr, key_name, -length);
						continue;
					}
					swprintf(key_name, sizeof(key_name), L"Replace%i", patch_id);
					GetPrivateProfileString(nbptr, key_name, NULL, replace_buffer, sizeof(replace_buffer), fullname);
					if((rlength = ParseHex(replace_buffer, replace_binary)) < 0) {
						LogMessage(L"Error parsing replacement code for %s.%s at index %i.", nbptr, key_name, -rlength);
						continue;
					}
					if(rlength != length) {
						LogMessage(L"Error parsing patch %s.%i, search and replace must have the same amount of bytes.", nbptr, patch_id);
						continue;
					}

					// Now backup the code and apply the patch.
					// TODO: Backup.
					unsigned char *address = SearchPattern(Sections[section]->address, Sections[section]->size, search_binary, length);
					if(address) {
						MemorySegment *mem = (MemorySegment*)malloc(sizeof(MemorySegment));
						std::wstring key = nbptr;
						WritePattern(address, replace_binary, length, mem);
						Backups[key].push_back(mem);
						LogMessage(L"Patch %s.%i SUCCESS", nbptr, patch_id);
					}
					else {
						LogMessage(L"Patch %s.%i FAILED", nbptr, patch_id);
					}
				}
				else {
					break;
				}
			}
		}
		nbptr += len + 1;
	}
	free(fullname);
}

int ParseHex(WCHAR *from, signed short *to) {
	WCHAR *orig = from, hex[] = L"\0\0", *error_point;
	signed short *orig_to = to;
	for(; *from != L'\0'; ++from) {
		if(*from == L' ') {
			continue;
		}
		else if(hex[0] == L'\0') {
			hex[0] = *from;
		}
		else if(hex[0] == L'?') {
			if(*from != L'?') {
				// Must be two sequential question marks.
				LogMessage(L"Wildcards must be two sequential question marks.");
				return -(from - orig);
			}
			// Set wildcard.
			*to = -1;
			++to;
			hex[0] = '\0';
		}
		else {
			// Read other hex digit and convert to integer.
			hex[1] = *from;
			*to = (signed short)wcstol(hex, &error_point, 16);
			int tmp =  1 - (error_point - hex);
			if(tmp >= 0) {
				// A digit was invalid.
				LogMessage(L"Invalid character in hex sequence.");
				// This will be off for the first digit if there were spaces between them.
				return -(from - orig - tmp);
			}
			++to;
			hex[0] = '\0';
		}
	}
	return to - orig_to;
}

void UnsetHooks() {
	// Unhook everything.
	for(std::map<std::wstring, std::vector<MemorySegment*>>::iterator i = Backups.begin(); i != Backups.end(); ++i) {
		for(std::vector<MemorySegment*>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
			RestoreMemory(*j);
			free(*j);
		}
		i->second.clear();
	}
	Backups.clear();

	// Close the log.
	CloseLog();
}

FILE *log_file = NULL;
void BeginLog() {
	if((log_file = fopen(LOG_FILE_NAME, "wb")) != NULL) {
		fwrite(LOG_BEGIN, 2, wcslen(LOG_BEGIN), log_file);
		fflush(log_file);
	}
}

void LogMessage(WCHAR *txt, ...) {
	if(log_file) {
		va_list args;
		va_start(args, txt);
		vfwprintf(log_file, txt, args);
		fwrite(L"\r\n", 1, 2, log_file);
		va_end(args);
		fflush(log_file);
	}
}

void CloseLog() {
	if(log_file) {
		fwrite(L"Log file closed\r\n", 2, 18, log_file);
		fclose(log_file);
	}
}