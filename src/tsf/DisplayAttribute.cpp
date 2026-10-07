#include "DisplayAttribute.h"
#include "Globals.h"

#include <new>

DisplayAttributeInfo::DisplayAttributeInfo() { InterlockedIncrement(&g_objectCount); }
DisplayAttributeInfo::~DisplayAttributeInfo() { InterlockedDecrement(&g_objectCount); }
HRESULT DisplayAttributeInfo::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) *object = static_cast<ITfDisplayAttributeInfo*>(this);
  if (!*object) return E_NOINTERFACE;
  AddRef(); return S_OK;
}
ULONG DisplayAttributeInfo::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG DisplayAttributeInfo::Release() { const auto n = InterlockedDecrement(&refs_); if (!n) delete this; return static_cast<ULONG>(n); }
HRESULT DisplayAttributeInfo::GetGUID(GUID* guid) {
  if (!guid) return E_INVALIDARG;
  *guid = GUID_AKSHARA_DISPLAY_ATTRIBUTE_INPUT; return S_OK;
}
HRESULT DisplayAttributeInfo::GetDescription(BSTR* description) {
  if (!description) return E_INVALIDARG;
  *description = SysAllocString(L"Akshara input text");
  return *description ? S_OK : E_OUTOFMEMORY;
}
HRESULT DisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) {
  if (!attribute) return E_INVALIDARG;
  *attribute = {};
  attribute->crText.type = TF_CT_NONE;
  attribute->crBk.type = TF_CT_NONE;
  attribute->lsStyle = TF_LS_DOT;
  attribute->fBoldLine = FALSE;
  attribute->crLine.type = TF_CT_NONE;
  attribute->bAttr = TF_ATTR_INPUT;
  return S_OK;
}
HRESULT DisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) { return E_NOTIMPL; }
HRESULT DisplayAttributeInfo::Reset() { return S_OK; }

DisplayAttributeEnum::DisplayAttributeEnum() { InterlockedIncrement(&g_objectCount); }
DisplayAttributeEnum::~DisplayAttributeEnum() { InterlockedDecrement(&g_objectCount); }
HRESULT DisplayAttributeEnum::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) *object = static_cast<IEnumTfDisplayAttributeInfo*>(this);
  if (!*object) return E_NOINTERFACE;
  AddRef(); return S_OK;
}
ULONG DisplayAttributeEnum::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG DisplayAttributeEnum::Release() { const auto n = InterlockedDecrement(&refs_); if (!n) delete this; return static_cast<ULONG>(n); }
HRESULT DisplayAttributeEnum::Clone(IEnumTfDisplayAttributeInfo** clone) {
  if (!clone) return E_INVALIDARG;
  auto* copy = new (std::nothrow) DisplayAttributeEnum();
  if (!copy) { *clone = nullptr; return E_OUTOFMEMORY; }
  copy->index_ = index_;
  *clone = copy; return S_OK;
}
HRESULT DisplayAttributeEnum::Next(ULONG count, ITfDisplayAttributeInfo** info, ULONG* fetched) {
  if (fetched) *fetched = 0;
  if (!info || (count > 1 && !fetched)) return E_INVALIDARG;
  ULONG done = 0;
  // One attribute: the input text.
  if (count > 0 && index_ == 0) {
    auto* attribute = new (std::nothrow) DisplayAttributeInfo();
    if (!attribute) return E_OUTOFMEMORY;
    info[0] = attribute;
    ++index_; ++done;
  }
  if (fetched) *fetched = done;
  return done == count ? S_OK : S_FALSE;
}
HRESULT DisplayAttributeEnum::Reset() { index_ = 0; return S_OK; }
HRESULT DisplayAttributeEnum::Skip(ULONG count) {
  const bool all = count > 0 && index_ == 0;
  if (count > 0) index_ = 1;
  return all && count == 1 ? S_OK : (count == 0 ? S_OK : S_FALSE);
}
