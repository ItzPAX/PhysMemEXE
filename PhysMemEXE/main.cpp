#ifndef KDLIBMODE

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>

#include "overlay/overlay.hpp"

int wmain(const int argc, wchar_t** argv) 
{
	bool status;
	physmem mem = physmem_setup::setup(&status);
	if (!status)
	{
		return -1;
	}
	
	system("pause");
	
	auto e = mem.attach(L"Clicker Heroes.exe");
	std::cout << std::hex << "KPROC: " << e.kprocess << std::endl << "BASE: " << e.base << std::endl << "DTB: " << e.directory_table << std::endl << "PID: " << e.pid << std::endl;
	
	system("pause");

	//auto dtb = mem.bruteforce_dtb_from_base(e.base);
	//std::cout << std::hex << dtb << std::endl;
	//system("pause");
	//
	//while (true)
	//{
	//	short s = mem.read_virtual_memory<short>(e.base);
	//	std::cout << s << std::endl;
	//	Sleep(1);
	//}

	overlay::draw(mem);

	system("pause");
	return 0;
}

#endif