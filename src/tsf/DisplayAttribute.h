#pragma once
#include <windows.h>
#include <msctf.h>

// The look of the word being composed: a dotted underline, as Microsoft's SampleIME draws input text.
// Applications that render TSF display attributes (Word, Notepad, browsers) show it; the TextService
// is the ITfDisplayAttributeProvider that hands this out.

// Stable public identity. Never regenerate.
inline constexpr GUID GUID_AKSHARA_DISPLAY_ATTRIBUTE_INPUT =
  {0x2edf5f3d,0x6213,0x405f,{0xb9,0x36,0x8b,0x58,0x04,0x9c,0x3e,0x58}};

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo {
 public:
  DisplayAttributeInfo();
  ~DisplayAttributeInfo();
  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP GetGUID(GUID* guid) override;
  STDMETHODIMP GetDescription(BSTR* description) override;
  STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override;
  STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override;
  STDMETHODIMP Reset() override;
 private:
  long refs_{1};
};

class DisplayAttributeEnum final : public IEnumTfDisplayAttributeInfo {
 public:
  DisplayAttributeEnum();
  ~DisplayAttributeEnum();
  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** clone) override;
  STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** info, ULONG* fetched) override;
  STDMETHODIMP Reset() override;
  STDMETHODIMP Skip(ULONG count) override;
 private:
  long refs_{1};
  ULONG index_{};
};
