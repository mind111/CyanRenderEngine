cmake_minimum_required(VERSION 3.0)

project(cyanRenderEngine)

#### src files ####
file(GLOB_RECURSE include "${PROJECT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE src "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE lib "${PROJECT_SOURCE_DIR}/lib/stb_image/*.cpp"
                      "${PROJECT_SOURCE_DIR}/lib/tinygltf/*.cc"
                      "${PROJECT_SOURCE_DIR}/lib/tinyobj/*.cc"
                      "${PROJECT_SOURCE_DIR}/lib/xatlas/*.cpp"
                      "${PROJECT_SOURCE_DIR}/lib/json/json.hpp")

#### dependencies ####
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW 2.2 REQUIRED)

include_directories(${GLFW_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIR})
include_directories(${ASSIMP_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/demo)
include_directories(${PROJECT_SOURCE_DIR}/src/imgui/)
include_directories(${PROJECT_SOURCE_DIR}/src/ImGuizmo/)
include_directories(${PROJECT_SOURCE_DIR}/lib/stb_image/include/)
include_directories(${PROJECT_SOURCE_DIR}/lib/tinygltf/include/)
include_directories(${PROJECT_SOURCE_DIR}/lib/tinyobj/include)
include_directories(${PROJECT_SOURCE_DIR}/lib/glm/glm)
include_directories(${PROJECT_SOURCE_DIR}/lib/json) 
include_directories(${PROJECT_SOURCE_DIR}/lib/xatlas/include)

#### preprocessor ####
add_definitions(-DGLEW_STATIC -DGLFW_INCLUDE_NONE)
add_executable(cyanRenderEngine ${include} ${src} ${lib})
target_compile_options(cyanRenderEngine PUBLIC /W1 /std:c++14 /DTINYGLTF_NOEXCEPTION)
target_link_libraries(cyanRenderEngine ${GLFW_LIBRARY} ${GLEW_LIBRARY} ${OPENGL_LIBRARIES})

#### copy over data folder after build ####
add_custom_command(TARGET cyanRenderEngine POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/asset/ $<TARGET_FILE_DIR:cyanRenderEngine>/asset/)

add_custom_command(TARGET cyanRenderEngine POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/shader/ $<TARGET_FILE_DIR:cyanRenderEngine>/shader/)

add_custom_command(TARGET cyanRenderEngine POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/scene/ $<TARGET_FILE_DIR:cyanRenderEngine>/scene/)