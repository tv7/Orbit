// Update check — version comparison + parsing GitHub's /releases/latest JSON, and
// the fetch path via a stub fetcher.

#include "test_util.h"

#include "../core/http.h"
#include "../core/updates.h"

using namespace ss;

TEST_CASE(compare_versions_orders_correctly) {
    CHECK(updates::compareVersions("1.2.0", "1.1.0") > 0);
    CHECK(updates::compareVersions("1.1.0", "1.2.0") < 0);
    CHECK_EQ(updates::compareVersions("1.1.0", "1.1.0"), 0);
    // leading 'v' tolerated on either side
    CHECK(updates::compareVersions("v1.2.0", "1.1.0") > 0);
    CHECK_EQ(updates::compareVersions("v1.1.0", "1.1.0"), 0);
    // different component counts pad with zero
    CHECK(updates::compareVersions("1.2", "1.2.0") == 0);
    CHECK(updates::compareVersions("1.2.1", "1.2") > 0);
    // patch + major bumps
    CHECK(updates::compareVersions("2.0.0", "1.9.9") > 0);
    CHECK(updates::compareVersions("1.10.0", "1.9.0") > 0);   // numeric, not lexical
    // pre-release suffix folds to the base (we don't ship these, just don't crash)
    CHECK_EQ(updates::compareVersions("1.2.0-beta", "1.2.0"), 0);
}

TEST_CASE(parse_latest_release_json) {
    std::string json = R"({
        "tag_name": "v1.3.0",
        "html_url": "https://github.com/tv7/Orbit/releases/tag/v1.3.0",
        "body": "## What's new\n- stuff",
        "draft": false
    })";
    auto r = updates::parseLatest(json);
    CHECK(r.has_value());
    CHECK_EQ(r->version, std::string("v1.3.0"));
    CHECK_EQ(r->url, std::string("https://github.com/tv7/Orbit/releases/tag/v1.3.0"));
    CHECK(r->notes.find("What's new") != std::string::npos);
    // No tag -> nothing.
    CHECK(!updates::parseLatest(R"({"name":"x"})").has_value());
    CHECK(!updates::parseLatest("not json").has_value());
}

TEST_CASE(latest_release_fetches_via_http) {
    http::setFetcher([](const std::string& url) -> std::optional<std::string> {
        if (url == "https://api.github.com/repos/tv7/Orbit/releases/latest")
            return std::string(R"({"tag_name":"1.4.0","html_url":"http://x","body":""})");
        return std::nullopt;
    });
    auto r = updates::latestRelease("tv7/Orbit");
    CHECK(r.has_value());
    CHECK_EQ(r->version, std::string("1.4.0"));
    CHECK(updates::compareVersions(r->version, "1.1.0") > 0);   // newer than a shipped build
    http::setFetcher({});
    // Offline (no fetcher) -> nullopt, never throws.
    CHECK(!updates::latestRelease("tv7/Orbit").has_value());
}
