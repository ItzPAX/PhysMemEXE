#include "includes.h"

uintptr_t drv_utils::convert_virtual_to_physical(HANDLE winio_handle, uintptr_t virtual_address, uintptr_t cr3)
{
	uintptr_t va = virtual_address;

	unsigned short PML4 = (unsigned short)((va >> 39) & 0x1FF);
	uintptr_t PML4E = 0;
	winio_driver::ReadPhysicalMemory(winio_handle, (cr3 + PML4 * sizeof(uintptr_t)), (uint8_t*) & PML4E, sizeof(PML4E));

	if (PML4E == 0)
		return 0;

	unsigned short DirectoryPtr = (unsigned short)((va >> 30) & 0x1FF);
	uintptr_t PDPTE = 0;
	winio_driver::ReadPhysicalMemory(winio_handle, ((PML4E & 0xFFFFFFFFFF000) + DirectoryPtr * sizeof(uintptr_t)), (uint8_t*)&PDPTE, sizeof(PDPTE));

	if (PDPTE == 0)
		return 0;

	if ((PDPTE & (1 << 7)) != 0)
		return (PDPTE & 0xFFFFFC0000000) + (va & 0x3FFFFFFF);

	unsigned short Directory = (unsigned short)((va >> 21) & 0x1FF);

	uintptr_t PDE = 0;
	winio_driver::ReadPhysicalMemory(winio_handle, ((PDPTE & 0xFFFFFFFFFF000) + Directory * sizeof(uintptr_t)), (uint8_t*)&PDE, sizeof(PDE));

	if (PDE == 0)
		return 0;

	if ((PDE & (1 << 7)) != 0)
	{
		return (PDE & 0xFFFFFFFE00000) + (va & 0x1FFFFF);
	}

	unsigned short Table = (unsigned short)((va >> 12) & 0x1FF);
	uintptr_t PTE = 0;

	winio_driver::ReadPhysicalMemory(winio_handle, ((PDE & 0xFFFFFFFFFF000) + Table * sizeof(uintptr_t)), (uint8_t*)&PTE, sizeof(PTE));

	if (PTE == 0)
		return 0;

	return (PTE & 0xFFFFFFFFFF000) + (va & 0xFFF);
}

void drv_utils::get_eprocess_offsets()
{

	NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
	OSVERSIONINFOEXW osInfo;

	*(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"),
		"RtlGetVersion");

	DWORD build = 0;

	if (NULL != RtlGetVersion)
	{
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);
		RtlGetVersion(&osInfo);
		build = osInfo.dwBuildNumber;
	}

	switch (build)
	{
	case 22631: //WIN11
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 22000: //WIN11
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 19045: // WIN10_22H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 19044: //WIN10_21H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 19043: //WIN10_21H1
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 19042: //WIN10_20H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		break;
	case 19041: //WIN10_20H1
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_VADROOT = 0x7d8;
		break;
	case 18363: //WIN10_19H2
		EP_UNIQUEPROCESSID = 0x2e8;
		EP_ACTIVEPROCESSLINK = 0x2f0;
		EP_VIRTUALSIZE = 0x340;
		EP_SECTIONBASE = 0x3c8;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x658;
		break;
	case 18362: //WIN10_19H1
		EP_UNIQUEPROCESSID = 0x2e8;
		EP_ACTIVEPROCESSLINK = 0x2f0;
		EP_VIRTUALSIZE = 0x340;
		EP_SECTIONBASE = 0x3c8;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x658;
		break;
	case 17763: //WIN10_RS5
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x628;
		break;
	case 17134: //WIN10_RS4
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x628;
		break;
	case 16299: //WIN10_RS3
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x628;
		break;
	case 15063: //WIN10_RS2
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x628;
		break;
	case 14393: //WIN10_RS1
		EP_UNIQUEPROCESSID = 0x2e8;
		EP_ACTIVEPROCESSLINK = 0x2f0;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_VADROOT = 0x620;
		break;
	default:
		exit(0);
		break;
	}
}
bool drv_utils::leak_kpointers(std::vector<uintptr_t>& pointers)
{
	const unsigned long SystemExtendedHandleInformation = 0x40;

	unsigned long buffer_length = 0;
	unsigned char tempbuffer[1024] = { 0 };
	NTSTATUS status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemExtendedHandleInformation), &tempbuffer, sizeof(tempbuffer), &buffer_length);

	buffer_length += 50 * (sizeof(SYSTEM_HANDLE_INFORMATION_EX) + sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX));

	PVOID buffer = VirtualAlloc(nullptr, buffer_length, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	RtlSecureZeroMemory(buffer, buffer_length);

	unsigned long buffer_length_correct = 0;
	status = NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemExtendedHandleInformation), buffer, buffer_length, &buffer_length_correct);

	SYSTEM_HANDLE_INFORMATION_EX* handle_information = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer);

	for (unsigned int i = 0; i < handle_information->NumberOfHandles; i++)
	{
		const unsigned int SystemUniqueReserved = 4;
		const unsigned int SystemKProcessHandleAttributes = 0x102A;

		if (handle_information->Handles[i].UniqueProcessId == SystemUniqueReserved &&
			handle_information->Handles[i].HandleAttributes == SystemKProcessHandleAttributes)
		{
			pointers.push_back(reinterpret_cast<uintptr_t>(handle_information->Handles[i].Object));
		}
	}

	VirtualFree(buffer, 0, MEM_RELEASE);
	return true;
}
uintptr_t drv_utils::leak_kprocess(HANDLE intel_handle)
{
	std::vector<uintptr_t> pointers;

	if (!leak_kpointers(pointers))
	{
		return false;
	}

	const unsigned int sanity_check = 0x3;

	for (uintptr_t pointer : pointers)
	{
		unsigned int check = 0;

		intel_driver::ReadMemory(intel_handle, pointer, &check, sizeof(unsigned int));

		if (check == sanity_check)
		{
			return pointer;
			break;
		}
	}

	return NULL;
}
EPROCESS_DATA drv_utils::get_eprocess(HANDLE intel_handle, std::wstring proc_name)
{
	get_eprocess_offsets();

	uintptr_t kprocess_initial = leak_kprocess(intel_handle);

	if (!kprocess_initial)
	{
		return EPROCESS_DATA{};
	}

	Log(L"[*] KPROCESS: " << std::hex << kprocess_initial << std::endl);

	const unsigned long limit = 400;

	uintptr_t link_start = kprocess_initial + EP_ACTIVEPROCESSLINK;
	uintptr_t flink = link_start;

	DWORD pid = utils::GetProcessId(proc_name);

	for (int a = 0; a < limit; a++)
	{
		intel_driver::ReadMemory(intel_handle, flink, &flink, sizeof(PVOID));

		uintptr_t kprocess = flink - EP_ACTIVEPROCESSLINK;
		uintptr_t virtual_size;
		intel_driver::ReadMemory(intel_handle, kprocess + EP_VIRTUALSIZE, &virtual_size, sizeof(virtual_size));

		if (virtual_size == 0)
			continue;

		uintptr_t directory_table;
		intel_driver::ReadMemory(intel_handle, kprocess + EP_DIRECTORYTABLE, &directory_table, sizeof(directory_table));

		int process_id = 0;
		intel_driver::ReadMemory(intel_handle, kprocess + EP_UNIQUEPROCESSID, &process_id, sizeof(process_id));

		if (process_id == pid)
		{
			EPROCESS_DATA data;
			data.base = kprocess;
			data.directory_table = directory_table & ~0xF;
			data.pid = process_id;

			return data;
		}
	}
	return EPROCESS_DATA{};
}
uintptr_t drv_utils::get_system_dirbase(HANDLE winio_handle)
{
	for (int i = 0; i < 10; i++)
	{
		winio_driver::winio_mem mem;
		uint8_t* lpBuffer = winio_driver::MapPhysicalMemory(winio_handle, i * 0x10000, 0x10000, mem);

		for (int uOffset = 0; uOffset < 0x10000; uOffset += 0x1000)
		{
			if (0x00000001000600E9 ^ (0xffffffffffff00ff & *reinterpret_cast<uintptr_t*>(lpBuffer + uOffset)))
				continue;
			if (0xfffff80000000000 ^ (0xfffff80000000000 & *reinterpret_cast<uintptr_t*>(lpBuffer + uOffset + 0x70)))
				continue;
			if (0xffffff0000000fff & *reinterpret_cast<uintptr_t*>(lpBuffer + uOffset + 0xa0))
				continue;

			return *reinterpret_cast<uintptr_t*>(lpBuffer + uOffset + 0xa0) & ~0xF;
		}

		winio_driver::UnmapPhysicalMemory(winio_handle, mem);
	}

	return NULL;
}