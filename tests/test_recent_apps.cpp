#include <livearea_search/recent_apps.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

livearea_search::Vector<livearea_search::AppRecord> Apps() {
    return {
        {"PCSA00001", "Alpha", "alpha", "gd", "", true},
        {"PCSB00002", "Beta", "beta", "gd", "", true},
        {"PCSC00003", "Gamma", "gamma", "gd", "", false},
        {"PCSD00004", "Delta", "delta", "gd", "", true},
    };
}

std::vector<std::string> ResultIds(
    const livearea_search::Vector<livearea_search::SearchResult>& results) {
    std::vector<std::string> ids;
    for (const auto& result : results) {
        ids.push_back(result.app == nullptr ? "<null>" : result.app->title_id.c_str());
    }
    return ids;
}

void TestRecordSelection() {
    livearea_search::RecentApps recent;
    recent.RecordSelection("PCSA00001");
    recent.RecordSelection("PCSB00002");
    recent.RecordSelection("PCSA00001");
    Check(
        std::vector<std::string>(
            recent.title_ids().begin(), recent.title_ids().end()) ==
            std::vector<std::string>({"PCSA00001", "PCSB00002"}),
        "reselecting an app moves it to the front without duplication");

    recent.RecordSelection("INVALID/ID");
    Check(
        recent.title_ids().size() == 2,
        "malformed title IDs are not recorded");

    for (int index = 0; index < 25; ++index) {
        char title_id[16];
        std::snprintf(title_id, sizeof(title_id), "PCSE%05d", index);
        recent.RecordSelection(title_id);
    }
    Check(recent.title_ids().size() == 20, "recent history is capped at 20");
    Check(recent.title_ids().front() == "PCSE00024", "newest selection is first");
    Check(recent.title_ids().back() == "PCSE00005", "oldest retained selection is last");
}

void TestParseAndSerialize() {
    const char input[] =
        "LIVEAREA_SEARCH_RECENTS_V1\n"
        "PCSB00002\n"
        "PCSA00001\n"
        "PCSB00002\n";
    livearea_search::RecentApps recent;
    Check(recent.Parse(input, sizeof(input) - 1), "valid recent file parses");
    Check(recent.title_ids().size() == 2, "duplicate stored IDs are ignored");
    Check(
        recent.Serialize() ==
            "LIVEAREA_SEARCH_RECENTS_V1\nPCSB00002\nPCSA00001\n",
        "serialization is canonical and versioned");

    livearea_search::RecentApps invalid;
    const char bad_header[] = "BAD\nPCSA00001\n";
    Check(
        !invalid.Parse(bad_header, sizeof(bad_header) - 1),
        "invalid header is rejected");

    const char malformed_title_id[] =
        "LIVEAREA_SEARCH_RECENTS_V1\nINVALID/ID\n";
    Check(
        !invalid.Parse(malformed_title_id, sizeof(malformed_title_id) - 1),
        "malformed title IDs are rejected");

    const char embedded_nul[] =
        "LIVEAREA_SEARCH_RECENTS_V1\nPCSA00001\0PCSB00002\n";
    Check(
        !invalid.Parse(embedded_nul, sizeof(embedded_nul) - 1),
        "embedded NUL is rejected");

    std::string too_many = "LIVEAREA_SEARCH_RECENTS_V1\n";
    for (int index = 0; index < 21; ++index) {
        char title_id[16];
        std::snprintf(title_id, sizeof(title_id), "PCSF%05d", index);
        too_many.append(title_id).push_back('\n');
    }
    Check(
        !invalid.Parse(too_many.data(), too_many.size()),
        "more than 20 stored lines are rejected");

    std::string oversized(
        livearea_search::RecentApps::kMaximumSerializedSize + 1,
        'A');
    Check(
        !invalid.Parse(oversized.data(), oversized.size()),
        "oversized recent data is rejected");
}

void TestResolveInstalledApps() {
    livearea_search::RecentApps recent;
    const char stored[] =
        "LIVEAREA_SEARCH_RECENTS_V1\n"
        "PCSD00004\n"
        "PCSC00003\n"
        "PCSZ99999\n"
        "PCSA00001\n";
    Check(
        recent.Parse(stored, sizeof(stored) - 1),
        "recent ordering fixture parses");
    const auto apps = Apps();
    livearea_search::Vector<livearea_search::SearchResult> results;
    recent.Resolve(apps, results);
    Check(
        ResultIds(results) == std::vector<std::string>({"PCSD00004", "PCSA00001"}),
        "resolve filters missing and non-launchable apps while preserving MRU order");

    recent.RetainAvailable(apps);
    Check(
        std::vector<std::string>(
            recent.title_ids().begin(), recent.title_ids().end()) ==
            std::vector<std::string>({"PCSD00004", "PCSA00001"}),
        "compaction removes unavailable IDs before the next save");
}

} // namespace

int main() {
    TestRecordSelection();
    TestParseAndSerialize();
    TestResolveInstalledApps();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All RecentApps tests passed\n";
    return EXIT_SUCCESS;
}
