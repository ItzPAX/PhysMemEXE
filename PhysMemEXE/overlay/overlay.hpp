#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <unordered_map>

#include "../physmem_setup/physmem.hpp"

namespace overlay
{
	namespace communication
	{
		/*
			key = pt index
			value = physical address
		*/
		std::unordered_map<uint64_t, uint64_t> file_physical_address_map;

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
			Header* MappedAddress;
		} ConnectedProcessInfo;

		inline uintptr_t get_pt(uintptr_t virtual_address, physmem& mem)
		{
			mem.attached_dtb = mem.attached_dtb & ~0xf;

			uintptr_t va = virtual_address;

			unsigned short pml4_ind = (unsigned short)((va >> 39) & 0x1FF);
			PML4E pml4e;
			if (!mem.read_physical_memory((mem.attached_dtb + pml4_ind * sizeof(uintptr_t)), (byte*)&pml4e, sizeof(pml4e)))
				return 0;

			if (pml4e.Present == 0 || pml4e.PageSize)
				return 0;

			unsigned short pdpt_ind = (unsigned short)((va >> 30) & 0x1FF);
			PDPTE pdpte;
			if (!mem.read_physical_memory(((pml4e.Value & 0xFFFFFFFFFF000) + pdpt_ind * sizeof(uintptr_t)), (byte*)&pdpte, sizeof(pdpte)))
				return 0;

			if (pdpte.Present == 0)
				return 0;

			if (pdpte.PageSize)
				return 0;

			unsigned short pd_ind = (unsigned short)((va >> 21) & 0x1FF);
			PDE pde;
			if (!mem.read_physical_memory(((pdpte.Value & 0xFFFFFFFFFF000) + pd_ind * sizeof(uintptr_t)), (byte*)&pde, sizeof(pde)))
				return 0;

			if (pde.Present == 0)
				return 0;

			if (pde.PageSize)
			{
				return 0;
			}

			return (pde.Value & 0xFFFFFFFFFF000);
		}

		inline void get_memory_file_physical_info(ConnectedProcessInfo& process_info, physmem& mem)
		{
			uintptr_t old = mem.attached_dtb;
			mem.attach(mem.local_process_name);
			uintptr_t pt = get_pt((uintptr_t)process_info.MappedAddress, mem);
			std::cout << "Test: " << std::hex << pt << std::endl;
			mem.attached_dtb = old;

			unsigned short pt_ind = (unsigned short)(((uintptr_t)process_info.MappedAddress >> 12) & 0x1FF);
			
			std::cout << "PAGE: " << std::floor(pt / 0x40000000) << std::endl;

			for (int index = 0; index < 512; index++)
			{
				PTE pte;
				mem.read_physical_memory(pt + index * sizeof(uintptr_t), (byte*)&pte, 8);
				std::cout << std::dec << index << " added: " << std::hex << pte.PageFrameNumber * 0x1000 << std::endl;
			}

			std::cout << file_physical_address_map.size() << std::endl;
		}

		inline bool ConnectToProcess(ConnectedProcessInfo& processInfo, physmem& mem)
		{
			std::string mappedFilename = "DiscordOverlay_Framebuffer_Memory_" + std::to_string(processInfo.ProcessId);
			processInfo.File = OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, mappedFilename.c_str());
			if (!processInfo.File || processInfo.File == INVALID_HANDLE_VALUE)
				return false;

			processInfo.MappedAddress = static_cast<Header*>(MapViewOfFile(processInfo.File, FILE_MAP_ALL_ACCESS, 0, 0, 0));

			get_memory_file_physical_info(processInfo, mem);

			std::cout << processInfo.MappedAddress << std::endl;
			return processInfo.MappedAddress;
		}

		inline void DisconnectFromProcess(ConnectedProcessInfo& processInfo)
		{
			UnmapViewOfFile(processInfo.MappedAddress);
			processInfo.MappedAddress = nullptr;

			CloseHandle(processInfo.File);
			processInfo.File = nullptr;
		}

		inline void SendFrame(ConnectedProcessInfo& processInfo, UINT width, UINT height, void* frame, UINT size)
		{
			// frame is in B8G8R8A8 format
			// size can be nearly anything since it will get resized
			// for the screen appropriately, although the maximum size is
			// game window width * height * 4 (BGRA)
			processInfo.MappedAddress->Width = width;
			processInfo.MappedAddress->Height = height;

			memcpy(processInfo.MappedAddress->Buffer, frame, size);

			processInfo.MappedAddress->FrameCount++; // this will cause the internal module to copy over the framebuffer
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

	int draw(physmem& mem)
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
		bool status = communication::ConnectToProcess(processInfo, mem);
		if (!status)
		{
			printf("[!] Failed to connect to the process overlay backend\n");
			Error();
		}

		printf("[+] Connected to the process with mapped address 0x%p\n", processInfo.MappedAddress);

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