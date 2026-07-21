// The app's own version — single source of truth. Bump this before building a
// release, and tag the GitHub release to match (the update check compares this
// against the latest release's tag). Keep it a plain dotted number ("1.2.0").

#pragma once

namespace ss {

inline constexpr const char* kVersion = "1.2.0";

}  // namespace ss
