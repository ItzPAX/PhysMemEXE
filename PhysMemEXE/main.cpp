#include "physmem_setup/physmem.hpp"

int wmain(const int argc, wchar_t** argv) 
{
	bool status;
	physmem mem = physmem_setup::setup(&status, 64);
	if (!status)
	{
		return -1;
	}
	
	system("pause");
	
	auto e = mem.attach(L"RustClient.exe");
	std::cout << std::hex << "KPROC: " << e.kprocess << std::endl << "BASE: " << e.base << std::endl << "DTB: " << e.directory_table << std::endl << "PID: " << e.pid << std::endl;
	
	auto ntdll = mem.get_module_base(L"ntdll.dll");
	std::cout << ntdll << std::endl;

	system("pause");

	auto dtb = mem.bruteforce_dtb_from_base(e.base);
	std::cout << std::hex << dtb << std::endl;
	system("pause");

	while (true)
	{
		short s = mem.read_virtual_memory<short>(e.base);
		std::cout << s << std::endl;
		Sleep(1);
	}

	system("pause");
	return 0;
}