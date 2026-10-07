#pragma once
#include <windows.h>
#include <msctf.h>
#include <string>

class TextService;

enum class EditKind {
  // Show the rendered buffer as the composition.
  Compose,
  // End the composition with the text it shows.
  Commit,
  // End the composition with `text` in its place (or insert `text` when nothing is composed).
  CommitText,
  // Read only: S_OK when `text` is right before the caret, S_FALSE otherwise.
  CheckTextBefore,
  // Replace `expected` right before the caret with `text`; S_FALSE, changing nothing, when it isn't there.
  ReplaceTextBefore,
  // Read only: S_OK when the caret follows a space that follows other text (the first of a double space).
  CheckDoubleSpace,
  // Replace that space with ". ".
  DoubleSpacePeriod,
};

struct EditRequest {
  EditKind kind{EditKind::Compose};
  std::u16string text;
  std::u16string expected;
};

class EditSession final : public ITfEditSession {
 public:
  EditSession(TextService* service, ITfContext* context, EditRequest request);
  ~EditSession();
  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP DoEditSession(TfEditCookie cookie) override;
 private:
  long refs_{1};
  TextService* service_;
  ITfContext* context_;
  EditRequest request_;
};
