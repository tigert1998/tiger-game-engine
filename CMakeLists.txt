project(tiger-game-engine)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory("third_party")

file(GLOB_RECURSE engine_srcs "engine/src/*.cc")
add_library(engine "${engine_srcs}")
target_link_libraries(engine glfw glad imgui glm assimp stb glog::glog)
target_include_directories(engine PUBLIC "engine/include")