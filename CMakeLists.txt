project(tiger-game-engine)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("third_party")

file(GLOB_RECURSE engine_srcs "engine/src/*.cc")
add_library(engine "${engine_srcs}")
target_link_libraries(engine glfw glad imgui glm gli soil2 fmt assimp physx5 meshoptimizer)
target_include_directories(engine PUBLIC "engine/include")
target_compile_options(engine PUBLIC "$<$<C_COMPILER_ID:MSVC>:/utf-8>")
target_compile_options(engine PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(model-visualizer "apps/model-visualizer/src/main.cc")
target_link_libraries(model-visualizer engine)

add_executable(grass-demo-generate-terrain "apps/grass-demo/src/generate_terrain.cc")
target_link_libraries(grass-demo-generate-terrain engine)
add_executable(grass-demo "apps/grass-demo/src/main.cc")
target_link_libraries(grass-demo engine)

add_executable(pbr-demo-generate-sphere "apps/pbr-demo/src/generate_sphere.cc")
target_link_libraries(pbr-demo-generate-sphere engine)

add_executable(clouds-demo "apps/clouds-demo/src/main.cc")
target_link_libraries(clouds-demo engine)

add_executable(px-demo "apps/px-demo/src/main.cc")
target_link_libraries(px-demo engine)

add_executable(instancing-demo "apps/instancing-demo/src/main.cc")
target_link_libraries(instancing-demo engine)

add_executable(omnidirectional-shadow-demo "apps/omnidirectional-shadow-demo/src/main.cc")
target_link_libraries(omnidirectional-shadow-demo engine)

add_executable(ibl-demo "apps/ibl-demo/src/main.cc")
target_link_libraries(ibl-demo engine)
