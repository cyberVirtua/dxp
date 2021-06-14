find_package(
  Boost
  COMPONENTS system filesystem unit_test_framework
  REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -pthread")

add_executable(socket_test socket.cpp ../src/dexpo_socket.cpp)

target_include_directories(socket_test PRIVATE ${Boost_INCLUDE_DIRS})

target_compile_definitions(socket_test PRIVATE "BOOST_TEST_DYN_LINK=1")

target_link_libraries(
  socket_test
  PRIVATE project_options project_warnings
  PUBLIC ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY}
         ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME test1 COMMAND socket_test)
