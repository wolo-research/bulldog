set(the_description "ACPC Server")

add_executable(bm_server bm_server.c)
add_executable(bm_widget bm_widget.c)
add_executable(bm_run_matches bm_run_matches.c)

target_link_libraries(bm_run_matches core)
target_link_libraries(bm_widget core)
target_link_libraries(bm_server core)

install(
        TARGETS bm_server core
        RUNTIME DESTINATION ${INSTALL_DIR}/acpc
        LIBRARY DESTINATION lib
)
install(
        TARGETS bm_widget core
        RUNTIME DESTINATION ${INSTALL_DIR}/acpc
        LIBRARY DESTINATION lib
)
install(
        TARGETS bm_run_matches core
        RUNTIME DESTINATION ${INSTALL_DIR}/acpc
        LIBRARY DESTINATION lib
)