project(third-party)

option(USE_SYSTEM_ASSIMP "Use system assimp" OFF)

# glfw
add_subdirectory("glfw")

# glm
set(glm_dir "glm")
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${glm_dir}")

# glad
set(glad_dir "glad")
add_library(glad STATIC "${glad_dir}/src/glad.c")
target_include_directories(glad PUBLIC "${glad_dir}/include")

# assimp
if(USE_SYSTEM_ASSIMP)
    find_package(assimp REQUIRED)
    add_library(assimp INTERFACE)
    target_include_directories(assimp INTERFACE "${ASSIMP_INCLUDE_DIRS}")
    target_link_libraries(assimp INTERFACE "${ASSIMP_LIBRARY_DIRS}/${ASSIMP_LIBRARIES}")
else()
    add_subdirectory(assimp)
    if(MSVC)
        target_compile_options(assimp PRIVATE /WX-) # disable WX
    endif()
endif()

# stb
set(stb_dir "stb")
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${stb_dir}")

# imgui
set(imgui_dir "imgui")
set(imgui_srcs 
    "${imgui_dir}/imgui_demo.cpp"
    "${imgui_dir}/imgui_draw.cpp"
    "${imgui_dir}/imgui_tables.cpp"
    "${imgui_dir}/imgui_widgets.cpp"
    "${imgui_dir}/imgui.cpp"
    "${imgui_dir}/backends/imgui_impl_glfw.cpp"
    "${imgui_dir}/backends/imgui_impl_opengl3.cpp"
)
add_library(imgui STATIC "${imgui_srcs}")
target_link_libraries(imgui glfw)
target_include_directories(imgui PUBLIC "${imgui_dir}")

# glog
add_subdirectory(glog)

# PhysX 5
add_library(physx5 INTERFACE)
target_include_directories(physx5 INTERFACE "${PHYSX5}/physx/include")
target_link_directories(physx5 INTERFACE "${PHYSX5}/physx/bin/${PHYSX5_PLATFORM}/release")
target_link_libraries(
    physx5 INTERFACE
    PhysXExtensions_static_64
    PhysX_64
    PhysXPvdSDK_static_64
    PhysXCommon_64
    PhysXFoundation_64
)
