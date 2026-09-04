#pragma once
#include <windows.h>
#include <msctf.h>

// Stable public identities. Never regenerate these during builds or upgrades.
inline constexpr CLSID CLSID_AksharaTextService =
  {0x8b8e29c7,0xe118,0x4c77,{0x9f,0x58,0x52,0x57,0x84,0xef,0xb9,0xc1}};
inline constexpr GUID GUID_PROFILE_AKSHARA_SMART_PHONETIC =
  {0xd602e665,0x86ad,0x42df,{0x9a,0x67,0xb8,0xb1,0x75,0x15,0xb1,0x72}};
inline constexpr GUID GUID_PROFILE_AKSHARA_PHONETIC =
  {0x132d8f4e,0x930a,0x4167,{0x86,0x6e,0xc3,0xc0,0x21,0xfa,0x0e,0x93}};
inline constexpr GUID GUID_PROFILE_AKSHARA_WIJESEKARA =
  {0xc98ca5c8,0x003c,0x4c20,{0xa9,0x3d,0xc4,0x90,0x86,0x25,0xf8,0x4a}};
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
