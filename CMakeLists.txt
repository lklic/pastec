cmake_minimum_required(VERSION 3.5)
project(Pastec VERSION 1.0.0)

# Include FetchContent for SimSIMD
include(FetchContent)

# Fetch SimSIMD
FetchContent_Declare(
    simsimd
    GIT_REPOSITORY https://github.com/ashvardanian/simsimd.git
    GIT_TAG v6.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simsimd)

# Fix simsimd include directories to use generator expressions for proper installation
if(TARGET simsimd)
    get_target_property(SIMSIMD_INCLUDE_DIRS simsimd INTERFACE_INCLUDE_DIRECTORIES)
    if(SIMSIMD_INCLUDE_DIRS)
        set_target_properties(simsimd PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
            "$<BUILD_INTERFACE:${SIMSIMD_INCLUDE_DIRS}>;$<INSTALL_INTERFACE:include>")
    endif()
endif()

# Include directories
include_directories(include include/orb include/pastec ${Boost_INCLUDE_DIRS})

# Define library sources (core functionality)
set(PASTEC_LIB_SOURCES
    src/imagereranker.cpp
    src/imagererankerransac.cpp
    src/imageloader.cpp
    src/jsoncpp.cpp
    src/imagedownloader.cpp
    src/batchprocessor.cpp
    src/orb/orbfeatureextractor.cpp
    src/orb/orbindex.cpp
    src/orb/orbsearcher.cpp
    src/orb/orbwordindex.cpp)

# Define executable sources (HTTP server and main.cpp)
set(PASTEC_EXE_SOURCES
    src/httpserver.cpp
    src/requesthandler.cpp
    src/main.cpp)

# Define headers
set(PASTEC_HEADERS
    include/thread.h
    include/messages.h
    include/hit.h
    include/searchResult.h
    include/imagereranker.h
    include/backwardindexreaderaccess.h
    include/imageloader.h
    include/index.h
    include/featureextractor.h
    include/imagedownloader.h
    include/batchprocessor.h
    include/json/json-forwards.h
    include/json/json.h
    include/orb/orbfeatureextractor.h
    include/orb/orbindex.h
    include/orb/orbsearcher.h
    include/orb/orbwordindex.h
    include/searcher.h
    include/httpserver.h
    include/requesthandler.h
    include/pastec/pastec.h
    include/pastec/core/index.h
    include/pastec/core/searcher.h
    include/pastec/core/featureextractor.h
    include/pastec/core/imageloader.h
    include/pastec/core/hit.h
    include/pastec/core/searchResult.h
    include/pastec/core/imagedownloader.h
    include/pastec/core/orb/orbindex.h
    include/pastec/core/orb/orbsearcher.h
    include/pastec/core/orb/orbfeatureextractor.h
    include/pastec/core/orb/orbwordindex.h)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Enhanced optimization flags for Release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG -march=native -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -Wl,--gc-sections")
endif()

# Options to control what gets built
# Library is always built, not optional
set(BUILD_PASTEC_LIB ON CACHE BOOL "Build Pastec as a library" FORCE)
option(BUILD_PASTEC_EXE "Build Pastec executable with HTTP server" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(INSTALL_TARGETS "Install targets and generate package files" ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(mimalloc)

# Only need microhttpd for the executable
if(BUILD_PASTEC_EXE)
    find_package(microhttpd REQUIRED)
endif()

# Build the library
if(BUILD_PASTEC_LIB)
    add_library(pastec ${PASTEC_LIB_SOURCES} ${PASTEC_HEADERS})
    target_include_directories(pastec PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_link_libraries(pastec ${CMAKE_THREAD_LIBS_INIT} ${OpenCV_LIBS} ${CURL_LIBRARIES} ${Boost_LIBRARIES} simsimd)
    
    # Link mimalloc if found
    if(MIMALLOC_FOUND)
        target_link_libraries(pastec ${MIMALLOC_LIBRARIES})
        target_compile_definitions(pastec PRIVATE -DUSE_MIMALLOC)
        message(STATUS "Building library with mimalloc allocator")
    endif()
    
    # Install targets if enabled
    if(INSTALL_TARGETS)
        install(TARGETS pastec simsimd
            EXPORT PastecTargets
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include)
        
        # Install headers
        install(DIRECTORY include/pastec
            DESTINATION include
            FILES_MATCHING PATTERN "*.h")
    endif()
endif()

# Build the executable
if(BUILD_PASTEC_EXE)
    add_executable(pastec_server ${PASTEC_EXE_SOURCES})
    
    if(BUILD_PASTEC_LIB)
        # Link against the library if we're building it
        target_link_libraries(pastec_server pastec ${LIBMICROHTTPD_LIBRARY} ${Boost_LIBRARIES})
    else()
        # Otherwise, include all sources directly
        target_sources(pastec_server PRIVATE ${PASTEC_LIB_SOURCES})
        target_link_libraries(pastec_server ${CMAKE_THREAD_LIBS_INIT} ${OpenCV_LIBS} ${CURL_LIBRARIES} ${LIBMICROHTTPD_LIBRARY} ${Boost_LIBRARIES} simsimd)
    endif()
    
    # Link mimalloc if found
    if(MIMALLOC_FOUND)
        target_link_libraries(pastec_server ${MIMALLOC_LIBRARIES})
        target_compile_definitions(pastec_server PRIVATE -DUSE_MIMALLOC)
        message(STATUS "Building executable with mimalloc allocator")
    else()
        message(STATUS "mimalloc not found. For better performance, install libmimalloc-dev")
        message(STATUS "On Ubuntu: sudo apt-get install libmimalloc-dev")
        message(STATUS "Alternatively, you can use LD_PRELOAD to use mimalloc at runtime")
    endif()
    
    # Install executable if enabled
    if(INSTALL_TARGETS)
        install(TARGETS pastec_server
            RUNTIME DESTINATION bin)
    endif()
endif()

# Generate and install package configuration files if enabled
if(BUILD_PASTEC_LIB AND INSTALL_TARGETS)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/PastecConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PastecConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/PastecConfig.cmake"
        @ONLY)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/PastecConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/PastecConfigVersion.cmake"
        DESTINATION lib/cmake/Pastec)

    install(EXPORT PastecTargets
        FILE PastecTargets.cmake
        NAMESPACE Pastec::
        DESTINATION lib/cmake/Pastec)
        
    # Build examples if library is being built
    option(BUILD_EXAMPLES "Build examples" ON)
    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()
