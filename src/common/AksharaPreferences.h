#pragma once

#include <windows.h>

// Per-user preferences.  A TIP is loaded into third-party processes, so it
// reads this small registry record lazily and never keeps user text or events.
namespace akshara::preferences {
inline constexpr wchar_t kRegistryPath[] = L"Software\\Akshara\\Settings";

struct Values {
  bool commitOnPunctuation{true};
  bool commitOnEnter{true};
  bool commitOnTab{true};
  bool commitOnCursorMovement{true};
};

struct Item { const wchar_t* name; const wchar_t* title; const wchar_t* description; bool Values::*member; };
inline constexpr Item kItems[] = {
  {L"CommitOnPunctuation", L"Commit on punctuation", L"Finish the current Sinhala composition before punctuation.", &Values::commitOnPunctuation},
  {L"CommitOnEnter", L"Commit on Enter", L"Finish composition before a new line.", &Values::commitOnEnter},
  {L"CommitOnTab", L"Commit on Tab", L"Finish composition before moving focus.", &Values::commitOnTab},
  {L"CommitOnCursorMovement", L"Commit on cursor movement", L"Finish composition when moving the caret.", &Values::commitOnCursorMovement},
};

inline Values Load() {
  Values values{}; HKEY key{};
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return values;
  for (const auto& item : kItems) { DWORD value{}, size = sizeof(value), type{}; if (RegQueryValueExW(key, item.name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD) values.*(item.member) = value != 0; }
  RegCloseKey(key); return values;
}
inline bool Save(const Values& values) {
  HKEY key{}; DWORD disposition{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return false;
  bool success = true;
  for (const auto& item : kItems) { const DWORD value = values.*(item.member) ? 1 : 0; success = RegSetValueExW(key, item.name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS && success; }
  RegCloseKey(key); return success;
}
}
