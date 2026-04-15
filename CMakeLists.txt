cmake_minimum_required(VERSION 3.12)
project(linapple VERSION 3.0.0)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
set(FRONTEND "sdl3" CACHE STRING "Frontend to build (sdl3 or headless)")
option(PROFILING "Enable profiling" OFF)
option(REGISTRY_WRITEABLE "Enable registry writing" OFF)

# Peripheral Options
option(PERIPHERAL_SPEAKER "Enable built-in speaker peripheral" ON)
option(PERIPHERAL_MOCKINGBOARD "Enable Mockingboard peripheral" ON)
option(PERIPHERAL_DISK "Enable Disk II peripheral" ON)
option(PERIPHERAL_SSC "Enable Super Serial Card peripheral" ON)
option(PERIPHERAL_PRINTER "Enable Parallel Printer peripheral" ON)
option(PERIPHERAL_HARDDISK "Enable Harddisk peripheral" ON)
option(PERIPHERAL_MOUSE "Enable Mouse peripheral" ON)
option(PERIPHERAL_CLOCK "Enable No-Slot Clock peripheral" ON)

# --- Compiler Flags ---
add_compile_options(-O3 -Wall -Wextra)

# Check for LTO support
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# --- Dependencies ---
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

if(FRONTEND STREQUAL "sdl3")
    find_package(SDL3 REQUIRED)
    find_package(SDL3_image REQUIRED)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED libzip)

# Doctest include path
find_path(DOCTEST_INCLUDE_DIR doctest.h PATH_SUFFIXES doctest)
if(NOT DOCTEST_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
      doctest
      GIT_REPOSITORY https://github.com/doctest/doctest.git
      GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
    set(DOCTEST_INCLUDE_DIR ${doctest_SOURCE_DIR}/doctest)
endif()

# --- Tools ---
find_program(MAGICK_EXECUTABLE NAMES magick convert)
if(NOT MAGICK_EXECUTABLE)
    message(FATAL_ERROR "ImageMagick (magick or convert) is required for asset conversion")
endif()

# --- Preprocessor Definitions ---
add_definitions(-DVERSIONSTRING="${PROJECT_VERSION}")
if(FRONTEND STREQUAL "headless")
    add_definitions(-DHEADLESS)
endif()
if(REGISTRY_WRITEABLE)
    add_definitions(-DREGISTRY_WRITEABLE=1)
endif()

# --- Asset Conversion (PNG to XPM) ---
set(RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
set(GEN_RES_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY ${GEN_RES_DIR})

set(PNG_ASSETS
    charset40_british.png
    charset40_french.png
    charset40_german.png
    charset40_IIplus.png
    charset40.png
    splash.png
)

set(XPM_HEADERS "")
foreach(PNG_FILE ${PNG_ASSETS})
    get_filename_component(BASE_NAME ${PNG_FILE} NAME_WE)
    set(XPM_FILE "${GEN_RES_DIR}/${BASE_NAME}.xpm")
    add_custom_command(
        OUTPUT ${XPM_FILE}
        COMMAND ${MAGICK_EXECUTABLE} "${RES_DIR}/${PNG_FILE}" -layers flatten "${XPM_FILE}"
        COMMAND ${CMAKE_COMMAND} -DXPM_FILE=${XPM_FILE} -DBASE_NAME=${BASE_NAME} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConvertXPM.cmake"
        DEPENDS "${RES_DIR}/${PNG_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConvertXPM.cmake"
        COMMENT "Converting ${PNG_FILE} to XPM"
        VERBATIM
    )
    list(APPEND XPM_HEADERS ${XPM_FILE})
endforeach()

add_custom_target(generate_xpms DEPENDS ${XPM_HEADERS})

# --- Source Discovery ---
set(SOURCES_CORE
    src/core/Common.cpp
    src/core/LinAppleCore.cpp
    src/core/Log.cpp
    src/core/Registry.cpp
    src/core/asset.cpp
    src/core/Peripheral.cpp
    src/core/Peripheral_Internal.cpp
)

# Mandatory Apple II hardware
set(SOURCES_A2
    src/apple2/6821.cpp
    src/apple2/CPU.cpp
    src/apple2/DiskImage.cpp
    src/apple2/Joystick.cpp
    src/apple2/Keyboard.cpp
    src/apple2/Memory.cpp
    src/apple2/SaveState.cpp
    src/apple2/SoundCore.cpp
    src/apple2/Timer.cpp
    src/apple2/Video.cpp
    src/apple2/ftpparse.cpp
    src/apple2/stretch.cpp
)

# Optional peripherals
set(PERIPHERAL_DEFINITIONS "")

if(PERIPHERAL_SPEAKER)
    list(APPEND SOURCES_A2 src/apple2/Speaker.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_SPEAKER)
endif()

if(PERIPHERAL_MOCKINGBOARD)
    list(APPEND SOURCES_A2 src/apple2/Mockingboard.cpp src/apple2/AY8910.cpp src/apple2/Riff.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_MOCKINGBOARD)
endif()

if(PERIPHERAL_DISK)
    list(APPEND SOURCES_A2 src/apple2/Disk.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_DISK)
endif()

if(PERIPHERAL_SSC)
    list(APPEND SOURCES_A2 src/apple2/SerialComms.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_SSC)
endif()

if(PERIPHERAL_PRINTER)
    list(APPEND SOURCES_A2 src/apple2/ParallelPrinter.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_PRINTER)
endif()

if(PERIPHERAL_HARDDISK)
    list(APPEND SOURCES_A2 src/apple2/Harddisk.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_HARDDISK)
endif()

if(PERIPHERAL_MOUSE)
    list(APPEND SOURCES_A2 src/apple2/MouseInterface.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_MOUSE)
endif()

if(PERIPHERAL_CLOCK)
    list(APPEND SOURCES_A2 src/apple2/Clock.cpp)
    list(APPEND PERIPHERAL_DEFINITIONS -DENABLE_PERIPHERAL_CLOCK)
endif()

set(SOURCES_DBG "")
if(NOT FRONTEND STREQUAL "headless")
    set(SOURCES_DBG
        src/Debugger/Debug.cpp
        src/Debugger/Debugger_Assembler.cpp
        src/Debugger/Debugger_Bookmarks.cpp
        src/Debugger/Debugger_Breakpoints.cpp
        src/Debugger/Debugger_Cmd_Benchmark.cpp
        src/Debugger/Debugger_Cmd_CPU.cpp
        src/Debugger/Debugger_Cmd_Config.cpp
        src/Debugger/Debugger_Cmd_Disk.cpp
        src/Debugger/Debugger_Cmd_Flags.cpp
        src/Debugger/Debugger_Cmd_Output.cpp
        src/Debugger/Debugger_Cmd_Video.cpp
        src/Debugger/Debugger_Cmd_Window.cpp
        src/Debugger/Debugger_Cmd_ZeroPage.cpp
        src/Debugger/Debugger_Color.cpp
        src/Debugger/Debugger_Commands.cpp
        src/Debugger/Debugger_Console.cpp
        src/Debugger/Debugger_DisassemblerData.cpp
        src/Debugger/Debugger_Display.cpp
        src/Debugger/Debugger_Help.cpp
        src/Debugger/Debugger_Memory.cpp
        src/Debugger/Debugger_Parser.cpp
        src/Debugger/Debugger_Range.cpp
        src/Debugger/Debugger_Symbols.cpp
        src/Debugger/Debugger_View_Code.cpp
        src/Debugger/Debugger_View_Console.cpp
        src/Debugger/Debugger_View_Memory.cpp
        src/Debugger/Util_MemoryTextFile.cpp
    )
endif()

if(FRONTEND STREQUAL "sdl3")
    set(SOURCES_FE
        src/frontends/sdl3/DiskChoose.cpp
        src/frontends/sdl3/DiskFTP.cpp
        src/frontends/sdl3/DiskSelect.cpp
        src/frontends/sdl3/Frame.cpp
        src/frontends/sdl3/Harddisk.cpp
        src/frontends/sdl3/JoystickFrontend.cpp
        src/frontends/sdl3/Keyboard.cpp
        src/frontends/sdl3/Main.cpp
        src/frontends/sdl3/MainSession.cpp
        src/frontends/sdl3/PrinterFrontend.cpp
        src/frontends/sdl3/SDL_Asset.cpp
        src/frontends/sdl3/SDL_Input.cpp
        src/frontends/sdl3/SDL_Path.cpp
        src/frontends/sdl3/SDL_Video.cpp
        src/frontends/sdl3/SerialCommsFrontend.cpp
    )
elseif(FRONTEND STREQUAL "headless")
    set(SOURCES_FE
        src/frontends/headless/Main.cpp
        src/frontends/headless/Stubs.cpp
        src/frontends/headless/Headless_Path.cpp
    )
endif()

set(SOURCES ${SOURCES_CORE} ${SOURCES_A2} ${SOURCES_DBG} ${SOURCES_FE})

# --- Target ---
add_executable(linapple ${SOURCES})
add_dependencies(linapple generate_xpms)

target_include_directories(linapple PRIVATE
    src
    src/core
    src/apple2
    src/Debugger
    src/frontends/${FRONTEND}
    ${GEN_RES_DIR}
    ${RES_DIR}
    ${CURL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIRS}
)

if(FRONTEND STREQUAL "sdl3")
    target_link_libraries(linapple PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
    )
endif()

target_link_libraries(linapple PRIVATE
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LIBZIP_LIBRARIES}
    Threads::Threads
)

# --- Installation ---
include(GNUInstallDirs)

# Default installation paths
set(BIN_DIR "${CMAKE_INSTALL_BINDIR}")
set(DATA_DIR "${CMAKE_INSTALL_DATADIR}/linapple")
set(CONF_DIR "${CMAKE_INSTALL_SYSCONFDIR}/linapple")

target_compile_definitions(linapple PRIVATE 
    ASSET_DIR="${CMAKE_INSTALL_PREFIX}/${DATA_DIR}"
    SYS_DIR="${CMAKE_INSTALL_PREFIX}/${CONF_DIR}"
    ${PERIPHERAL_DEFINITIONS}
)

install(TARGETS linapple DESTINATION ${BIN_DIR})
install(FILES "${RES_DIR}/Master.dsk" DESTINATION ${DATA_DIR})
install(FILES "${RES_DIR}/linapple.conf" DESTINATION ${CONF_DIR})

# --- Tests ---
enable_testing()

# Test Integration
add_executable(test-integration tests/test_main.cpp ${SOURCES_CORE} ${SOURCES_A2} ${SOURCES_DBG} src/frontends/headless/Stubs.cpp src/frontends/headless/Headless_Path.cpp)
add_dependencies(test-integration generate_xpms)
target_include_directories(test-integration PRIVATE src src/core src/apple2 src/Debugger ${GEN_RES_DIR} ${RES_DIR} ${DOCTEST_INCLUDE_DIR} ${RES_DIR})
target_compile_definitions(test-integration PRIVATE ${PERIPHERAL_DEFINITIONS})
target_link_libraries(test-integration PRIVATE ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} Threads::Threads)
add_test(NAME test-integration COMMAND test-integration)

# Test SSC
add_executable(test-ssc tests/test_ssc.cpp src/apple2/SerialComms.cpp src/core/Common.cpp src/core/Log.cpp src/frontends/headless/Headless_Path.cpp)
target_include_directories(test-ssc PRIVATE src src/core src/apple2 ${DOCTEST_INCLUDE_DIR} ${RES_DIR})
target_compile_definitions(test-ssc PRIVATE ${PERIPHERAL_DEFINITIONS})
target_link_libraries(test-ssc PRIVATE ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} Threads::Threads)
add_test(NAME test-ssc COMMAND test-ssc)

# Test Keyboard
if(FRONTEND STREQUAL "sdl3")
    add_executable(test-keyboard tests/full_keyboard_test.cpp src/core/Common.cpp src/core/Log.cpp src/frontends/headless/Headless_Path.cpp)
    target_include_directories(test-keyboard PRIVATE src src/core ${DOCTEST_INCLUDE_DIR} ${RES_DIR})
    target_compile_definitions(test-keyboard PRIVATE ${PERIPHERAL_DEFINITIONS})
    target_link_libraries(test-keyboard PRIVATE SDL3::SDL3 ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} Threads::Threads)
    add_test(NAME test-keyboard COMMAND test-keyboard)
endif()

# Test ABI
add_executable(test-abi tests/test_abi.cpp ${SOURCES_CORE} ${SOURCES_A2} ${SOURCES_DBG} src/frontends/headless/Stubs.cpp src/frontends/headless/Headless_Path.cpp)
add_dependencies(test-abi generate_xpms)
target_include_directories(test-abi PRIVATE src src/core src/apple2 src/Debugger ${GEN_RES_DIR} ${RES_DIR} ${DOCTEST_INCLUDE_DIR})
target_compile_definitions(test-abi PRIVATE ${PERIPHERAL_DEFINITIONS})
target_link_libraries(test-abi PRIVATE ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} Threads::Threads)
add_test(NAME test-abi COMMAND test-abi)

# Test Peripheral ABI (C and C++)
add_executable(test-peripheral-abi 
    tests/test_main.cpp
    tests/test_peripheral_abi.cpp 
    tests/test_peripheral_abi_c.c 
    ${SOURCES_CORE} 
    ${SOURCES_A2} 
    ${SOURCES_DBG} 
    src/frontends/headless/Stubs.cpp 
    src/frontends/headless/Headless_Path.cpp
)
add_dependencies(test-peripheral-abi generate_xpms)
target_include_directories(test-peripheral-abi PRIVATE src src/core src/apple2 src/Debugger ${GEN_RES_DIR} ${RES_DIR} ${DOCTEST_INCLUDE_DIR})
target_compile_definitions(test-peripheral-abi PRIVATE ${PERIPHERAL_DEFINITIONS})
target_link_libraries(test-peripheral-abi PRIVATE ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} Threads::Threads)
add_test(NAME test-peripheral-abi COMMAND test-peripheral-abi)

# CPU tests (using the built linapple binary)
add_test(NAME test-cpu COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_cpu_tests.sh")
set_tests_properties(test-cpu PROPERTIES ENVIRONMENT "EMULATOR=$<TARGET_FILE:linapple>")
