cmake_minimum_required(VERSION 3.16)
project(TestRig VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt ────────────────────────────────────────────────────────────────────────
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

# ── gclib ─────────────────────────────────────────────────────────────────────
# gclib is installed system-wide via the Galil APT repo.
# Typical install locations on ARM64 Debian:
#   Headers : /usr/include/gclib.h  /usr/include/gclibo.h
#   Library : /usr/lib/libgclib.so
#
# If gclib was installed somewhere else, override with:
#   cmake -DGCLIB_INCLUDE_DIR=/path/to/include -DGCLIB_LIB=/path/to/libgclib.so ..

find_path(GCLIB_INCLUDE_DIR gclib.h
    PATHS /usr/include /usr/local/include
    REQUIRED
)
find_library(GCLIB_LIB
    NAMES gclib
    PATHS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
    REQUIRED
)

# ── IDS peak SDK ─────────────────────────────────────────────────────────────

option(WITH_IDS_PEAK "Build with IDS peak camera support" ON)

if(WITH_IDS_PEAK)

    set(IDS_PEAK_ROOT "/opt/ids/ids-peak-with-ueyetl_26.06-720_arm64"
        CACHE PATH "IDS peak SDK root")

    set(IDS_PEAK_INCLUDE
        "${IDS_PEAK_ROOT}/include/ids-peak")

    set(IDS_PEAK_IPL_INCLUDE
        "${IDS_PEAK_ROOT}/include/ids-peak")

    set(IDS_PEAK_LIB_DIR
        "${IDS_PEAK_ROOT}/lib/aarch64-linux-gnu")

    find_library(IDS_PEAK_LIB
        NAMES ids_peak
        PATHS "${IDS_PEAK_LIB_DIR}"
    )

    find_library(IDS_PEAK_IPL_LIB
        NAMES ids_peak_ipl
        PATHS "${IDS_PEAK_LIB_DIR}"
    )

    if(IDS_PEAK_LIB AND IDS_PEAK_IPL_LIB)
        message(STATUS "IDS peak SDK found at ${IDS_PEAK_ROOT}")
        set(HAS_IDS_PEAK TRUE)
    else()
        message(WARNING
            "IDS peak SDK not found at ${IDS_PEAK_ROOT}. "
            "Camera support will be disabled at runtime.")
        set(HAS_IDS_PEAK FALSE)
    endif()

else()
    set(HAS_IDS_PEAK FALSE)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(PROJECT_SOURCES
    src/main.cpp

    # Core / Galil
    src/printer.h
    src/printer.cpp
    src/galilcontroller.h
    src/galilcontroller.cpp

    # JetForge (Added Scientific MJ printhead)
    src/asyncserialdevice.h
    src/asyncserialdevice.cpp
    src/mfjdrv.h
    src/mjdriver.h
    src/mjdriver.cpp
    src/printheadwidget.h
    src/printheadwidget.cpp

    # Camera / strobe
    src/cameracontroller.h
    src/cameracontroller.cpp
    src/cameraworker.h
    src/cameraworker.cpp
    src/camerawidget.h
    src/camerawidget.cpp
    src/liveviewwidget.h
    src/liveviewwidget.cpp
    src/strobewidget.h
    src/strobewidget.cpp

    # Strobe Teensy
    src/arduinocontroller.h
    src/arduinocontroller.cpp
    src/arduinowidget.h
    src/arduinowidget.cpp

    # Main window
    src/mainwindow.h
    src/mainwindow.cpp
)

# ── Executable ────────────────────────────────────────────────────────────────
qt_add_executable(TestRig MANUAL_FINALIZATION ${PROJECT_SOURCES})

# ── Include paths ────────────────────────────────────────────────────────────
target_include_directories(TestRig PRIVATE
    src/
    "${GCLIB_INCLUDE_DIR}"
)

if(HAS_IDS_PEAK)
    target_include_directories(TestRig PRIVATE
        "${IDS_PEAK_INCLUDE}"
        "${IDS_PEAK_IPL_INCLUDE}"
        "${IDS_PEAK_ROOT}/common/include"
    )
    target_compile_definitions(TestRig PRIVATE HAS_IDS_PEAK)
endif()

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(TestRig PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    "${GCLIB_LIB}"
)

if(HAS_IDS_PEAK)
    target_link_libraries(TestRig PRIVATE
        "${IDS_PEAK_LIB}"
        "${IDS_PEAK_IPL_LIB}"
    )
endif()

# ── Install ───────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS TestRig
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Runtime library paths ─────────────────────────────────────────────────────
if(HAS_IDS_PEAK)
    set_target_properties(TestRig PROPERTIES
        BUILD_RPATH "/opt/ids/ids-peak-with-ueyetl_26.06-720_arm64/lib/aarch64-linux-gnu"
    )
endif()

qt_finalize_executable(TestRig)
