#include "physmem.hpp"

physmem::physmem(const wchar_t* process, uint64_t gb_to_map, HANDLE winio, HANDLE intel)
{
	EPROCESS_DATA data = drv_utils::get_eprocess(intel, process);

	Log(L"\n");
	for (int i = 0; i < gb_to_map; i++)
	{
		uintptr_t huge_page_virt = winio_driver::insert_custom_pdpte(winio, i, data.directory_table);
		pdpt_page_table[i] = huge_page_virt;
	}
	Log(L"\n");

	get_eprocess_offsets();
}

uintptr_t physmem::get_local_virt_from_phys(uintptr_t phys)
{
	uint64_t page = std::floor(phys / 0x40000000);

	uintptr_t local_virt = pdpt_page_table[page];
	uint64_t offset = phys - (page * 0x40000000);

	return local_virt + offset;
}

bool physmem::read_physical_memory(uintptr_t phys, byte* buf, size_t size)
{
	if (size > 0x40000000)
		return false;

	uintptr_t local_virt = get_local_virt_from_phys(phys);
	memcpy(buf, (void*)local_virt, size);

	return true;
}

bool physmem::write_physical_memory(uintptr_t phys, byte* buf, size_t size)
{
	if (size > 0x40000000)
		return false;

	uintptr_t local_virt = get_local_virt_from_phys(phys);
	memcpy((void*)local_virt, buf, size);

	return true;
}

uintptr_t physmem::convert_virtual_to_physical(uintptr_t virtual_address)
{
	if (!attached_dtb)
		return 0;

	uintptr_t va = virtual_address;

	unsigned short PML4 = (unsigned short)((va >> 39) & 0x1FF);
	uintptr_t PML4E = 0;
	read_physical_memory((attached_dtb + PML4 * sizeof(uintptr_t)), (byte*) & PML4E, sizeof(PML4E));

	if (PML4E == 0)
		return 0;

	unsigned short DirectoryPtr = (unsigned short)((va >> 30) & 0x1FF);
	uintptr_t PDPTE = 0;
	read_physical_memory(((PML4E & 0xFFFFFFFFFF000) + DirectoryPtr * sizeof(uintptr_t)), (byte*) & PDPTE, sizeof(PDPTE));

	if (PDPTE == 0)
		return 0;

	if ((PDPTE & (1 << 7)) != 0)
		return (PDPTE & 0xFFFFFC0000000) + (va & 0x3FFFFFFF);

	unsigned short Directory = (unsigned short)((va >> 21) & 0x1FF);

	uintptr_t PDE = 0;
	read_physical_memory(((PDPTE & 0xFFFFFFFFFF000) + Directory * sizeof(uintptr_t)), (byte*)&PDE, sizeof(PDE));

	if (PDE == 0)
		return 0;

	if ((PDE & (1 << 7)) != 0)
	{
		return (PDE & 0xFFFFFFFE00000) + (va & 0x1FFFFF);
	}

	unsigned short Table = (unsigned short)((va >> 12) & 0x1FF);
	uintptr_t PTE = 0;

	read_physical_memory(((PDE & 0xFFFFFFFFFF000) + Table * sizeof(uintptr_t)), (byte*)&PTE, sizeof(PTE));

	if (PTE == 0)
		return 0;

	return (PTE & 0xFFFFFFFFFF000) + (va & 0xFFF);
}

bool physmem::read_virtual_memory(uintptr_t virt, byte* buf, size_t size)
{
	uintptr_t phys = convert_virtual_to_physical(virt);
	if (!phys)
		return false;

	return read_physical_memory(phys, buf, size);
}

uintptr_t physmem::get_system_dirbase()
{
	for (int i = 0; i < 10; i++)
	{
		winio_driver::winio_mem mem;
		static uint8_t* lpBuffer = (uint8_t*)malloc(0x10000);
		RtlZeroMemory(lpBuffer, 0x10000);

		read_physical_memory(i * 0x10000, lpBuffer, 0x10000);

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
	}

	return NULL;
}

void physmem::get_eprocess_offsets()
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
		Log(L"[-] Unrecognized Windows version...\n");
		system("pause");
		exit(0);
		break;
	}
}

bool physmem::leak_kpointers(std::vector<uintptr_t>& pointers)
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

uintptr_t physmem::leak_kprocess()
{
	std::vector<uintptr_t> pointers;

	if (!leak_kpointers(pointers))
	{
		return NULL;
	}

	const unsigned int sanity_check = 0x3;

	for (uintptr_t pointer : pointers)
	{
		unsigned int check = 0;

		read_virtual_memory(pointer, (byte*) & check, sizeof(unsigned int));

		if (check == sanity_check)
		{
			return pointer;
			break;
		}
	}

	return NULL;
}

EPROCESS_DATA physmem::attach(std::wstring proc_name)
{
	attached_dtb = get_system_dirbase();

	if (!attached_dtb)
	{
		return EPROCESS_DATA{};
	}

	uintptr_t kprocess_initial = leak_kprocess();

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
		read_virtual_memory(flink, (byte*) & flink, sizeof(flink));

		uintptr_t kprocess = flink - EP_ACTIVEPROCESSLINK;
		uintptr_t virtual_size;
		read_virtual_memory(kprocess + EP_VIRTUALSIZE, (byte*) & virtual_size, sizeof(virtual_size));

		if (virtual_size == 0)
			continue;

		uintptr_t directory_table;
		read_virtual_memory(kprocess + EP_DIRECTORYTABLE, (byte*) & directory_table, sizeof(directory_table));

		int process_id = 0;
		read_virtual_memory(kprocess + EP_UNIQUEPROCESSID, (byte*) & process_id, sizeof(process_id));

		char name[16] = { };
		read_virtual_memory(kprocess + EP_IMAGEFILENAME, (byte*) & name, sizeof(name));

		uintptr_t base_address;
		read_virtual_memory(kprocess + EP_SECTIONBASE, (byte*) & base_address, sizeof(base_address));

		if (process_id == pid)
		{
			EPROCESS_DATA data;
			data.kprocess = kprocess;
			data.directory_table = directory_table & ~0xF;
			data.base = base_address;
			data.pid = process_id;

			return data;
		}
	}
	return EPROCESS_DATA{};
}