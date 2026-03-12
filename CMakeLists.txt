cmake_minimum_required(VERSION 3.20)
project(tfl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CEF paths
set(CEF_ROOT "/tmp/electrobun-fork/vendors" CACHE PATH "CEF binary distribution root")
set(CEF_LIB_DIR "${CEF_ROOT}/Release")
set(CEF_RES_DIR "${CEF_ROOT}/Resources")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_WRAPPER_LIB "${CEF_ROOT}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")

# Find GTK3 and AppIndicator
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)

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
)

target_include_directories(tfl PRIVATE
    ${CEF_ROOT}
    ${CEF_INCLUDE_DIR}/..
    ${GTK3_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    src
)

target_link_libraries(tfl PRIVATE
    ${CEF_WRAPPER_LIB}
    ${CEF_LIB_DIR}/libcef.so
    ${GTK3_LIBRARIES}
    ${APPINDICATOR_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
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

# Symlink CEF binaries and resources flat next to the executable
add_custom_command(TARGET tfl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libcef.so $<TARGET_FILE_DIR:tfl>/libcef.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libEGL.so $<TARGET_FILE_DIR:tfl>/libEGL.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libGLESv2.so $<TARGET_FILE_DIR:tfl>/libGLESv2.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libvk_swiftshader.so $<TARGET_FILE_DIR:tfl>/libvk_swiftshader.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libvulkan.so.1 $<TARGET_FILE_DIR:tfl>/libvulkan.so.1
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/v8_context_snapshot.bin $<TARGET_FILE_DIR:tfl>/v8_context_snapshot.bin
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/vk_swiftshader_icd.json $<TARGET_FILE_DIR:tfl>/vk_swiftshader_icd.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_RES_DIR}/icudtl.dat $<TARGET_FILE_DIR:tfl>/icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_RES_DIR}/chrome_100_percent.pak $<TARGET_FILE_DIR:tfl>/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_RES_DIR}/chrome_200_percent.pak $<TARGET_FILE_DIR:tfl>/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_RES_DIR}/resources.pak $<TARGET_FILE_DIR:tfl>/resources.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_RES_DIR}/locales $<TARGET_FILE_DIR:tfl>/locales
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

# Generate launcher script
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/tfl-launcher
"#!/bin/sh\nexec \"${CMAKE_INSTALL_PREFIX}/${TFL_INSTALL_LIBDIR}/tfl\" \"$@\"\n")
