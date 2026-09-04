#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <string>

using RegistrationFunction = HRESULT(__stdcall*)();
using InstallLayoutOrTipFunction = BOOL(WINAPI*)(LPCWSTR, DWORD);

namespace {
int ClearDiagnosticsLog() {
  wchar_t localAppData[MAX_PATH]{};
  const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
  if (!length || length >= _countof(localAppData)) return ERROR_SUCCESS;
  const std::wstring path = std::wstring(localAppData, length) + L"\\Akshara\\tsf-diagnostics.log";
  // The running text service opens this file with write sharing but without
  // delete sharing. Truncating therefore clears old diagnostics even when an
  // app still has the previous DLL loaded during an upgrade.
  const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : static_cast<int>(GetLastError());
  LARGE_INTEGER zero{};
  const bool cleared = SetFilePointerEx(file, zero, nullptr, FILE_BEGIN) && SetEndOfFile(file);
  const auto error = cleared ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  return static_cast<int>(error);
}

int EnableProfile(std::wstring_view name) {
  const wchar_t* profile = nullptr;
  if (name == L"smart") {
    profile = L"0x045B:{8B8E29C7-E118-4C77-9F58-525784EFB9C1}{D602E665-86AD-42DF-9A67-B8B17515B172}";
  } else if (name == L"phonetic") {
    profile = L"0x045B:{8B8E29C7-E118-4C77-9F58-525784EFB9C1}{132D8F4E-930A-4167-866E-C3C021FA0E93}";
  } else if (name == L"wijesekara") {
    profile = L"0x045B:{8B8E29C7-E118-4C77-9F58-525784EFB9C1}{C98CA5C8-003C-4C20-A93D-C4908625F84A}";
  } else {
    return ERROR_INVALID_PARAMETER;
  }

  const auto input = LoadLibraryExW(L"input.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!input) return static_cast<int>(GetLastError());
  const auto install = reinterpret_cast<InstallLayoutOrTipFunction>(GetProcAddress(input, "InstallLayoutOrTip"));
  if (!install) { const auto error = GetLastError(); FreeLibrary(input); return static_cast<int>(error); }
  const bool enabled = install(profile, 0) != FALSE;
  const auto error = enabled ? ERROR_SUCCESS : GetLastError();
  FreeLibrary(input);
  return static_cast<int>(error);
}
}

int wmain(int argc, wchar_t** argv) {
  if (argc == 3 && (std::wstring_view(argv[1]) == L"install" || std::wstring_view(argv[1]) == L"uninstall")) {
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return static_cast<int>(HRESULT_CODE(com));
    const auto module = LoadLibraryExW(argv[2], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) { const auto error = GetLastError(); if (SUCCEEDED(com)) CoUninitialize(); return static_cast<int>(error); }
    const char* functionName = std::wstring_view(argv[1]) == L"install" ? "DllRegisterServer" : "DllUnregisterServer";
    const auto function = reinterpret_cast<RegistrationFunction>(GetProcAddress(module, functionName));
    if (!function) { const auto error = GetLastError(); FreeLibrary(module); if (SUCCEEDED(com)) CoUninitialize(); return static_cast<int>(error); }
    const auto hr = function(); FreeLibrary(module);
    if (SUCCEEDED(com)) CoUninitialize();
    return SUCCEEDED(hr) ? ERROR_SUCCESS : static_cast<int>(HRESULT_CODE(hr));
  }
  if (argc == 3 && std::wstring_view(argv[1]) == L"enable") return EnableProfile(argv[2]);
  if (argc == 2 && std::wstring_view(argv[1]) == L"clear-log") return ClearDiagnosticsLog();

  std::wcerr << L"Usage: AksharaRegister.exe <install|uninstall> <absolute-dll-path>\n"
                L"       AksharaRegister.exe enable <smart|phonetic|wijesekara>\n"
                L"       AksharaRegister.exe clear-log\n";
    return ERROR_INVALID_PARAMETER;
}
