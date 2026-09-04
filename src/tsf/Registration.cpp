#include "Module.h"
#include "Globals.h"

#include <array>
#include <string>

namespace {
std::wstring GuidString(REFGUID guid) {
  wchar_t value[40]{};
  return StringFromGUID2(guid, value, static_cast<int>(std::size(value))) ? value : L"";
}
HRESULT ModulePath(std::wstring& path) {
  std::array<wchar_t, 32768> buffer{};
  const auto count = GetModuleFileNameW(g_module, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (!count || count == buffer.size()) return HRESULT_FROM_WIN32(GetLastError());
  path.assign(buffer.data(), count); return S_OK;
}
HRESULT RegisterComServer(const std::wstring& module) {
  const auto subkey = L"Software\\Classes\\CLSID\\" + GuidString(CLSID_AksharaTextService);
  HKEY clsid = nullptr;
  auto error = RegCreateKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &clsid, nullptr);
  if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
  constexpr wchar_t name[] = L"Akshara Text Service";
  RegSetValueExW(clsid, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(name), sizeof(name));
  HKEY server = nullptr;
  error = RegCreateKeyExW(clsid, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &server, nullptr);
  if (error == ERROR_SUCCESS) {
    RegSetValueExW(server, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(module.c_str()), static_cast<DWORD>((module.size() + 1) * sizeof(wchar_t)));
    constexpr wchar_t apartment[] = L"Apartment";
    RegSetValueExW(server, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(apartment), sizeof(apartment));
    RegCloseKey(server);
  }
  RegCloseKey(clsid);
  return HRESULT_FROM_WIN32(error);
}
void UnregisterComServer() {
  const auto subkey = L"Software\\Classes\\CLSID\\" + GuidString(CLSID_AksharaTextService);
  RegDeleteTreeW(HKEY_LOCAL_MACHINE, subkey.c_str());
}
HRESULT RegisterCategories(bool add) {
  ITfCategoryMgr* categories = nullptr;
  auto hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories));
  if (FAILED(hr)) return hr;
  constexpr std::array<const GUID*, 4> values{
      &GUID_TFCAT_TIP_KEYBOARD,
      &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
      &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
      &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT};
  for (const auto* category : values) {
    hr = add ? categories->RegisterCategory(CLSID_AksharaTextService, *category, CLSID_AksharaTextService)
             : categories->UnregisterCategory(CLSID_AksharaTextService, *category, CLSID_AksharaTextService);
    if (FAILED(hr) && add) break;
  }
  categories->Release(); return hr;
}
HRESULT RegisterTextServiceAndProfiles(const std::wstring& module) {
  // Register the service itself before registering its profiles.  Windows can
  // display a profile that was added without this step, but it will not
  // reliably instantiate the text service when that profile is selected.
  ITfInputProcessorProfiles* legacyProfiles = nullptr;
  auto hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(&legacyProfiles));
  if (FAILED(hr)) return hr;
  hr = legacyProfiles->Register(CLSID_AksharaTextService);
  legacyProfiles->Release();
  if (FAILED(hr)) return hr;

  ITfInputProcessorProfileMgr* profiles = nullptr;
  hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
  if (FAILED(hr)) return hr;
  struct Profile { const GUID* guid; const wchar_t* name; };
  constexpr Profile list[] = {
    {&GUID_PROFILE_AKSHARA_SMART_PHONETIC, L"Akshara - Smart Phonetic"},
    {&GUID_PROFILE_AKSHARA_PHONETIC, L"Akshara - Phonetic"},
    {&GUID_PROFILE_AKSHARA_WIJESEKARA, L"Akshara - Wijesekara"}
  };
  for (const auto& profile : list) {
    hr = profiles->RegisterProfile(CLSID_AksharaTextService, kSinhalaSriLanka, *profile.guid,
      profile.name, static_cast<ULONG>(wcslen(profile.name)), module.c_str(), static_cast<ULONG>(module.size()),
      kImeIconResourceId, nullptr, 0, TRUE, 0);
    if (FAILED(hr)) break;
  }
  profiles->Release(); return hr;
}
HRESULT UnregisterProfiles() {
  ITfInputProcessorProfileMgr* profiles = nullptr;
  auto hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
  if (FAILED(hr)) return hr;
  constexpr std::array<const GUID*, 3> list{&GUID_PROFILE_AKSHARA_SMART_PHONETIC, &GUID_PROFILE_AKSHARA_PHONETIC, &GUID_PROFILE_AKSHARA_WIJESEKARA};
  for (const auto* profile : list) profiles->UnregisterProfile(CLSID_AksharaTextService, kSinhalaSriLanka, *profile, 0);
  profiles->Release(); return S_OK;
}

void UnregisterTextService() {
  ITfInputProcessorProfiles* profiles = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&profiles)))) {
    profiles->Unregister(CLSID_AksharaTextService);
    profiles->Release();
  }
}
}

HRESULT RegisterAkshara() {
  std::wstring module; auto hr = ModulePath(module); if (FAILED(hr)) return hr;
  hr = RegisterComServer(module); if (FAILED(hr)) return hr;
  // Categories describe the service capabilities to TSF.  They must exist
  // before profiles are made available to the current user/session.
  hr = RegisterCategories(true);
  if (SUCCEEDED(hr)) hr = RegisterTextServiceAndProfiles(module);
  return hr;
}
HRESULT UnregisterAkshara() {
  UnregisterProfiles(); UnregisterTextService(); RegisterCategories(false);
  UnregisterComServer(); return S_OK;
}
