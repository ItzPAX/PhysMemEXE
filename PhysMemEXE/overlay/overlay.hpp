#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <tlhelp32.h>

#define SystemHandleInformation 16 
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)

namespace overlay
{
	namespace communication
	{
		std::wstring mapped_filename;

		typedef struct _Header
		{
			UINT Magic;
			UINT FrameCount;
			UINT NoClue;
			UINT Width;
			UINT Height;
			BYTE Buffer[1];
		} Header;

		typedef struct _ConnectedProcessInfo
		{
			UINT pte_start;
			UINT ProcessId;
			HANDLE File;

			Header* MappedAddress_External;
			DWORD MappedAddress_PID;
			HANDLE ProcessHandle;
		} ConnectedProcessInfo;

		typedef struct _SYSTEM_HANDLE {
			ULONG ProcessId;
			BYTE ObjectTypeNumber;
			BYTE Flags;
			USHORT Handle;
			PVOID Object;
			ACCESS_MASK GrantedAccess;
		} SYSTEM_HANDLE;

		typedef struct _SYSTEM_HANDLE_INFORMATION {
			ULONG HandleCount;
			SYSTEM_HANDLE Handles[1];
		} SYSTEM_HANDLE_INFORMATION;

		typedef enum _OBJECT_INFORMATION_CLASS {
			ObjectBasicInformation,
			ObjectNameInformation,
			ObjectTypeInformation,
			ObjectAllTypesInformation,
			ObjectHandleInformation
		} OBJECT_INFORMATION_CLASS;

		typedef struct _UNICODE_STRING {
			USHORT Length;
			USHORT MaximumLength;
			PWSTR Buffer;
		} UNICODE_STRING;

		typedef struct _OBJECT_NAME_INFORMATION {
			UNICODE_STRING Name;
		} OBJECT_NAME_INFORMATION, * POBJECT_NAME_INFORMATION;

		typedef NTSTATUS(WINAPI* NTQUERYOBJECT)(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG);
		using f_RtlAdjustPrivilege = NTSTATUS(NTAPI*)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
		using f_NtQuerySystemInformation = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);


		BOOL IsFileMappingHandle(HANDLE hProcess, HANDLE hHandle, const std::wstring& mappingName) {
			// Load NtQueryObject dynamically
			HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
			if (!hNtDll) return FALSE;

			NTQUERYOBJECT NtQueryObject = (NTQUERYOBJECT)GetProcAddress(hNtDll, "NtQueryObject");
			if (!NtQueryObject) return FALSE;

			ULONG returnLength = 0;
			OBJECT_NAME_INFORMATION objectNameInfo;
			NTSTATUS status = NtQueryObject(hHandle, ObjectNameInformation, &objectNameInfo, sizeof(objectNameInfo), &returnLength);

			if (status == STATUS_INFO_LENGTH_MISMATCH) {
				std::vector<BYTE> buffer(returnLength);
				status = NtQueryObject(hHandle, ObjectNameInformation, buffer.data(), returnLength, &returnLength);
				if (!NT_SUCCESS(status)) return FALSE;

				POBJECT_NAME_INFORMATION pObjectNameInfo = (POBJECT_NAME_INFORMATION)buffer.data();
				std::wstring objectName(pObjectNameInfo->Name.Buffer, pObjectNameInfo->Name.Length / sizeof(WCHAR));

				if (wcsstr(objectName.c_str(), mappingName.c_str()) != 0)
				{
					return TRUE;
				}
			}

			return FALSE;
		}

		BOOL GetMappedAddress(HANDLE hProcess, HANDLE hFileMapping, void** mappedAddress) 
		{
			MEMORY_BASIC_INFORMATION mbi;
			PVOID address = nullptr;
			SIZE_T returnLength;

			while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi)) == sizeof(mbi)) 
			{
				if (mbi.Type == MEM_MAPPED) 
				{
					byte pattern[] = { '\x14','\x00','\x20','\x03' };
					byte buffer[4];

					ReadProcessMemory(hProcess, mbi.BaseAddress, buffer, sizeof(buffer), NULL);
					if (!memcmp(pattern, buffer, sizeof(buffer)) && mbi.RegionSize == 0x3201000) // small pattern and page is 50MB
					{
						*mappedAddress = mbi.BaseAddress;
						return TRUE;
					}
				}
				address = (PBYTE)address + mbi.RegionSize;
			}
			return FALSE;
		}

		inline void find_mapping_in_process(ConnectedProcessInfo& processInfo)
		{
			HMODULE ntdll = GetModuleHandleA("ntdll");
			f_RtlAdjustPrivilege RtlAdjustPrivilege = (f_RtlAdjustPrivilege)GetProcAddress(ntdll, "RtlAdjustPrivilege");
			boolean old_priv;
			RtlAdjustPrivilege(20, TRUE, FALSE, &old_priv);

			// Take a snapshot of all processes in the system
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hSnapshot == INVALID_HANDLE_VALUE) {
				std::cerr << "Failed to take process snapshot" << std::endl;
				return;
			}

			PROCESSENTRY32 pe;
			pe.dwSize = sizeof(PROCESSENTRY32);

			if (!Process32First(hSnapshot, &pe)) {
				std::cerr << "Failed to get the first process" << std::endl;
				CloseHandle(hSnapshot);
				return;
			}

			do {
				// Open the process
				HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
				if (hProcess == nullptr) {
					continue;
				}
				
				ULONG handle_info_size = 0x10000;
				SYSTEM_HANDLE_INFORMATION* handle_info = (SYSTEM_HANDLE_INFORMATION*)malloc(handle_info_size);

				f_NtQuerySystemInformation NtQuerySystemInformation = (f_NtQuerySystemInformation)GetProcAddress(ntdll, "NtQuerySystemInformation");

				while (NtQuerySystemInformation(SystemHandleInformation, handle_info, handle_info_size, NULL) == STATUS_INFO_LENGTH_MISMATCH)
				{
					handle_info_size *= 2;
					handle_info = (SYSTEM_HANDLE_INFORMATION*)realloc(handle_info, handle_info_size);
				}

				for (ULONG i = 0; i < handle_info->HandleCount; ++i) {
					SYSTEM_HANDLE& handle = handle_info->Handles[i];
					if (handle.ProcessId != pe.th32ProcessID) {
						continue;
					}

					HANDLE hHandle = (HANDLE)handle.Handle;
					if (IsFileMappingHandle(hProcess, hHandle, mapped_filename)) {
						void* mappedAddress = nullptr;
						if (GetMappedAddress(hProcess, hHandle, &mappedAddress)) {
							std::cout << "[*] Found DiscordFile in Process: " << pe.th32ProcessID << "\n[*] Mapped to address: " << mappedAddress << std::endl;
							processInfo.MappedAddress_External = (Header*)mappedAddress;
							processInfo.MappedAddress_PID = pe.th32ProcessID;
							CloseHandle(hProcess);
							CloseHandle(hSnapshot);
							processInfo.ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processInfo.MappedAddress_PID);
							return;
						}
					}
				}

				CloseHandle(hProcess);
			} while (Process32Next(hSnapshot, &pe));

			CloseHandle(hSnapshot);
			return;
		}

		inline bool ConnectToProcess(ConnectedProcessInfo& processInfo)
		{
			find_mapping_in_process(processInfo);

			return processInfo.MappedAddress_External;
		}

		inline void SendFrame(ConnectedProcessInfo& processInfo, UINT width, UINT height, void* frame, UINT size)
		{
			// frame is in B8G8R8A8 format
			// size can be nearly anything since it will get resized
			// for the screen appropriately, although the maximum size is
			// game window width * height * 4 (BGRA)

			WriteProcessMemory(processInfo.ProcessHandle, processInfo.MappedAddress_External + offsetof(Header, Width), &width, sizeof(width), NULL);
			WriteProcessMemory(processInfo.ProcessHandle, processInfo.MappedAddress_External + offsetof(Header, Height), &height, sizeof(height), NULL);

			WriteProcessMemory(processInfo.ProcessHandle, processInfo.MappedAddress_External + offsetof(Header, Buffer), frame, size, NULL);

			UINT old_fc;
			ReadProcessMemory(processInfo.ProcessHandle, processInfo.MappedAddress_External + offsetof(Header, FrameCount), &old_fc, sizeof(old_fc), NULL);
			old_fc += 1;
			WriteProcessMemory(processInfo.ProcessHandle, processInfo.MappedAddress_External + offsetof(Header, FrameCount), &old_fc, sizeof(old_fc), NULL);
		}
	}

	namespace drawing
	{
		typedef struct _Frame
		{
			UINT Width;
			UINT Height;
			UINT Size;
			void* Buffer;
		} Frame;

		typedef struct _Pixel
		{
			BYTE B, G, R, A;
		} Pixel;

		inline Frame CreateFrame(UINT width, UINT height)
		{
			Frame output;
			output.Width = width;
			output.Height = height;
			output.Size = width * height * 4;

			output.Buffer = malloc(output.Size);
			memset(output.Buffer, 0, output.Size);

			return output;
		}

		inline void CleanFrame(Frame& frame)
		{
			memset(frame.Buffer, 0, frame.Size);
		}

		inline void SetPixel(Frame& frame, UINT x, UINT y, Pixel color)
		{
			if (x < frame.Width && y < frame.Height)
			{
				Pixel* buf = static_cast<Pixel*>(frame.Buffer);
				buf[y * frame.Width + x] = color;
			}
		}

		inline void DrawLine(Frame& frame, UINT x1, UINT y1, UINT x2, UINT y2, Pixel color)
		{
			int dx = abs(static_cast<int>(x2 - x1)), sx = x1 < x2 ? 1 : -1;
			int dy = -abs(static_cast<int>(y2 - y1)), sy = y1 < y2 ? 1 : -1;
			int err = dx + dy, e2;

			while (true)
			{
				SetPixel(frame, x1, y1, color);
				if (x1 == x2 && y1 == y2)
					break;
				e2 = 2 * err;
				if (e2 >= dy) { err += dy; x1 += sx; }
				if (e2 <= dx) { err += dx; y1 += sy; }
			}
		}

		inline void DrawRectangle(Frame& frame, UINT x1, UINT y1, UINT width, UINT height, Pixel color)
		{
			for (UINT x = x1; x < x1 + width; x++)
			{
				SetPixel(frame, x, y1, color);
				SetPixel(frame, x, y1 + height - 1, color);
			}

			for (UINT y = y1; y < y1 + height; y++)
			{
				SetPixel(frame, x1, y, color);
				SetPixel(frame, x1 + width - 1, y, color);
			}
		}

		inline void DrawCircle(Frame& frame, UINT x0, UINT y0, UINT radius, Pixel color)
		{
			int x = radius;
			int y = 0;
			int radiusError = 1 - x;

			while (x >= y)
			{
				SetPixel(frame, x0 + x, y0 + y, color);
				SetPixel(frame, x0 - x, y0 + y, color);
				SetPixel(frame, x0 - x, y0 - y, color);
				SetPixel(frame, x0 + x, y0 - y, color);
				SetPixel(frame, x0 + y, y0 + x, color);
				SetPixel(frame, x0 - y, y0 + x, color);
				SetPixel(frame, x0 - y, y0 - x, color);
				SetPixel(frame, x0 + y, y0 - x, color);

				y++;
				if (radiusError < 0)
				{
					radiusError += 2 * y + 1;
				}
				else
				{
					x--;
					radiusError += 2 * (y - x + 1);
				}
			}
		}
	}

	communication::ConnectedProcessInfo processInfo = { 0 };

	void Error()
	{
		getchar();
		exit(-1);
	}

	int draw()
	{
		printf("[>] Searching for target window...\n");
		HWND targetWindow = FindWindowA(nullptr, "Clicker Heroes");
		if (!targetWindow)
		{
			printf("[!] Target window not found\n");
			Error();
		}

		printf("[+] Target window found with HWND 0x%p\n", targetWindow);

		printf("[>] Resolving process ID...\n");
		UINT targetProcessId;
		GetWindowThreadProcessId(targetWindow, reinterpret_cast<LPDWORD>(&targetProcessId));
		if (!targetProcessId)
		{
			printf("[!] Failed to resolve process ID\n");
			Error();
		}

		printf("[+] Process ID resolved to %u\n", targetProcessId);

		printf("[>] Connecting to the process...\n");
		processInfo.ProcessId = targetProcessId;
		bool status = communication::ConnectToProcess(processInfo);
		if (!status)
		{
			printf("[!] Failed to connect to the process overlay backend\n");
			Error();
		}

		printf("[+] Connected to the process with mapped address 0x%p\n", processInfo.MappedAddress_External);

		printf("[>] Drawing...\n");
		drawing::Frame mainFrame = drawing::CreateFrame(1280, 720);

		const UINT rectangleWidth = 100;
		const UINT rectangleHeight = 200;
		constexpr double speed = 200.0;

		double currentPosition = 0.0;
		int direction = 1;
		drawing::Pixel color = { 0, 0, 255, 255 };

		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		LARGE_INTEGER t1, t2;
		QueryPerformanceCounter(&t1);

		while (!(GetKeyState(VK_INSERT) & 0x8000))
		{
			QueryPerformanceCounter(&t2);
		
			double deltaTime = (t2.QuadPart - t1.QuadPart) / static_cast<double>(frequency.QuadPart);
			t1 = t2;
		
			drawing::CleanFrame(mainFrame);
		
			drawing::DrawRectangle(mainFrame, static_cast<UINT>(currentPosition), mainFrame.Height / 2 - rectangleHeight / 2, rectangleWidth, rectangleHeight, color);
		
			currentPosition += direction * speed * deltaTime;
		
			if (currentPosition + rectangleWidth >= mainFrame.Width)
				direction = -1;
			else if (currentPosition <= 0)
				direction = 1;
		
			communication::SendFrame(processInfo, mainFrame.Width, mainFrame.Height, mainFrame.Buffer, mainFrame.Size);
		}

		getchar();
		return 0;
	}
}