#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <array>
#include <string>

namespace {
constexpr int kNav = 100, kContent = 101, kSettings = 102, kProject = 103;
constexpr wchar_t kClassName[] = L"AksharaHelpWindow";

std::wstring Page(int index) {
  switch (index) {
    case 1: return L"SMART PHONETIC\r\n\r\nType Sinhala by sound.\r\n\r\namma  →  අම්ම\r\nsiMhala  →  සිංහල\r\nAa  →  ඈ\r\nx  →  ං\r\nN  →  ණ්\r\nkru  →  කෘ\r\n\r\nUppercase letters are meaningful. Shift is safe; Ctrl and Alt shortcuts pass to the application.";
    case 2: return L"CLASSIC PHONETIC\r\n\r\nA conservative Akshara transliteration scheme.\r\n\r\namma  →  අම්ම\r\nmama  →  මම\r\nsiMhala  →  සිංහල\r\nkramaya  →  ක්‍රමය";
    case 3: return L"WIJESEKARA / SLS 1134\r\n\r\nAkshara follows physical US key positions and natural written order.\r\n\r\nf + l       →  කෙ\r\nf + l + d   →  කො\r\nf + l + d + a  →  කෝ\r\nf + f + l   →  කෛ\r\n\r\nRight Alt (AltGr) exposes the third layer. Ctrl+Alt shortcuts that are not Right Alt are left alone.";
    case 4: return L"PRIVACY\r\n\r\nTyping is processed locally inside the active application. The text service performs no network access, telemetry, analytics, logging, persistence, or background updating. Typed content remains in memory only for the current composition.";
    case 5: return L"ABOUT\r\n\r\nAkshara for Windows " AKSHARA_VERSION_STRING_W L"\r\nNative C++20 Text Services Framework input method\r\nSinhala (Sri Lanka), si-LK\r\n\r\nMIT licensed. Copyright © 2026 Akshara contributors.";
    default: return L"WELCOME TO AKSHARA\r\n\r\nA fast, private Sinhala input method for Windows.\r\n\r\n1. Open Windows language settings and add Sinhala if needed.\r\n2. Press Windows+Space.\r\n3. Choose Smart Phonetic, Phonetic, or Wijesekara.\r\n\r\nAkshara installs alongside Microsoft Sinhala keyboards and never changes your default input method.";
  }
}

void SetFont(HWND window, HFONT font) {
  SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  static HFONT titleFont = nullptr, bodyFont = nullptr;
  switch (message) {
    case WM_CREATE: {
      titleFont = CreateFontW(30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
      bodyFont = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
      auto title = CreateWindowW(L"STATIC", L"Akshara", WS_CHILD | WS_VISIBLE, 28, 20, 300, 42, window, nullptr, nullptr, nullptr);
      SetFont(title, titleFont);
      auto nav = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOXW, nullptr, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 28, 78, 210, 390, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNav)), nullptr, nullptr);
      for (const auto* item : std::array{L"Welcome", L"Smart Phonetic", L"Classic Phonetic", L"Wijesekara", L"Privacy", L"About"}) SendMessageW(nav, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
      SendMessageW(nav, LB_SETCURSEL, 0, 0); SetFont(nav, bodyFont);
      auto content = CreateWindowW(L"EDIT", Page(0).c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 268, 78, 574, 390, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kContent)), nullptr, nullptr);
      SetFont(content, bodyFont);
      auto settings = CreateWindowW(L"BUTTON", L"Open language settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 268, 488, 210, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettings)), nullptr, nullptr);
      auto project = CreateWindowW(L"BUTTON", L"Project website", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 492, 488, 170, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kProject)), nullptr, nullptr);
      SetFont(settings, bodyFont); SetFont(project, bodyFont); return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == kNav && HIWORD(wParam) == LBN_SELCHANGE) {
        const auto index = static_cast<int>(SendDlgItemMessageW(window, kNav, LB_GETCURSEL, 0, 0));
        SetWindowTextW(GetDlgItem(window, kContent), Page(index).c_str()); return 0;
      }
      if (LOWORD(wParam) == kSettings) { ShellExecuteW(window, L"open", L"ms-settings:regionlanguage", nullptr, nullptr, SW_SHOWNORMAL); return 0; }
      if (LOWORD(wParam) == kProject) { ShellExecuteW(window, L"open", L"https://github.com/AksharaOrg/akshara-windows", nullptr, nullptr, SW_SHOWNORMAL); return 0; }
      break;
    case WM_DESTROY: DeleteObject(titleFont); DeleteObject(bodyFont); PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
  const WNDCLASSEXW wc{sizeof(wc), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, instance,
    LoadIconW(instance, MAKEINTRESOURCEW(201)), LoadCursorW(nullptr, IDC_ARROW),
    reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr, kClassName, LoadIconW(instance, MAKEINTRESOURCEW(201))};
  if (!RegisterClassExW(&wc)) return 1;
  auto window = CreateWindowExW(0, kClassName, L"Akshara Help", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
    CW_USEDEFAULT, CW_USEDEFAULT, 900, 590, nullptr, nullptr, instance, nullptr);
  if (!window) return 2;
  ShowWindow(window, show); UpdateWindow(window);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return static_cast<int>(message.wParam);
}
