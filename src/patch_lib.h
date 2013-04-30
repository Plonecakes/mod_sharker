#pragma once

#ifndef PATCH_LIB_H_
#define PATCH_LIB_H_

#define CLIENT_TEXT_SECTION  0

static inline unsigned char* GetReferredAddressByte(unsigned char* referrer)
{
	return referrer + 1 + *referrer;
}

static inline unsigned char* GetReferredAddress(unsigned char* referrer)
{
	return referrer + sizeof(unsigned int) + *reinterpret_cast<unsigned int*>(referrer);
}

static inline unsigned int CalcReferrerData(unsigned char* referrer, unsigned char* referred)
{
	return (referred - (referrer + sizeof(unsigned int)));
}

typedef struct _MemorySegment {
	LPVOID address;
	unsigned int size;
	unsigned char *data;
} MemorySegment;

unsigned char *SearchPattern(unsigned char *start, int range, const signed short *pattern, int length);
void WritePattern(LPVOID address, const signed short *data, SIZE_T size, MemorySegment *mem = NULL);
void RestoreMemory(MemorySegment *mem);
bool GetSectionAddress(HMODULE module, unsigned int number, DWORD *address, DWORD *size);

#endif
