cmake_minimum_required(VERSION 3.20)
project(tfl VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CEF paths
set(CEF_ROOT "/tmp/cef" CACHE PATH "CEF binary distribution root")
set(CEF_LIB_DIR "${CEF_ROOT}/Release")
set(CEF_RES_DIR "${CEF_ROOT}/Resources")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_WRAPPER_LIB "${CEF_ROOT}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")

# Find GTK3 and AppIndicator
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(NSS REQUIRED nss)
pkg_check_modules(ALSA REQUIRED alsa)

# Sources
add_executable(tfl
    src/main.cc
    src/app.cc
    src/client.cc
    src/config.cc
    src/window.cc
    src/tray.cc
    src/notifications.cc
    src/idle.cc
    src/webrtc_fix.cc
    src/openh264.cc
    src/theme.cc
)

target_include_directories(tfl PRIVATE
    ${CEF_ROOT}
    ${CEF_INCLUDE_DIR}/..
    ${GTK3_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${NSS_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    src
)

target_link_libraries(tfl PRIVATE
    ${CEF_WRAPPER_LIB}
    ${CEF_LIB_DIR}/libcef.so
    ${GTK3_LIBRARIES}
    ${APPINDICATOR_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${NSS_LIBRARIES}
    ${ALSA_LIBRARIES}
    pthread
    dl
)

target_link_directories(tfl PRIVATE
    ${CEF_LIB_DIR}
)

# RPATH so it finds libcef.so at runtime (same directory)
set_target_properties(tfl PROPERTIES
    BUILD_RPATH "${CEF_LIB_DIR}"
    INSTALL_RPATH "$ORIGIN"
)

# CEF resolves resources relative to libcef.so's loaded location (via RPATH).
# For dev builds, copy resources next to libcef.so in CEF_LIB_DIR.
# For installs, everything goes to the same TFL_INSTALL_LIBDIR.
add_custom_command(TARGET tfl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/icudtl.dat ${CEF_LIB_DIR}/icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/chrome_100_percent.pak ${CEF_LIB_DIR}/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/chrome_200_percent.pak ${CEF_LIB_DIR}/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/resources.pak ${CEF_LIB_DIR}/resources.pak
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CEF_RES_DIR}/locales ${CEF_LIB_DIR}/locales
    COMMENT "Copying CEF resources next to libcef.so"
)

# Install targets
set(TFL_INSTALL_LIBDIR "lib/tfl" CACHE STRING "Install directory for tfl libs")

install(TARGETS tfl RUNTIME DESTINATION ${TFL_INSTALL_LIBDIR})

# CEF shared libraries
install(FILES
    ${CEF_LIB_DIR}/libcef.so
    ${CEF_LIB_DIR}/libEGL.so
    ${CEF_LIB_DIR}/libGLESv2.so
    ${CEF_LIB_DIR}/libvk_swiftshader.so
    ${CEF_LIB_DIR}/libvulkan.so.1
    ${CEF_LIB_DIR}/v8_context_snapshot.bin
    ${CEF_LIB_DIR}/vk_swiftshader_icd.json
    DESTINATION ${TFL_INSTALL_LIBDIR}
)

# CEF resources
install(FILES
    ${CEF_RES_DIR}/icudtl.dat
    ${CEF_RES_DIR}/chrome_100_percent.pak
    ${CEF_RES_DIR}/chrome_200_percent.pak
    ${CEF_RES_DIR}/resources.pak
    DESTINATION ${TFL_INSTALL_LIBDIR}
)
install(DIRECTORY ${CEF_RES_DIR}/locales DESTINATION ${TFL_INSTALL_LIBDIR})

# Launcher script in bin/
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/tfl-launcher DESTINATION bin RENAME tfl)

# Desktop integration
install(FILES data/tfl.desktop DESTINATION share/applications)
install(FILES data/dev.tfl.teams-for-linux.appdata.xml DESTINATION share/metainfo)
install(FILES data/tfl-app.svg DESTINATION share/icons/hicolor/scalable/apps)
install(DIRECTORY data/themes DESTINATION ${TFL_INSTALL_LIBDIR})

# Generate launcher script
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/tfl-launcher
"#!/bin/sh\nexec \"${CMAKE_INSTALL_PREFIX}/${TFL_INSTALL_LIBDIR}/tfl\" \"$@\"\n")
