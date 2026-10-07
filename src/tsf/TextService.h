#pragma once

#include "AksharaEngine.h"
#include "EditSession.h"
#include "SmartPhoneticService.h"
#include "../common/AksharaPreferences.h"
#include <windows.h>
#include <msctf.h>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>

// Composition TIP, following the relevant SampleIME interfaces.
class TextService final : public ITfTextInputProcessorEx,
                          public ITfThreadMgrEventSink,
                          public ITfKeyEventSink,
                          public ITfContextKeyEventSink,
                          public ITfCompositionSink,
                          public ITfActiveLanguageProfileNotifySink,
                          public ITfDisplayAttributeProvider {
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
  STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** attributes) override;
  STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** attribute) override;
  HRESULT ApplyEdit(ITfContext* context, TfEditCookie cookie, const EditRequest& request);

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
  bool IsHandledKey(ITfContext* context, WPARAM key);
  bool HandleKey(ITfContext* context, WPARAM key);
  bool ShouldCommitOnBoundary(WPARAM key) const;
  HRESULT RequestEdit(ITfContext* context, EditRequest request, bool synchronous = false);
  HRESULT ComposeEdit(ITfContext* context, TfEditCookie cookie);
  HRESULT CommitEdit(ITfContext* context, TfEditCookie cookie, const std::u16string* text);
  HRESULT TextBeforeEdit(ITfContext* context, TfEditCookie cookie, const EditRequest& request);
  void SetInputAttribute(ITfContext* context, TfEditCookie cookie, ITfRange* range, bool on);
  void ResetComposition();
  void SelectProfile(REFGUID profile);

  // Smart Phonetic v2
  [[nodiscard]] bool V2Active() const;
  [[nodiscard]] std::u16string Render() const;
  void LoadPreferences();
  void RefreshWordList();
  void CommitWithSpace(ITfContext* context);
  bool CanUndoChoice(ITfContext* context);
  bool UndoChoice(ITfContext* context);
  bool IsDoubleSpace(ITfContext* context);
  void BeforeKey(WPARAM key);
  void NoteSpace();
  void ClearPendingChoice();

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
  TfGuidAtom inputAttribute_{TF_INVALID_GUIDATOM};
  akshara::AksharaEngine engine_;
  akshara::CompositionBuffer buffer_{akshara::InputMode::SmartPhonetic};
  akshara::preferences::Values preferences_{};
  akshara::SmartPhoneticService smart_;
  // The last Space choice, until the next key: Backspace puts back what was typed (as on Android and macOS).
  std::u16string pendingOriginal_;
  std::u16string pendingReplacement_;
  // A spelling the user put back: Space keeps it from then on.
  std::u16string rejectedChoice_;
  // Message times of the last Space and of the last double-space period.
  std::optional<LONG> lastSpaceTime_;
  std::optional<LONG> doubleSpaceTime_;
};
