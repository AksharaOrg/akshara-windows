#include "TextService.h"
#include "DisplayAttribute.h"
#include "EditSession.h"
#include "Globals.h"
#include "Utf.h"
#include "WordList.h"

#include <cwctype>
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
bool isModifier(WPARAM key) {
  switch (key) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU: case VK_CAPITAL: case VK_LWIN: case VK_RWIN: return true;
    default: return false;
  }
}
// Two Spaces this close together type ". " (macOS uses the same half second).
constexpr DWORD kDoubleSpaceMs = 500;
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
  else if (riid == IID_ITfDisplayAttributeProvider) *object = static_cast<ITfDisplayAttributeProvider*>(this);
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
  LoadPreferences();
  ITfCategoryMgr* categories = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories)))) {
    if (FAILED(categories->RegisterGUID(GUID_AKSHARA_DISPLAY_ATTRIBUTE_INPUT, &inputAttribute_))) inputAttribute_ = TF_INVALID_GUIDATOM;
    categories->Release();
  }
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
  if (context) { RequestEdit(context, {EditKind::Commit}); context->Release(); }
  release(document);
  UnadviseSinks(); ResetComposition(); buffer_.clear(); ClearPendingChoice(); clientId_ = TF_CLIENTID_NULL; release(threadManager_);
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
  BeforeKey(key);
  if (HandleKey(contextKeyContext_, key)) return true;
  if (key == VK_SPACE) NoteSpace();
  if (!buffer_.empty() && ShouldCommitOnBoundary(key)) RequestEdit(contextKeyContext_, {EditKind::Commit});
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
bool TextService::IsHandledKey(ITfContext* context, WPARAM key) {
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  const bool altGr = ctrl && alt && (GetKeyState(VK_RMENU) & 0x8000) != 0;
  if ((ctrl || alt) && !(buffer_.mode() == akshara::InputMode::Wijesekara && altGr)) return false;
  if (key == VK_BACK) return !buffer_.empty() || CanUndoChoice(context);
  // AltGr+Space is the public SLS 1134 ZWNJ entry. It must reach the
  // Wijesekara mapper before the ordinary Space boundary handling below.
  if (key == VK_SPACE && buffer_.mode() == akshara::InputMode::Wijesekara && altGr) return true;
  // Own an ordinary Space only while composing so the rendered word and its
  // trailing boundary can be committed atomically in one TSF edit session.
  if (key == VK_SPACE) return !buffer_.empty() || IsDoubleSpace(context);
  if (buffer_.mode() != akshara::InputMode::Wijesekara && !buffer_.empty() &&
      preferences_.commitOnPunctuation && isPunctuation(key)) return TranslateKey(key).has_value();
  if (isBoundary(key)) return false;
  if (buffer_.mode() == akshara::InputMode::Wijesekara)
    return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || isOem(key) || key == VK_PACKET;
  const auto character = TranslateKey(key);
  if (!character) return false;
  if ((*character >= u'a' && *character <= u'z') || (*character >= u'A' && *character <= u'Z')) return true;
  // v2's archaic letters are typed with ~ (~l, ~ll, ~n) and + joins touching letters.
  return V2Active() && preferences_.v2Archaic && (*character == u'~' || *character == u'+');
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
  if (writable) BeforeKey(key);
  *eaten = writable && IsHandledKey(context, key);
  if (!*eaten && writable && key == VK_SPACE) NoteSpace();
  if (!*eaten && !buffer_.empty() && ShouldCommitOnBoundary(key)) RequestEdit(context, {EditKind::Commit});
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.keyDownCalls);
  const bool writable = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(context);
  if (writable) BeforeKey(key);
  *eaten = writable && HandleKey(context, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnTestKeyDown(WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.testKeyDownCalls);
  const bool writable = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(contextKeyContext_);
  if (writable) BeforeKey(key);
  *eaten = writable && IsHandledKey(contextKeyContext_, key);
  if (!*eaten && writable && key == VK_SPACE) NoteSpace();
  if (!*eaten && writable && !buffer_.empty() && ShouldCommitOnBoundary(key)) RequestEdit(contextKeyContext_, {EditKind::Commit});
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnKeyDown(WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  InterlockedIncrement(&g_tsfDiagnostics.keyDownCalls);
  const bool writable = profileActive_ && !IsKeyboardDisabled() && IsContextWritable(contextKeyContext_);
  if (writable) BeforeKey(key);
  *eaten = writable && HandleKey(contextKeyContext_, key);
  InterlockedExchange(&g_tsfDiagnostics.lastKeyWasEaten, *eaten);
  return S_OK;
}
HRESULT TextService::OnTestKeyUp(WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
bool TextService::HandleKey(ITfContext* context, WPARAM key) {
  if (!IsHandledKey(context, key)) return false;
  if (key == VK_BACK) {
    if (buffer_.empty()) return UndoChoice(context);
    buffer_.backspace(); RequestEdit(context, {EditKind::Compose}); return true;
  }
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  const bool altGr = ctrl && alt && (GetKeyState(VK_RMENU) & 0x8000) != 0;
  if (key == VK_SPACE && !altGr) {
    if (buffer_.empty()) {   // the second of a double space: IsHandledKey checked the text before the caret
      doubleSpaceTime_ = GetMessageTime();
      lastSpaceTime_.reset();
      return RequestEdit(context, {EditKind::DoubleSpacePeriod}, true) == S_OK;
    }
    CommitWithSpace(context);
    NoteSpace();
    return true;
  }
  if (buffer_.mode() != akshara::InputMode::Wijesekara && preferences_.commitOnPunctuation && isPunctuation(key)) {
    const auto punctuation = TranslateKey(key);
    if (!punctuation) return false;
    RequestEdit(context, {EditKind::CommitText, Render() + std::u16string(1, *punctuation)});
    return true;
  }
  if (buffer_.mode() == akshara::InputMode::Wijesekara) {
    const auto translated = TranslateKey(key);
    const bool shift = key == VK_PACKET && translated ? (*translated >= u'A' && *translated <= u'Z') : (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool wijesekaraAltGr = (GetKeyState(VK_CONTROL) & 0x8000) != 0 && (GetKeyState(VK_RMENU) & 0x8000) != 0;
    const auto virtualKey = key == VK_PACKET && translated
        ? static_cast<std::uint32_t>((*translated >= u'a' && *translated <= u'z') ? *translated - u'a' + u'A' : *translated)
        : static_cast<std::uint32_t>(key);
    const auto input = engine_.mapWijesekaraKey({virtualKey, shift, wijesekaraAltGr});
    if (input.empty()) return false;
    buffer_.append(input);
  } else {
    const auto character = TranslateKey(key);
    if (!character) return false;
    buffer_.append(std::u16string(1, *character));
  }
  RequestEdit(context, {EditKind::Compose});
  return true;
}
HRESULT TextService::RequestEdit(ITfContext* context, EditRequest request, bool synchronous) {
  if (!context) return E_INVALIDARG;
  const bool readOnly = request.kind == EditKind::CheckTextBefore || request.kind == EditKind::CheckDoubleSpace;
  auto* session = new (std::nothrow) EditSession(this, context, std::move(request));
  if (!session) return E_OUTOFMEMORY;
  HRESULT sessionResult = E_FAIL;
  const DWORD flags = (synchronous ? TF_ES_SYNC : TF_ES_ASYNCDONTCARE) | (readOnly ? TF_ES_READ : TF_ES_READWRITE);
  const auto hr = context->RequestEditSession(clientId_, session, flags, &sessionResult);
  session->Release();
  return FAILED(hr) ? hr : sessionResult;
}
HRESULT TextService::ApplyEdit(ITfContext* context, TfEditCookie cookie, const EditRequest& request) {
  switch (request.kind) {
    case EditKind::Compose: return ComposeEdit(context, cookie);
    case EditKind::Commit: return CommitEdit(context, cookie, nullptr);
    case EditKind::CommitText: return CommitEdit(context, cookie, &request.text);
    default: return TextBeforeEdit(context, cookie, request);
  }
}
HRESULT TextService::CommitEdit(ITfContext* context, TfEditCookie cookie, const std::u16string* text) {
  ITfRange* range = nullptr;
  HRESULT textHr = S_OK;
  if (composition_) {
    textHr = composition_->GetRange(&range);
    if (SUCCEEDED(textHr) && !range) textHr = E_FAIL;
    if (SUCCEEDED(textHr)) {
      if (text) textHr = range->SetText(cookie, 0, reinterpret_cast<const WCHAR*>(text->data()), static_cast<LONG>(text->size()));
      SetInputAttribute(context, cookie, range, false);
      if (text && SUCCEEDED(textHr)) {
        range->Collapse(cookie, TF_ANCHOR_END);
        TF_SELECTION selection{}; selection.range = range; selection.style.ase = TF_AE_NONE; selection.style.fInterimChar = FALSE;
        context->SetSelection(cookie, 1, &selection);
      }
    }
  } else if (text && !text->empty()) {
    ITfInsertAtSelection* insert = nullptr;
    textHr = context->QueryInterface(IID_PPV_ARGS(&insert));
    if (SUCCEEDED(textHr)) {
      textHr = insert->InsertTextAtSelection(cookie, 0, reinterpret_cast<const WCHAR*>(text->data()), static_cast<LONG>(text->size()), &range);
      insert->Release();
    }
  }
  if (range) range->Release();
  if (FAILED(textHr)) return textHr;
  if (composition_) composition_->EndComposition(cookie);
  ResetComposition(); buffer_.clear();
  return S_OK;
}
HRESULT TextService::ComposeEdit(ITfContext* context, TfEditCookie cookie) {
  const auto rendered = Render();
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
  const auto textHr = range->SetText(cookie, 0, reinterpret_cast<const WCHAR*>(rendered.data()), static_cast<LONG>(rendered.size()));
  if (SUCCEEDED(textHr)) {
    SetInputAttribute(context, cookie, range, true);
    range->Collapse(cookie, TF_ANCHOR_END);
    TF_SELECTION selection{}; selection.range = range; selection.style.ase = TF_AE_NONE; selection.style.fInterimChar = FALSE;
    context->SetSelection(cookie, 1, &selection);
  }
  range->Release();
  return textHr;
}
// The checks and replacements just before an empty selection: undoing a Space choice and the double-space
// period. Nothing is composed while they run. S_FALSE: the text isn't there, and nothing changed.
HRESULT TextService::TextBeforeEdit(ITfContext* context, TfEditCookie cookie, const EditRequest& request) {
  const bool doubleSpace = request.kind == EditKind::CheckDoubleSpace || request.kind == EditKind::DoubleSpacePeriod;
  const auto& expected = request.kind == EditKind::ReplaceTextBefore ? request.expected : request.text;
  const LONG length = doubleSpace ? 2 : static_cast<LONG>(expected.size());
  if (length <= 0 || length > 256 || composition_) return S_FALSE;
  TF_SELECTION selection{}; ULONG fetched = 0;
  if (FAILED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched != 1 || !selection.range) return S_FALSE;
  ITfRange* before = nullptr;
  BOOL empty = FALSE;
  LONG moved = 0;
  if (FAILED(selection.range->IsEmpty(cookie, &empty)) || !empty || FAILED(selection.range->Clone(&before)) ||
      FAILED(before->ShiftStart(cookie, -length, &moved, nullptr)) || moved != -length) {
    if (before) before->Release();
    selection.range->Release();
    return S_FALSE;
  }
  selection.range->Release();
  std::u16string text(static_cast<std::size_t>(length), u'\0');
  ULONG read = 0;
  HRESULT hr = before->GetText(cookie, 0, reinterpret_cast<WCHAR*>(text.data()), static_cast<ULONG>(length), &read);
  text.resize(SUCCEEDED(hr) ? read : 0);
  const bool found = doubleSpace ? text.size() == 2 && text[1] == u' ' && !std::iswspace(static_cast<wint_t>(text[0]))
                                 : text == expected;
  hr = found ? S_OK : S_FALSE;
  if (found && (request.kind == EditKind::ReplaceTextBefore || request.kind == EditKind::DoubleSpacePeriod)) {
    if (doubleSpace) before->ShiftStart(cookie, 1, &moved, nullptr);   // keep the word; replace the space
    const std::u16string replacement = doubleSpace ? u". " : request.text;
    hr = before->SetText(cookie, 0, reinterpret_cast<const WCHAR*>(replacement.data()), static_cast<LONG>(replacement.size()));
    if (SUCCEEDED(hr)) {
      before->Collapse(cookie, TF_ANCHOR_END);
      TF_SELECTION caret{}; caret.range = before; caret.style.ase = TF_AE_NONE; caret.style.fInterimChar = FALSE;
      context->SetSelection(cookie, 1, &caret);
      hr = S_OK;
    }
  }
  before->Release();
  return hr;
}
// The dotted underline under the word being composed (DisplayAttribute.h).
void TextService::SetInputAttribute(ITfContext* context, TfEditCookie cookie, ITfRange* range, bool on) {
  if (inputAttribute_ == TF_INVALID_GUIDATOM) return;
  ITfProperty* property = nullptr;
  if (FAILED(context->GetProperty(GUID_PROP_ATTRIBUTE, &property)) || !property) return;
  if (on) {
    VARIANT value{}; VariantInit(&value);
    value.vt = VT_I4; value.lVal = static_cast<LONG>(inputAttribute_);
    property->SetValue(cookie, range, &value);
  } else {
    property->Clear(cookie, range);
  }
  property->Release();
}
void TextService::ResetComposition() { release(composition_); }
HRESULT TextService::OnCompositionTerminated(TfEditCookie, ITfComposition*) { ResetComposition(); buffer_.clear(); return S_OK; }
HRESULT TextService::OnSetFocus(BOOL foreground) {
  if (foreground) LoadPreferences();
  return S_OK;
}
HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
HRESULT TextService::OnPushContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnPopContext(ITfContext*) { return S_OK; }
HRESULT TextService::OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) {
  LoadPreferences();
  ClearPendingChoice();
  lastSpaceTime_.reset();
  const auto hr = AdviseFocusedContext(focus);
  if (!previous) { ResetComposition(); buffer_.clear(); }
  return hr;
}
void TextService::SelectProfile(REFGUID profile) {
  if (profile == GUID_PROFILE_AKSHARA_SMART_PHONETIC) buffer_.setMode(akshara::InputMode::SmartPhonetic);
  else if (profile == GUID_PROFILE_AKSHARA_PHONETIC) buffer_.setMode(akshara::InputMode::Phonetic);
  else if (profile == GUID_PROFILE_AKSHARA_WIJESEKARA) buffer_.setMode(akshara::InputMode::Wijesekara);
  ClearPendingChoice();
  RefreshWordList();
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

HRESULT TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** attributes) {
  if (!attributes) return E_INVALIDARG;
  *attributes = new (std::nothrow) DisplayAttributeEnum();
  return *attributes ? S_OK : E_OUTOFMEMORY;
}
HRESULT TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** attribute) {
  if (!attribute) return E_INVALIDARG;
  *attribute = nullptr;
  if (guid != GUID_AKSHARA_DISPLAY_ATTRIBUTE_INPUT) return E_INVALIDARG;
  *attribute = new (std::nothrow) DisplayAttributeInfo();
  return *attribute ? S_OK : E_OUTOFMEMORY;
}

// Grammar-correct Smart Phonetic (v2): SmartPhoneticV2.cpp, on by default for the Smart Phonetic profile.
bool TextService::V2Active() const {
  return buffer_.mode() == akshara::InputMode::SmartPhonetic && preferences_.smartPhoneticV2;
}
std::u16string TextService::Render() const {
  if (V2Active()) return akshara::utf::toUtf16(smart_.transliterate(akshara::utf::fromUtf16(buffer_.raw())));
  return buffer_.render(engine_).text;
}
void TextService::LoadPreferences() {
  preferences_ = akshara::preferences::Load();
  smart_.options = {preferences_.v2Archaic, preferences_.v2RepayaZwj, preferences_.v2Classical, preferences_.v2RakaransayaU,
                    preferences_.v2RetroflexD};
  RefreshWordList();
}
void TextService::RefreshWordList() {
  if (!V2Active() || smart_.isLoaded()) return;
  akshara::wordlist::LoadAsync();
  if (auto lexicon = akshara::wordlist::Get()) smart_.setLexicon(std::move(lexicon));
}
// Space commits the dictionary spelling of the word (හොඳ for "honda", which the rules spell හොන්ද) and a
// space, unless it is already spelled that way or the user put back this spelling before. Backspace right
// after puts back what was typed.
void TextService::CommitWithSpace(ITfContext* context) {
  const auto typed = Render();
  auto text = typed;
  if (V2Active()) {
    RefreshWordList();
    if (const auto choice = smart_.choice(akshara::utf::fromUtf16(buffer_.raw()))) {
      auto word = akshara::utf::toUtf16(*choice);
      if (word != typed && typed != rejectedChoice_) {
        text = std::move(word);
        pendingOriginal_ = typed;
        pendingReplacement_ = text + u" ";
      }
    }
  }
  RequestEdit(context, {EditKind::CommitText, text + u" "});
}
// Only while the choice and its space are still right before the caret.
bool TextService::CanUndoChoice(ITfContext* context) {
  return buffer_.empty() && !pendingReplacement_.empty() &&
         RequestEdit(context, {EditKind::CheckTextBefore, pendingReplacement_}, true) == S_OK;
}
bool TextService::UndoChoice(ITfContext* context) {
  const auto original = pendingOriginal_;
  const auto replacement = pendingReplacement_;
  ClearPendingChoice();
  if (RequestEdit(context, {EditKind::ReplaceTextBefore, original, replacement}, true) != S_OK) return false;
  rejectedChoice_ = original;
  return true;
}
// A Space soon after another one (not the same keystroke), right after a space that follows text.
bool TextService::IsDoubleSpace(ITfContext* context) {
  if (!buffer_.empty() || !preferences_.doubleSpacePeriod || !lastSpaceTime_) return false;
  const LONG now = GetMessageTime();
  if (now == *lastSpaceTime_ || static_cast<DWORD>(now - *lastSpaceTime_) >= kDoubleSpaceMs) return false;
  return RequestEdit(context, {EditKind::CheckDoubleSpace}, true) == S_OK;
}
// Called for every key before it is handled; TSF may ask about the same key more than once.
void TextService::BeforeKey(WPARAM key) {
  if (isModifier(key)) return;
  if (key != VK_BACK) ClearPendingChoice();   // only the Backspace right after a Space choice undoes it
  if (key != VK_SPACE && key != VK_BACK) lastSpaceTime_.reset();
}
void TextService::NoteSpace() {
  const LONG now = GetMessageTime();
  if (doubleSpaceTime_ && *doubleSpaceTime_ == now) return;   // this Space typed the period
  lastSpaceTime_ = now;
}
void TextService::ClearPendingChoice() {
  pendingOriginal_.clear();
  pendingReplacement_.clear();
}
