// User settings persisted to settings.json in the app data dir. Port of
// server.py's _load_settings/_save_settings — today it holds the UI language,
// the same key the web app used ("language", default "en").

#pragma once

#include <string>

namespace ss::settings {

// The selected UI language code (e.g. "en", "ar"); "en" if unset/unreadable.
std::string language();

// Persist the selected UI language. Best-effort (swallows I/O errors like Python).
void setLanguage(const std::string& lang);

}  // namespace ss::settings
