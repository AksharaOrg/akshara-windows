#pragma once

#include "AksharaEngine.h"
#include <windows.h>
#include <msctf.h>
#include <atomic>

class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfCompositionSink,
                          public ITfThreadMgrEventSink,
                          public ITfThreadFocusSink,
                          public ITfActiveLanguageProfileNotifySink,
                          public ITfTextEditSink,
                          public ITfCompartmentEventSink {
 public:
  TextService();
  ~TextService();

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP Activate(ITfThreadMgr* threadManager, TfClientId clientId) override;
  STDMETHODIMP Deactivate() override;
  STDMETHODIMP ActivateEx(ITfThreadMgr* threadManager, TfClientId clientId, DWORD flags) override;

  STDMETHODIMP OnSetFocus(BOOL foreground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override;
  STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition*) override;
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override;
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override;
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* previous) override;
  STDMETHODIMP OnPushContext(ITfContext*) override;
  STDMETHODIMP OnPopContext(ITfContext*) override;
  STDMETHODIMP OnSetThreadFocus() override;
  STDMETHODIMP OnKillThreadFocus() override;
  STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID profile, BOOL activated) override;
  STDMETHODIMP OnEndEdit(ITfContext* context, TfEditCookie readOnlyCookie, ITfEditRecord* editRecord) override;
  STDMETHODIMP OnChange(REFGUID guid) override;

  HRESULT ApplyEdit(ITfContext* context, TfEditCookie cookie, bool commitOnly);

 private:
  HRESULT AdviseSinks();
  HRESULT LogKeySinkForeground();
  HRESULT AdviseFocusedContext(ITfDocumentMgr* document);
  HRESULT AdviseKeyboardOpenCompartment();
  void UnadviseFocusedContext();
  void UnadviseSinks();
  bool ShouldEatKey(ITfContext* context, WPARAM key) const;
  bool IsContextWritable(ITfContext* context) const;
  bool HandleKey(ITfContext* context, WPARAM key);
  HRESULT RequestEdit(ITfContext* context, bool commitOnly);
  void ResetComposition();
  void SelectProfile(REFGUID profile);

  std::atomic<ULONG> refs_{1};
  ITfThreadMgr* threadManager_{};
  TfClientId clientId_{TF_CLIENTID_NULL};
  DWORD threadSinkCookie_{TF_INVALID_COOKIE};
  DWORD threadFocusSinkCookie_{TF_INVALID_COOKIE};
  DWORD profileSinkCookie_{TF_INVALID_COOKIE};
  DWORD textEditSinkCookie_{TF_INVALID_COOKIE};
  DWORD keyboardOpenSinkCookie_{TF_INVALID_COOKIE};
  ITfContext* textEditContext_{};
  ITfCompartment* keyboardOpenCompartment_{};
  bool keyboardOpen_{true};
  ITfComposition* composition_{};
  akshara::AksharaEngine engine_;
  akshara::CompositionBuffer buffer_{akshara::InputMode::SmartPhonetic};
};
