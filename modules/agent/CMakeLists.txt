set(the_description "Game Agent")

add_executable(agent src/agent.cpp
        src/slumbot_connector.cpp
        src/acpc_connector.cpp
        ${HOME_DIR}/modules/engine/src/bucket.cpp)

add_executable(dealer src/dealer.c)

add_executable(webserver src/webserver.cpp
        src/network_utils.cpp
        src/bulldog_controller.cpp
        src/basic_controller.cpp src/bulldog_sdk.cpp)

add_executable(engine_eval src/engine_eval.cpp
        src/network_utils.cpp
        src/bulldog_controller.cpp
        src/basic_controller.cpp src/bulldog_sdk.cpp)

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

install(
        TARGETS agent
        RUNTIME DESTINATION ${INSTALL_DIR}
)
install(
        TARGETS dealer
        RUNTIME DESTINATION ${INSTALL_DIR}
)
install(
        TARGETS webserver
        RUNTIME DESTINATION ${INSTALL_DIR}
)

# Make test executable
add_executable(test_agent ${CMAKE_CURRENT_SOURCE_DIR}/test/test_agent.cpp
        src/slumbot_connector.cpp
        src/acpc_connector.cpp
        ${HOME_DIR}/modules/engine/src/bucket.cpp)
add_executable(test_sdk ${CMAKE_CURRENT_SOURCE_DIR}/test/test_sdk.cpp
        src/network_utils.cpp
        src/bulldog_controller.cpp
        src/basic_controller.cpp
        src/bulldog_sdk.cpp)

if (UNIX AND NOT APPLE)
    target_link_libraries(agent PRIVATE stdc++fs core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY})
    target_link_libraries(test_agent PRIVATE stdc++fs core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} Catch2::Catch2)
    target_link_libraries(test_sdk PRIVATE stdc++fs core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} Catch2::Catch2 uuid)
    target_link_libraries(webserver PRIVATE stdc++fs core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} uuid)
    target_link_libraries(engine_eval PRIVATE stdc++fs core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} uuid)
else ()
    target_link_libraries(agent PRIVATE core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY})
    target_link_libraries(test_agent PRIVATE core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} Catch2::Catch2)
    target_link_libraries(test_sdk PRIVATE core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY} Catch2::Catch2)
    target_link_libraries(webserver PRIVATE core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY})
    target_link_libraries(engine_eval PRIVATE core engine cpprestsdk::cpprest ${Boost_SYSTEM_LIBRARY})
endif ()
target_link_libraries(dealer PRIVATE core)

include(Catch)
include(CTest)
#adds test to "make test"
catch_discover_tests(test_agent)
catch_discover_tests(test_sdk)
