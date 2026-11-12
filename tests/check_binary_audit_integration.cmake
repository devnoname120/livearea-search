if(NOT DEFINED ROOT_CMAKE OR
   NOT DEFINED BUILD_SCRIPT OR
   NOT DEFINED AUDIT_SOURCE OR
   NOT DEFINED CONTRACT_FILE)
    message(FATAL_ERROR "All binary-audit integration paths are required")
endif()

file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${BUILD_SCRIPT}" build_script)
file(READ "${AUDIT_SOURCE}" audit_source)
file(READ "${CONTRACT_FILE}" contract_file)

foreach(required IN ITEMS
    "if(LIVEAREA_SEARCH_DIAGNOSTICS AND NOT LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE)"
    "if(LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE AND NOT LIVEAREA_SEARCH_DIAGNOSTICS)")
    string(FIND "${root_cmake}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Vita build modes are not fail-closed: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "tools/firmware/release_binary_audit.py"
    "tools/firmware/release_binary_contract.json"
    "BINARY_AUDIT_PROFILE=release"
    "BINARY_AUDIT_PROFILE=experimental"
    "--profile \"$BINARY_AUDIT_PROFILE\""
    "--binary \"$LIVEAREA_BUILD_DIR/livearea_search_vita.velf\""
    "--json-out \"$LIVEAREA_BUILD_DIR/binary-audit.json\"")
    string(FIND "${build_script}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Vita build does not enforce the binary audit: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "def audit_binary("
    "forbidden import function NID"
    "forbidden string"
    "required import function NID")
    string(FIND "${audit_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Binary auditor is missing contract enforcement: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "\"release\""
    "\"experimental\""
    "\"0xFA26BC62\""
    "\"ur0:tai/config.txt\""
    "\"ur0:data/livearea-search/startup.log\"")
    string(FIND "${contract_file}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Binary contract is missing release/experimental evidence: ${required}")
    endif()
endforeach()
