#pragma once
#include <windows.h>
#include <msctf.h>

class TextService;

class EditSession final : public ITfEditSession {
 public:
  EditSession(TextService* service, ITfContext* context, bool commitOnly);
  ~EditSession();
  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP DoEditSession(TfEditCookie cookie) override;
 private:
  long refs_{1};
  TextService* service_;
  ITfContext* context_;
  bool commitOnly_;
};
