# TIGER GAME ENGINE

## Build

Tiger game engine relies on PhysX5 as the physics engine.
Since PhysX5 is hard to be integrated as a submodule, you need to manually download and build it.
Pass PhysX5 path to CMake to build tiger game engine.

```bash
git submodule update --init --recursive
python configure.py # download glad
mkdir build
cd build
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DPHYSX5=${PHYSX5_PATH} \
    -DPHYSX5_PLATFORM=${PHYSX5_PLATFORM} \
    ..
```

Currently, this engine only supports NVIDIA cards since it relies on the OpenGL extension:
"GL_NV_shader_atomic_fp16_vector".
Besides, don't forget to go to NVIDIA control panel
and set "OpenGL GDI compatibility" to "Compatibility first".

## Supported Features

- Clouds
- Grass
- Character controller to navigate an agent to wander around the world
- Simple perlin noise terrain generator
- Deferred shading pipeline
- SSAO
- Point/Directional lights
- Skeletal animation
- Multi draw call
- Order Independent Transparency
- Cascaded shadow mapping
- Omnidirectional shadow mapping (still under improving)
- PCF/PCSS
- Skybox
- SMAA
- **VXGI**
- ACES/Bilateral grid tone mapping

## Resources

Some of the models I used to test this engine is linked below:

- [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/blob/main/2.0/Sponza/glTF/Sponza.gltf)
- [Games 202 homework model: Cave](https://drive.google.com/drive/folders/1g7gr8XlrlyZllI8f-r4OM-uMdX0KMRWS?usp=drive_link)
- [San Miguel 2.0](https://casual-effects.com/data/)
- [Bistro](https://developer.nvidia.com/orca/amazon-lumberyard-bistro)

## Showcases

I post videos on YouTube and Bilibili to show my engine.
Please check it out!

- My YouTube channel: https://www.youtube.com/channel/UCvHmuypv_mckA0j0VClgVLg
- My Bilibili channel: https://space.bilibili.com/32883962