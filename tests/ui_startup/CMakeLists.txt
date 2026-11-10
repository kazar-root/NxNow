cmake_minimum_required(VERSION 3.20)
project(nxnow_ui_startup LANGUAGES C CXX)
get_filename_component(ROOT "../.." ABSOLUTE)
option(NXNOW_UI_SANITIZERS "Use address and undefined behavior sanitizers" ON)
if(NXNOW_UI_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
set(PLATFORM_DESKTOP ON)
set(USE_GLFW ON)
set(USE_SDL2 OFF)
set(BRLS_UNITY_BUILD OFF)
set(BRLS_RESOURCES_DIR "${ROOT}")
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
include("${ROOT}/extern/borealis/library/cmake/commonOption.cmake")
include("${ROOT}/extern/borealis/library/cmake/toolchain.cmake")
add_subdirectory("${ROOT}/extern/borealis/library" borealis)
if(APPLE)
  # Yoga's -fno-rtti emits a private std::exception typeinfo with recent
  # Apple Clang, preventing catches of libc++ exceptions in this host target.
  target_compile_options(yogacore PRIVATE -frtti)
endif()
# Use actual Borealis core, layout and NanoVG, with no OS window or GPU.
get_target_property(BRLS_SOURCES borealis SOURCES)
list(FILTER BRLS_SOURCES EXCLUDE REGEX "/core/platform.cpp$|/platforms/glfw/|/platforms/desktop/")
set_property(TARGET borealis PROPERTY SOURCES "${BRLS_SOURCES}")
find_library(JANSSON_LIBRARY jansson HINTS /tmp/nxnow-jansson-host/lib REQUIRED)
find_path(JANSSON_INCLUDE jansson.h HINTS /tmp/nxnow-jansson-host/include REQUIRED)
add_executable(ui_startup headless_platform.cpp ui_startup.cpp
    "${ROOT}/tests/ui_layout/ui_layout_stubs.cpp"
    "${ROOT}/app/src/main_tabs_view.cpp"
    "${ROOT}/app/src/main_activity.cpp"
    "${ROOT}/app/src/library_tab.cpp"
    "${ROOT}/app/src/game_carousel.cpp"
    "${ROOT}/app/src/console_widgets.cpp"
    "${ROOT}/app/src/ui_image_lifecycle.cpp"
    "${ROOT}/app/src/library_favorites.cpp"
    "${ROOT}/app/src/gfn/manual_library.cpp"
    "${ROOT}/app/src/update_client.cpp"
    "${ROOT}/app/src/library_row_view.cpp"
    "${ROOT}/app/src/catalog_tab.cpp"
    "${ROOT}/app/src/settings_tab.cpp"
    "${ROOT}/app/src/settings_tab_pages.cpp"
    "${ROOT}/app/src/controls_help_view.cpp"
    "${ROOT}/app/src/settings_tab_actions.cpp"
    "${ROOT}/app/src/settings_tab_account.cpp"
    "${ROOT}/app/src/providers_tab.cpp"
    "${ROOT}/app/src/qr_login_dialog.cpp"
    "${ROOT}/app/src/qrcodegen.c"
    "${ROOT}/app/src/ui_action_guard.cpp"
    "${ROOT}/app/src/ui_helpers.cpp"
    "${ROOT}/app/src/queue_view.cpp"
    "${ROOT}/app/src/stream_overlay_view.cpp"
    "${ROOT}/app/src/ui_theme.cpp"
    "${ROOT}/app/src/stream_settings.cpp"
    "${ROOT}/app/src/stream_diagnostics.cpp"
    "${ROOT}/app/src/top_bar_frame.cpp"
    "${ROOT}/app/src/game_browser_header.cpp"
    "${ROOT}/app/src/game_card_view.cpp"
    "${ROOT}/app/src/game_detail_view.cpp"
    "${ROOT}/app/src/app_state.cpp"
    "${ROOT}/app/src/keyboard_layout.cpp"
    "${ROOT}/app/src/localization.cpp")
target_compile_features(ui_startup PRIVATE cxx_std_20)
target_include_directories(ui_startup PRIVATE "${ROOT}/app/src" "${ROOT}/tests/ui_layout" "${JANSSON_INCLUDE}")
target_link_libraries(ui_startup PRIVATE borealis "${JANSSON_LIBRARY}")
target_compile_definitions(ui_startup PRIVATE NXNOW_UI_HEADLESS NXNOW_UI_SOURCE_ROOT="${ROOT}")
if(APPLE)
  target_link_options(ui_startup PRIVATE "-Wl,-dead_strip")
endif()
