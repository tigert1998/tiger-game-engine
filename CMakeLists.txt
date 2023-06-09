project(tiger-game-engine)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory("third_party")

file(GLOB_RECURSE engine_srcs "engine/src/*.cc")
add_library(engine "${engine_srcs}")
target_link_libraries(engine glfw glad imgui glm stb glog::glog assimp)
target_include_directories(engine PUBLIC "engine/include")

add_executable(model-visualizer "apps/model-visualizer/src/main.cc")
target_link_libraries(model-visualizer engine)

add_executable(grass-demo-generate-terrain "apps/grass-demo/src/generate_terrain.cc")
target_link_libraries(grass-demo-generate-terrain engine)
add_executable(grass-demo "apps/grass-demo/src/main.cc")
target_link_libraries(grass-demo engine)

add_executable(pbr-demo-generate-sphere "apps/pbr-demo/src/generate_sphere.cc")
target_link_libraries(pbr-demo-generate-sphere engine)

add_executable(clouds-demo-visualize-texture "apps/clouds-demo/src/visualize_texture.cc")
target_link_libraries(clouds-demo-visualize-texture engine)
add_executable(clouds-demo "apps/clouds-demo/src/main.cc")
target_link_libraries(clouds-demo engine)