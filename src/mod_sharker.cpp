// mod_sharker.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "mod_sharker.h"
#include "patch_lib.h"
#include "logging.h"

static LPCTSTR const INI_FILE_NAME = L".\\mod_sharker.ini";
static LPCTSTR const INI_HEADER = L"Options";
static LPCTSTR const INI_LOADINI = L"LoadINI";
static LPCTSTR const INI_LOADFOLDER = L"LoadFolder";

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
		while(hFolder && hFolder != INVALID_HANDLE_VALUE) {
			LoadINI(file.cFileName, token);
			FindNextFile(hFolder, &file);
		}

		if((error = GetLastError()) == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
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
		// First check if the option is in the current INI.
		int option = GetPrivateProfileInt(INI_HEADER, nbptr, -1, fullname);
		// If not, retrieve it from the main INI, defaulting to 1.
		if(option == -1) option = GetPrivateProfileInt(INI_HEADER, nbptr, 1, INI_FILE_NAME);
		// Skip options section.
		// Check if patch is enabled or missing from options.
		if(wcscmp(nbptr, INI_HEADER) != 0 && option != 0) {
			// That is so. Apply patches.
			int patch_id = 1, length, rlength, section = 0, j;
			WCHAR key_name[20], condition_buffer[2000], section_buffer[30], search_buffer[2000], replace_buffer[2000];
			signed short search_binary[1000], replace_binary[1000];
			for(; true; ++patch_id) {
				swprintf(key_name, sizeof(key_name), L"Condition%i", patch_id);
				if(GetPrivateProfileString(nbptr, key_name, NULL, condition_buffer, sizeof(condition_buffer), fullname)) {
					// If there is a condition, we must assure that it is true before continuing.
					WCHAR *pvariable, *pcomparison, *pvalue, *plogic, *next_token;
					int variable, value, concat = 0;
					bool result = true, next_result;
					pvariable = wcstok_s(condition_buffer, L" ", &next_token);
					while(true) {
						if(pvariable == NULL) break;
						variable = GetPrivateProfileInt(INI_HEADER, pvariable, 1, INI_FILE_NAME);

						pcomparison = wcstok_s(NULL, L" ", &next_token);
						if(pcomparison == NULL) {
							LogMessage(L"Error when parsing condition for %s.%s at index %i: no comparison.", nbptr, key_name, pvariable + wcslen(pvariable) + 1 - condition_buffer);
							result = false;
							break;
						}

						pvalue = wcstok_s(NULL, L" ", &next_token);
						if(pvalue == NULL) {
							LogMessage(L"Error when parsing condition for %s.%s at index %i: comparison but no value.", nbptr, key_name, pcomparison + wcslen(pcomparison) + 1 - condition_buffer);
							result = false;
							break;
						}
						value = _wtoi(pvalue);

						if(pcomparison[1] == L'=' && pcomparison[2] == L'\0') {
							switch(*pcomparison) {
							case L'=': next_result = variable == value; break;
							case L'!': next_result = variable != value; break;
							case L'<': next_result = variable <= value; break;
							case L'>': next_result = variable >= value; break;
							default: goto InvalidComparison;
							}
						}
						else if(pcomparison[1] == L'\0') {
							switch(*pcomparison) {
							case L'<': next_result = variable < value; break;
							case L'>': next_result = variable > value; break;
							default: goto InvalidComparison;
							}
						}
						else {
							InvalidComparison:
							LogMessage(L"Error when parsing condition for %s.%s at index %i: invalid comparison.", nbptr, key_name, pcomparison + wcslen(pcomparison) + 1 - condition_buffer);
							result = false;
							break;
						}

						switch(concat) {
						case 0: result = result && next_result; break;
						case 1: result = result || next_result; break;
						case 2: result = (result && !next_result) || (!result && next_result); break;
						}

						plogic = wcstok_s(NULL, L" ", &next_token);
						if(plogic == NULL) break;
						if(wcscmp(plogic, L"&&") == 0 || _wcsicmp(plogic, L"and") == 0) {
							concat = 0;
						}
						else if(wcscmp(plogic, L"||") == 0 || _wcsicmp(plogic, L"or") == 0) {
							concat = 1;
						}
						else if(wcscmp(plogic, L"^^") == 0 || _wcsicmp(plogic, L"xor") == 0) {
							concat = 2;
						}

						pvariable = wcstok_s(NULL, L" ", &next_token);
					}
					if(!result) {
						continue;
					}
				}
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
					if((length = ParseHex(search_buffer, search_binary, fullname, nbptr)) < 0) {
						LogMessage(L"Error parsing search code for %s.%s at index %i.", nbptr, key_name, -length);
						continue;
					}

					// Now backup the code and apply the patch.
					unsigned char *address = SearchPattern(Sections[section]->address, Sections[section]->size, search_binary, length);
					if(address) {
						DWORD has_replacement;
						swprintf(key_name, sizeof(key_name), L"Replace%i", patch_id);
						if((has_replacement = GetPrivateProfileString(nbptr, key_name, NULL, replace_buffer, sizeof(replace_buffer), fullname))) {
							if((rlength = ParseHex(replace_buffer, replace_binary, fullname, nbptr, (unsigned int)address)) < 0) {
								LogMessage(L"Error parsing replacement code for %s.%s at index %i.", nbptr, key_name, -rlength);
								continue;
							}
							if(rlength != length) {
								LogMessage(L"Error parsing patch %s.%i, search and replace must have the same amount of bytes. Search had %i, replace had %i.", nbptr, patch_id, length, rlength);
								continue;
							}
						}

						MemorySegment *mem = (MemorySegment*)malloc(sizeof(MemorySegment));
						std::wstring key = nbptr;
						WritePattern(address, has_replacement ? replace_binary : search_binary, length, mem);
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

#define CHECK_UNEXPECTED_END if(*from == L'\0') {\
	LogMessage(L"Line terminated before closing the variable.");\
	return -(begin - orig);\
}

int ParseHex(WCHAR *from, signed short *to, WCHAR *file, WCHAR *title, unsigned int starting_address, int *from_change) {
	WCHAR *orig = from, hex[] = L"\0\0", *error_point;
	signed short *orig_to = to;
	for(; *from != L'\0'; ++from) {
		if(*from == L' ') {
			continue;
		}
		else if(from_change != NULL && (*from == L'>' || *from == ',' || *from == L'}')) {
			*from_change = from - orig;
			return to - orig_to;
		}
		else if(*from == L'$') {
			// Insert address of a named item.
			WCHAR *begin = from, name[200], *p, *dot = NULL, *colon = NULL;
			unsigned int address, index, size = 4;
			bool absolute = from[1] == L'$';

			if(absolute) ++from;
			else if(!starting_address) {
				LogMessage(L"Error during addressing, cannot use relative addresses in search pattern.");
				return -(from - orig);
			}

			for(++from, p = name; *from != L' ' && *from != L'\0'; ++from, ++p) {
				if(*p == L'.') {
					dot = p;
					*p = L'\0';
				}
				else if(*p == L':') {
					colon = p;
					*p = L'\0';
				}
				else {
					*p = *from;
				}
			}
			*p = L'\0';

			// Retrieve size. Default to 4.
			if(colon) {
				size = _wtoi(colon + 1);
			}

			// First check exact names.
			if(wcscmp(name, L"LogMessage") == 0) {
				address = (unsigned int)(&LogMessage);
			}

			// Then check internal stuff.
			else if(dot != NULL && (index = _wtoi(dot + 1))) {
				*dot = L'\0';
				// Do note that this index is of applied patches, rather than actual index.
				if(index <= Backups[name].size()) {
					address = (unsigned int)Backups[name][index - 1]->address;
				}
				else {
					LogMessage(L"Error when looking up patch address, index %i out of range.", index);
					return -(begin - orig);
				}
			}

			// TODO: Finally, search the symbols tables.

			// Make address relative.
			if(!absolute) {
				address = (signed int)address - ((signed int)starting_address + (to - orig_to) + size);
			}

			// Output the address.
			for(BYTE *pi = (BYTE *)&address; size; --size, ++to, ++pi) {
				*to = (short)(*pi);
			}
		}
		else if(*from == L'<') {
			// Format for variables:
			// <Name:Size or Default>
			// <Name:Size map {key:val, ...} or Default>
			// <Name:Size -= Offset or Default>
			// <Name:Size += Offset or Default>
			// Size is required, Name and "or Default" are not. If Name is omitted, the colon must still exist,
			// and the current mod name is assumed. Name may refer to entries in Options directly, or in relative
			// wildcards with the form ?#@ID.C.
			// # refers to the Search#.
			// ID is the index of the wildcard within that line, based at 0. If omitted, 0 is assumed.
			// C is the number of wildcards to read. If omitted, Size is used.
			// The wildcards read from Search do not have to be sequential, so be wary.
			int value, size;
			WCHAR *begin = from, _name[50], *name = _name, *p, sizebuf[10];

			// Name ends at : only.
			for(p = name, ++from; *from != L':' && *from != L'\0'; ++from, ++p) *p = *from;
			*p = L'\0';
			CHECK_UNEXPECTED_END

			// For size, accept all numbers only.
			for(p = sizebuf, ++from; *from >= L'0' && *from <= L'9'; ++from, ++p) *p = *from;
			*p = L'\0';
			size = _wtoi(sizebuf);

			// Skip trailing spaces.
			for(; *from == L' '; ++from);
			CHECK_UNEXPECTED_END

			// Fetch the value from the name. If it exists, check mapping or offset.
			if(*name == L'\0') name = title;
			value = GetPrivateProfileInt(INI_HEADER, name, -1, file);
			if(value == -1) value = GetPrivateProfileInt(INI_HEADER, name, -1, INI_FILE_NAME);

			if(value > -1) {
				if(wcsncmp(from, L"map", 3) == 0) {
					bool unfound = true;
					WCHAR number[10];
					int from_number, to_number;
					// Map form... first read until the opening brace.
					for(from += 3; *from != L'{'; ++from);
					for(++from; *from != L'}';) {
						// Collect "from" number.
						for(p = number; *from >= L'0' && *from <= L'9'; ++from, ++p) *p = *from;
						*p = L'\0';
						from_number = _wtoi(number);

						// Ignore midsection.
						for(; *from == L' ' || *from == L'\t' || *from == L':' || *from == L'='; ++from);
						if(*from == L',' || *from == L'}' || *from == L'\0') {
							LogMessage(L"Unexpected end of map entry.");
							return -(from - orig);
						}

						// Collect "to" number.
						if(*from == L'x' || *from == L'X') {
							// Hex form, sequence of bytes.
							int from_change, tmp = ParseHex(++from, to, file, title, starting_address ? starting_address + (to - orig_to) : 0, &from_change);
							if(tmp < 0) {
								return tmp - (from - orig);
							}
							else {
								from += from_change;
								if(value == from_number) {
									to += tmp;
									goto VariableEnd;
								}
							}
						}
						else if(*from < L'0' || *from > L'9') {
							LogMessage(L"Unexpected character '%c'.", *from);
							return -(from - orig);
						}
						else {
							// Decimal form.
							for(p = number; *from >= L'0' && *from <= L'9'; ++from, ++p) *p = *from;
							*p = L'\0';
							to_number = _wtoi(number);

							// Check and map number.
							if(value == from_number) {
								value = to_number;
								unfound = false;
								break;
							}
						}

						// Move to the beginning of the next entry.
						for(; *from == L' ' || *from == L','; ++from);
						if(*from != L'}' && (*from < L'0' || *from > L'9')) {
							LogMessage(L"Unexpected character '%c'.", *from);
							return -(from - orig);
						}
					}

					// Check if a mapping was found.
					if(unfound) {
						LogMessage(L"Not a valid option for %s, please see the mod's documentation.", name);
						return -(begin - orig);
					}
				}
				else if(from[1] == L'=') {
					WCHAR sign = *from, offset[20];
					// Ignore spaces.
					for(from += 2; *from == L' '; ++from);

					// Read in the offset.
					for(p = offset; *from >= L'0' && *from <= L'9'; ++from, ++p) *p = *from;

					// Apply offset.
					if(sign == L'-') value -= _wtoi(offset);
					else if(sign == L'+') value += _wtoi(offset);
					else if(sign == L'*') value *= _wtoi(offset);
					else {
						LogMessage(L"Unknown offset method %c.", sign);
						return -(begin - orig);
					}
				}
			}
			else {
				WCHAR number[20];
				// Move to "or" position.
				for(int open = 0; (*from != L'\0' && *from != L'o' && from[1] != L'r') || open; ++from) {
					if(*from == L'<') ++open;
					else if(*from == L'>') --open;
				}

				// If there is no default given...
				if(*from == L'\0') {
					LogMessage(L"You must enter a value for %s, see documentation.", name);
					return -(begin - orig);
				}

				// Ignore spaces.
				for(from += 2; *from == L' '; ++from);

				// Collect default value.
				if(*from == L'x' || *from == L'X') {
					// Hex form, sequence of bytes.
					int from_change = 0, tmp = ParseHex(++from, to, file, title, starting_address ? starting_address + (to - orig_to) : 0, &from_change);
					if(tmp < 0) {
						return tmp - (from - orig);
					}
					else {
						to += tmp;
						from += from_change;
					}

					goto VariableEnd;
				}
				else {
					// Decimal form.
					for(p = number; *from >= L'0' && *from <= L'9'; ++from, ++p) *p = *from;
					*p = L'\0';
					value = _wtoi(number);
				}
			}

			// Write out value.
			for(BYTE *pi = (BYTE *)&value; size; --size, ++to, ++pi) {
				*to = (short)(*pi);
			}

			// Move to end of variable section.
VariableEnd:
			for(int open = 0; *from != L'\0' && (*from != L'>' || open-- > 0); ++from) {
				if(*from == L'<') ++open;
			}
			CHECK_UNEXPECTED_END
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