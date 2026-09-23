# Smoke test: loads the built ifmqoi.sph and drives it through the Susie API
# on a QOI file it encodes itself.

add_executable(smoke_qoi smoke_qoi.cpp)
add_dependencies(smoke_qoi ifmqoi)
target_include_directories(smoke_qoi PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(smoke_qoi PRIVATE PLUGIN_PATH=L"$<TARGET_FILE:ifmqoi>")
add_test(NAME smoke_qoi COMMAND smoke_qoi "${CMAKE_CURRENT_BINARY_DIR}")
