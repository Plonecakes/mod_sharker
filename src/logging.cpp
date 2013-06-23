#include "stdafx.h"
#include "logging.h"

static char* const LOG_FILE_NAME = "mod_sharker.log";
static const wchar_t LOG_BEGIN[] = L"\ufeffmod_sharker version 2.3 by Plonecakes\nLog file begin\r\n";

FILE *log_file = NULL;
void BeginLog() {
	if((log_file = fopen(LOG_FILE_NAME, "wb")) != NULL) {
		fwrite(LOG_BEGIN, 2, sizeof(LOG_BEGIN) / 2 - 1, log_file);
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