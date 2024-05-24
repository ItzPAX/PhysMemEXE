# PhysMemEXE

PhysMemEXE is a program that allows your usermode process to gain access to all of physical memory, without the need of an open handle to "\device\physicalmemory"

## How it works

It works by injecting a set amount (n) of pdptes with page frame numbers ranging from 0 to n and the page size bit set to 1 into our process. These pdptes allow us to access all of physical memory as if it was normal virtual memory our process can access

## Usage

```cpp
#include "physmem.hpp"

int wmain(const int argc, wchar_t** argv) 
{
	bool status;
	physmem mem = physmem_setup::setup(&status, 64);
	if (!status)
	{
		return -1;
	}
	
	auto e = mem.attach(L"explorer.exe");
	std::cout << std::hex << "KPROC: " << e.kprocess << std::endl << "BASE: " << e.base << std::endl << "DTB: " << e.directory_table << std::endl << "PID: " << e.pid << std::endl;
	
	auto dtb = mem.bruteforce_dtb_from_base(e.base);
	std::cout << std::hex << dtb << std::endl;

	while (true)
	{
		short s = mem.read_virtual_memory<short>(e.base);
		std::cout << s << std::endl;
		Sleep(1);
	}

	system("pause");
	return 0;
}
```

## Credits

[kdmapper](https://github.com/TheCruZ/kdmapper) by TheCruZ and more
