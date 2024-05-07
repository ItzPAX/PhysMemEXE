#include "drv.h"
#include <unordered_map>

std::unordered_map<uint64_t, uint64_t> pdpt_page_table;

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

int main()
{
	wnbios_lib lib;
	lib.attach("PhysMemEXE.exe");

	// map 32gb of physical memory (not accurate representation)
	for (int i = 0; i < 32; i++)
	{
		uintptr_t huge_page_virt = lib.insert_custom_pdpte(i);
		pdpt_page_table[i] = huge_page_virt;
	}

	byte buf[0x1000];
	read_physical_memory(0x8f90, buf, 0x1000);

	for (int i = 0; i < 0x1000; i++)
	{
		std::cout << std::hex << (int)buf[i];
		if (i % 8 == 7)
			std::cout << "\n";
		else
			std::cout << "\t";
	}

	system("pause");
	return 0;
}