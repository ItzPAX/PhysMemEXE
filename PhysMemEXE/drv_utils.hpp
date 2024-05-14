#pragma once
#include "includes.h"

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
	PVOID Object;
	ULONG UniqueProcessId;
	ULONG HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG HandleAttributes;
	ULONG Reserved;
};
struct SYSTEM_HANDLE_INFORMATION_EX
{
	ULONG NumberOfHandles;
	ULONG Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
};
struct EPROCESS_DATA
{
	uintptr_t kprocess, base, directory_table;
	DWORD pid;
};


namespace drv_utils
{
	static uintptr_t EP_DIRECTORYTABLE = 0x028;
	static uintptr_t EP_UNIQUEPROCESSID = 0;
	static uintptr_t EP_ACTIVEPROCESSLINK = 0;
	static uintptr_t EP_VIRTUALSIZE = 0;
	static uintptr_t EP_SECTIONBASE = 0;
	static uintptr_t EP_IMAGEFILENAME = 0;
	static uintptr_t EP_VADROOT = 0;

	// easy to use interface for drivers
	uintptr_t convert_virtual_to_physical(HANDLE winio_handle, uintptr_t virtual_address, uintptr_t cr3);

	void get_eprocess_offsets();
	bool leak_kpointers(std::vector<uintptr_t>& pointers);
	uintptr_t leak_kprocess(HANDLE intel_handle);
	EPROCESS_DATA get_eprocess(HANDLE intel_handle, std::wstring proc_name);
	uintptr_t get_system_dirbase(HANDLE winio_handle);
}