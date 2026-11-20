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
  // Grammar-correct Smart Phonetic (v2), on by default; off types the classic Smart Phonetic.
  bool smartPhoneticV2{true};
  // v2's spelling options (SmartPhoneticOptions), all off by default.
  bool v2Archaic{};
  bool v2RepayaZwj{};
  bool v2Classical{};
  bool v2RakaransayaU{};
  // Off: d types ද, dh ධ, D ඩ. On: the older keyboard convention, d types ඩ and dh ද.
  bool v2RetroflexD{};
  // Two Spaces in quick succession type ". ".
  bool doubleSpacePeriod{true};
};

struct Item { const wchar_t* name; const wchar_t* title; const wchar_t* description; bool Values::*member; };
inline constexpr Item kItems[] = {
  {L"CommitOnPunctuation", L"Commit on punctuation", L"Finish the current Sinhala composition before punctuation.", &Values::commitOnPunctuation},
  {L"CommitOnEnter", L"Commit on Enter", L"Finish composition before a new line.", &Values::commitOnEnter},
  {L"CommitOnTab", L"Commit on Tab", L"Finish composition before moving focus.", &Values::commitOnTab},
  {L"CommitOnCursorMovement", L"Commit on cursor movement", L"Finish composition when moving the caret.", &Values::commitOnCursorMovement},
  {L"SmartPhoneticV2", L"Grammar-correct Smart Phonetic", L"Spell by the Sinhala rules, and let Space pick the dictionary word.", &Values::smartPhoneticV2},
  {L"SmartPhoneticV2Archaic", L"Archaic letters", L"Type the old letters with ~ (~l, ~ll, ~n) and join touching letters with +.", &Values::v2Archaic},
  {L"SmartPhoneticV2RepayaZwj", L"Joined repaya", L"Write repaya with a joiner, as in older text.", &Values::v2RepayaZwj},
  {L"SmartPhoneticV2Classical", L"Classical conjuncts", L"Join the classical bandi akuru pairs.", &Values::v2Classical},
  {L"SmartPhoneticV2RakaransayaU", L"Rakaransaya for ru", L"Write kru as rakaransaya with a u sign instead of the gaetta-pilla.", &Values::v2RakaransayaU},
  {L"SmartPhoneticV2RetroflexD", L"Type ඩ with d", L"d types ඩ and dh types ද, as on older Singlish keyboards.", &Values::v2RetroflexD},
  {L"DoubleSpacePeriod", L"Double-space period", L"Two quick Spaces type a full stop and a space.", &Values::doubleSpacePeriod},
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
