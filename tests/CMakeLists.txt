add_executable(mediaplayer_contract_tests contract.cpp)
target_link_libraries(mediaplayer_contract_tests PRIVATE huxerui_mediaplayer HuxerUI::huxerui_static)
target_compile_features(mediaplayer_contract_tests PRIVATE cxx_std_20)
target_include_directories(mediaplayer_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME mediaplayer_contract COMMAND mediaplayer_contract_tests)

find_program(MEDIAPLAYER_NODE_EXECUTABLE node)
if (MEDIAPLAYER_NODE_EXECUTABLE AND EXISTS "${HUXERUI_HOME}/lib/cmake/HuxerUI/HuxerUIWebPlatformRegistry.js")
    add_test(NAME mediaplayer_web_contract
            COMMAND "${MEDIAPLAYER_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/web_contract.cjs" "${HUXERUI_HOME}")
endif ()

if (WIN32)
    add_executable(mediaplayer_windows_video windows_video.cpp)
    target_compile_definitions(mediaplayer_windows_video PRIVATE NOMINMAX)
    target_link_libraries(mediaplayer_windows_video PRIVATE huxerui_mediaplayer HuxerUI::huxerui_static)
    target_compile_features(mediaplayer_windows_video PRIVATE cxx_std_20)
    target_include_directories(mediaplayer_windows_video PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/platform/windows/src")
    add_test(NAME mediaplayer_windows_video_geometry COMMAND mediaplayer_windows_video)

    add_executable(mediaplayer_windows_smoke windows_smoke.cpp)
    target_compile_definitions(mediaplayer_windows_smoke PRIVATE NOMINMAX)
    target_link_libraries(mediaplayer_windows_smoke PRIVATE huxerui_mediaplayer HuxerUI::huxerui_static)
    target_compile_features(mediaplayer_windows_smoke PRIVATE cxx_std_20)
    target_include_directories(mediaplayer_windows_smoke PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME mediaplayer_windows_audio COMMAND mediaplayer_windows_smoke)
    set_tests_properties(mediaplayer_windows_audio PROPERTIES TIMEOUT 30)
endif ()
