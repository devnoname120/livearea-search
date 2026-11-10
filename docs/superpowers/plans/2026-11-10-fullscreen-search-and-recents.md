# Full-Screen Search and Recent Apps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the inset result dialog with an opaque full-width search page below the Vita information bar and add a persistent 20-entry MRU list of apps selected through search.

**Architecture:** Add a portable `RecentApps` model for validation, ordering, filtering, and serialization; add a Vita-only `RecentAppsFile` adapter for bounded same-directory atomic persistence; then integrate both into `SearchOverlay`. Keep the existing Home-only magnifier, progressive TextBox polling, icon cache, title-open route, and deferred kinetic-scroll teardown. Redesign the CXML and runtime geometry so display mode owns `960 × 512` below the information bar and IME mode uses a full-width `960 × 200` result viewport above the keyboard without obscuring it.

**Tech Stack:** C++11, VitaSDK, ScePaf-RE, CMake/CTest, `psp2cxml-tool`, taiHEN, Vita Companion, FTP validation on Vita 146.

---

## File map

### New files

- `include/livearea_search/recent_apps.h` — portable bounded MRU model and serialization contract.
- `src/core/recent_apps.cpp` — parse, record, serialize, and resolve logic.
- `tests/test_recent_apps.cpp` — host behavior tests for the portable model.
- `src/vita/recent_apps_file.h` — Vita persistence adapter interface.
- `src/vita/recent_apps_file.cpp` — bounded read, verified temporary write, and same-directory replacement.
- `tests/check_recent_apps_file.cmake` — source-level persistence safety contract.
- `tests/check_fullscreen_results.cmake` — exact CXML/runtime geometry and opacity contract.
- `tests/check_recent_overlay.cmake` — overlay ordering contract: record and save before title-open, Recent on empty query, no empty-query close.

### Modified files

- `CMakeLists.txt` — build the portable recent-app model.
- `cmake/VitaShellPlugin.cmake` — build the Vita persistence adapter.
- `tests/CMakeLists.txt` — register the new executable and source/resource contract tests.
- `resources/livearea_search_plugin.xml` — full-width opaque result geometry below the information bar.
- `src/vita/search_overlay.h` — own `RecentApps`, `RecentAppsFile`, and full-screen layout state.
- `src/vita/search_overlay.cpp` — load/display/persist recents and apply the new geometry.
- `README.md` — document full-screen composition and persistent Recent semantics.
- `docs/superpowers/specs/2026-11-09-livearea-search-design.md` — point superseded layout/empty-query text at the approved follow-up spec.

---

### Task 1: Add the portable RecentApps model

**Files:**
- Create: `include/livearea_search/recent_apps.h`
- Create: `src/core/recent_apps.cpp`
- Create: `tests/test_recent_apps.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing RecentApps host tests**

Create `tests/test_recent_apps.cpp` with these behaviors:

```cpp
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
```

Register the test executable in `tests/CMakeLists.txt`:

```cmake
add_executable(livearea_search_recent_apps_tests test_recent_apps.cpp)
target_link_libraries(livearea_search_recent_apps_tests PRIVATE livearea_search_core)
target_compile_features(livearea_search_recent_apps_tests PRIVATE cxx_std_11)
target_compile_options(livearea_search_recent_apps_tests PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>)
add_test(NAME livearea_search_recent_apps_tests COMMAND livearea_search_recent_apps_tests)
```

- [ ] **Step 2: Run the test to verify RED**

Run:

```sh
cmake -S . -B build-host-goal4-red \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF
cmake --build build-host-goal4-red --parallel 4
```

Expected: compilation fails because `livearea_search/recent_apps.h` does not exist.

- [ ] **Step 3: Add the RecentApps public interface**

Create `include/livearea_search/recent_apps.h`:

```cpp
#pragma once

#include <cstddef>

#include <livearea_search/app_index.h>

namespace livearea_search {

class RecentApps {
public:
    enum : std::size_t {
        kCapacity = 20,
        kMaximumSerializedSize = 1024,
    };

    bool Parse(const char* data, std::size_t size);
    void Clear();
    void RecordSelection(const String& title_id);
    void RetainAvailable(const Vector<AppRecord>& apps);
    void Resolve(
        const Vector<AppRecord>& apps,
        Vector<SearchResult>& results) const;
    String Serialize() const;

    const Vector<String>& title_ids() const;

private:
    Vector<String> title_ids_;
};

} // namespace livearea_search
```

- [ ] **Step 4: Implement the minimal portable model**

Create `src/core/recent_apps.cpp` with these exact rules:

```cpp
#include <livearea_search/recent_apps.h>
#include <livearea_search/launch_uri.h>

namespace livearea_search {
namespace {

const char kHeader[] = "LIVEAREA_SEARCH_RECENTS_V1";

bool EqualBytes(
    const char* data,
    std::size_t length,
    const char* expected,
    std::size_t expected_length) {
    if (length != expected_length) {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        if (data[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

bool Contains(const Vector<String>& values, const String& value) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == value) {
            return true;
        }
    }
    return false;
}

} // namespace

bool RecentApps::Parse(const char* data, std::size_t size) {
    Clear();
    if (data == NULL || size == 0 || size > kMaximumSerializedSize) {
        return false;
    }
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] == '\0' || data[index] == '\r') {
            return false;
        }
    }

    Vector<String> parsed;
    std::size_t entry_lines = 0;
    std::size_t line_start = 0;
    std::size_t line_number = 0;
    while (line_start < size) {
        std::size_t line_end = line_start;
        while (line_end < size && data[line_end] != '\n') {
            ++line_end;
        }
        const std::size_t length = line_end - line_start;
        if (line_number == 0) {
            if (!EqualBytes(data + line_start, length, kHeader, sizeof(kHeader) - 1)) {
                return false;
            }
        } else if (length != 0) {
            ++entry_lines;
            if (entry_lines > kCapacity) {
                return false;
            }
            const String title_id(data + line_start, length);
            if (!IsValidTitleId(title_id)) {
                return false;
            }
            if (!Contains(parsed, title_id) && parsed.size() < kCapacity) {
                parsed.push_back(title_id);
            }
        }
        ++line_number;
        if (line_end == size) {
            line_start = size;
        } else {
            line_start = line_end + 1;
        }
    }
    if (line_number == 0) {
        return false;
    }
    title_ids_.swap(parsed);
    return true;
}

void RecentApps::Clear() {
    title_ids_.clear();
}

void RecentApps::RecordSelection(const String& title_id) {
    if (!IsValidTitleId(title_id)) {
        return;
    }
    Vector<String> updated;
    updated.reserve(kCapacity);
    updated.push_back(title_id);
    for (std::size_t index = 0;
         index < title_ids_.size() && updated.size() < kCapacity;
         ++index) {
        if (title_ids_[index] != title_id) {
            updated.push_back(title_ids_[index]);
        }
    }
    title_ids_.swap(updated);
}

void RecentApps::Resolve(
    const Vector<AppRecord>& apps,
    Vector<SearchResult>& results) const {
    results.clear();
    results.reserve(title_ids_.size());
    for (std::size_t recent_index = 0;
         recent_index < title_ids_.size();
         ++recent_index) {
        for (std::size_t app_index = 0; app_index < apps.size(); ++app_index) {
            if (apps[app_index].launchable &&
                apps[app_index].title_id == title_ids_[recent_index]) {
                results.push_back(SearchResult(&apps[app_index], MatchClass::Metadata));
                break;
            }
        }
    }
}

void RecentApps::RetainAvailable(const Vector<AppRecord>& apps) {
    Vector<String> retained;
    retained.reserve(title_ids_.size());
    for (std::size_t recent_index = 0;
         recent_index < title_ids_.size();
         ++recent_index) {
        for (std::size_t app_index = 0; app_index < apps.size(); ++app_index) {
            if (apps[app_index].launchable &&
                apps[app_index].title_id == title_ids_[recent_index]) {
                retained.push_back(title_ids_[recent_index]);
                break;
            }
        }
    }
    title_ids_.swap(retained);
}

String RecentApps::Serialize() const {
    String output(kHeader);
    output.push_back('\n');
    for (std::size_t index = 0;
         index < title_ids_.size() && index < kCapacity;
         ++index) {
        output.append(title_ids_[index]);
        output.push_back('\n');
    }
    return output;
}

const Vector<String>& RecentApps::title_ids() const {
    return title_ids_;
}

} // namespace livearea_search
```

Add `src/core/recent_apps.cpp` to `livearea_search_core` in `CMakeLists.txt`.

- [ ] **Step 5: Run the focused tests and verify GREEN**

Run:

```sh
cmake --build build-host-goal4-red --parallel 4
ctest --test-dir build-host-goal4-red \
  -R 'recent_apps|livearea_search_tests' \
  --output-on-failure
```

Expected: both the new RecentApps executable and the existing core suite pass.

- [ ] **Step 6: Commit the portable model**

```sh
git add CMakeLists.txt tests/CMakeLists.txt tests/test_recent_apps.cpp \
  include/livearea_search/recent_apps.h src/core/recent_apps.cpp
git commit -m "Add persistent recent-app model"
```

---

### Task 2: Add bounded Vita persistence

**Files:**
- Create: `src/vita/recent_apps_file.h`
- Create: `src/vita/recent_apps_file.cpp`
- Create: `tests/check_recent_apps_file.cmake`
- Modify: `cmake/VitaShellPlugin.cmake`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing persistence source contract**

Create `tests/check_recent_apps_file.cmake`:

```cmake
if(NOT DEFINED RECENT_FILE_HEADER OR NOT DEFINED RECENT_FILE_SOURCE)
    message(FATAL_ERROR "RECENT_FILE_HEADER and RECENT_FILE_SOURCE are required")
endif()

file(READ "${RECENT_FILE_HEADER}" recent_header)
file(READ "${RECENT_FILE_SOURCE}" recent_source)

foreach(required_header IN ITEMS
    "class RecentAppsFile"
    "int Load(RecentApps& recent_apps) const;"
    "int Save(const RecentApps& recent_apps) const;")
    string(FIND "${recent_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing RecentAppsFile API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "ur0:data/livearea-search/recent_apps.txt"
    "ur0:data/livearea-search/recent_apps.txt.tmp"
    "sceIoSyncByFd"
    "VerifyFile"
    "sceIoRename"
    "RecentApps::kMaximumSerializedSize")
    string(FIND "${recent_source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing bounded atomic persistence step: ${required_source}")
    endif()
endforeach()

string(FIND "${recent_source}" "sceIoRemove(kRecentPath);" destructive_remove)
if(NOT destructive_remove EQUAL -1)
    message(FATAL_ERROR "Do not delete the valid destination before replacement")
endif()
```

Register it in `tests/CMakeLists.txt`:

```cmake
add_test(
    NAME livearea_search_recent_file_contract
    COMMAND ${CMAKE_COMMAND}
        -DRECENT_FILE_HEADER=${CMAKE_SOURCE_DIR}/src/vita/recent_apps_file.h
        -DRECENT_FILE_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/recent_apps_file.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_recent_apps_file.cmake)
```

- [ ] **Step 2: Run the contract to verify RED**

Run:

```sh
ctest --test-dir build-host-goal4-red \
  -R livearea_search_recent_file_contract \
  --output-on-failure
```

Expected: FAIL because the adapter files do not exist.

- [ ] **Step 3: Add the adapter interface**

Create `src/vita/recent_apps_file.h`:

```cpp
#pragma once

#include <livearea_search/recent_apps.h>

namespace livearea_search {

class RecentAppsFile {
public:
    int Load(RecentApps& recent_apps) const;
    int Save(const RecentApps& recent_apps) const;
};

} // namespace livearea_search
```

- [ ] **Step 4: Implement bounded read and verified replacement**

Create `src/vita/recent_apps_file.cpp` with these helpers and ordering:

```cpp
#include "recent_apps_file.h"

#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>

namespace livearea_search {
namespace {

const char kRecentDirectory[] = "ur0:data/livearea-search";
const char kRecentPath[] = "ur0:data/livearea-search/recent_apps.txt";
const char kTemporaryPath[] = "ur0:data/livearea-search/recent_apps.txt.tmp";
const int kErrorNoEntry = static_cast<int>(0x80010002U);
const int kErrorIo = static_cast<int>(0x80010005U);

int ReadExact(SceUID fd, char* data, SceSize size) {
    SceSize total = 0;
    while (total < size) {
        const int result = sceIoRead(fd, data + total, size - total);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            return kErrorIo;
        }
        total += static_cast<SceSize>(result);
    }
    return 0;
}

int WriteExact(SceUID fd, const char* data, SceSize size) {
    SceSize total = 0;
    while (total < size) {
        const int result = sceIoWrite(fd, data + total, size - total);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            return kErrorIo;
        }
        total += static_cast<SceSize>(result);
    }
    return 0;
}

int VerifyFile(const char* path, const char* expected, SceSize expected_size) {
    const SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    char bytes[RecentApps::kMaximumSerializedSize];
    int result = 0;
    const SceOff size = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (size != static_cast<SceOff>(expected_size) ||
        sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
        result = kErrorIo;
    } else {
        result = ReadExact(fd, bytes, expected_size);
    }
    if (result >= 0 && sceClibMemcmp(bytes, expected, expected_size) != 0) {
        result = kErrorIo;
    }
    const int close_result = sceIoClose(fd);
    return result < 0 ? result : close_result;
}

} // namespace

int RecentAppsFile::Load(RecentApps& recent_apps) const {
    recent_apps.Clear();
    const SceUID fd = sceIoOpen(kRecentPath, SCE_O_RDONLY, 0);
    if (fd == kErrorNoEntry) {
        return 0;
    }
    if (fd < 0) {
        return fd;
    }
    char bytes[RecentApps::kMaximumSerializedSize];
    int result = 0;
    const SceOff size = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (size < 0 || size > static_cast<SceOff>(sizeof(bytes)) ||
        sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
        result = kErrorIo;
    } else {
        result = ReadExact(fd, bytes, static_cast<SceSize>(size));
        if (result >= 0 && !recent_apps.Parse(bytes, static_cast<std::size_t>(size))) {
            recent_apps.Clear();
            result = 0;
        }
    }
    const int close_result = sceIoClose(fd);
    return result < 0 ? result : close_result;
}

int RecentAppsFile::Save(const RecentApps& recent_apps) const {
    const String serialized = recent_apps.Serialize();
    if (serialized.size() > RecentApps::kMaximumSerializedSize) {
        return kErrorIo;
    }
    sceIoMkdir(kRecentDirectory, 0777);
    const SceUID fd = sceIoOpen(
        kTemporaryPath,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0666);
    if (fd < 0) {
        return fd;
    }
    int result = WriteExact(
        fd,
        serialized.data(),
        static_cast<SceSize>(serialized.size()));
    if (result >= 0) {
        result = sceIoSyncByFd(fd, 0);
    }
    const int close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) {
        result = close_result;
    }
    if (result >= 0) {
        result = VerifyFile(
            kTemporaryPath,
            serialized.data(),
            static_cast<SceSize>(serialized.size()));
    }
    if (result >= 0) {
        result = sceIoRename(kTemporaryPath, kRecentPath);
    }
    if (result >= 0) {
        result = VerifyFile(
            kRecentPath,
            serialized.data(),
            static_cast<SceSize>(serialized.size()));
    }
    if (result < 0) {
        sceIoRemove(kTemporaryPath);
    }
    return result;
}

} // namespace livearea_search
```

Before accepting this implementation, verify on Vita that same-directory `sceIoRename(temp, existing)` replaces the destination. If the target filesystem returns `SCE_ERROR_ERRNO_EEXIST`, replace only the final rename block with a verified backup-rename transaction: rename destination to `.bak`, rename temporary to destination, verify destination, then delete `.bak`; on any failure, restore `.bak`. Do not remove the valid destination before a recoverable copy exists.

Add `src/vita/recent_apps_file.cpp` to `livearea_search_vita` in `cmake/VitaShellPlugin.cmake`.

- [ ] **Step 5: Run the source contract and host suite**

```sh
cmake --build build-host-goal4-red --parallel 4
ctest --test-dir build-host-goal4-red \
  -R 'recent_file_contract|recent_apps' \
  --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit the persistence adapter**

```sh
git add cmake/VitaShellPlugin.cmake tests/CMakeLists.txt \
  tests/check_recent_apps_file.cmake \
  src/vita/recent_apps_file.h src/vita/recent_apps_file.cpp
git commit -m "Persist recent search selections"
```

---

### Task 3: Integrate Recent mode into SearchOverlay

**Files:**
- Create: `tests/check_recent_overlay.cmake`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/vita/search_overlay.h`
- Modify: `src/vita/search_overlay.cpp`

- [ ] **Step 1: Write the failing overlay contract**

Create `tests/check_recent_overlay.cmake`:

```cmake
if(NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "SEARCH_OVERLAY is required")
endif()
file(READ "${SEARCH_OVERLAY}" source)

foreach(required IN ITEMS
    "recent_apps_file_.Load(recent_apps_)"
    "recent_apps_.Resolve(index_.apps(), next_results);"
    "SetText(result_count_, \"Recent\");"
    "SetText(empty_text_, \"No recent apps\");"
    "recent_apps_.RecordSelection(title_id);"
    "recent_apps_file_.Save(recent_apps_);"
    "OpenTitleLiveArea(title_id)")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing Recent overlay behavior: ${required}")
    endif()
endforeach()

string(FIND "${source}" "recent_apps_.RecordSelection(title_id);" record_index)
string(FIND "${source}" "recent_apps_file_.Save(recent_apps_);" save_index)
string(FIND "${source}" "OpenTitleLiveArea(title_id)" launch_index)
if(NOT record_index LESS save_index OR NOT save_index LESS launch_index)
    message(FATAL_ERROR "Selection must be recorded and persisted before title-open")
endif()

string(FIND "${source}" "ime_close_pending_ = normalized_query_.empty();" empty_close)
if(NOT empty_close EQUAL -1)
    message(FATAL_ERROR "An empty query must show Recent instead of closing")
endif()
```

Register it in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run the contract to verify RED**

```sh
ctest --test-dir build-host-goal4-red \
  -R livearea_search_recent_overlay \
  --output-on-failure
```

Expected: FAIL because the overlay has no Recent integration.

- [ ] **Step 3: Add Recent ownership to SearchOverlay**

In `src/vita/search_overlay.h`, include both headers and add members:

```cpp
#include <livearea_search/recent_apps.h>
#include "recent_apps_file.h"
```

```cpp
RecentApps recent_apps_;
RecentAppsFile recent_apps_file_;
```

Add a private helper:

```cpp
void PopulateRecentResults(Vector<SearchResult>& results) const;
```

- [ ] **Step 4: Load history during initialization without making it fatal**

After `index_source_.Load(index_)` succeeds in `SearchOverlay::Initialize()`:

```cpp
const int recent_result = recent_apps_file_.Load(recent_apps_);
StartupLogResult("recent apps load", recent_result);
if (recent_result < 0) {
    recent_apps_.Clear();
}
```

Persistence failure must not prevent the overlay from opening.

- [ ] **Step 5: Populate Recent whenever the normalized query is empty**

Replace the result construction in `RefreshSearchFromText` with:

```cpp
Vector<SearchResult> next_results;
if (normalized.empty()) {
    recent_apps_.Resolve(index_.apps(), next_results);
} else {
    index_.Search(query, next_results, 128);
}
```

After the page widgets are configured in `OpenResultsPage`, force the first model build before `BeginImeEdit()`:

```cpp
RefreshSearchFromText(true);
icon_cache_->SetNotificationsEnabled(true);
return BeginImeEdit();
```

- [ ] **Step 6: Keep the page open when IME editing ends with an empty query**

In `BeginImeEdit`, external-edit adoption, and `SubmitSearch`, set:

```cpp
results_pending_display_ = true;
ime_close_pending_ = false;
```

Do not derive either flag from `normalized_query_.empty()`.

`ComputeImeFrameAction` then reaches `ShowResults` after editing ends, including for Recent mode, while the separate deferred Circle close remains unchanged.

- [ ] **Step 7: Display Recent labels and empty state**

Change `UpdateResultPresentation`:

```cpp
if (!results_.empty()) {
    result_list_->Show(paf::common::transition::Type_FadeinFast);
    empty_text_->Hide(paf::common::transition::Type_FadeinFast);
} else {
    result_list_->Hide(paf::common::transition::Type_FadeinFast);
    SetText(
        empty_text_,
        normalized_query_.empty() ? "No recent apps" : "No matching applications");
    empty_text_->Show(paf::common::transition::Type_FadeinFast);
}
```

Change `UpdateResultCount`:

```cpp
if (normalized_query_.empty()) {
    SetText(result_count_, "Recent");
    return;
}
```

Keep the existing numeric count formatting for non-empty queries.

- [ ] **Step 8: Record row activation before attempting title-open**

Change `SearchOverlay::Launch` to:

```cpp
int SearchOverlay::Launch(const String& title_id) {
    recent_apps_.RetainAvailable(index_.apps());
    recent_apps_.RecordSelection(title_id);
    const int save_result = recent_apps_file_.Save(recent_apps_);
    QueueInteractionLogResult("recent apps save", save_result);

    const int result = OpenTitleLiveArea(title_id);
    QueueInteractionLogResult("result launch", result);
    if (result >= 0) {
        RequestCloseResultsPage();
    } else if (normalized_query_.empty()) {
        Vector<SearchResult> next_results;
        recent_apps_.Resolve(index_.apps(), next_results);
        const std::size_t old_count = result_list_ == NULL ? 0 : result_list_->GetCellNum(0);
        if (result_list_ != NULL && old_count != 0) {
            result_list_->DeleteCell(0, 0, old_count);
        }
        results_.clear();
        results_.swap(next_results);
        RebuildResultList();
    }
    return result;
}
```

The in-memory MRU remains updated even when saving or title-open fails. A title-open failure keeps the page open as before.

- [ ] **Step 9: Run tests and verify GREEN**

```sh
cmake --build build-host-goal4-red --parallel 4
ctest --test-dir build-host-goal4-red \
  -R 'recent_overlay|recent_apps|close_lifecycle|livearea_search_tests' \
  --output-on-failure
```

Expected: PASS.

- [ ] **Step 10: Commit Recent overlay integration**

```sh
git add tests/CMakeLists.txt tests/check_recent_overlay.cmake \
  src/vita/search_overlay.h src/vita/search_overlay.cpp
git commit -m "Show persistent recent apps for empty search"
```

---

### Task 4: Replace the inset result layout with full-width geometry

**Files:**
- Create: `tests/check_fullscreen_results.cmake`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/check_search_experience.cmake`
- Modify: `resources/livearea_search_plugin.xml`
- Modify: `src/vita/search_overlay.cpp`

- [ ] **Step 1: Write the failing full-screen resource/runtime contract**

Create `tests/check_fullscreen_results.cmake`:

```cmake
if(NOT DEFINED RESOURCE_XML OR NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "RESOURCE_XML and SEARCH_OVERLAY are required")
endif()
file(READ "${RESOURCE_XML}" resource)
file(READ "${SEARCH_OVERLAY}" overlay)

foreach(required IN ITEMS
    "id=\"plane_results_surface\""
    "size=\"960,512\" pos=\"0,-16\""
    "id=\"plane_results_header\""
    "size=\"960,72\" pos=\"0,-32\""
    "id=\"list_results\""
    "size=\"960,440\" pos=\"0,-52\""
    "id=\"plane_results_ime_surface\""
    "size=\"960,200\" pos=\"0,68\""
    "color=\"0.075,0.09,0.12,1\"")
    string(FIND "${resource}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing full-screen result geometry: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "size=\"920,208\""
    "size=\"920,440\""
    "size=\"920,70\""
    "color=\"0.075,0.09,0.12,0.96\"")
    string(FIND "${resource}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Inset/translucent result geometry remains: ${forbidden}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "SetCellSizeDefault(0, paf::math::v4(960.0f, 70.0f))"
    "result_list_->SetSize(960.0f, 200.0f, 0.0f);"
    "result_list_->SetPos(0.0f, 68.0f, 0.0f);"
    "result_list_->SetSize(960.0f, 440.0f, 0.0f);"
    "result_list_->SetPos(0.0f, -52.0f, 0.0f);")
    string(FIND "${overlay}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing runtime full-screen geometry: ${required_source}")
    endif()
endforeach()
```

Register the test in `tests/CMakeLists.txt`.

- [ ] **Step 2: Update the existing experience contract to require the new geometry**

In `tests/check_search_experience.cmake`, replace old requirements for `920 × 208`, `920 × 170`, and y=`73` with the exact full-width values above. Keep the progressive-search, IME re-entry, row lifetime, icon-cache, and title-ID-removal checks unchanged.

- [ ] **Step 3: Run the contracts to verify RED**

```sh
ctest --test-dir build-host-goal4-red \
  -R 'fullscreen_results|experience_contract' \
  --output-on-failure
```

Expected: both tests fail on the current inset dimensions.

- [ ] **Step 4: Change the CXML geometry and opacity**

Apply these exact resource changes in `resources/livearea_search_plugin.xml`:

```xml
<plane style="style_results_surface" texture="_common_texture_dialog_fill_white" id="plane_results_surface">
    <layout_hint size="960,512" pos="0,-16" />
</plane>

<plane style="style_results_ime_surface" texture="_common_texture_dialog_fill_white" id="plane_results_ime_surface">
    <layout_hint size="960,200" pos="0,68" />
</plane>

<plane style="style_results_header" texture="_common_texture_dialog_fill_white" id="plane_results_header">
    <layout_hint align="0,2" anchor="0,2" size="960,72" pos="0,-32" />
```

Use internal padding only:

```xml
<text_box ...>
    <layout_hint align="1,1" anchor="1,1" size="770,58" pos="18,0" />
</text_box>
<text ...>
    <layout_hint align="2,1" anchor="2,1" size="150,58" pos="-18,0" />
</text>
<plane ... id="plane_header_rule">
    <layout_hint align="0,1" anchor="0,1" size="960,1" />
</plane>
```

Set the display list and rows to full width:

```xml
<list_view style="style_results_list" id="list_results">
    <layout_hint align="0,0" anchor="0,0" size="960,440" pos="0,-52" />
</list_view>
```

Change every result `list_item`/`image_button` width from `920` to `960`.

Make both result surfaces opaque:

```xml
<style_plane color="0.075,0.09,0.12,1" id="style_results_surface" />
<style_plane color="0.075,0.09,0.12,1" id="style_results_ime_surface" />
```

The full-width opaque header and surface naturally cover the Home notification corner below the information bar. Do not hide the indicator plugin or information bar.

- [ ] **Step 5: Update runtime geometry without touching close ordering**

In `OpenResultsPage`:

```cpp
result_list_->SetCellSizeDefault(0, paf::math::v4(960.0f, 70.0f));
```

In `SetResultsEditingLayout`:

```cpp
result_list_->SetSize(960.0f, 200.0f, 0.0f);
result_list_->SetPos(0.0f, 68.0f, 0.0f);
empty_text_->SetPos(0.0f, 68.0f, 0.0f);
```

In `SetResultsDisplayLayout`:

```cpp
result_list_->SetSize(960.0f, 440.0f, 0.0f);
result_list_->SetPos(0.0f, -52.0f, 0.0f);
empty_text_->SetPos(0.0f, -52.0f, 0.0f);
```

Do not modify `RequestCloseResultsPage`, `ProcessCloseRequest`, list quiescence, notification suppression, pin reset, synchronous `Clear`, or non-fading `PageClose`.

- [ ] **Step 6: Compile the RCO and run contracts**

```sh
cmake --build build-host-goal4-red --parallel 4
ctest --test-dir build-host-goal4-red \
  -R 'fullscreen_results|experience_contract|close_lifecycle|paf_composition' \
  --output-on-failure
LIVEAREA_SEARCH_BUILD_DIR=build-vita-goal4-layout tools/build-vita.sh
```

Expected: all named host contracts pass and `psp2cxml-tool` generates the RCO successfully.

- [ ] **Step 7: Commit the layout**

```sh
git add resources/livearea_search_plugin.xml src/vita/search_overlay.cpp \
  tests/CMakeLists.txt tests/check_fullscreen_results.cmake \
  tests/check_search_experience.cmake
git commit -m "Use full-width search result layout"
```

---

### Task 5: Update documentation and run clean offline verification

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/specs/2026-11-09-livearea-search-design.md`

- [ ] **Step 1: Update user-facing behavior documentation**

Document these points in `README.md`:

- Search results own the complete area below the Vita information bar.
- The information bar remains visible; the Home notification corner is occluded while search is open.
- Empty query displays `Recent`, based only on LiveArea Search row selections.
- The 20-entry MRU persists across reboot and filters unavailable apps.
- Recency updates before the title-open request, so an open failure does not lose the selection.
- Kinetic-scroll close uses deferred quiescent teardown.

Add a short supersession note to `docs/superpowers/specs/2026-11-09-livearea-search-design.md` linking to `2026-11-10-fullscreen-search-and-recents-design.md` for result geometry and empty-query behavior.

- [ ] **Step 2: Run a fresh normal host build**

```sh
rm -rf build-host-goal4-final
cmake -S . -B build-host-goal4-final \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF
cmake --build build-host-goal4-final --parallel 4
ctest --test-dir build-host-goal4-final --output-on-failure
```

Expected: 100% pass.

- [ ] **Step 3: Run a fresh sanitizer host build**

```sh
rm -rf build-host-goal4-sanitized
cmake -S . -B build-host-goal4-sanitized \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-host-goal4-sanitized --parallel 4
ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir build-host-goal4-sanitized --output-on-failure
```

Expected: 100% pass with no ASan or UBSan report.

- [ ] **Step 4: Run a fresh pinned VitaSDK target build**

```sh
rm -rf build-vita-goal4-final
LIVEAREA_SEARCH_BUILD_DIR=build-vita-goal4-final \
  LIVEAREA_SEARCH_JOBS=4 \
  tools/build-vita.sh 2>&1 | tee build-vita-goal4-final.log
```

Expected: SUPRX and RCO complete with no `warning:` or `error:` line.

- [ ] **Step 5: Run static and binary checks**

```sh
xmllint --noout resources/livearea_search_plugin.xml
sh -n tools/build-vita.sh
git diff --check
! rg -n '\b(strcpy|strcat|sprintf|gets)\s*\(' src include
! rg -n '920,208|920\.0f, 170\.0f|SetPageClosing|CB_TEXT_BOX_STRING_CHANGED' \
  resources src tests
sha256sum \
  build-vita-goal4-final/dist/livearea_search.suprx \
  build-vita-goal4-final/dist/ur0/data/livearea-search/livearea_search_plugin.rco
```

Inside the pinned VitaSDK container, inspect the unconverted ELF and VELF:

```sh
docker run --rm -v "$PWD:/src" -w /src \
  vitasdk/vitasdk-softfp@sha256:99814094e0240b7ebe8700ba3742ee761bbcfb22bf8eedeac8db5c1dbe3e3d32 \
  sh -lc '
    arm-vita-eabi-readelf -h build-vita-goal4-final/livearea_search
    arm-vita-eabi-nm -u build-vita-goal4-final/livearea_search | grep . && exit 1 || true
    arm-vita-eabi-nm build-vita-goal4-final/livearea_search | \
      grep -E " module_start$| module_stop$| sceAppMgrLaunchAppByUri$| taiGetModuleInfo$"
    arm-vita-eabi-readelf -S build-vita-goal4-final/livearea_search_vita.velf | \
      grep -E "sceLib.ent|sceLib.stubs"
  '
```

Expected: ARM EABI5, no unresolved symbols, expected entry/import symbols, and SCE library sections present.

- [ ] **Step 6: Commit documentation and verified integration**

```sh
git add README.md docs/superpowers/specs/2026-11-09-livearea-search-design.md
git commit -m "Document full-screen search and recents"
```

---

### Task 6: Deploy and validate on Vita 146

**Files:**
- Runtime evidence only under ignored `build-vita-goal4-final/device-validation/`
- No source changes unless a hardware observation falsifies a specific tested assumption.

- [ ] **Step 1: Preserve the current device state**

Before uploading anything, retrieve to a timestamped evidence directory:

- `ur0:/tai/config.txt`
- `ur0:/tai/livearea_search.suprx`
- `ur0:/data/livearea-search/livearea_search_plugin.rco`
- `ur0:/data/livearea-search/recent_apps.txt` when present
- `ur0:/data/livearea-search/startup.log`
- the current recent `ux0:/data/psp2core-*shell.self*` name window

Record sizes and SHA-256 hashes. Require the active config entry to be disabled as `#r0:/tai/livearea_search.suprx` before deployment.

- [ ] **Step 2: Upload with backup and byte-for-byte readback**

Using the existing reviewed FTP deployment pattern:

1. Back up the installed SUPRX and RCO with a UTC suffix.
2. Upload the fresh `build-vita-goal4-final` artifacts.
3. Download both installed files and compare complete bytes and SHA-256.
4. Download `config.txt`, replace exactly the first `#` of the unique disabled entry with `u`, and upload it.
5. Download the complete config and verify all bytes except that one expected byte are unchanged.
6. Request one reboot through Vita Companion.
7. Wait for FTP and Companion to return.
8. Confirm startup changed the entry back to `#r0:/tai/livearea_search.suprx`.

Do not leave the plugin armed for a later reboot.

- [ ] **Step 3: Validate initial empty-history composition**

Temporarily move any existing `recent_apps.txt` aside, reopen Search, and capture a native screenshot proving:

- information bar fully visible;
- top-right Home notification corner absent;
- no blue left/right/bottom bleed;
- search header begins below the information bar;
- `Recent` label visible;
- `No recent apps` visible;
- no black gap between header divider and list viewport.

- [ ] **Step 4: Validate IME and progressive search composition**

With the IME open:

- capture an empty-query screenshot;
- type a query one character at a time and verify result counts/rows update before Return;
- capture a screenshot showing full-width header and list region to the keyboard boundary;
- verify no plugin surface obscures any keyboard row;
- press Return and capture the expanded `960 × 440` list;
- verify the first row begins immediately below the divider and the final visible row is clipped at the bottom edge.

- [ ] **Step 5: Validate selection-based Recent semantics**

Run these cases and inspect `recent_apps.txt` after each:

1. Select a result whose title-open succeeds; reopen Search and verify it is first.
2. Use a diagnostic-only build or a selected stale/unopenable title to make `OpenTitleLiveArea` return an error; verify the selected title is still written first and the search page remains open.
3. Select at least three unique results, then reselect the oldest; verify move-to-front without duplication.
4. Populate more than 20 valid title IDs using selections or a host-generated valid fixture, reopen Search, and verify exactly the newest 20 display.
5. Restore a file containing one uninstalled valid-format title ID; verify it is omitted without changing the order of installed entries.
6. Clear a non-empty query; verify Recent appears immediately.
7. Reboot, rearm once, reopen Search, and verify Recent order persists.

Any diagnostic build used to force a title-open failure must be replaced by the clean final artifact and read back before completion.

- [ ] **Step 6: Re-run the kinetic-scroll close regression**

Run five cycles:

1. Open Search.
2. Enter a broad query.
3. End IME editing.
4. Perform an upward touch flick.
5. Press Circle 20 ms after touch release.
6. Verify return to Home, FTP/Companion responsiveness, and screenshot creation.

After all cycles, require:

- no new core filename compared with the pre-test window;
- unchanged disabled config entry;
- five clean close callbacks in `startup.log`;
- no blue bleed or notification corner in result screenshots.

- [ ] **Step 7: Save hardware evidence**

Store screenshots, logs, config, recent file, artifact readbacks, core-name comparison, hashes, and a machine-readable validation report under:

```text
build-vita-goal4-final/device-validation/
```

---

### Task 7: Final review, aggregate diff, commit, and push

**Files:**
- All source, test, resource, and documentation changes from Tasks 1–5.

- [ ] **Step 1: Re-run fresh verification after the final hardware-tested source state**

Repeat the complete normal host suite, sanitizer suite, fresh target build, XML/static checks, and artifact hashes. Do not rely on pre-hardware build output if source changed during device iteration.

- [ ] **Step 2: Review lifecycle and persistence ordering**

Confirm manually:

- result pointers reference stable `AppIndex` storage;
- rows are deleted before `results_` is replaced;
- recency is recorded and saved before title-open;
- save failure cannot prevent title-open;
- invalid persistence cannot prevent overlay initialization;
- IME mode does not render below the keyboard boundary;
- full mode covers all content below the information bar;
- Circle still uses one-frame deferred close;
- list input is disabled before row/page teardown;
- icon notifications are disabled before rows are cleared;
- the startup config fail-safe remains the first `module_start` action.

- [ ] **Step 3: Present the aggregate project diff once**

Use the project diff tool against the last checkpoint and inspect every changed file. Fix any Critical or Important issue, then rerun the affected verification.

- [ ] **Step 4: Commit the final source revision**

```sh
git status --short
git add CMakeLists.txt cmake/VitaShellPlugin.cmake README.md \
  include/livearea_search/recent_apps.h \
  src/core/recent_apps.cpp \
  src/vita/recent_apps_file.h src/vita/recent_apps_file.cpp \
  src/vita/search_overlay.h src/vita/search_overlay.cpp \
  resources/livearea_search_plugin.xml \
  tests/CMakeLists.txt tests/test_recent_apps.cpp \
  tests/check_recent_apps_file.cmake tests/check_recent_overlay.cmake \
  tests/check_fullscreen_results.cmake tests/check_search_experience.cmake \
  docs/superpowers/specs/2026-11-09-livearea-search-design.md
git diff --cached --check
git commit -m "Add full-screen search and persistent recents"
```

If earlier task commits already contain some paths, stage only remaining changes rather than amending.

- [ ] **Step 5: Push and verify the remote commit**

```sh
git push origin dev/livearea-search
gh api repos/devnoname120/livearea-search/commits/dev/livearea-search \
  --jq '{sha: .sha, message: .commit.message}'
git status --short --branch
```

Expected: remote SHA matches local `HEAD`, message matches the final commit, and the worktree is clean.

- [ ] **Step 6: Complete the active goal**

Report:

- exact root causes of the hidden header, blue bleed, notification overlap, and empty-query behavior;
- final SUPRX/RCO hashes;
- host/sanitizer/target/static results;
- Vita 146 screenshot and interaction evidence;
- persisted Recent semantics including failed title-open;
- kinetic-close regression result;
- final commit SHA and branch.
