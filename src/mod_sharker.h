#pragma once

#include "stdafx.h"

void LoadHooks(HMODULE hModule);
void LoadINI(LPCTSTR filename, LPCTSTR folder = NULL);
int ParseHex(WCHAR *from, signed short *to);
void UnsetHooks();
void BeginLog();
void LogMessage(WCHAR *txt, ...);
void CloseLog();
