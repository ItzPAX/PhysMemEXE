#include "includes.h"
#include <iostream>
#include <unordered_map>

class physmem
{
private:
	uintptr_t EP_DIRECTORYTABLE = 0x028;
	uintptr_t EP_UNIQUEPROCESSID = 0;
	uintptr_t EP_ACTIVEPROCESSLINK = 0;
	uintptr_t EP_VIRTUALSIZE = 0;
	uintptr_t EP_SECTIONBASE = 0;
	uintptr_t EP_IMAGEFILENAME = 0;
	uintptr_t EP_VADROOT = 0;

	uintptr_t attached_dtb = 0;

public:
	std::unordered_map<uint64_t, uint64_t> pdpt_page_table;

public:
	physmem(const wchar_t* process, uint64_t gb_to_map, HANDLE winio, HANDLE intel);

public:
	uintptr_t get_local_virt_from_phys(uintptr_t phys);
	bool read_physical_memory(uintptr_t phys, byte* buf, size_t size);
	bool write_physical_memory(uintptr_t phys, byte* buf, size_t size);

	uintptr_t convert_virtual_to_physical(uintptr_t virtual_address);

	bool read_virtual_memory(uintptr_t virt, byte* buf, size_t size);

	uintptr_t get_system_dirbase();

	void get_eprocess_offsets();
	bool leak_kpointers(std::vector<uintptr_t>& pointers);
	uintptr_t leak_kprocess();
	EPROCESS_DATA attach(std::wstring proc_name);
};