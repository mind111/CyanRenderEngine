
set(FIND_ASSIMP_PATHS ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
set(ASSIMP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/assimp/include)
find_library(ASSIMP_LIBRARY NAMES assimp-vc140-mt.lib PATHS ${FIND_ASSIMP_PATHS})