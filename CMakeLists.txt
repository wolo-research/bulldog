cmake_minimum_required(VERSION 3.10)

# SetAvg project name, version and laguages here.
# Version numbers are available by including "exampleConfig.h" in
# the source. See exampleConfig.h.in for some more details.
project(bulldog)

set(HOME_DIR ${CMAKE_SOURCE_DIR})

# specify c++17 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set default cmake build type to release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

#set c++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/Build")

#set executable installation directory, default /usr/local/bin
set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

#set file paths
add_definitions(-DBULLDOG_DIR_LOG="${CMAKE_CURRENT_SOURCE_DIR}/logs")
add_definitions(-DBULLDOG_DIR_DATA_ABS="${CMAKE_CURRENT_SOURCE_DIR}/data/abstraction")
add_definitions(-DBULLDOG_DIR_DATA_STG="${CMAKE_CURRENT_SOURCE_DIR}/data/strategy")
add_definitions(-DBULLDOG_DIR_DATA_LAB="${CMAKE_CURRENT_SOURCE_DIR}/data/lab")
add_definitions(-DBULLDOG_DIR_CFG_GAME="${CMAKE_CURRENT_SOURCE_DIR}/config/games")
add_definitions(-DBULLDOG_DIR_CFG_ENG="${CMAKE_CURRENT_SOURCE_DIR}/config/engine")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEV=2)
    #    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    #    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=[sanitizer_name] [additional_options] [-g] [-OX]")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -O1")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
elseif (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_definitions(-DDEV=1)
endif ()

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    message(WARNING "please update cmake")
endif ()

# Fix RPATH https://stackoverflow.com/questions/30398238/cmake-rpath-not-working-could-not-find-shared-object-file
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${HOME_DIR}/3rdparty/Catch2/contrib/")

#Add submodule dependencies
add_subdirectory(3rdparty/Catch2)
#submodules that are headeronly
include_directories(3rdparty/cxxopts/include)
include_directories(3rdparty/spdlog/include)
include_directories(3rdparty/cereal/include)
option(THREAD_SAFE ON)

#These packages need to be installed locally, i.e. with brew / apt-get etc
if (UNIX AND NOT APPLE)
    set(cpprestsdk_DIR /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/)
    #find_package(UUID REQUIRED uuid)
elseif (APPLE)
    #this is not safe, should add a FindTBB.cmake to automatially find and include the right directories
    #do something like this https://github.com/justusc/FindTBB
    include_directories(/usr/local/Cellar/parallelstl/20200330/stdlib)
    include_directories(/usr/local/Cellar/parallelstl/20200330/include)
#    include_directories(/usr/local/Cellar/tbb/2020_U2/include)
#    link_directories(/usr/local/Cellar/tbb/2020_U2/lib)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif ()
find_package(cpprestsdk REQUIRED)
find_package(Threads REQUIRED)
#
# set up boost
#
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED system)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})
message(STATUS "Boost Include: ${Boost_INCLUDE_DIR}")
message(STATUS "Boost Library Dir: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost Libraries: ${Boost_LIBRARIES}")

message(STATUS "C++ Flags:${CMAKE_CXX_FLAGS}")

add_subdirectory(modules/core)
add_subdirectory(modules/engine)
add_subdirectory(modules/agent)

#Enable "make test" command
enable_testing()
