#pragma once

#include "SoundLexicon.h"

#include <memory>

// The Smart Phonetic v2 word list (sinhala_frequency_model.tsv, installed next to AksharaIME.dll), loaded once
// per process on a background thread. Until it is ready, Smart Phonetic uses the converter's spelling as is.
namespace akshara::wordlist {
// Starts loading the list, once per process. Safe to call from any thread.
void LoadAsync();
// The loaded list, or null while it is loading or when it could not be read.
std::shared_ptr<const SoundLexicon> Get();
}
