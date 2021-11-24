set(the_description "Core Functionality")

#file(GLOB source_files
#        "src/*.h"
#        "src/*.cpp"
#        "src/*.c")
#add_executable(${PROJECT_NAME} ${source_files})
#install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_DIR})

# peval library
add_library(peval src/lib/pokerstove/peval/Card.cpp
        src/lib/pokerstove/peval/CardSet.cpp
        src/lib/pokerstove/peval/CardSetGenerators.cpp
        src/lib/pokerstove/peval/PokerEvaluation.cpp
        src/lib/pokerstove/peval/Rank.cpp
        src/lib/pokerstove/peval/Suit.cpp
        src/lib/pokerstove/peval/PokerHandEvaluator.cpp)
target_include_directories(peval PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/lib)

set(CORE_LIB src/game.c
        src/rng.c
        src/net.c
        src/card_util.cpp src/game_util.cpp src/table_util.cpp)
add_library(core STATIC ${CORE_LIB})
#platform specific
target_include_directories(core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# Make test executable
add_executable(test_core ${CMAKE_CURRENT_SOURCE_DIR}/test/test_core.cpp)
add_executable(test_card_util ${CMAKE_CURRENT_SOURCE_DIR}/test/test_card_util.cpp)
add_executable(test_table_util ${CMAKE_CURRENT_SOURCE_DIR}/test/test_table_util.cpp)
# Custom command that runs test on build
#add_custom_command(TARGET test_core
#        POST_BUILD
#        COMMAND test_core
#        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)

if(UNIX AND NOT APPLE)
    target_link_libraries(core stdc++fs peval)
    target_link_libraries(test_core stdc++fs core Catch2::Catch2)
else()
    target_link_libraries(core peval)
    target_link_libraries(test_core core Catch2::Catch2)
endif()

target_link_libraries(test_card_util core Catch2::Catch2 peval)
target_link_libraries(test_table_util core Catch2::Catch2 peval)

include(Catch)
include(CTest)
#adds test to "make test"
catch_discover_tests( test_core )
catch_discover_tests( test_card_util )
catch_discover_tests( test_table_util )
