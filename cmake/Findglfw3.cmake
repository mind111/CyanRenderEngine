set(FIND_GLFW_PATHS ${CMAKE_SOURCE_DIR}/lib/glfw/lib)
find_path(GLFW_INCLUDE_DIR glfw3.h ${CMAKE_SOURCE_DIR}/lib/glfw/include)
find_library(
    GLFW_LIBRARY 
    #NAMES libglfw3.a
    NAMES glfw3.lib
    PATHS ${FIND_GLFW_PATHS})