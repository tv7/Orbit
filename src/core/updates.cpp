#include "updates.h"

#include "http.h"
#include "json.h"

#include <vector>

namespace ss::updates {

namespace {

// Numeric components of a version string ("v1.2.0" -> {1,2,0}); a pre-release
// suffix folds into the last component as 0 ("1.2.0-beta" -> {1,2,0}).
std::vector<long> components(const std::string& s) {
    std::vector<long> out;
    long cur = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') cur = cur * 10 + (c - '0');
        else if (c == '.') { out.push_back(cur); cur = 0; }
        // any other char (a leading 'v', a '-beta' suffix) is skipped
    }
    out.push_back(cur);
    return out;
}

}  // namespace

int compareVersions(const std::string& a, const std::string& b) {
    std::vector<long> va = components(a), vb = components(b);
    size_t n = va.size() > vb.size() ? va.size() : vb.size();
    for (size_t i = 0; i < n; ++i) {
        long x = i < va.size() ? va[i] : 0;
        long y = i < vb.size() ? vb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

std::optional<Release> parseLatest(const std::string& jsonText) {
    bool ok = false;
    json::Value v = json::parse(jsonText, &ok);
    if (!ok || !v.isObject()) return std::nullopt;
    const json::Value* tag = v.get("tag_name");
    if (!tag || !tag->isString() || tag->str.empty()) return std::nullopt;
    Release r;
    r.version = tag->str;
    if (const json::Value* u = v.get("html_url")) r.url = u->asString();
    if (const json::Value* b = v.get("body")) r.notes = b->asString();
    return r;
}

std::optional<Release> latestRelease(const std::string& repo) {
    auto raw = http::get("https://api.github.com/repos/" + repo + "/releases/latest");
    if (!raw) return std::nullopt;
    return parseLatest(*raw);
}

}  // namespace ss::updates
