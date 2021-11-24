set(the_description "Poker Engine")

set(ENGINE_LIB src/bucket_reader.hpp
        src/strategy.cpp
        src/strategy_io.cpp
        src/cfr.cpp
        src/node.cpp
        src/action_abs.cpp
        src/cfr_worker.cpp
        src/cfr_worker_scalar.cpp
        src/bucket.cpp
        src/term_eval_kernel.cpp
        src/subgame_solver.hpp
        src/cfr_command.hpp
        src/action_chooser.hpp src/hand_belief.cpp src/engine.cpp src/abstract_game.cpp src/engine_util.cpp src/strategy_pool.hpp)
add_library(engine ${ENGINE_LIB})
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_executable(train src/train.cpp)
add_executable(check_strategy src/check_strategy.cpp)
add_executable(check_kernel src/check_kernel_size.cpp)
add_executable(eval-h2h src/eval-h2h.cpp)
add_executable(build_buckets src/build_buckets.cpp
        src/kmeans.cpp
        src/emd.cpp
        src/bucket.cpp
        src/builder.hpp)
#generic
target_link_libraries(eval-h2h core engine cpprestsdk::cpprest)

#platform specific
if (UNIX AND NOT APPLE)
    target_link_libraries(engine core stdc++fs cpprestsdk::cpprest)
    target_link_libraries(build_buckets PRIVATE stdc++fs core Threads::Threads peval ${Boost_LIBRARIES})
    target_link_libraries(train PRIVATE engine Threads::Threads)
    target_link_libraries(check_strategy PRIVATE engine Threads::Threads)
    target_link_libraries(check_kernel PRIVATE engine Threads::Threads)
else ()
    target_link_libraries(engine core cpprestsdk::cpprest)
    target_link_libraries(build_buckets PRIVATE core Threads::Threads peval ${Boost_LIBRARIES})
    target_link_libraries(train PRIVATE engine Threads::Threads)
    target_link_libraries(check_strategy PRIVATE engine Threads::Threads)
    target_link_libraries(check_kernel PRIVATE engine Threads::Threads)
endif ()
set_property(TARGET engine PROPERTY CXX_STANDARD 17)

install(
        TARGETS train
        RUNTIME DESTINATION ${INSTALL_DIR}
        LIBRARY DESTINATION lib
)

install(
        TARGETS build_buckets
        RUNTIME DESTINATION ${INSTALL_DIR}
)

add_executable(test_cfr ${CMAKE_CURRENT_SOURCE_DIR}/test/test_cfr.cpp)
add_executable(test_util ${CMAKE_CURRENT_SOURCE_DIR}/test/test_engine_util.cpp)
add_executable(test_engine ${CMAKE_CURRENT_SOURCE_DIR}/test/test_engine.cpp)
add_executable(test_hand_kernel ${CMAKE_CURRENT_SOURCE_DIR}/test/test_hand_kernel.cpp)
add_executable(test_abstraction ${CMAKE_CURRENT_SOURCE_DIR}/test/test_abstraction.cpp
        src/bucket.cpp
        src/kmeans.cpp
        src/emd.cpp)
add_executable(test_rnba_kernel ${CMAKE_CURRENT_SOURCE_DIR}/test/test_rnba_kernel.cpp)
target_link_libraries(test_cfr PRIVATE engine Catch2::Catch2)
target_link_libraries(test_util PRIVATE engine Catch2::Catch2)
if (UNIX AND NOT APPLE)
    target_link_libraries(test_hand_kernel PRIVATE stdc++fs engine Catch2::Catch2)
    target_link_libraries(test_engine PRIVATE stdc++fs core engine Catch2::Catch2)
    target_link_libraries(test_abstraction PRIVATE stdc++fs core engine Catch2::Catch2 Threads::Threads peval ${Boost_LIBRARIES})
    target_link_libraries(test_rnba_kernel PRIVATE stdc++fs engine Catch2::Catch2)
else ()
    target_link_libraries(test_hand_kernel PRIVATE engine Catch2::Catch2)
    target_link_libraries(test_engine PRIVATE core engine Catch2::Catch2)
    target_link_libraries(test_abstraction PRIVATE core engine Catch2::Catch2 Threads::Threads peval ${Boost_LIBRARIES})
    target_link_libraries(test_rnba_kernel PRIVATE engine Catch2::Catch2)
endif ()

include(Catch)
include(CTest)
#adds test to "make test"
catch_discover_tests(test_cfr)
catch_discover_tests(test_hand_kernel)
catch_discover_tests(test_abstraction)
catch_discover_tests(test_rnba_kernel)
catch_discover_tests(test_engine)
catch_discover_tests(test_util)
