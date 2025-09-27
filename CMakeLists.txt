cmake_minimum_required(VERSION 3.16)
project(MinecraftLauncher VERSION 2.4.104 LANGUAGES CXX RC)

# Set C++ standard and features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific compiler flags
if(MSVC)
    # MSVC-specific optimization flags - fixed runtime library conflicts
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /MDd /DDEBUG /EHsc")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /MD /EHsc")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG /MD /EHsc")

    # Enable parallel compilation and modern C++
    add_compile_options(/MP)
    add_compile_options(/permissive-)
    add_compile_options(/W3)  # Changed from /W4 to reduce warnings
    add_compile_options(/EHsc)  # Enable exception handling

    # Disable specific warnings that are too verbose
    add_compile_options(/wd4996) # Disable deprecation warnings
    add_compile_options(/wd4267) # Disable size_t conversion warnings
    add_compile_options(/wd4244) # Disable conversion warnings
    add_compile_options(/wd4530) # Disable exception handler warnings

    # Set consistent runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    # GCC/Clang-specific flags
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
endif()

# Find required packages with Windows-friendly approach
find_package(Threads REQUIRED)

# Find cURL with vcpkg support
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "cURL not found. Please install via vcpkg: vcpkg install curl")
endif()

# Find nlohmann/json with vcpkg support and fallback options
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    # Try alternative package name
    find_package(nlohmann_json 3.7.0 QUIET)
    if(NOT nlohmann_json_FOUND)
        # Only use pkg-config on Unix systems where it's available
        if(UNIX AND NOT APPLE)
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(JSON nlohmann_json>=3.7.0)
            endif()
        endif()

        # If still not found, download it
        if(NOT JSON_FOUND AND NOT nlohmann_json_FOUND)
            message(STATUS "nlohmann_json not found locally, downloading...")
            include(FetchContent)
            FetchContent_Declare(
                nlohmann_json
                URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
                URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d0
            )
            FetchContent_MakeAvailable(nlohmann_json)
        endif()
    endif()
endif()

# Create source file groups for better organization
set(HEADER_FILES
    include/archive.h
    include/config.h
    include/crypto.h
    include/download.h
    include/java.h
    include/logging.h
    include/minecraft.h
)

set(SOURCE_FILES
    main.cpp
    minecraft.cpp
    download.cpp
    config.cpp
    logging.cpp
    crypto.cpp
    java.cpp
    archive.cpp
    plugin_downloader.cpp
)

set(RESOURCE_FILES
    MinecraftLauncher.rc
    favicon.ico
)

# Create the executable
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${RESOURCE_FILES}
)

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "PurrLauncher"
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries with proper error handling
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# Link cURL
if(TARGET CURL::libcurl)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Link nlohmann_json
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
elseif(TARGET nlohmann_json)
    target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)
elseif(JSON_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${JSON_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${JSON_LIBRARIES})
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        crypt32
        wininet
        ws2_32
        advapi32
        user32
    )
endif()

# Conditional compilation features
option(ENABLE_PLUGIN_DOWNLOAD "Enable automatic plugin downloading" ON)
option(ENABLE_PERFORMANCE_LOGGING "Enable detailed performance logging" OFF)
option(ENABLE_MEMORY_DEBUGGING "Enable memory debugging features" OFF)

if(ENABLE_PLUGIN_DOWNLOAD)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PLUGIN_DOWNLOAD)
endif()

if(ENABLE_PERFORMANCE_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PERFORMANCE_LOGGING)
endif()

if(ENABLE_MEMORY_DEBUGGING AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MEMORY_DEBUGGING)
endif()

# Platform-specific optimizations
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )

    # Set subsystem for release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        )
    endif()
endif()

# Create installation targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

# Install additional files
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION .
    OPTIONAL
)

# Create launcher_version.txt during build
file(WRITE "${CMAKE_BINARY_DIR}/launcher_version.txt" "${PROJECT_VERSION}")
install(FILES "${CMAKE_BINARY_DIR}/launcher_version.txt" DESTINATION .)

# Custom targets for development
add_custom_target(format
    COMMAND clang-format -i ${SOURCE_FILES} ${HEADER_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting source code"
)

add_custom_target(clean-logs
    COMMAND ${CMAKE_COMMAND} -E remove -f launcher.log
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning log files"
)

# Copy runtime dependencies for Windows
if(WIN32)
    # Copy cURL DLL if available
    if(EXISTS "${CURL_LIBRARY_DIR}/../bin/libcurl.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CURL_LIBRARY_DIR}/../bin/libcurl.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endif()

# Development helper targets
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Create debug configuration
    add_custom_target(debug-run
        COMMAND ${PROJECT_NAME}
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running ${PROJECT_NAME} in debug mode"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Plugin download: ${ENABLE_PLUGIN_DOWNLOAD}")
message(STATUS "Performance logging: ${ENABLE_PERFORMANCE_LOGGING}")
message(STATUS "Memory debugging: ${ENABLE_MEMORY_DEBUGGING}")
message(STATUS "cURL version: ${CURL_VERSION_STRING}")
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann_json: Found via CMake")
elseif(JSON_FOUND)
    message(STATUS "nlohmann_json: Found via pkg-config")
endif()
message(STATUS "==============================")
message(STATUS "")
