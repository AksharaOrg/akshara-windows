#include "WordList.h"
#include "Globals.h"

#include <atomic>
#include <mutex>
#include <string>

namespace akshara::wordlist {
namespace {
constexpr wchar_t kFileName[] = L"sinhala_frequency_model.tsv";
// The bundled list is about 1 MB. Refuse anything far larger rather than allocating without bound.
constexpr LONGLONG kMaxBytes = 16LL * 1024 * 1024;

std::atomic<bool> started{false};
std::mutex lock;
std::shared_ptr<const SoundLexicon> loaded;

std::wstring ListPath() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const auto count = GetModuleFileNameW(g_module, path.data(), static_cast<DWORD>(path.size()));
    if (!count) return {};
    if (count < path.size()) { path.resize(count); break; }
    if (path.size() >= 32768) return {};
    path.resize(path.size() * 2);
  }
  const auto slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return {};
  path.resize(slash + 1);
  return path + kFileName;
}

bool ReadFile(const std::wstring& path, std::string& text) {
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size{};
  bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= kMaxBytes;
  if (ok) {
    text.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    ok = ::ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) && read == text.size();
  }
  CloseHandle(file);
  return ok;
}

DWORD WINAPI LoadThread(void* module) {
  try {
    std::string text;
    if (const auto path = ListPath(); !path.empty() && ReadFile(path, text)) {
      auto lexicon = std::make_shared<const SoundLexicon>(SoundLexicon::parse(text));
      if (lexicon->size() > 0) {
        std::lock_guard guard(lock);
        loaded = std::move(lexicon);
      }
    }
  } catch (...) {
    // Out of memory or a malformed list: keep typing with the converter's spelling.
  }
  // The thread holds its own reference to the DLL so it cannot be unloaded under it.
  FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}
}  // namespace

void LoadAsync() {
  if (started.exchange(true)) return;
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&LoadThread), &module)) {
    started = false;
    return;
  }
  const HANDLE thread = CreateThread(nullptr, 0, LoadThread, module, 0, nullptr);
  if (!thread) {
    FreeLibrary(module);
    started = false;
    return;
  }
  CloseHandle(thread);
}

std::shared_ptr<const SoundLexicon> Get() {
  std::lock_guard guard(lock);
  return loaded;
}
}  // namespace akshara::wordlist
