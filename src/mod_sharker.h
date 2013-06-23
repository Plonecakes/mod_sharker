#pragma once

#include "stdafx.h"

void LoadHooks(HMODULE hModule);
void LoadINI(LPCTSTR filename, LPCTSTR folder = NULL);
int ParseHex(WCHAR *from, signed short *to, WCHAR *file, WCHAR *title, unsigned int start_address = 0, int *from_change = NULL);
void UnsetHooks();
