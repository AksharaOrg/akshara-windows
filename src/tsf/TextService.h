#pragma once

#include "AksharaEngine.h"
#include "../common/AksharaPreferences.h"
#include <windows.h>
#include <msctf.h>
#include <atomic>
#include <optional>
#include <string_view>

// Minimal composition-only TIP, following the relevant SampleIME interfaces.
class TextService final : public ITfTextInputProcessorEx,
                          public ITfThreadMgrEventSink,
                          public ITfKeyEventSink,
                          public ITfContextKeyEventSink,
                          public ITfCompositionSink,
                          public ITfActiveLanguageProfileNotifySink {
 public:
  TextService();
  ~TextService();
  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP Activate(ITfThreadMgr* manager, TfClientId clientId) override;
  STDMETHODIMP ActivateEx(ITfThreadMgr* manager, TfClientId clientId, DWORD flags) override;
  STDMETHODIMP Deactivate() override;
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override;
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override;
  STDMETHODIMP OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override;
  STDMETHODIMP OnPushContext(ITfContext*) override;
  STDMETHODIMP OnPopContext(ITfContext*) override;
  STDMETHODIMP OnSetFocus(BOOL foreground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnKeyDown(ITfContext*, WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL*) override;
  STDMETHODIMP OnTestKeyDown(WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnKeyDown(WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnTestKeyUp(WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnKeyUp(WPARAM, LPARAM, BOOL*) override;
  STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition*) override;
  STDMETHODIMP OnActivated(REFCLSID, REFGUID, BOOL) override;
  HRESULT ApplyEdit(ITfContext* context, TfEditCookie cookie, bool commit, std::u16string_view commitSuffix = {});

 private:
  HRESULT AdviseSinks();
  void UnadviseSinks();
  HRESULT AdviseFocusedContext(ITfDocumentMgr* document);
  void UnadviseFocusedContext();
  HRESULT InstallKeyboardFallback();
  void RemoveKeyboardFallback();
  bool HandleKeyboardHook(WPARAM key, LPARAM flags);
  static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM key, LPARAM flags);
  bool IsKeyboardDisabled();
  static bool IsContextWritable(ITfContext* context);
  [[nodiscard]] std::optional<char16_t> TranslateKey(WPARAM key) const;
  bool IsHandledKey(WPARAM key) const;
  bool HandleKey(ITfContext* context, WPARAM key);
  bool ShouldCommitOnBoundary(WPARAM key) const;
  HRESULT RequestEdit(ITfContext* context, bool commit, std::u16string_view commitSuffix = {});
  void ResetComposition();
  void SelectProfile(REFGUID profile);
  std::atomic<ULONG> refs_{1};
  ITfThreadMgr* threadManager_{};
  TfClientId clientId_{TF_CLIENTID_NULL};
  DWORD threadSinkCookie_{TF_INVALID_COOKIE};
  DWORD profileSinkCookie_{TF_INVALID_COOKIE};
  HHOOK keyboardHook_{};
  bool profileActive_{};
  ITfContext* contextKeyContext_{};
  DWORD contextKeyCookie_{TF_INVALID_COOKIE};
  ITfComposition* composition_{};
  akshara::AksharaEngine engine_;
  akshara::CompositionBuffer buffer_{akshara::InputMode::SmartPhonetic};
  akshara::preferences::Values preferences_{};
};
