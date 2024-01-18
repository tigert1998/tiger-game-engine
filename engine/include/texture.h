#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <optional>
#include <string>
#include <vector>

class Texture {
 private:
  bool has_ownership_ = false;
  uint32_t target_ = 0, id_ = 0;
  mutable std::optional<uint64_t> handle_ = std::nullopt;

  void Load2DTextureFromPath(const std::string &path, uint32_t wrap,
                             uint32_t min_filter, uint32_t mag_filter,
                             const std::vector<float> &border_color,
                             bool mipmap, bool flip_y);

  void LoadCubeMapTextureFromPath(const std::string &path, uint32_t wrap,
                                  uint32_t min_filter, uint32_t mag_filter,
                                  const std::vector<float> &border_color,
                                  bool mipmap, bool flip_y);

  inline void MoveOwnership(Texture &&texture) {
    this->target_ = texture.target_;
    this->id_ = texture.id_;
    this->has_ownership_ = texture.has_ownership_;
    texture.has_ownership_ = false;
  }

 public:
  inline Texture() = default;
  inline Texture(Texture &texture) { MoveOwnership(std::move(texture)); }
  Texture &operator=(Texture &texture) {
    Clear();
    MoveOwnership(std::move(texture));
    return *this;
  }

  Texture Reference() const;

  // load from image/images
  explicit Texture(const std::string &path, uint32_t wrap, uint32_t min_filter,
                   uint32_t mag_filter, const std::vector<float> &border_color,
                   bool mipmap, bool flip_y);

  // Load from image/images with deduplication
  static Texture LoadFromFS(const std::string &path, uint32_t wrap,
                            uint32_t min_filter, uint32_t mag_filter,
                            const std::vector<float> &border_color, bool mipmap,
                            bool flip_y);

  static Texture Empty(uint32_t target);

  // create 2D texture
  explicit Texture(void *data, uint32_t width, uint32_t height,
                   uint32_t internal_format, uint32_t format, uint32_t type,
                   uint32_t wrap, uint32_t min_filter, uint32_t mag_filter,
                   const std::vector<float> &border_color, bool mipmap);

  // create 2D_ARRAY/3D texture
  explicit Texture(void *data, uint32_t target, uint32_t width, uint32_t height,
                   uint32_t depth, uint32_t internal_format, uint32_t format,
                   uint32_t type, uint32_t wrap, uint32_t min_filter,
                   uint32_t mag_filter, const std::vector<float> &border_color,
                   bool mipmap);

  inline uint32_t target() const { return target_; }
  inline uint32_t id() const { return id_; }
  uint64_t handle() const;
  void MakeResident() const;
  void MakeNonResident() const;

  void Clear();

  ~Texture();
};

#endif