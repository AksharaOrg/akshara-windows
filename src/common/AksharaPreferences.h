#pragma once

#include <windows.h>

// Per-user preferences.  A TIP is loaded into third-party processes, so it
// reads this small registry record lazily and never keeps user text or events.
namespace akshara::preferences {
inline constexpr wchar_t kRegistryPath[] = L"Software\\Akshara\\Settings";

struct Values {
  bool doubleSpacePeriod{true};
  bool autoSpaceAfterPunctuation{false};
  bool smartPunctuationSpacing{true};
  bool preserveRepeatedSpaces{false};
  bool smartBackspace{true};
  bool commitOnPunctuation{true};
  bool commitOnEnter{true};
  bool commitOnTab{true};
  bool commitOnCursorMovement{true};
  bool capsLockIndicator{true};
  bool showInputModeIndicator{true};
  bool rememberLastMode{true};
  bool soundOnInvalidComposition{false};
  bool showCompositionUnderline{true};
  bool automaticZwjConjuncts{true};
};

struct Item { const wchar_t* name; const wchar_t* title; const wchar_t* description; bool Values::*member; };
inline constexpr Item kItems[] = {
  {L"DoubleSpacePeriod", L"Double-space period", L"Two spaces finish a sentence with a period.", &Values::doubleSpacePeriod},
  {L"AutoSpaceAfterPunctuation", L"Auto-space after punctuation", L"Add a space after . , or ?.", &Values::autoSpaceAfterPunctuation},
  {L"SmartPunctuationSpacing", L"Smart punctuation spacing", L"Avoid accidental spaces before . , ? ! : and ;.", &Values::smartPunctuationSpacing},
  {L"PreserveRepeatedSpaces", L"Preserve repeated spaces", L"Keep repeated spaces instead of collapsing accidental ones.", &Values::preserveRepeatedSpaces},
  {L"SmartBackspace", L"Smart Backspace", L"Remove the last logical Akshara input.", &Values::smartBackspace},
  {L"CommitOnPunctuation", L"Commit on punctuation", L"Finish the current Sinhala composition before punctuation.", &Values::commitOnPunctuation},
  {L"CommitOnEnter", L"Commit on Enter", L"Finish composition before a new line.", &Values::commitOnEnter},
  {L"CommitOnTab", L"Commit on Tab", L"Finish composition before moving focus.", &Values::commitOnTab},
  {L"CommitOnCursorMovement", L"Commit on cursor movement", L"Finish composition when moving the caret.", &Values::commitOnCursorMovement},
  {L"CapsLockIndicator", L"Caps Lock indicator", L"Show a small notice when Caps Lock changes.", &Values::capsLockIndicator},
  {L"ShowInputModeIndicator", L"Show input mode indicator", L"Show the active Akshara layout after switching.", &Values::showInputModeIndicator},
  {L"RememberLastMode", L"Remember last Akshara mode", L"Restore the last Akshara layout you used.", &Values::rememberLastMode},
  {L"SoundOnInvalidComposition", L"Sound on invalid composition", L"Play a subtle system sound for an impossible sequence.", &Values::soundOnInvalidComposition},
  {L"ShowCompositionUnderline", L"Show composition underline", L"Underline text that is still being composed.", &Values::showCompositionUnderline},
  {L"AutomaticZwjConjuncts", L"Automatic ZWJ conjuncts", L"Generate correct yansaya and rakaransaya conjuncts.", &Values::automaticZwjConjuncts},
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
