#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <iostream>

#include "hv_mem.h"

class Kmem
{
	DWORD  pid = 0;
	uintptr_t base_addr = 0;
	DWORD_PTR old_affinity = 0;
	bool pinned = false;

public:
	Kmem() = default;
	~Kmem() { unitialize_driver(); }

	auto GetProcessPid(const wchar_t* process_name) -> DWORD
	{
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE) return 0;

		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(pe);
		if (Process32FirstW(snap, &pe)) {
			do {
				if (_wcsicmp(pe.szExeFile, process_name) == 0) {
					CloseHandle(snap);
					return pe.th32ProcessID;
				}
			} while (Process32NextW(snap, &pe));
		}
		CloseHandle(snap);
		return 0;
	}

	auto init_driver() -> bool
	{
		return hv::ping();
	}

	auto setup(const wchar_t* process_name) -> bool
	{
		if (!init_driver())
		{
			std::cout << "    > HYPERVISOR NOT DETECTED" << std::endl;
			return false;
		}

		old_affinity = hv::pin_to_core(0);
		pinned = true;

		pid = GetProcessPid(process_name);
		if (!pid) {
			std::cout << "    > PROCESS NOT FOUND" << std::endl;
			return false;
		}

		if (!hv::attach(pid)) {
			std::cout << "    > ATTACH FAILED" << std::endl;
			return false;
		}

		base_addr = hv::get_module_base(process_name);
		return base_addr != 0;
	}

	auto unitialize_driver() -> void
	{
		if (pid) {
			hv::detach();
		}
		if (pinned) {
			hv::unpin(old_affinity);
			pinned = false;
		}
		pid = 0;
		base_addr = 0;
	}

	auto Pid() -> DWORD { return pid; }
	auto GetBase() -> uintptr_t { return base_addr; }
	auto GetCR3() -> uintptr_t { return 0; }

	template <typename T>
	auto read(uintptr_t address) -> T
	{
		if (!address) return T{};

		if constexpr (sizeof(T) <= 8)
		{
			return hv::read<T>((uint64_t)address);
		}
		else
		{
			T val{};
			hv::read_buffer((uint64_t)address, &val, sizeof(T));
			return val;
		}
	}

	template <typename T>
	auto write(uintptr_t address, T value) -> void
	{
		if (!address) return;
		hv::write<T>((uint64_t)address, value);
	}

	auto read_physical(PVOID address, void* buffer, size_t size) -> bool
	{
		if (!address || !buffer || !size) return false;
		return hv::read_buffer((uint64_t)(uintptr_t)address, buffer, size);
	}
};
