set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -pthread")

add_executable(dexpo dexpo.cpp)

add_executable(daemon daemon.cpp)

target_link_libraries(
  dexpo
  PRIVATE dexpo_lib project_options project_warnings
  PUBLIC xcb xcb-randr)

target_link_libraries(
  daemon
  PRIVATE dexpo_lib project_options project_warnings
  PUBLIC xcb xcb-randr)
