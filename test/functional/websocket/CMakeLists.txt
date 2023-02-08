set(SOURCES client.cpp)

add_executable(client ${SOURCES})

if (WIN32)
    target_link_libraries(client PRIVATE ws2_32)
    target_compile_definitions(client PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

target_link_libraries(client
    ${Boost_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT})

if (RT_FOUND)
	target_link_libraries(client ${RT_LIBRARIES})
endif()
