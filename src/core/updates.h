// Update check — asks GitHub for the repo's latest release and compares its tag
// against the running version. Notify-only: for a portable app the honest flow is
// to tell the user and open the download page, not swap a running exe. Uses the
// injected http fetcher (Qt-free, headless-testable via a stub).

#pragma once

#include <optional>
#include <string>

namespace ss::updates {

struct Release {
    std::string version;   // tag, leading 'v' tolerated ("1.2.0")
    std::string url;       // release page (html_url) to open in a browser
    std::string notes;     // release body / changelog (markdown)
};

// Compare dotted numeric versions: >0 if a is newer, <0 if older, 0 if equal.
// Tolerates a leading 'v' and ignores any pre-release suffix ("1.2.0-beta" ~ "1.2.0").
int compareVersions(const std::string& a, const std::string& b);

// The latest published (non-draft, non-prerelease) release of `repo` ("owner/name"),
// or nullopt when offline / on any error. Does NOT compare — the caller decides.
std::optional<Release> latestRelease(const std::string& repo = "tv7/Orbit");

// Parse GitHub's /releases/latest JSON into a Release (exposed for tests).
std::optional<Release> parseLatest(const std::string& json);

}  // namespace ss::updates
