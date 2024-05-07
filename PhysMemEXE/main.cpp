#include "physmem.hpp"

int main()
{
	physmem phys(32);

	byte buf[0x1000];
	phys.read_physical_memory(0x8f90, buf, 0x1000);

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