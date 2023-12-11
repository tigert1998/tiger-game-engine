#ifndef CLOUDS_NOISE_TEXTURE_GENERATOR_H_
#define CLOUDS_NOISE_TEXTURE_GENERATOR_H_

#include <stdint.h>
#include <texture.h>

class NoiseTextureGenerator {
 public:
  NoiseTextureGenerator(float weather_texture_perlin_frequency);
  ~NoiseTextureGenerator();

  inline const Texture& perlin_worley_texture() {
    return perlin_worley_texture_;
  }
  inline const Texture& worley_texture() { return worley_texture_; }
  inline const Texture& weather_texture() { return weather_texture_; }

 private:
  float weather_texture_perlin_frequency_;

  Texture perlin_worley_texture_, weather_texture_, worley_texture_;
};

#endif