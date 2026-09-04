#include <windows.h>
#include <objbase.h>
#include <msctf.h>
#include <iostream>
#include <string>

#include "../tsf/Globals.h"

using RegistrationFunction = HRESULT(__stdcall*)();

namespace {
int ExitCode(HRESULT hr) {
  if (SUCCEEDED(hr)) return ERROR_SUCCESS;
  std::wcerr << L"Akshara TSF probe failed: 0x" << std::hex << static_cast<unsigned long>(hr) << L"\n";
  return static_cast<int>(HRESULT_CODE(hr));
}

int ProbeActivation() {
  ITfThreadMgr* manager = nullptr;
  auto hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager));
  if (FAILED(hr)) return ExitCode(hr);

  TfClientId clientId = TF_CLIENTID_NULL;
  hr = manager->Activate(&clientId);
  if (FAILED(hr)) { manager->Release(); return ExitCode(hr); }

  ITfTextInputProcessorEx* service = nullptr;
  hr = CoCreateInstance(CLSID_AksharaTextService, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&service));
  if (SUCCEEDED(hr)) {
    hr = service->ActivateEx(manager, clientId, 0);
    if (SUCCEEDED(hr)) service->Deactivate();
    service->Release();
  }
  manager->Deactivate();
  manager->Release();
  if (SUCCEEDED(hr)) std::wcout << L"Akshara TSF activation probe succeeded.\n";
  return ExitCode(hr);
}
}

int wmain(int argc, wchar_t** argv) {
  const bool probe = argc == 2 && std::wstring_view(argv[1]) == L"probe";
  if ((!probe && argc != 3) || (!probe && std::wstring_view(argv[1]) != L"install" && std::wstring_view(argv[1]) != L"uninstall")) {
    std::wcerr << L"Usage: AksharaRegister.exe <install|uninstall> <absolute-dll-path> | probe\n";
    return ERROR_INVALID_PARAMETER;
  }
  const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(com) && com != RPC_E_CHANGED_MODE) return static_cast<int>(HRESULT_CODE(com));
  if (probe) {
    const auto result = ProbeActivation();
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
  }
  const auto module = LoadLibraryExW(argv[2], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!module) { const auto error = GetLastError(); if (SUCCEEDED(com)) CoUninitialize(); return static_cast<int>(error); }
  const char* functionName = std::wstring_view(argv[1]) == L"install" ? "DllRegisterServer" : "DllUnregisterServer";
  const auto function = reinterpret_cast<RegistrationFunction>(GetProcAddress(module, functionName));
  if (!function) { const auto error = GetLastError(); FreeLibrary(module); if (SUCCEEDED(com)) CoUninitialize(); return static_cast<int>(error); }
  const auto hr = function(); FreeLibrary(module);
  if (SUCCEEDED(com)) CoUninitialize();
  return SUCCEEDED(hr) ? ERROR_SUCCESS : static_cast<int>(HRESULT_CODE(hr));
}
