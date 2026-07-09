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
# Linux (e.g. Raspberry Pi): installed system-wide via the Galil APT repo.
#   Headers : /usr/include/gclib.h  /usr/include/gclibo.h
#   Library : /usr/lib/libgclib.so
#
# Windows: installed by the Galil gclib installer.
#   Headers : C:/Program Files/Galil/gclib/include
#   Library : C:/Program Files/Galil/gclib/dll (gclib.lib / gclibo.lib import libs,
#             built for MSVC; matching gclib.dll/gclibo.dll must be on PATH
#             at runtime, which you've already done)
#
# Override on either platform with:
#   cmake -DGCLIB_ROOT=/path/to/gclib ..

if(WIN32)
    set(GCLIB_ROOT "C:/Program Files/Galil/gclib" CACHE PATH "gclib root")

    find_path(GCLIB_INCLUDE_DIR gclib.h
        PATHS "${GCLIB_ROOT}/include"
        REQUIRED
    )

    find_library(GCLIB_LIB
        NAMES gclib
        PATHS "${GCLIB_ROOT}/dll" "${GCLIB_ROOT}/lib"
        REQUIRED
    )

    find_library(GCLIBO_LIB
        NAMES gclibo
        PATHS "${GCLIB_ROOT}/dll" "${GCLIB_ROOT}/lib"
        REQUIRED
    )
else()
    find_path(GCLIB_INCLUDE_DIR gclib.h
        PATHS /usr/include /usr/local/include
        REQUIRED
    )

    find_library(GCLIB_LIB
        NAMES gclib
        PATHS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
        REQUIRED
    )

    find_library(GCLIBO_LIB
        NAMES gclibo
        PATHS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
        REQUIRED
    )
endif()

# ── IDS peak SDK ─────────────────────────────────────────────────────────────
# Windows default install path is IDS's standard SDK location; override with:
#   cmake -DIDS_PEAK_ROOT=/your/path ..

if(WIN32)
    set(IDS_PEAK_ROOT "C:/Program Files/IDS/ids_peak"
        CACHE PATH "IDS peak SDK root")

    set(IDS_PEAK_INCLUDE_DIR
        "${IDS_PEAK_ROOT}/generic_sdk/api/include")

    set(IDS_PEAK_COMMON_INCLUDE_DIR
        "${IDS_PEAK_ROOT}/common/include")

    set(IDS_PEAK_IPL_LIB_DIR
        "${IDS_PEAK_ROOT}/generic_sdk/ipl/lib/x86_64")

    set(IDS_PEAK_LIB_DIR
        "${IDS_PEAK_ROOT}/generic_sdk/api/lib/x86_64")

    set(IDS_PEAK_IPL_INCLUDE_DIR
        "${IDS_PEAK_ROOT}/generic_sdk/ipl/include")

else()
    set(IDS_PEAK_ROOT "/opt/ids/ids-peak-with-ueyetl_26.06-720_arm64"
        CACHE PATH "IDS peak SDK root")

    set(IDS_PEAK_INCLUDE_DIR
        "${IDS_PEAK_ROOT}/include/ids-peak")

    set(IDS_PEAK_COMMON_INCLUDE_DIR
        "${IDS_PEAK_ROOT}/include")

    set(IDS_PEAK_LIB_DIR
        "${IDS_PEAK_ROOT}/lib/aarch64-linux-gnu")
endif()

find_path(IDS_PEAK_HEADER
    peak/peak.hpp
    PATHS "${IDS_PEAK_INCLUDE_DIR}"
    REQUIRED
)

find_library(IDS_PEAK_LIB
    NAMES ids_peak
    PATHS "${IDS_PEAK_LIB_DIR}"
    REQUIRED
)

find_library(IDS_PEAK_IPL_LIB
    NAMES ids_peak_ipl
    PATHS "${IDS_PEAK_IPL_LIB_DIR}"
    REQUIRED
)

message(STATUS "IDS peak SDK found at ${IDS_PEAK_ROOT}")
message(STATUS "  IDS peak library: ${IDS_PEAK_LIB}")
message(STATUS "  IDS peak IPL library: ${IDS_PEAK_IPL_LIB}")

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

target_include_directories(TestRig PRIVATE
    "${IDS_PEAK_INCLUDE_DIR}"
    "${IDS_PEAK_COMMON_INCLUDE_DIR}"
    "${IDS_PEAK_IPL_INCLUDE_DIR}"
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(TestRig PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    "${GCLIB_LIB}"
    "${GCLIBO_LIB}"
)

target_link_libraries(TestRig PRIVATE
    "${IDS_PEAK_LIB}"
    "${IDS_PEAK_IPL_LIB}"
)

# ── Install ───────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS TestRig
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Runtime library paths ─────────────────────────────────────────────────────
if(WIN32)
    # Windows has no rpath concept; the required DLLs (gclib.dll, gclibo.dll,
    # ids_peak.dll, ids_peak_ipl.dll, and the Qt DLLs) must be next to the .exe
    # or on PATH. Since gclib's bin dir is already on your PATH, gclib should
    # resolve at runtime. Copy the IDS peak DLLs next to the executable so it
    # runs without extra PATH edits:
    file(GLOB IDS_PEAK_DLLS "${IDS_PEAK_LIB_DIR}/*.dll")
    if(IDS_PEAK_DLLS)
        add_custom_command(TARGET TestRig POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${IDS_PEAK_DLLS} $<TARGET_FILE_DIR:TestRig>
            COMMENT "Copying IDS peak DLLs next to TestRig.exe"
        )
    endif()
else()
    set_target_properties(TestRig PROPERTIES
        BUILD_RPATH "${IDS_PEAK_LIB_DIR}"
    )
endif()

qt_finalize_executable(TestRig)
