#pragma once
#include <windows.h>

HRESULT CreateTextService(REFIID riid, void** object);
HRESULT RegisterAkshara();
HRESULT UnregisterAkshara();

