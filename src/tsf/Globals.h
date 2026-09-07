#pragma once
#include <windows.h>
#include <msctf.h>

// Stable public identities. Never regenerate these during builds or upgrades.
// Version-two identities deliberately replace the experimental TIP identities.
// TSF caches profile metadata aggressively, so a clean implementation must not
// inherit the previous service's cached activation state.
inline constexpr CLSID CLSID_AksharaTextService =
  {0x4f06b8d9,0x27fc,0x4a9b,{0x88,0xa7,0x25,0x03,0xb8,0xf0,0x75,0xc4}};
inline constexpr GUID GUID_PROFILE_AKSHARA_SMART_PHONETIC =
  {0x303b8d4e,0xbefb,0x4708,{0x95,0xa8,0x99,0xd7,0x99,0x98,0x68,0x8a}};
inline constexpr GUID GUID_PROFILE_AKSHARA_PHONETIC =
  {0x19c49470,0x8e7b,0x47f8,{0xa1,0x5f,0x84,0x3e,0x8a,0xd5,0x88,0x5f}};
inline constexpr GUID GUID_PROFILE_AKSHARA_WIJESEKARA =
  {0xf3594735,0x783b,0x4a9e,{0x84,0x15,0x4c,0x2a,0x3a,0x5d,0xda,0x63}};
inline constexpr LANGID kSinhalaSriLanka = 0x045B;
inline constexpr UINT kImeIconResourceId = 201;

// Read-only support data for the in-process TSF integration test.  It records
// lifecycle state only; no keystrokes or text are retained.
struct AksharaTsfDiagnostics {
  LONG activationCalls{};
  LONG keySinkAdviceResult{};
  LONG testKeyDownCalls{};
  LONG keyDownCalls{};
  LONG lastKeyWasEaten{};
  LONG lastContextWasWritable{};
  LONG clientId{};
};

extern HINSTANCE g_module;
extern long g_objectCount;
extern long g_lockCount;
extern AksharaTsfDiagnostics g_tsfDiagnostics;
