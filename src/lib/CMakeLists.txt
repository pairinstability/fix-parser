add_library(libfixparser INTERFACE)
target_include_directories(libfixparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libfixparser INTERFACE pugixml absl::statusor absl::log absl::strings)