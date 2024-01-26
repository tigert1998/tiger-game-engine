#include "random/perlin_noise.h"

#include <fmt/core.h>

#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>

using glm::vec3;
using std::tuple;
using std::vector;

double PerlinNoise::F(double x) {
  return 6 * pow(x, 5) - 15 * pow(x, 4) + 10 * pow(x, 3);
}

double PerlinNoise::DerivativeF(double x) {
  return 30 * pow(x, 4) - 60 * pow(x, 3) + 30 * pow(x, 2);
}

PerlinNoise::WeightsType PerlinNoise::CalcWeights(double x, double y,
                                                  double z) const {
  static auto lerp = [](double a, double b, double t) -> double {
    return a * (1 - t) + b * t;
  };
  vector<double> w(8), xw(4), xyw(2), xyzw(1);
  int ix = floor(x), iy = floor(y), iz = floor(z);
  double dx = x - ix, dy = y - iy, dz = z - iz;
  for (int i = 0; i < 8; i++) {
    int nix = ix + !!(i & 4);
    int niy = iy + !!(i & 2);
    int niz = iz + !!(i & 1);
    double ndx = dx - !!(i & 4);
    double ndy = dy - !!(i & 2);
    double ndz = dz - !!(i & 1);
    w[i] = Grad(nix, niy, niz, ndx, ndy, ndz);
  }
  double wdx = F(dx), wdy = F(dy), wdz = F(dz);
  for (int i = 0; i < 4; i++) xw[i] = lerp(w[i], w[i + 4], wdx);
  for (int i = 0; i < 2; i++) xyw[i] = lerp(xw[i], xw[i + 2], wdy);
  xyzw[0] = lerp(xyw[0], xyw[1], wdz);
  return WeightsType(w, xw, xyw, xyzw);
}

PerlinNoise::PerlinNoise(int perm_size, int seed) : PERM_SIZE(perm_size) {
  if (perm_size != (perm_size & -perm_size)) {
    fmt::print(stderr, "[error] perm_size != (perm_size & -perm_size)\n");
    exit(1);
  }
  static std::default_random_engine engine(seed);
  std::uniform_int_distribution<int> dis(0, perm_size - 1);
  auto dice = std::bind(dis, engine);
  perm_random_.resize(perm_size << 1);
  for (int i = 0; i < perm_random_.size(); i++) {
    perm_random_[i] = dice();
  }
}

double PerlinNoise::Noise(double x, double y, double z) const {
  using std::ignore;
  vector<double> xyzw;
  std::tie(ignore, ignore, ignore, xyzw) = CalcWeights(x, y, z);
  return xyzw[0];
}

double PerlinNoise::Grad(int x, int y, int z, double dx, double dy,
                         double dz) const {
  x &= PERM_SIZE - 1;
  y &= PERM_SIZE - 1;
  z &= PERM_SIZE - 1;
  int h = perm_random_[perm_random_[perm_random_[x] + y] + z] & 15;
  double u = h < 8 || h == 12 || h == 13 ? dx : dy;
  double v = h < 4 || h == 12 || h == 13 ? dy : dz;
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

double PerlinNoise::Noise(vec3 position) const {
  return Noise(position.x, position.y, position.z);
}

vec3 PerlinNoise::DerivativeNoise(double x, double y, double z) const {
  vec3 ans;
  vector<double> w, xw, xyw, xyzw;
  std::tie(w, xw, xyw, xyzw) = CalcWeights(x, y, z);
  double dx = x - floor(x), dy = y - floor(y), dz = z - floor(z);
  {
    // x
    vector<double> dxw(4), dxyw(2), dxyzw(1);
    for (int i = 0; i < 4; i++) dxw[i] = (w[i + 4] - w[i]) * DerivativeF(dx);
    for (int i = 0; i < 2; i++)
      dxyw[i] = dxw[i] * (1 - F(dy)) + dxw[i + 2] * F(dy);
    for (int i = 0; i < 1; i++)
      dxyzw[i] = dxyw[i] * (1 - F(dz)) + dxyw[i + 1] * F(dz);
    ans.x = dxyzw[0];
  }
  {
    // y
    vector<double> dxyw(2), dxyzw(1);
    for (int i = 0; i < 2; i++) dxyw[i] = (xw[i + 2] - xw[i]) * DerivativeF(dy);
    for (int i = 0; i < 1; i++)
      dxyzw[i] = dxyw[i] * (1 - F(dz)) + dxyw[i + 1] * F(dz);
    ans.y = dxyzw[0];
  }
  {
    // z
    vector<double> dxyzw(1);
    for (int i = 0; i < 1; i++)
      dxyzw[i] = (xyw[i + 1] - xyw[i]) * DerivativeF(dz);
    ans.z = dxyzw[0];
  }
  return ans;
}

vec3 PerlinNoise::DerivativeNoise(vec3 position) const {
  return DerivativeNoise(position.x, position.y, position.z);
}

double PerlinNoise::Turbulent(glm::vec3 position, uint32_t depth) const {
  if (depth == 0) throw std::invalid_argument("");
  double weight = 1;
  double scale = 1;
  double total_weight = 0;
  double res = 0;
  for (int i = 0; i < depth; i++) {
    res += Noise(position * float(scale)) * weight;
    total_weight += weight;
    weight *= 0.5;
    scale *= 2;
  }
  res /= total_weight;
  return res;
}