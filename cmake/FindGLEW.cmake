set(FIND_GLEW_PATHS ${CMAKE_SOURCE_DIR}/lib/glew/lib)
find_path(GLEW_INCLUDE_DIR glew.h ${CMAKE_SOURCE_DIR}/lib/glew/include)
#find_library(GLEW_LIBRARY NAMES libglew32.a PATHS ${FIND_GLEW_PATHS})
find_library(GLEW_LIBRARY NAMES glew32s.lib PATHS ${FIND_GLEW_PATHS})