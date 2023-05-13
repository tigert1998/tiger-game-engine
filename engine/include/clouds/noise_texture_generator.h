#ifndef CLOUDS_NOISE_TEXTURE_GENERATOR_H_
#define CLOUDS_NOISE_TEXTURE_GENERATOR_H_

#include <stdint.h>

class NoiseTextureGenerator {
 public:
  NoiseTextureGenerator(float weather_texture_perlin_frequency);
  ~NoiseTextureGenerator();

  inline uint32_t perlin_worley_texture_id() {
    return perlin_worley_texture_id_;
  }
  inline uint32_t worley_texture_id() { return worley_texture_id_; }
  inline uint32_t weather_texture_id() { return weather_texture_id_; }

 private:
  float weather_texture_perlin_frequency_;

  uint32_t perlin_worley_texture_id_ = 0, weather_texture_id_ = 0,
           worley_texture_id_ = 0;
};

#endif