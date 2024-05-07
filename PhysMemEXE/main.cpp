#ifndef KDLIBMODE

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>

#include "physmem.hpp"

HANDLE iqvw64e_device_handle;
HANDLE winio_device_handle;


int wmain(const int argc, wchar_t** argv) 
{
	iqvw64e_device_handle = intel_driver::Load();
	if (iqvw64e_device_handle == INVALID_HANDLE_VALUE)
	{
		system("pause");
		return -1;
	}
	
	winio_device_handle = winio_driver::Load(iqvw64e_device_handle);
	if (winio_device_handle == INVALID_HANDLE_VALUE)
	{
		system("pause");
		return -1;
	}

	physmem physmem(L"PhysMemEXE.exe", 32, winio_device_handle, iqvw64e_device_handle);

	if (!intel_driver::Unload(iqvw64e_device_handle)) {
		Log(L"[-] Warning failed to fully unload vulnerable driver " << std::endl);
	}
	if (!winio_driver::Unload(winio_device_handle)) {
		Log(L"[-] Warning failed to fully unload vulnerable driver " << std::endl);
	}


	byte buf[0x1000];
	
	while (true)
	{
		physmem.read_physical_memory(0x1ad000, buf, 0x1000);
		for (int i = 0; i < 0x1000; i++)
			std::cout << std::hex << (int)buf[i] << std::endl;
	
		Sleep(1);
	}

	system("pause");
}

#endif