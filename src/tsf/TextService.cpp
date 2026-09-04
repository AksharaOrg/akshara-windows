#include "TextService.h"
#include "EditSession.h"
#include "Globals.h"

#include <new>

namespace {
template<class T> void ComRelease(T*& value) { if (value) { value->Release(); value = nullptr; } }

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
  else if (riid == IID_ITfActiveLanguageProfileNotifySink) *object = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
  if (!*object) return E_NOINTERFACE;
  AddRef(); return S_OK;
}
ULONG TextService::AddRef() { return ++refs_; }
ULONG TextService::Release() { const auto count = --refs_; if (!count) delete this; return count; }
HRESULT TextService::Activate(ITfThreadMgr* manager, TfClientId id) { return ActivateEx(manager, id, 0); }
HRESULT TextService::ActivateEx(ITfThreadMgr* manager, TfClientId id, DWORD) {
  if (!manager || threadManager_) return E_INVALIDARG;
  threadManager_ = manager; manager->AddRef(); clientId_ = id;
  if (AdviseSinks()) return S_OK;
  Deactivate();
  return E_FAIL;
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
bool TextService::AdviseSinks() {
  ITfKeystrokeMgr* keys = nullptr;
  if (FAILED(threadManager_->QueryInterface(IID_PPV_ARGS(&keys)))) return false;
  const auto keyHr = keys->AdviseKeyEventSink(clientId_, this, TRUE); keys->Release();
  if (FAILED(keyHr)) return false;

  // Keystroke delivery is the only sink required for basic IME operation.
  // Profile and thread notifications only improve composition cleanup and mode
  // selection; a failure to subscribe to either must not turn the IME into an
  // inert keyboard that passes every key through as Latin text.
  ITfSource* source = nullptr;
  if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&source)))) {
    if (FAILED(source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadSinkCookie_))) {
      threadSinkCookie_ = TF_INVALID_COOKIE;
    }
    if (FAILED(source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink, static_cast<ITfActiveLanguageProfileNotifySink*>(this), &profileSinkCookie_))) {
      profileSinkCookie_ = TF_INVALID_COOKIE;
    }
    source->Release();
  }
  return true;
}
void TextService::UnadviseSinks() {
  if (!threadManager_) return;
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
  if (!eaten) return E_POINTER; *eaten = ShouldEatKey(context, key); return S_OK;
}
HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = FALSE; return S_OK; }
HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) {
  if (!eaten) return E_POINTER;
  *eaten = HandleKey(context, key);
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
HRESULT TextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr* previous) {
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
