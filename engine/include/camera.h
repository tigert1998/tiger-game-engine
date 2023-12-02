#ifndef CAMERA_H_
#define CAMERA_H_

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

struct FrustumPlane {
  // unit vector
  glm::vec3 normal;
  // distance from origin to the nearest point in the plane
  float distance;

  inline FrustumPlane() = default;

  inline FrustumPlane(glm::vec3 normal, glm::vec3 point) {
    this->normal = glm::normalize(normal);
    this->distance = glm::dot(point, this->normal);
  }

  inline float GetSignedDistanceToPlane(glm::vec3 point) const {
    return glm::dot(normal, point) - distance;
  }
};

struct Frustum {
  FrustumPlane top_plane;
  FrustumPlane bottom_plane;
  FrustumPlane right_plane;
  FrustumPlane left_plane;
  FrustumPlane far_plane;
  FrustumPlane near_plane;
};

class Camera {
 public:
  enum class MoveDirectionType {
    kForward,
    kBackward,
    kLeftward,
    kRightward,
    kUpward,
    kDownward
  };
  Camera() = delete;
  Camera(glm::vec3 position, double width_height_ratio,
         double alpha = -glm::pi<double>() / 2, double beta = 0);
  void Rotate(double delta_alpha, double delta_beta);
  void Move(MoveDirectionType direction, float time);
  glm::vec3 position() const;
  void set_position(glm::vec3 position);
  glm::mat4 view_matrix() const;
  void set_width_height_ratio(double width_height_ratio);
  glm::mat4 projection_matrix() const;
  double alpha() const;
  double beta() const;
  void set_alpha(double alpha);
  void set_beta(double beta);
  glm::vec3 front() const;
  void set_front(glm::vec3 new_front);
  glm::vec3 center() const;
  void set_center(glm::vec3 new_center);
  Frustum frustum() const;
  std::vector<glm::vec3> frustum_corners(double z_near, double z_far) const;
  double z_near() const { return near_; }
  double z_far() const { return far_; }

 private:
  static const double kMaxElevationAngle;
  const glm::vec3 up_ = glm::vec3(0, 1, 0);
  glm::vec3 position_;
  double alpha_, beta_, width_height_ratio_;
  double fovy_ = glm::radians(60.f), near_ = 0.1, far_ = 50;
};

#endif