#include "TextService.h"
#include "EditSession.h"
#include "Globals.h"

#include <new>
#include <string>

namespace {
template<class T> void ComRelease(T*& value) { if (value) { value->Release(); value = nullptr; } }

// Developer setup diagnostics intentionally record lifecycle events only—never
// keys or text.  They make it possible to distinguish Windows routing failures
// from composition failures when the service runs inside another application.
void TraceTsfEvent(const wchar_t* event, HRESULT result = S_OK) {
  wchar_t localAppData[MAX_PATH]{};
  const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
  if (!length || length >= _countof(localAppData)) return;
  const std::wstring directory = std::wstring(localAppData, length) + L"\\Akshara";
  CreateDirectoryW(directory.c_str(), nullptr);
  const std::wstring path = directory + L"\\tsf-diagnostics.log";
  const auto file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  wchar_t process[MAX_PATH]{};
  GetModuleFileNameW(nullptr, process, _countof(process));
  wchar_t line[768]{};
  const auto written = swprintf_s(line, L"pid=%lu process=%ls event=%ls result=0x%08lX\r\n",
                                  GetCurrentProcessId(), process, event,
                                  static_cast<unsigned long>(result));
  if (written > 0) {
    DWORD bytes{};
    WriteFile(file, line, static_cast<DWORD>(written * sizeof(wchar_t)), &bytes, nullptr);
  }
  CloseHandle(file);
}

bool IsBoundary(WPARAM key) {
  switch (key) {
    case VK_SPACE: case VK_RETURN: case VK_TAB: case VK_ESCAPE:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: case VK_DELETE:
      return true;
    default: return false;
  }
}
bool IsOemOrLetter(WPARAM key) {
  return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') ||
         key == VK_OEM_1 || key == VK_OEM_PLUS || key == VK_OEM_COMMA || key == VK_OEM_MINUS ||
         key == VK_OEM_PERIOD || key == VK_OEM_2 || key == VK_OEM_3 || key == VK_OEM_4 ||
         key == VK_OEM_5 || key == VK_OEM_6 || key == VK_OEM_7;
}
}

TextService::TextService() { InterlockedIncrement(&g_objectCount); }
TextService::~TextService() { Deactivate(); InterlockedDecrement(&g_objectCount); }

HRESULT TextService::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx)
    *object = static_cast<ITfTextInputProcessorEx*>(this);
  else if (riid == IID_ITfKeyEventSink) *object = static_cast<ITfKeyEventSink*>(this);
  else if (riid == IID_ITfCompositionSink) *object = static_cast<ITfCompositionSink*>(this);
  else if (riid == IID_ITfThreadMgrEventSink) *object = static_cast<ITfThreadMgrEventSink*>(this);
  else if (riid == IID_ITfThreadFocusSink) *object = static_cast<ITfThreadFocusSink*>(this);
  else if (riid == IID_ITfActiveLanguageProfileNotifySink) *object = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
  else if (riid == IID_ITfTextEditSink) *object = static_cast<ITfTextEditSink*>(this);
  else if (riid == IID_ITfCompartmentEventSink) *object = static_cast<ITfCompartmentEventSink*>(this);
  if (!*object) return E_NOINTERFACE;
  AddRef(); return S_OK;
}
ULONG TextService::AddRef() { return ++refs_; }
ULONG TextService::Release() { const auto count = --refs_; if (!count) delete this; return count; }
HRESULT TextService::Activate(ITfThreadMgr* manager, TfClientId id) { return ActivateEx(manager, id, 0); }
HRESULT TextService::ActivateEx(ITfThreadMgr* manager, TfClientId id, DWORD) {
  if (!manager || threadManager_) return E_INVALIDARG;
  InterlockedIncrement(&g_tsfDiagnostics.activationCalls);
  InterlockedExchange(&g_tsfDiagnostics.clientId, static_cast<LONG>(id));
  TraceTsfEvent(L"ActivateEx");
  threadManager_ = manager; manager->AddRef(); clientId_ = id;
  const auto adviseHr = AdviseSinks();
  if (SUCCEEDED(adviseHr)) return S_OK;
  Deactivate();
  return adviseHr;
}
HRESULT TextService::Deactivate() {
  if (!threadManager_) return S_OK;
  ITfDocumentMgr* document = nullptr; ITfContext* context = nullptr;
  if (SUCCEEDED(threadManager_->GetFocus(&document)) && document) document->GetTop(&context);
  if (context) { RequestEdit(context, true); context->Release(); }
  ComRelease(document);
  UnadviseSinks(); ResetComposition(); buffer_.clear(); clientId_ = TF_CLIENTID_NULL; ComRelease(threadManager_);
  return S_OK;
}
HRESULT TextService::AdviseSinks() {
  ITfSource* source = nullptr;
  auto hr = threadManager_->QueryInterface(IID_PPV_ARGS(&source));
  if (FAILED(hr)) return hr;
  hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadSinkCookie_);
  TraceTsfEvent(L"AdviseThreadMgrEventSink", hr);
  if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this), &threadFocusSinkCookie_);
  TraceTsfEvent(L"AdviseThreadFocusSink", hr);
  if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink, static_cast<ITfActiveLanguageProfileNotifySink*>(this), &profileSinkCookie_);
  TraceTsfEvent(L"AdviseProfileNotifySink", hr);
  source->Release();
  if (FAILED(hr)) return hr;
  ITfDocumentMgr* document = nullptr;
  hr = threadManager_->GetFocus(&document);
  if (SUCCEEDED(hr) && document) { hr = AdviseFocusedContext(document); document->Release(); }
  if (FAILED(hr)) return hr;
  hr = AdviseKeyboardOpenCompartment();
  if (FAILED(hr)) return hr;
  ITfKeystrokeMgr* keys = nullptr;
  hr = threadManager_->QueryInterface(IID_PPV_ARGS(&keys));
  if (FAILED(hr)) return hr;
  hr = keys->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
  keys->Release();
  InterlockedExchange(&g_tsfDiagnostics.keySinkAdviceResult, static_cast<LONG>(hr));
  TraceTsfEvent(L"AdviseKeyEventSink", hr);
  return hr;
}
HRESULT TextService::AdviseFocusedContext(ITfDocumentMgr* document) {
  UnadviseFocusedContext();
  if (!document) return S_OK;
  ITfContext* context = nullptr;
  auto hr = document->GetTop(&context);
  if (FAILED(hr) || !context) return FAILED(hr) ? hr : S_OK;
  ITfSource* source = nullptr;
  hr = context->QueryInterface(IID_PPV_ARGS(&source));
  if (SUCCEEDED(hr)) {
    hr = source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &textEditSinkCookie_);
    source->Release();
  }
  TraceTsfEvent(L"AdviseTextEditSink", hr);
  if (SUCCEEDED(hr)) textEditContext_ = context;
  else context->Release();
  return hr;
}
HRESULT TextService::AdviseKeyboardOpenCompartment() {
  ITfCompartmentMgr* manager = nullptr;
  auto hr = threadManager_->QueryInterface(IID_PPV_ARGS(&manager));
  if (FAILED(hr)) return hr;
  hr = manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &keyboardOpenCompartment_);
  manager->Release();
  if (FAILED(hr)) return hr;
  ITfSource* source = nullptr;
  hr = keyboardOpenCompartment_->QueryInterface(IID_PPV_ARGS(&source));
  if (SUCCEEDED(hr)) { hr = source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &keyboardOpenSinkCookie_); source->Release(); }
  TraceTsfEvent(L"AdviseKeyboardOpenSink", hr);
  if (FAILED(hr)) return hr;
  return OnChange(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
}
void TextService::UnadviseFocusedContext() {
  if (textEditContext_ && textEditSinkCookie_ != TF_INVALID_COOKIE) {
    ITfSource* source = nullptr;
    if (SUCCEEDED(textEditContext_->QueryInterface(IID_PPV_ARGS(&source)))) { source->UnadviseSink(textEditSinkCookie_); source->Release(); }
  }
  textEditSinkCookie_ = TF_INVALID_COOKIE;
  ComRelease(textEditContext_);
}
void TextService::UnadviseSinks() {
  if (!threadManager_) return;
  UnadviseFocusedContext();
  if (keyboardOpenCompartment_ && keyboardOpenSinkCookie_ != TF_INVALID_COOKIE) {
    ITfSource* source = nullptr;
    if (SUCCEEDED(keyboardOpenCompartment_->QueryInterface(IID_PPV_ARGS(&source)))) { source->UnadviseSink(keyboardOpenSinkCookie_); source->Release(); }
  }
  keyboardOpenSinkCookie_ = TF_INVALID_COOKIE;
  ComRelease(keyboardOpenCompartment_);
  ITfKeystrokeMgr* keys = nullptr;
  if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&keys)))) { keys->UnadviseKeyEventSink(clientId_); keys->Release(); }
  ITfSource* source = nullptr;
  if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&source)))) {
    if (threadSinkCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(threadSinkCookie_);
    if (threadFocusSinkCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(threadFocusSinkCookie_);
    if (profileSinkCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(profileSinkCookie_);
    source->Release();
  }
  threadSinkCookie_ = threadFocusSinkCookie_ = profileSinkCookie_ = TF_INVALID_COOKIE;
}
bool TextService::IsContextWritable(ITfContext* context) const {
  TF_STATUS status{};
  if (!context || FAILED(context->GetStatus(&status)) || (status.dwDynamicFlags & TS_SD_READONLY)) return false;
  ITfCompartmentMgr* compartments = nullptr;
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&compartments)))) {
    ITfCompartment* disabled = nullptr;
    if (SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED, &disabled))) {
      VARIANT value{}; VariantInit(&value);
      const bool blocked = SUCCEEDED(disabled->GetValue(&value)) && value.vt == VT_I4 && value.lVal != 0;
      VariantClear(&value); disabled->Release();
      if (blocked) { compartments->Release(); return false; }
    }
    compartments->Release();
  }
  return true;
}
bool TextService::ShouldEatKey(ITfContext* context, WPARAM key) const {
  if (!IsContextWritable(context)) return false;
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  const bool rightAlt = (GetKeyState(VK_RMENU) & 0x8000) != 0;
  const bool altGr = ctrl && alt && rightAlt;
  if ((ctrl || alt) && !(buffer_.mode() == akshara::InputMode::Wijesekara && altGr)) return false;
  if (key == VK_BACK) return !buffer_.empty();
  if (IsBoundary(key)) return false;
  if (buffer_.mode() == akshara::InputMode::Wijesekara) return IsOemOrLetter(key) || (altGr && key == VK_SPACE);
  return key >= 'A' && key <= 'Z';
}
HRESULT TextService::OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  const auto calls = InterlockedIncrement(&g_tsfDiagnostics.testKeyDownCalls);
  if (calls == 1) TraceTsfEvent(L"OnTestKeyDown");
  const bool writable = IsContextWritable(context);
  InterlockedExchange(&g_tsfDiagnostics.lastContextWasWritable, writable);
  if (!eaten) return E_POINTER;
  *eaten = writable && ShouldEatKey(context, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  const auto calls = InterlockedIncrement(&g_tsfDiagnostics.keyDownCalls);
  if (calls == 1) TraceTsfEvent(L"OnKeyDown");
  *eaten = HandleKey(context, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
bool TextService::HandleKey(ITfContext* context, WPARAM key) {
  if (!ShouldEatKey(context, key)) {
    if (!buffer_.empty()) RequestEdit(context, true);
    return false;
  }
  if (key == VK_BACK) {
    buffer_.backspace(); RequestEdit(context, false); return true;
  }
  if (buffer_.mode() == akshara::InputMode::Wijesekara) {
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool altGr = (GetKeyState(VK_RMENU) & 0x8000) != 0 && (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const auto mapped = engine_.mapWijesekaraKey({static_cast<std::uint32_t>(key), shift, altGr});
    if (mapped.empty()) return false;
    if (!akshara::AksharaEngine::isComposableWijesekaraUnit(mapped)) {
      buffer_.append(mapped); RequestEdit(context, false); RequestEdit(context, true); return true;
    }
    buffer_.append(mapped);
  } else {
    const bool upper = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) != ((GetKeyState(VK_CAPITAL) & 1) != 0);
    buffer_.append(std::u16string(1, static_cast<char16_t>((upper ? u'A' : u'a') + (key - 'A'))));
  }
  RequestEdit(context, false); return true;
}
HRESULT TextService::RequestEdit(ITfContext* context, bool commitOnly) {
  auto* session = new (std::nothrow) EditSession(this, context, commitOnly);
  if (!session) return E_OUTOFMEMORY;
  HRESULT sessionResult = E_FAIL;
  const auto hr = context->RequestEditSession(clientId_, session, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult);
  session->Release();
  return FAILED(hr) ? hr : sessionResult;
}
HRESULT TextService::ApplyEdit(ITfContext* context, TfEditCookie cookie, bool commitOnly) {
  if (commitOnly) {
    if (composition_) composition_->EndComposition(cookie);
    ResetComposition(); buffer_.clear(); return S_OK;
  }
  const auto rendered = buffer_.render(engine_);
  ITfRange* range = nullptr;
  if (!composition_) {
    TF_SELECTION selection{}; ULONG fetched = 0;
    if (FAILED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched != 1) return E_FAIL;
    range = selection.range; range->Collapse(cookie, TF_ANCHOR_START);
    ITfContextComposition* compositions = nullptr;
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&compositions)))) { range->Release(); return E_NOINTERFACE; }
    const auto hr = compositions->StartComposition(cookie, range, this, &composition_); compositions->Release();
    if (FAILED(hr) || !composition_) { range->Release(); return FAILED(hr) ? hr : E_FAIL; }
  } else if (FAILED(composition_->GetRange(&range))) return E_FAIL;
  static_assert(sizeof(char16_t) == sizeof(WCHAR));
  const auto textHr = range->SetText(cookie, 0, reinterpret_cast<const WCHAR*>(rendered.text.data()), static_cast<LONG>(rendered.text.size()));
  if (SUCCEEDED(textHr)) {
    range->Collapse(cookie, TF_ANCHOR_END);
    TF_SELECTION selection{}; selection.range = range; selection.style.ase = TF_AE_NONE; selection.style.fInterimChar = FALSE;
    context->SetSelection(cookie, 1, &selection);
  }
  range->Release();
  if (buffer_.empty()) { if (composition_) composition_->EndComposition(cookie); ResetComposition(); }
  return textHr;
}
void TextService::ResetComposition() { ComRelease(composition_); }
HRESULT TextService::OnCompositionTerminated(TfEditCookie, ITfComposition*) { ResetComposition(); buffer_.clear(); return S_OK; }
HRESULT TextService::OnSetFocus(BOOL) { return S_OK; }
HRESULT TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnPushContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnPopContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnSetThreadFocus() {
  TraceTsfEvent(L"OnSetThreadFocus");
  return S_OK;
}
HRESULT TextService::OnKillThreadFocus() { TraceTsfEvent(L"OnKillThreadFocus"); return S_OK; }
HRESULT TextService::OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) {
  TraceTsfEvent(L"OnSetFocus");
  if (focus) {
    AdviseFocusedContext(focus);
  }
  ITfContext* context = nullptr;
  if (previous && SUCCEEDED(previous->GetTop(&context)) && context) { RequestEdit(context, true); context->Release(); }
  else { ResetComposition(); buffer_.clear(); }
  return S_OK;
}
void TextService::SelectProfile(REFGUID profile) {
  if (profile == GUID_PROFILE_AKSHARA_SMART_PHONETIC) buffer_.setMode(akshara::InputMode::SmartPhonetic);
  else if (profile == GUID_PROFILE_AKSHARA_PHONETIC) buffer_.setMode(akshara::InputMode::Phonetic);
  else if (profile == GUID_PROFILE_AKSHARA_WIJESEKARA) buffer_.setMode(akshara::InputMode::Wijesekara);
}
HRESULT TextService::OnActivated(REFCLSID clsid, REFGUID profile, BOOL activated) {
  if (clsid == CLSID_AksharaTextService && activated) {
    TraceTsfEvent(L"OnActivated");
    if (!buffer_.empty() && threadManager_) {
      ITfDocumentMgr* document = nullptr; ITfContext* context = nullptr;
      if (SUCCEEDED(threadManager_->GetFocus(&document)) && document) document->GetTop(&context);
      if (context) { RequestEdit(context, true); context->Release(); }
      ComRelease(document);
    }
    SelectProfile(profile);
  }
  else if (!activated) { ResetComposition(); buffer_.clear(); }
  return S_OK;
}
HRESULT TextService::OnEndEdit(ITfContext*, TfEditCookie, ITfEditRecord*) { return S_OK; }
HRESULT TextService::OnChange(REFGUID guid) {
  if (guid != GUID_COMPARTMENT_KEYBOARD_OPENCLOSE || !keyboardOpenCompartment_) return S_OK;
  VARIANT value{};
  VariantInit(&value);
  const auto hr = keyboardOpenCompartment_->GetValue(&value);
  if (SUCCEEDED(hr) && value.vt == VT_I4) keyboardOpen_ = value.lVal != 0;
  VariantClear(&value);
  TraceTsfEvent(keyboardOpen_ ? L"KeyboardOpen" : L"KeyboardClosed", hr);
  return hr;
}
