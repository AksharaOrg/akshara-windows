#include "EditSession.h"
#include "TextService.h"

EditSession::EditSession(TextService* service, ITfContext* context, bool commitOnly)
    : service_(service), context_(context), commitOnly_(commitOnly) {
  service_->AddRef(); context_->AddRef();
}
EditSession::~EditSession() { context_->Release(); service_->Release(); }
HRESULT EditSession::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfEditSession) *object = static_cast<ITfEditSession*>(this);
  if (!*object) return E_NOINTERFACE;
  AddRef(); return S_OK;
}
ULONG EditSession::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG EditSession::Release() { const auto count = InterlockedDecrement(&refs_); if (!count) delete this; return static_cast<ULONG>(count); }
HRESULT EditSession::DoEditSession(TfEditCookie cookie) { return service_->ApplyEdit(context_, cookie, commitOnly_); }

