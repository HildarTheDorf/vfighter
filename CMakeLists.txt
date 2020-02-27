cmake_minimum_required(VERSION 3.13)
project(vfighter)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(XCB REQUIRED COMPONENTS XCB)

pkg_check_modules(VULKAN REQUIRED vulkan)

add_subdirectory(shaders)

add_executable(vfighter BadVkResult.cpp main.cpp Mesh.cpp Renderer.cpp RendererBase.cpp Window.cpp tiny_obj_loader.cpp vk_mem_alloc.cpp)
add_dependencies(vfighter vfighter_shaders)
set_target_properties(vfighter PROPERTIES CXX_STANDARD 17)
target_include_directories(vfighter PRIVATE SYSTEM include)
target_link_libraries(vfighter ${CMAKE_THREAD_LIBS_INIT} ${VULKAN_LIBRARIES} ${XCB_LIBRARIES})