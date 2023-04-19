project(tiger-game-engine)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory("third_party")

file(GLOB_RECURSE engine_srcs "engine/src/*.cc")
add_library(engine "${engine_srcs}")
target_link_libraries(engine glfw glad imgui glm stb glog::glog assimp)
target_include_directories(engine PUBLIC "engine/include")

add_executable(model-visualizer "apps/model-visualizer/src/main.cc")
target_link_libraries(model-visualizer engine glog::glog)