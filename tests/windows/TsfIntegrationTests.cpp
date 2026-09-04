#include "../../src/tsf/Globals.h"

#include <windows.h>
#include <msctf.h>

#include <iostream>

namespace {
HRESULT Fail(const wchar_t* stage, HRESULT hr) {
  std::wcerr << stage << L" failed: 0x" << std::hex << static_cast<unsigned long>(hr) << L"\n";
  return hr;
}
}

int wmain() {
  const auto initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(initHr)) return static_cast<int>(HRESULT_CODE(Fail(L"CoInitializeEx", initHr)));

  ITfThreadMgr* manager = nullptr;
  ITfDocumentMgr* document = nullptr;
  ITfContext* context = nullptr;
  ITfInputProcessorProfileMgr* profiles = nullptr;
  ITfKeystrokeMgr* keystrokes = nullptr;
  TfClientId clientId = TF_CLIENTID_NULL;
  TfEditCookie cookie{};
  HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager));
  if (SUCCEEDED(hr)) hr = manager->Activate(&clientId);
  if (SUCCEEDED(hr)) hr = manager->CreateDocumentMgr(&document);
  if (SUCCEEDED(hr)) hr = document->CreateContext(clientId, 0, nullptr, &context, &cookie);
  if (SUCCEEDED(hr)) hr = document->Push(context);
  if (SUCCEEDED(hr)) hr = manager->SetFocus(document);
  if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
  if (SUCCEEDED(hr)) {
    hr = profiles->ActivateProfile(
        TF_PROFILETYPE_INPUTPROCESSOR, kSinhalaSriLanka, CLSID_AksharaTextService,
        GUID_PROFILE_AKSHARA_SMART_PHONETIC, nullptr,
        TF_IPPMF_FORPROCESS | TF_IPPMF_ENABLEPROFILE | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
  }
  TF_INPUTPROCESSORPROFILE active{};
  if (SUCCEEDED(hr)) hr = profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active);
  if (SUCCEEDED(hr) &&
      (!IsEqualCLSID(active.clsid, CLSID_AksharaTextService) ||
       !IsEqualGUID(active.guidProfile, GUID_PROFILE_AKSHARA_SMART_PHONETIC))) {
    hr = E_FAIL;
  }
  if (SUCCEEDED(hr)) hr = manager->QueryInterface(IID_PPV_ARGS(&keystrokes));

  BOOL eaten = FALSE;
  if (SUCCEEDED(hr)) hr = keystrokes->TestKeyDown(L'M', 0, &eaten);
  if (SUCCEEDED(hr) && !eaten) hr = E_FAIL;

  if (keystrokes) keystrokes->Release();
  if (profiles) profiles->Release();
  if (context) context->Release();
  if (document) document->Release();
  if (manager) { manager->Deactivate(); manager->Release(); }
  CoUninitialize();

  if (FAILED(hr)) {
    const auto* stage = eaten ? L"TSF key test" :
                        (IsEqualCLSID(active.clsid, CLSID_AksharaTextService) ?
                         L"Akshara did not intercept M" : L"Akshara did not become the active TIP");
    return static_cast<int>(HRESULT_CODE(Fail(stage, hr)));
  }
  std::wcout << L"Akshara TSF integration test passed.\n";
  return 0;
}
