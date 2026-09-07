#include "TextService.h"
#include "EditSession.h"
#include "Globals.h"

#include <new>

namespace {
template <typename T> void release(T*& value) { if (value) { value->Release(); value = nullptr; } }
thread_local TextService* keyHookService = nullptr;
bool isBoundary(WPARAM key) {
  switch (key) {
    case VK_SPACE: case VK_RETURN: case VK_TAB: case VK_ESCAPE:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: case VK_DELETE: return true;
    default: return false;
  }
}
bool isPunctuation(WPARAM key) {
  return key == VK_OEM_1 || key == VK_OEM_COMMA || key == VK_OEM_PERIOD || key == VK_OEM_2 || key == VK_OEM_7;
}
bool isOem(WPARAM key) {
  return key == VK_OEM_1 || key == VK_OEM_PLUS || key == VK_OEM_COMMA || key == VK_OEM_MINUS ||
         key == VK_OEM_PERIOD || key == VK_OEM_2 || key == VK_OEM_3 || key == VK_OEM_4 ||
         key == VK_OEM_5 || key == VK_OEM_6 || key == VK_OEM_7;
}
}

TextService::TextService() { InterlockedIncrement(&g_objectCount); }
TextService::~TextService() { Deactivate(); InterlockedDecrement(&g_objectCount); }
HRESULT TextService::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx) *object = static_cast<ITfTextInputProcessorEx*>(this);
  else if (riid == IID_ITfThreadMgrEventSink) *object = static_cast<ITfThreadMgrEventSink*>(this);
  else if (riid == IID_ITfKeyEventSink) *object = static_cast<ITfKeyEventSink*>(this);
  else if (riid == IID_ITfContextKeyEventSink) *object = static_cast<ITfContextKeyEventSink*>(this);
  else if (riid == IID_ITfCompositionSink) *object = static_cast<ITfCompositionSink*>(this);
  else if (riid == IID_ITfActiveLanguageProfileNotifySink) *object = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
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
  threadManager_ = manager; threadManager_->AddRef(); clientId_ = id;
  preferences_ = akshara::preferences::Load();
  const auto hr = AdviseSinks();
  if (FAILED(hr)) Deactivate();
  // The profile may already be active before this service subscribes to the
  // profile-notification sink. Read it once so all three Akshara profiles use
  // their own rule engine on the first keypress.
  if (SUCCEEDED(hr)) {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles)))) {
      TF_INPUTPROCESSORPROFILE active{};
      profileActive_ = SUCCEEDED(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active)) &&
                       IsEqualCLSID(active.clsid, CLSID_AksharaTextService);
      if (profileActive_) SelectProfile(active.guidProfile);
      profiles->Release();
    }
  }
  return hr;
}
HRESULT TextService::Deactivate() {
  if (!threadManager_) return S_OK;
  ITfDocumentMgr* document = nullptr; ITfContext* context = nullptr;
  if (SUCCEEDED(threadManager_->GetFocus(&document)) && document) document->GetTop(&context);
  if (context) { RequestEdit(context, true); context->Release(); }
  release(document);
  UnadviseSinks(); ResetComposition(); buffer_.clear(); clientId_ = TF_CLIENTID_NULL; release(threadManager_);
  return S_OK;
}
HRESULT TextService::AdviseSinks() {
  ITfSource* source = nullptr;
  auto hr = threadManager_->QueryInterface(IID_PPV_ARGS(&source));
  if (FAILED(hr)) return hr;
  hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadSinkCookie_);
  if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink, static_cast<ITfActiveLanguageProfileNotifySink*>(this), &profileSinkCookie_);
  source->Release();
  if (FAILED(hr)) return hr;
  ITfDocumentMgr* focused = nullptr;
  if (SUCCEEDED(threadManager_->GetFocus(&focused)) && focused) {
    hr = AdviseFocusedContext(focused);
    focused->Release();
    if (FAILED(hr)) return hr;
  }
  ITfKeystrokeMgr* keys = nullptr;
  hr = threadManager_->QueryInterface(IID_PPV_ARGS(&keys));
  if (FAILED(hr)) return hr;
  hr = keys->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
  keys->Release();
  InterlockedExchange(&g_tsfDiagnostics.keySinkAdviceResult, static_cast<LONG>(hr));
  if (FAILED(hr)) return hr;
  // RDP on this Windows 10 host delivers physical keys to the TSF trace
  // stage but bypasses both ITfKeyEventSink and ITfContextKeyEventSink. A
  // thread-only hook is a narrow compatibility bridge: it is installed only
  // inside the host process that already loaded this active TIP, and it eats
  // only the keys that the normal TSF key sink would eat.
  return InstallKeyboardFallback();
}

HRESULT TextService::InstallKeyboardFallback() {
  if (keyboardHook_) return S_OK;
  keyHookService = this;
  keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD, KeyboardHookProc, g_module, GetCurrentThreadId());
  if (!keyboardHook_) {
    keyHookService = nullptr;
    // Keep the standard TSF route available on hosts that disallow a thread
    // hook; failure here must not deactivate the text service.
    return S_OK;
  }
  return S_OK;
}

void TextService::RemoveKeyboardFallback() {
  if (keyboardHook_) UnhookWindowsHookEx(keyboardHook_);
  keyboardHook_ = nullptr;
  if (keyHookService == this) keyHookService = nullptr;
}

LRESULT CALLBACK TextService::KeyboardHookProc(int code, WPARAM key, LPARAM flags) {
  if (code < 0) return CallNextHookEx(nullptr, code, key, flags);
  auto* service = keyHookService;
  if (!service || (flags & 0x80000000) != 0) return CallNextHookEx(nullptr, code, key, flags);
  return service->HandleKeyboardHook(key, flags) ? 1 : CallNextHookEx(nullptr, code, key, flags);
}

bool TextService::HandleKeyboardHook(WPARAM key, LPARAM) {
  if (!profileActive_ || IsKeyboardDisabled() || !IsContextWritable(contextKeyContext_)) return false;
  if (IsHandledKey(key)) {
    return HandleKey(contextKeyContext_, key);
  }
  if (!buffer_.empty() && ShouldCommitOnBoundary(key)) RequestEdit(contextKeyContext_, true);
  return false;
}
HRESULT TextService::AdviseFocusedContext(ITfDocumentMgr* document) {
  UnadviseFocusedContext();
  if (!document) return S_OK;
  ITfContext* context = nullptr;
  const auto getHr = document->GetTop(&context);
  if (FAILED(getHr) || !context) return FAILED(getHr) ? getHr : S_OK;
  ITfSource* source = nullptr;
  const auto sourceHr = context->QueryInterface(IID_PPV_ARGS(&source));
  if (FAILED(sourceHr)) { context->Release(); return sourceHr; }
  const auto adviseHr = source->AdviseSink(IID_ITfContextKeyEventSink,
      static_cast<ITfContextKeyEventSink*>(this), &contextKeyCookie_);
  source->Release();
  if (FAILED(adviseHr)) { context->Release(); contextKeyCookie_ = TF_INVALID_COOKIE; return adviseHr; }
  contextKeyContext_ = context;
  return S_OK;
}
void TextService::UnadviseFocusedContext() {
  if (contextKeyContext_ && contextKeyCookie_ != TF_INVALID_COOKIE) {
    ITfSource* source = nullptr;
    if (SUCCEEDED(contextKeyContext_->QueryInterface(IID_PPV_ARGS(&source)))) {
      source->UnadviseSink(contextKeyCookie_);
      source->Release();
    }
  }
  contextKeyCookie_ = TF_INVALID_COOKIE;
  release(contextKeyContext_);
}
void TextService::UnadviseSinks() {
  if (!threadManager_) return;
  RemoveKeyboardFallback();
  UnadviseFocusedContext();
  ITfKeystrokeMgr* keys = nullptr;
  if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&keys)))) { keys->UnadviseKeyEventSink(clientId_); keys->Release(); }
  ITfSource* source = nullptr;
  if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&source)))) {
    if (threadSinkCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(threadSinkCookie_);
    if (profileSinkCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(profileSinkCookie_);
    source->Release();
  }
  threadSinkCookie_ = profileSinkCookie_ = TF_INVALID_COOKIE;
}
bool TextService::IsKeyboardDisabled() {
  ITfCompartmentMgr* compartments = nullptr;
  if (FAILED(threadManager_->QueryInterface(IID_PPV_ARGS(&compartments)))) return true;
  ITfCompartment* disabled = nullptr;
  const auto hr = compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED, &disabled);
  compartments->Release();
  if (FAILED(hr) || !disabled) return false;
  VARIANT value{}; VariantInit(&value);
  const bool blocked = SUCCEEDED(disabled->GetValue(&value)) && value.vt == VT_I4 && value.lVal != 0;
  VariantClear(&value); disabled->Release();
  return blocked;
}
bool TextService::IsContextWritable(ITfContext* context) {
  TF_STATUS status{};
  return context && SUCCEEDED(context->GetStatus(&status)) && (status.dwDynamicFlags & TS_SD_READONLY) == 0;
}
std::optional<char16_t> TextService::TranslateKey(WPARAM key) const {
  // Remote Desktop can forward text as VK_PACKET rather than as the physical
  // A-Z virtual keys. This is the same ToUnicode conversion used by
  // Microsoft's SampleIME KeyEventSink before it decides whether to eat a key.
  const auto virtualKey = static_cast<UINT>(key);
  const auto scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
  BYTE state[256]{};
  if (!GetKeyboardState(state)) return std::nullopt;
  WCHAR character{};
  if (ToUnicode(virtualKey, scanCode, state, &character, 1, 0) == 1) return static_cast<char16_t>(character);
  return std::nullopt;
}
bool TextService::IsHandledKey(WPARAM key) const {
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  const bool altGr = ctrl && alt && (GetKeyState(VK_RMENU) & 0x8000) != 0;
  if ((ctrl || alt) && !(buffer_.mode() == akshara::InputMode::Wijesekara && altGr)) return false;
  if (key == VK_BACK) return !buffer_.empty();
  if (isBoundary(key)) return false;
  if (buffer_.mode() == akshara::InputMode::Wijesekara)
    return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || isOem(key) || key == VK_PACKET;
  const auto character = TranslateKey(key);
  return character && ((*character >= u'a' && *character <= u'z') || (*character >= u'A' && *character <= u'Z'));
}
bool TextService::ShouldCommitOnBoundary(WPARAM key) const {
  if (key == VK_RETURN) return preferences_.commitOnEnter;
  if (key == VK_TAB) return preferences_.commitOnTab;
  if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT) return preferences_.commitOnCursorMovement;
  return isPunctuation(key) && preferences_.commitOnPunctuation;
}
HRESULT TextService::OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.testKeyDownCalls);
  const bool writable = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(context);
  InterlockedExchange(&g_tsfDiagnostics.lastContextWasWritable, writable);
  *eaten = writable && IsHandledKey(key);
  if (!*eaten && !buffer_.empty() && ShouldCommitOnBoundary(key)) RequestEdit(context, true);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.keyDownCalls);
  *eaten = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(context) && HandleKey(context, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnTestKeyDown(WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.testKeyDownCalls);
  *eaten = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(contextKeyContext_) && IsHandledKey(key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnKeyDown(WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.keyDownCalls);
  *eaten = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(contextKeyContext_) && HandleKey(contextKeyContext_, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnTestKeyUp(WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
bool TextService::HandleKey(ITfContext* context, WPARAM key) {
  if (!IsHandledKey(key)) return false;
  if (key == VK_BACK) { buffer_.backspace(); RequestEdit(context, false); return true; }
  if (buffer_.mode() == akshara::InputMode::Wijesekara) {
    const auto translated = TranslateKey(key);
    const bool shift = key == VK_PACKET && translated ? (*translated >= u'A' && *translated <= u'Z') : (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool altGr = (GetKeyState(VK_CONTROL) & 0x8000) != 0 && (GetKeyState(VK_RMENU) & 0x8000) != 0;
    const auto virtualKey = key == VK_PACKET && translated
        ? static_cast<std::uint32_t>((*translated >= u'a' && *translated <= u'z') ? *translated - u'a' + u'A' : *translated)
        : static_cast<std::uint32_t>(key);
    const auto input = engine_.mapWijesekaraKey({virtualKey, shift, altGr});
    if (input.empty()) return false;
    buffer_.append(input);
  } else {
    const auto character = TranslateKey(key);
    if (!character) return false;
    buffer_.append(std::u16string(1, *character));
  }
  RequestEdit(context, false);
  return true;
}
HRESULT TextService::RequestEdit(ITfContext* context, bool commit) {
  if (!context) return E_INVALIDARG;
  auto* session = new (std::nothrow) EditSession(this, context, commit);
  if (!session) return E_OUTOFMEMORY;
  HRESULT sessionResult = E_FAIL;
  const auto hr = context->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &sessionResult);
  session->Release();
  return FAILED(hr) ? hr : sessionResult;
}
HRESULT TextService::ApplyEdit(ITfContext* context, TfEditCookie cookie, bool commit) {
  if (commit) {
    if (composition_) composition_->EndComposition(cookie);
    ResetComposition(); buffer_.clear();
    return S_OK;
  }
  const auto rendered = buffer_.render(engine_);
  ITfRange* range = nullptr;
  if (!composition_) {
    ITfInsertAtSelection* insert = nullptr;
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&insert)))) return E_NOINTERFACE;
    const auto queryHr = insert->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, nullptr, 0, &range);
    insert->Release();
    if (FAILED(queryHr) || !range) return queryHr;
    ITfContextComposition* compositions = nullptr;
    const auto interfaceHr = context->QueryInterface(IID_PPV_ARGS(&compositions));
    if (FAILED(interfaceHr)) { range->Release(); return interfaceHr; }
    const auto startHr = compositions->StartComposition(cookie, range, this, &composition_);
    compositions->Release();
    if (FAILED(startHr)) { range->Release(); return startHr; }
  } else if (FAILED(composition_->GetRange(&range))) return E_FAIL;
  const auto textHr = range->SetText(cookie, 0, reinterpret_cast<const WCHAR*>(rendered.text.data()), static_cast<LONG>(rendered.text.size()));
  if (SUCCEEDED(textHr)) {
    range->Collapse(cookie, TF_ANCHOR_END);
    TF_SELECTION selection{}; selection.range = range; selection.style.ase = TF_AE_NONE; selection.style.fInterimChar = FALSE;
    context->SetSelection(cookie, 1, &selection);
  }
  range->Release();
  return textHr;
}
void TextService::ResetComposition() { release(composition_); }
HRESULT TextService::OnCompositionTerminated(TfEditCookie, ITfComposition*) { ResetComposition(); buffer_.clear(); return S_OK; }
HRESULT TextService::OnSetFocus(BOOL) { return S_OK; }
HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnPushContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnPopContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) {
  const auto hr = AdviseFocusedContext(focus);
  if (!previous) { ResetComposition(); buffer_.clear(); }
  return hr;
}
void TextService::SelectProfile(REFGUID profile) {
  if (profile == GUID_PROFILE_AKSHARA_SMART_PHONETIC) buffer_.setMode(akshara::InputMode::SmartPhonetic);
  else if (profile == GUID_PROFILE_AKSHARA_PHONETIC) buffer_.setMode(akshara::InputMode::Phonetic);
  else if (profile == GUID_PROFILE_AKSHARA_WIJESEKARA) buffer_.setMode(akshara::InputMode::Wijesekara);
}
HRESULT TextService::OnActivated(REFCLSID clsid, REFGUID profile, BOOL active) {
  if (active) {
    profileActive_ = clsid == CLSID_AksharaTextService;
    if (profileActive_) SelectProfile(profile);
    else { ResetComposition(); buffer_.clear(); }
  } else if (clsid == CLSID_AksharaTextService) {
    profileActive_ = false;
    ResetComposition(); buffer_.clear();
  }
  return S_OK;
}
