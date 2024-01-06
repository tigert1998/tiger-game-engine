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
- Skybox
