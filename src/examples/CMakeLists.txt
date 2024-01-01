set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_executable(runexamples example.cpp)
target_link_libraries(runexamples PRIVATE libfixparser)