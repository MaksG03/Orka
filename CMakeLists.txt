cmake_minimum_required(VERSION 3.15)
project(Orka VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build configuration ─────────────────────────────────────────────
option(ENTERPRISE_BUILD "Enterprise: no global hooks, UIA-only" OFF)
option(BUILD_TESTS      "Build unit-test runner"                 ON)

if(ENTERPRISE_BUILD)
    add_compile_definitions(ENTERPRISE_BUILD)
    message(STATUS "[ORKA] Enterprise build — global hooks DISABLED")
else()
    message(STATUS "[ORKA] Consumer build  — global hooks ENABLED")
endif()

# ── Core sources ────────────────────────────────────────────────────
set(CORE_SOURCES
    src/core/language_engine.cpp
    src/core/language_engine.h
    src/core/hangul_engine.cpp
    src/core/hangul_engine.h
    src/core/hebrew_engine.cpp
    src/core/hebrew_engine.h
    src/core/utf8_utils.h
)

# ── ICU (required for Hebrew BiDi v2.0) ─────────────────────────────
find_package(ICU COMPONENTS uc data)
if(ICU_FOUND)
    message(STATUS "[ORKA] ICU found — Hebrew engine ENABLED")
    add_compile_definitions(ORKA_HAS_ICU)
else()
    message(WARNING "[ORKA] ICU not found — Hebrew engine DISABLED")
endif()

# ── libhangul (optional, for Korean v1.5) ───────────────────────────
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(HANGUL libhangul)
endif()
if(HANGUL_FOUND)
    message(STATUS "[ORKA] libhangul found — Korean engine ENABLED")
    add_compile_definitions(ORKA_HAS_HANGUL)
else()
    message(WARNING "[ORKA] libhangul not found — Korean engine uses built-in Dubeolsik table")
endif()

# ── Platform sources ────────────────────────────────────────────────
if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/win/selection_monitor.cpp
        src/platform/win/selection_monitor.h
        src/platform/win/text_injector.cpp
        src/platform/win/text_injector.h
    )
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_SOURCES
        src/platform/linux/selection_monitor.cpp
        src/platform/linux/selection_monitor.h
        src/platform/linux/text_injector.cpp
        src/platform/linux/text_injector.h
    )
endif()

# ── UI sources ──────────────────────────────────────────────────────
set(UI_SOURCES
    src/ui/overlay.cpp
    src/ui/overlay.h
)

# ── Main executable ─────────────────────────────────────────────────
add_executable(orka
    src/main.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${UI_SOURCES}
)

target_include_directories(orka PRIVATE src)

# Link ICU
if(ICU_FOUND)
    target_include_directories(orka PRIVATE ${ICU_INCLUDE_DIRS})
    target_link_libraries(orka PRIVATE ICU::uc ICU::data)
endif()

# Link libhangul
if(HANGUL_FOUND)
    target_include_directories(orka PRIVATE ${HANGUL_INCLUDE_DIRS})
    target_link_libraries(orka PRIVATE ${HANGUL_LIBRARIES})
endif()

# Platform libraries
if(WIN32)
    target_link_libraries(orka PRIVATE user32 gdi32 ole32 oleaut32 uiautomationcore oleacc)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_include_directories(orka PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(orka PRIVATE ${X11_LIBRARIES} Xfixes)
endif()

# ── Test runner ─────────────────────────────────────────────────────
if(BUILD_TESTS)
    add_executable(orka_tests
        tests/test_language_engine.cpp
        ${CORE_SOURCES}
    )
    target_include_directories(orka_tests PRIVATE src)
    if(ICU_FOUND)
        target_include_directories(orka_tests PRIVATE ${ICU_INCLUDE_DIRS})
        target_link_libraries(orka_tests PRIVATE ICU::uc ICU::data)
    endif()
    if(HANGUL_FOUND)
        target_include_directories(orka_tests PRIVATE ${HANGUL_INCLUDE_DIRS})
        target_link_libraries(orka_tests PRIVATE ${HANGUL_LIBRARIES})
    endif()
endif()
