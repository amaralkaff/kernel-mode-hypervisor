#pragma once
#include <Windows.h>
#include <string>
#include <iostream>
#include <filesystem>

#include "hv_mem.h"

namespace driver
{
	// KDU provider ID — change if current one gets detected
	// 6=EneIo64(WHQL)  8=EneTechIo64(WHQL)  54=NeacSafe64(WHQL)  55=ThrottleStop(WHQL)
	constexpr int KDU_PROVIDER = 54;

	inline bool is_loaded()
	{
		return hv::ping();
	}

	inline std::string get_exe_dir()
	{
		char path[MAX_PATH]{};
		GetModuleFileNameA(nullptr, path, MAX_PATH);
		std::string s(path);
		auto pos = s.find_last_of("\\/");
		return (pos != std::string::npos) ? s.substr(0, pos + 1) : s;
	}

	inline bool load()
	{
		if (is_loaded()) {
			std::cout << "    > HYPERVISOR ALREADY LOADED" << std::endl;
			return true;
		}

		std::string dir = get_exe_dir();
		std::string loader = dir + "svcloader.exe";
		std::string sys = dir + "vmhv.sys";

		if (!std::filesystem::exists(loader)) {
			std::cout << "    > svcloader.exe NOT FOUND" << std::endl;
			return false;
		}
		if (!std::filesystem::exists(sys)) {
			std::cout << "    > vmhv.sys NOT FOUND" << std::endl;
			return false;
		}

		std::string cmd = "\"" + loader + "\" -prv " +
			std::to_string(KDU_PROVIDER) + " -scv 3 -drvn WinSockProxy -map \"" + sys + "\"";
		std::cout << "    > LOADING HYPERVISOR (KDU prv " <<
			KDU_PROVIDER << ")..." << std::endl;

		STARTUPINFOA si{};
		PROCESS_INFORMATION pi{};
		si.cb = sizeof(si);

		if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
			nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
			nullptr, nullptr, &si, &pi))
		{
			std::cout << "    > LOADER LAUNCH FAILED" << std::endl;
			return false;
		}

		WaitForSingleObject(pi.hProcess, 30000);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		for (int i = 0; i < 10; i++) {
			if (is_loaded()) {
				std::cout << "    > HYPERVISOR LOADED" << std::endl;
				return true;
			}
			Sleep(1000);
		}

		std::cout << "    > HYPERVISOR LOAD FAILED" << std::endl;
		return false;
	}
}
