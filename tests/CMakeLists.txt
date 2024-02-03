option(BUILD_TESTING "Create all tests" NO)

if (BUILD_TESTING)
    enable_testing()
    find_package(Check REQUIRED)

    # Needed for ubuntu - see https://github.com/libcheck/check/issues/48
    find_package(Threads REQUIRED)

    include_directories(${CHECK_INCLUDE_DIRS})

    set(LIBS ${LIBS} ${CHECK_LIBRARIES}
         ${ZE_LINK_LIBRARIES}
         ${CMAKE_THREAD_LIBS_INIT}
         libze util)

    include_directories(. ../include)

    add_executable(zectl_tests zectl_tests.c zectl_tests.h)
    target_link_libraries(zectl_tests ${LIBS})
    add_test(zectl_tests ${CMAKE_CURRENT_BINARY_DIR}/zectl_tests)
endif()
