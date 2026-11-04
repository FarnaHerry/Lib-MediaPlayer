add_executable(media_contract_tests contract.cpp)
target_link_libraries(media_contract_tests PRIVATE huxerui_media HuxerUI::huxerui_static)
target_compile_features(media_contract_tests PRIVATE cxx_std_20)
target_include_directories(media_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME media_contract COMMAND media_contract_tests)

find_program(MEDIA_NODE_EXECUTABLE node)
if (MEDIA_NODE_EXECUTABLE AND EXISTS "${HUXERUI_HOME}/lib/cmake/HuxerUI/HuxerUIWebPlatformRegistry.js")
    add_test(NAME media_web_contract
            COMMAND "${MEDIA_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/web_contract.cjs" "${HUXERUI_HOME}")
endif ()

if (WIN32)
    add_executable(media_windows_video windows_video.cpp)
    target_compile_definitions(media_windows_video PRIVATE NOMINMAX)
    target_link_libraries(media_windows_video PRIVATE huxerui_media HuxerUI::huxerui_static)
    target_compile_features(media_windows_video PRIVATE cxx_std_20)
    target_include_directories(media_windows_video PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/platform/windows/src")
    add_test(NAME media_windows_video_geometry COMMAND media_windows_video)

    add_executable(media_windows_smoke windows_smoke.cpp)
    target_compile_definitions(media_windows_smoke PRIVATE NOMINMAX)
    target_link_libraries(media_windows_smoke PRIVATE huxerui_media HuxerUI::huxerui_static)
    target_compile_features(media_windows_smoke PRIVATE cxx_std_20)
    target_include_directories(media_windows_smoke PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME media_windows_audio COMMAND media_windows_smoke)
    set_tests_properties(media_windows_audio PROPERTIES TIMEOUT 30)
endif ()
