#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <string>

using RegistrationFunction = HRESULT(__stdcall*)();

int wmain(int argc, wchar_t** argv) {
  if (argc != 3 || (std::wstring_view(argv[1]) != L"install" && std::wstring_view(argv[1]) != L"uninstall")) {
    std::wcerr << L"Usage: AksharaRegister.exe <install|uninstall> <absolute-dll-path>\n";
    return ERROR_INVALID_PARAMETER;
  }
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
