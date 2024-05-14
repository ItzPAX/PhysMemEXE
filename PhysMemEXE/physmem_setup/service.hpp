#pragma once
#include <Windows.h>
#include <string>

#include "intel_driver.hpp"

namespace service
{
	bool RegisterAndStart(const std::wstring& driver_path, std::wstring name);
	bool StopAndRemove(const std::wstring& driver_name);
};