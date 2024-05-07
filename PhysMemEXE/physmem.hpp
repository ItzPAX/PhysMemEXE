#include "drv.h"
#include <iostream>
#include <unordered_map>

class physmem
{
public:
	std::unordered_map<uint64_t, uint64_t> pdpt_page_table;
	wnbios_lib lib;

public:
	physmem(uint64_t gb_to_map)
	{
		lib.attach("PhysMemEXE.exe");

		for (int i = 0; i < gb_to_map; i++)
		{
			uintptr_t huge_page_virt = lib.insert_custom_pdpte(i);
			pdpt_page_table[i] = huge_page_virt;
		}

		lib.~wnbios_lib();
	}

	uintptr_t get_local_virt_from_phys(uintptr_t phys)
	{
		uint64_t page = (int)(phys / 0x40000000);
		uintptr_t local_virt = pdpt_page_table[page];
		uint64_t offset = phys - (page * 0x40000000);
		return local_virt + offset;
	}

	bool read_physical_memory(uintptr_t phys, byte* buf, size_t size)
	{
		if (size > 0x40000000)
			return false;

		uintptr_t local_virt = get_local_virt_from_phys(phys);
		memcpy(buf, (void*)local_virt, 0x1000);

		return true;
	}
};