#include <livearea_search/app_index.h>
#include <livearea_search/catalog_record.h>
#include <livearea_search/config_failsafe.h>
#include <livearea_search/deferred_log_queue.h>
#include <livearea_search/icon_cache_policy.h>
#include <livearea_search/launch_uri.h>
#include <livearea_search/module_readiness.h>
#include <livearea_search/overlay_visibility.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::vector<std::string> TitleIds(
    const std::vector<livearea_search::SearchResult>& results) {
    std::vector<std::string> ids;
    ids.reserve(results.size());
    for (const auto& result : results) {
        ids.push_back(result.app == nullptr ? "<null>" : result.app->title_id);
    }
    return ids;
}

livearea_search::AppIndex MakeIndex() {
    livearea_search::AppIndex index;
    livearea_search::Vector<livearea_search::AppRecord> apps{
        {"PCSE00764", "Persona 4: Dancing All Night", "persona4d", "gd", "", true},
        {"NPXS10015", "Settings", "settings", "gda", "", true},
        {"PCSE00120", "Persona 4 Golden", "persona4g", "gd", "", true},
        {"ULUS-10512", "Persona 3 Portable", "pspemu", "eg", "", true},
        {"PCSE00000", "Persona Test Harness", "persona-dev", "tool", "", false},
        {"PCSB00001", "Alpha Game", "alpha_game", "gd", "", true},
        {"PCSA00001", "Alpha Game", "alpha_game_us", "gd", "", true},
        {"NPXS10026", "Content Manager", "cm_service", "gda", "", true},
    };
    index.Assign(apps);
    return index;
}

std::vector<livearea_search::SearchResult> Search(
    livearea_search::AppIndex& index,
    const std::string& query,
    std::size_t limit = 64) {
    std::vector<livearea_search::SearchResult> results;
    index.Search(query, results, limit);
    return results;
}

std::string ApplyConfigSelfDisable(std::string config) {
    std::size_t search_from = 0;
    while (true) {
        const std::size_t offset = livearea_search::FindConfigSelfDisableOffset(
            config.data(), config.size(), search_from);
        if (offset == livearea_search::kConfigEntryNotFound) {
            return config;
        }
        config[offset] = '#';
        search_from = offset + 1;
    }
}

void TestConfigSelfDisable() {
    Check(
        ApplyConfigSelfDisable(
            "*main\n"
            "ur0:/tai/livearea_search.suprx \n"
            "ur0:tai/other.suprx\n") ==
            "*main\n"
            "#r0:/tai/livearea_search.suprx \n"
            "ur0:tai/other.suprx\n",
        "slash-form plugin entry is disabled with a one-byte replacement");

    Check(
        ApplyConfigSelfDisable(
            "*main\r\n"
            "ur0:tai/livearea_search.suprx\r\n") ==
            "*main\r\n"
            "#r0:tai/livearea_search.suprx\r\n",
        "canonical Vita path form is disabled with CRLF line endings");

    const std::string unterminated_entry =
        "ur0:/tai/livearea_search.suprx";
    Check(
        ApplyConfigSelfDisable(unterminated_entry) ==
            "#r0:/tai/livearea_search.suprx",
        "a final unterminated plugin entry is disabled");
    Check(
        livearea_search::CountConfigSelfDisableEntries(
            unterminated_entry.data(), unterminated_entry.size()) == 1,
        "a unique active entry is counted exactly once");

    const std::string duplicate_entries =
        "ur0:tai/livearea_search.suprx\n"
        "ur0:/tai/livearea_search.suprx\n";
    Check(
        livearea_search::CountConfigSelfDisableEntries(
            duplicate_entries.data(), duplicate_entries.size()) == 2,
        "duplicate active entries are detected before any write");

    Check(
        ApplyConfigSelfDisable(
            "#r0:tai/livearea_search.suprx\n"
            "#ur0:/tai/livearea_search.suprx\n"
            "prefix ur0:tai/livearea_search.suprx\n"
            "ur0:tai/livearea_search.suprx.bak\n"
            "ur0:tai/livearea_search.suprx extra\n") ==
            "#r0:tai/livearea_search.suprx\n"
            "#ur0:/tai/livearea_search.suprx\n"
            "prefix ur0:tai/livearea_search.suprx\n"
            "ur0:tai/livearea_search.suprx.bak\n"
            "ur0:tai/livearea_search.suprx extra\n",
        "commented, embedded, longer, and token-suffixed paths remain unchanged");

    Check(
        ApplyConfigSelfDisable("  ur0:tai/livearea_search.suprx\n") ==
            "# ur0:tai/livearea_search.suprx\n",
        "an indented entry is disabled by commenting the physical line start");

    Check(
        livearea_search::FindConfigSelfDisableOffset(nullptr, 0) ==
            livearea_search::kConfigEntryNotFound,
        "empty config input has no self-disable offset");
    Check(
        livearea_search::CountConfigSelfDisableEntries(nullptr, 0) == 0,
        "empty config input has no active self-disable entries");
}

void TestNormalization() {
    using livearea_search::NormalizeForSearch;

    Check(NormalizeForSearch("  Persona 4: GOLDEN  ") == "persona 4 golden",
          "normalization folds ASCII case and punctuation");
    Check(NormalizeForSearch("A---B___C") == "a b c",
          "normalization collapses separators");
    Check(NormalizeForSearch("Pok\xC3\xA9mon") == "pokemon",
          "normalization folds common Latin diacritics");
    Check(NormalizeForSearch("\xEF\xBC\xB0\xEF\xBC\xA3\xEF\xBC\xB3\xEF\xBC\xA5") == "pcse",
          "normalization folds full-width ASCII");
}

void TestRanking() {
    auto index = MakeIndex();

    auto results = Search(index, "persona 4 golden");
    Check(!results.empty(), "exact title query produces a result");
    if (!results.empty()) {
        Check(results.front().app != nullptr && results.front().app->title_id == "PCSE00120",
              "exact title match ranks first");
        Check(results.front().match_class == livearea_search::MatchClass::Exact,
              "exact title match reports Exact");
    }

    results = Search(index, "pcse001");
    Check(TitleIds(results) == std::vector<std::string>{"PCSE00120"},
          "title-ID prefix search is case-insensitive");
    if (!results.empty()) {
        Check(results.front().match_class == livearea_search::MatchClass::Prefix,
              "title-ID prefix reports Prefix");
    }

    results = Search(index, "pers gol");
    Check(!results.empty() && results.front().app->title_id == "PCSE00120",
          "query tokens match title-token prefixes");
    if (!results.empty()) {
        Check(results.front().match_class == livearea_search::MatchClass::TokenPrefix,
              "token-prefix match reports TokenPrefix");
    }

    results = Search(index, "olden");
    Check(!results.empty() && results.front().app->title_id == "PCSE00120",
          "interior substring matches title");
    if (!results.empty()) {
        Check(results.front().match_class == livearea_search::MatchClass::Substring,
              "interior title match reports Substring");
    }

    results = Search(index, "cm_service");
    Check(!results.empty() && results.front().app->title_id == "NPXS10026",
          "application-name metadata is searchable");
    if (!results.empty()) {
        Check(results.front().match_class == livearea_search::MatchClass::Metadata,
              "metadata-only match reports Metadata");
    }

    results = Search(index, "tool");
    Check(results.empty(), "non-launchable records are excluded");

    results = Search(index, "   ");
    Check(results.empty(), "empty normalized query returns no results");
}

void TestStableOrderingAndLimit() {
    auto index = MakeIndex();

    auto results = Search(index, "alpha");
    Check(TitleIds(results) == std::vector<std::string>({"PCSA00001", "PCSB00001"}),
          "equal title matches use title ID as a deterministic tie breaker");

    results = Search(index, "persona", 2);
    Check(results.size() == 2, "result limit is enforced");
    Check(TitleIds(results) == std::vector<std::string>({"ULUS-10512", "PCSE00764"}),
          "limited results retain deterministic rank ordering");

    results = Search(index, "persona", 0);
    Check(results.empty(), "zero result limit returns no results");
}

void TestLaunchUri() {
    using livearea_search::BuildOpenLiveAreaUri;
    using livearea_search::IsValidTitleId;

    Check(IsValidTitleId("NPXS10015"), "native Vita title ID is valid");
    Check(IsValidTitleId("ULUS-10512"), "legacy title ID with hyphen is valid");
    Check(IsValidTitleId("PCSX_00001"), "underscore in title ID is valid");
    Check(!IsValidTitleId(""), "empty title ID is invalid");
    Check(!IsValidTitleId("NPXS10015&mode=play"), "query injection is rejected");
    Check(!IsValidTitleId("NPXS 10015"), "spaces are rejected");
    Check(!IsValidTitleId("NPXS/10015"), "path separators are rejected");
    Check(!IsValidTitleId(std::string(32, 'A')), "overlong title IDs are rejected");

    std::string uri;
    Check(BuildOpenLiveAreaUri("NPXS10015", uri) &&
              uri == "psgm:open?titleid=NPXS10015",
          "LiveArea URI uses SceShell's open route");
    Check(!BuildOpenLiveAreaUri("NPXS10015#fragment", uri) && uri.empty(),
          "invalid title ID does not produce a URI");
}

void TestCatalogRecordPolicy() {
    using livearea_search::BuildCatalogRecord;
    using livearea_search::CatalogMetadataView;

    livearea_search::AppRecord record;
    bool accepted = BuildCatalogRecord(CatalogMetadataView{
        "PCSE00120",
        "Persona 4 Golden",
        "P4G",
        "persona4g",
        "gd",
        "ur0:appmeta/PCSE00120/icon0.png",
        1}, record);
    Check(accepted, "launchable catalog entry is accepted");
    if (accepted) {
        Check(record.title_id == "PCSE00120", "catalog entry copies title ID");
        Check(record.display_title == "Persona 4 Golden", "full title is preferred");
        Check(record.app_name == "persona4g", "catalog entry copies app name");
        Check(record.category == "gd", "catalog entry copies category");
        Check(record.icon_path == "ur0:appmeta/PCSE00120/icon0.png",
              "catalog entry copies icon path");
    }

    accepted = BuildCatalogRecord(CatalogMetadataView{
        "NPXS10015", "", "Settings", "settings", "gda", "", 1}, record);
    Check(accepted && record.display_title == "Settings",
          "short title is used when the full title is empty");

    accepted = BuildCatalogRecord(CatalogMetadataView{
        "NPXS10026", "", "", "content_manager", "gda", "", -1}, record);
    Check(accepted && record.display_title == "content_manager",
          "application name is used when localized titles are absent");

    accepted = BuildCatalogRecord(CatalogMetadataView{
        "PCSE00001", "", "", "", "gd", "", 1}, record);
    Check(accepted && record.display_title == "PCSE00001",
          "title ID is the final display fallback");

    Check(!BuildCatalogRecord(CatalogMetadataView{
               "NPXS10015&x=1", "Settings", "", "", "gda", "", 1}, record),
          "catalog entry with unsafe title ID is rejected");
    Check(!BuildCatalogRecord(CatalogMetadataView{
               "NPXS10015", "Settings", "", "", "gda", "", 0}, record),
          "explicitly non-bootable catalog entry is rejected");
}

void TestModuleLookupReadiness() {
    using livearea_search::AdvanceShellReadyFrames;
    using livearea_search::HasStableShellReadiness;
    using livearea_search::IsModuleLookupReady;

    Check(!IsModuleLookupReady(static_cast<int>(0x8002D082U), -1),
          "module-not-found remains a waiting state");
    Check(IsModuleLookupReady(0, static_cast<std::int32_t>(0x40040075U)),
          "successful taiHEN lookup with a valid module ID is ready");
    Check(!IsModuleLookupReady(0, -1),
          "successful status without a valid module ID is not ready");

    Check(AdvanceShellReadyFrames(0, true) == 1,
          "shell readiness begins after the first ready frame");
    Check(AdvanceShellReadyFrames(41, true) == 42,
          "shell readiness accumulates across consecutive frames");
    Check(AdvanceShellReadyFrames(41, false) == 0,
          "shell readiness resets immediately when a dependency disappears");
    Check(
        AdvanceShellReadyFrames(UINT32_MAX, true) == UINT32_MAX,
        "shell readiness saturates instead of overflowing");
    Check(!HasStableShellReadiness(59, 60),
          "overlay loading waits for the full stability window");
    Check(HasStableShellReadiness(60, 60),
          "overlay loading begins at the stability threshold");
    Check(!HasStableShellReadiness(0, 0),
          "a zero-length stability policy never reports ready");
}

void TestSafeVectorOwnership() {
    livearea_search::Vector<livearea_search::AppRecord> apps{
        {"NPXS10015", "Settings", "settings", "gda", "", true},
        {"PCSE00120", "Persona 4 Golden", "persona4g", "gd", "", true},
    };
    livearea_search::AppIndex index;
    index.Assign(apps);
    Check(apps.empty(), "assign consumes the source vector without copying it");
    Check(index.apps().size() == 2, "assign retains all consumed applications");

    livearea_search::Vector<livearea_search::SearchResult> results;
    results.push_back(livearea_search::SearchResult());
    index.Search("settings", results);
    Check(TitleIds(results) == std::vector<std::string>{"NPXS10015"},
          "search replaces an existing output vector");
    index.Search("no such title", results);
    Check(results.empty(), "search clears reusable output when no records match");
}

void TestSafeRecordDeduplication() {
    livearea_search::Vector<livearea_search::AppRecord> records{
        {"PCSE00120", "Persona 4 Golden", "persona4g", "gd", "", true},
        {"NPXS10015", "Settings", "settings", "gda", "", true},
        {"PCSE00120", "Persona 4 Golden", "persona4g", "gd", "", true},
    };
    livearea_search::SortAndDeduplicateAppRecords(records);
    Check(records.size() == 2, "deduplication removes repeated title IDs");
    Check(records[0].title_id == "NPXS10015" && records[1].title_id == "PCSE00120",
          "deduplication leaves records sorted by title ID");
}

void TestOverlayLoadGate() {
    using livearea_search::CanLoadOverlay;

    Check(!CanLoadOverlay(true, false, false, false),
          "overlay waits until the shell has remained stable");
    Check(CanLoadOverlay(true, false, false, true),
          "overlay can load once all startup dependencies are ready");
    Check(!CanLoadOverlay(false, false, false, true),
          "overlay does not load after shutdown begins");
    Check(!CanLoadOverlay(true, true, false, true),
          "overlay does not load twice when already active");
    Check(!CanLoadOverlay(true, false, true, true),
          "overlay does not overlap an existing load");
}

void TestHomeVisibilityTransitions() {
    using livearea_search::ComputeHomeVisibilityAction;
    using livearea_search::HomeVisibilityAction;
    using livearea_search::IsBubbleHomeSurface;
    using livearea_search::ShouldCloseResultsPage;

    Check(
        IsBubbleHomeSurface(
            livearea_search::kTopMenuHomePageHash,
            0.0f,
            0.0f,
            0.0f,
            0.0f),
        "the bubble grid is recognized as the actual Home surface");
    Check(
        !IsBubbleHomeSurface(
            livearea_search::kTopMenuHomePageHash,
            0.0f,
            0.0f,
            960.0f,
            0.0f),
        "a transition toward a title LiveArea hides the Home control immediately");
    Check(
        !IsBubbleHomeSurface(
            livearea_search::kTopMenuHomePageHash,
            960.0f,
            0.0f,
            960.0f,
            0.0f),
        "a title LiveArea is not treated as the bubble Home surface");
    Check(
        !IsBubbleHomeSurface(0x9F7C030DU, 0.0f, 0.0f, 0.0f, 0.0f),
        "the system-application topmenu page is not treated as Home");
    Check(
        !IsBubbleHomeSurface(
            livearea_search::kTopMenuHomePageHash,
            -1.0f,
            0.0f,
            0.0f,
            0.0f),
        "an unsettled Home transition does not expose the control");

    Check(
        ComputeHomeVisibilityAction(false, true, false) ==
            HomeVisibilityAction::Show,
        "a newly opened Home page is explicitly shown");
    Check(
        ComputeHomeVisibilityAction(true, true, false) ==
            HomeVisibilityAction::None,
        "an already visible Home page is left unchanged");
    Check(
        ComputeHomeVisibilityAction(true, false, false) ==
            HomeVisibilityAction::Hide,
        "the Home control hides when topmenu is not visible");
    Check(
        ComputeHomeVisibilityAction(true, true, true) ==
            HomeVisibilityAction::Hide,
        "the Home control hides while the results page is open");
    Check(
        ComputeHomeVisibilityAction(false, false, false) ==
            HomeVisibilityAction::None,
        "an already hidden Home page is left unchanged");

    Check(!ShouldCloseResultsPage(true, false, true, true),
          "temporary topmenu loss does not close a page while the IME is editing");
    Check(ShouldCloseResultsPage(true, false, true, false),
          "topmenu loss closes a results page after IME editing ends");
    Check(!ShouldCloseResultsPage(false, true, true, true),
          "an owned IME transition suppresses all shell visibility closure");
    Check(ShouldCloseResultsPage(false, true, true, false),
          "a real foreground-app change closes results outside an IME transition");
    Check(!ShouldCloseResultsPage(true, false, false, false),
          "visibility changes do not close a page that is not open");

}

void TestExternalImeEditAdoption() {
    using livearea_search::ShouldAdoptExternalImeEdit;

    Check(
        ShouldAdoptExternalImeEdit(true, false, true),
        "a native TextBox edit started from the result header is adopted");
    Check(
        !ShouldAdoptExternalImeEdit(false, false, true),
        "editing outside an open results page is ignored");
    Check(
        !ShouldAdoptExternalImeEdit(true, true, true),
        "an edit already owned by the transition state is not adopted twice");
    Check(
        !ShouldAdoptExternalImeEdit(true, false, false),
        "an inactive TextBox does not start an IME transition");
}

void TestImeTransitionPolicy() {
    using livearea_search::ComputeImeFrameAction;
    using livearea_search::ImeFrameAction;

    Check(
        ComputeImeFrameAction(false, false, false, false, false, true) ==
            ImeFrameAction::None,
        "inactive IME workflow does not mutate the results page");
    Check(
        ComputeImeFrameAction(true, false, false, false, false, true) ==
            ImeFrameAction::None,
        "IME opening waits until editing becomes observable");
    Check(
        ComputeImeFrameAction(true, false, false, true, false, true) ==
            ImeFrameAction::None,
        "reopening a populated query cannot restore full results before the IME opens");
    Check(
        ComputeImeFrameAction(true, false, true, false, false, false) ==
            ImeFrameAction::MarkEditing,
        "the first editing frame records that the IME opened");
    Check(
        ComputeImeFrameAction(true, true, true, true, false, false) ==
            ImeFrameAction::None,
        "submission waits while the IME is still editing");
    Check(
        ComputeImeFrameAction(true, true, false, true, false, false) ==
            ImeFrameAction::None,
        "results remain hidden until the shell is visible after IME close");
    Check(
        ComputeImeFrameAction(true, true, false, true, false, true) ==
            ImeFrameAction::ShowResults,
        "results appear after editing ends and LiveArea visibility recovers");
    Check(
        ComputeImeFrameAction(true, true, false, false, true, false) ==
            ImeFrameAction::CloseResults,
        "an empty submission closes after the IME ends");
    Check(
        ComputeImeFrameAction(true, true, false, false, false, false) ==
            ImeFrameAction::CloseResults,
        "IME cancellation closes the hidden results page");
}

void TestDeferredResultClosePolicy() {
    using livearea_search::AdvanceDeferredResultClose;
    using livearea_search::DeferredResultCloseAction;

    std::uint32_t idle_frames = 2;
    Check(
        AdvanceDeferredResultClose(false, idle_frames) ==
                DeferredResultCloseAction::None &&
            idle_frames == 2,
        "an absent close request leaves the deferral counter untouched");

    std::uint32_t deferred_frames = 1;
    Check(
        AdvanceDeferredResultClose(true, deferred_frames) ==
                DeferredResultCloseAction::Wait &&
            deferred_frames == 0,
        "a close request waits one full PAF frame before teardown");
    Check(
        AdvanceDeferredResultClose(true, deferred_frames) ==
                DeferredResultCloseAction::Close,
        "a deferred close executes after the input callback has unwound");

    std::uint32_t immediate_frames = 0;
    Check(
        AdvanceDeferredResultClose(true, immediate_frames) ==
                DeferredResultCloseAction::Close,
        "a zero-frame request is ready for synchronous teardown");
}

void TestDeferredLogQueue() {
    livearea_search::DeferredLogQueue queue;
    char line[livearea_search::DeferredLogQueue::kLineCapacity];

    Check(!queue.Pop(line, sizeof(line)), "an empty deferred log queue has no entry");
    queue.Push("first");
    queue.Push("second");
    Check(queue.size() == 2, "deferred log queue tracks queued entries");
    Check(queue.Pop(line, sizeof(line)) && std::string(line) == "first",
          "deferred log queue preserves FIFO order");
    Check(queue.Pop(line, sizeof(line)) && std::string(line) == "second",
          "deferred log queue returns the second entry");

    for (std::size_t index = 0; index < livearea_search::DeferredLogQueue::kCapacity + 1;
         ++index) {
        std::string message = "entry-" + std::to_string(index);
        queue.Push(message.c_str());
    }
    Check(queue.size() == livearea_search::DeferredLogQueue::kCapacity,
          "deferred log queue remains bounded");
    Check(queue.Pop(line, sizeof(line)) && std::string(line) == "entry-1",
          "deferred log queue drops the oldest entry on overflow");
}

void TestIconCacheEvictionPolicy() {
    using livearea_search::IconCacheEvictionEntry;
    using livearea_search::SelectIconCacheEvictionCandidate;

    const IconCacheEvictionEntry entries[] = {
        {true, false, true, 0, 40},
        {true, false, true, 0, 10},
        {true, false, true, 1, 1},
        {true, true, true, 0, 0},
        {true, false, false, 0, 2},
        {false, false, true, 0, 0},
    };
    Check(
        SelectIconCacheEvictionCandidate(entries, 6) == 1,
        "icon eviction chooses the least-recently-used unpinned cache-owned surface");
    Check(
        SelectIconCacheEvictionCandidate(entries, 6, 1) == 0,
        "icon eviction excludes the surface currently being installed");

    const IconCacheEvictionEntry tied[] = {
        {true, false, true, 0, 7},
        {true, false, true, 0, 7},
    };
    Check(
        SelectIconCacheEvictionCandidate(tied, 2) == 0,
        "equal-age icon candidates retain deterministic traversal order");

    const IconCacheEvictionEntry protected_entries[] = {
        {true, false, true, 1, 0},
        {true, true, true, 0, 0},
        {true, false, false, 0, 0},
    };
    Check(
        SelectIconCacheEvictionCandidate(protected_entries, 3) ==
            livearea_search::kNoIconCacheEvictionCandidate,
        "pinned, loading, and externally retained surfaces are not evicted");
    Check(
        SelectIconCacheEvictionCandidate(nullptr, 0) ==
            livearea_search::kNoIconCacheEvictionCandidate,
        "an empty cache has no eviction candidate");
}

} // namespace

int main() {
    TestConfigSelfDisable();
    TestNormalization();
    TestRanking();
    TestStableOrderingAndLimit();
    TestLaunchUri();
    TestCatalogRecordPolicy();
    TestModuleLookupReadiness();
    TestSafeVectorOwnership();
    TestSafeRecordDeduplication();
    TestOverlayLoadGate();
    TestHomeVisibilityTransitions();
    TestExternalImeEditAdoption();
    TestImeTransitionPolicy();
    TestDeferredResultClosePolicy();
    TestDeferredLogQueue();
    TestIconCacheEvictionPolicy();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}
