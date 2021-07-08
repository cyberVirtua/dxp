set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(dxp dxp.cpp)

add_executable(dxpd dxpd.cpp)

target_link_libraries(
  dxp
  PRIVATE dxp_lib project_options project_warnings
  PUBLIC xcb xcb-randr xcb-keysyms)

target_link_libraries(
  dxpd
  PRIVATE dxp_lib project_options project_warnings
  PUBLIC xcb xcb-randr xcb-keysyms)
