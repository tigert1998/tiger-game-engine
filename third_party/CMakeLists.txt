project(third-party)

option(USE_SYSTEM_ASSIMP "Use system assimp" OFF)
option(USE_SYSTEM_BULLET "Use system bullet" OFF)

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

# bullet3
if(USE_SYSTEM_BULLET)
    find_package(Bullet REQUIRED)
    add_library(bullet INTERFACE)
    target_include_directories(bullet INTERFACE "${BULLET_INCLUDE_DIR}")
    target_link_libraries(bullet INTERFACE "${BULLET_LIBRARIES}")
else()
    add_subdirectory(bullet3)
    add_library(bullet INTERFACE)
    target_include_directories(bullet INTERFACE "bullet3/src")
    target_link_libraries(bullet INTERFACE
        Bullet3Collision
        Bullet3Common
        Bullet3Dynamics
        Bullet3Geometry
        Bullet3OpenCL_clew
        Bullet2FileLoader
        BulletCollision
        BulletDynamics
        BulletInverseDynamics
        BulletSoftBody
        LinearMath
    )
endif()