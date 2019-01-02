find_package(Catch2 REQUIRED)

target_link_libraries(zectl_tests Catch2::Catch2)

add_executable(zectl_tests zectl_tests.c zectl_tests.h)

include(CTest)
include(Catch)

catch_discover_tests(zectl_tests)