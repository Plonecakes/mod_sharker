#include "stdafx.h"
#include "patch_lib.h"

unsigned char *SearchPattern(unsigned char *start, int range, const signed short *pattern, int length)
{
	unsigned char *end = start + range - length, *pstart;
	const signed short *orig = pattern, *pattern_end = pattern + length;

	for (unsigned char *p = start; p <= end; ++p) {
		pstart = p;
		// -1 is the wildcard.
		for(; pattern < pattern_end && (*pattern == -1 || *p == (unsigned char)*pattern); ++p, ++pattern);
		if(pattern == pattern_end) return pstart;
		p = pstart;
		pattern = orig;
	}

	return NULL;
}

void WritePattern(LPVOID address, const signed short *data, SIZE_T size, MemorySegment *mem)
{
	DWORD oldProtect;

	// Allowing reading from and writing to this memory space.
	VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect);

	// Backup memory.
	if(mem != NULL) {
		mem->address = address;
		mem->size = size;
		mem->data = (unsigned char*)malloc(size * sizeof(unsigned char));
		memcpy(mem->data, address, size);
	}

	unsigned char *a, *end = (unsigned char*)address + size;
	for(a = (unsigned char*)address; a < end; ++a, ++data) {
		// Ignore -1s.
		if(*data != -1) *a = (unsigned char)*data;
	}

	// Restore permissions to this memory space.
	VirtualProtect(address, size, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), address, size);
}

void RestoreMemory(MemorySegment *mem) {
	DWORD oldProtect;

	// Allowing reading from and writing to this memory space.
	VirtualProtect(mem->address, mem->size, PAGE_EXECUTE_READWRITE, &oldProtect);

	// Restore.
	memcpy(mem->address, mem->data, mem->size);

	// Restore permissions to this memory space.
	VirtualProtect(mem->address, mem->size, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), mem->address, mem->size);

	// Free MemorySegment.
	free(mem->data);
}

bool GetSectionAddress(HMODULE module, unsigned int number, DWORD *address, DWORD *size)
{
	DWORD base = reinterpret_cast<DWORD>(module);

	IMAGE_DOS_HEADER *dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	IMAGE_NT_HEADERS *nt_header = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos_header->e_lfanew);
	if (number >= nt_header->FileHeader.NumberOfSections)
		return false;

	IMAGE_SECTION_HEADER *section = reinterpret_cast<IMAGE_SECTION_HEADER*>(base + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	*address = base + section[number].VirtualAddress;
	if (size)
		*size = section[number].Misc.VirtualSize;

	return true;
}
