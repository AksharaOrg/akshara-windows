#include "Module.h"
#include "Globals.h"
#include "TextService.h"

#include <new>

HINSTANCE g_module = nullptr;
long g_objectCount = 0;
long g_lockCount = 0;

class ClassFactory final : public IClassFactory {
 public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) *object = static_cast<IClassFactory*>(this);
    if (!*object) return E_NOINTERFACE;
    AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
  ULONG STDMETHODCALLTYPE Release() override { const auto n = InterlockedDecrement(&refs_); if (!n) delete this; return static_cast<ULONG>(n); }
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** object) override {
    if (outer) return CLASS_E_NOAGGREGATION;
    return CreateTextService(riid, object);
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
    lock ? InterlockedIncrement(&g_lockCount) : InterlockedDecrement(&g_lockCount); return S_OK;
  }
 private:
  long refs_{1};
};

HRESULT CreateTextService(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  auto* service = new (std::nothrow) TextService();
  if (!service) return E_OUTOFMEMORY;
  const auto hr = service->QueryInterface(riid, object);
  service->Release();
  return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
  return g_objectCount == 0 && g_lockCount == 0 ? S_OK : S_FALSE;
}
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID riid, void** object) {
  if (clsid != CLSID_AksharaTextService) return CLASS_E_CLASSNOTAVAILABLE;
  auto* factory = new (std::nothrow) ClassFactory();
  if (!factory) return E_OUTOFMEMORY;
  const auto hr = factory->QueryInterface(riid, object); factory->Release(); return hr;
}
extern "C" HRESULT __stdcall DllRegisterServer() { return RegisterAkshara(); }
extern "C" HRESULT __stdcall DllUnregisterServer() { return UnregisterAkshara(); }

