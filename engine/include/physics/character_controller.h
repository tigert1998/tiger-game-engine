#ifndef PHYSICS_CHARACTER_CONTROLLER_H_
#define PHYSICS_CHARACTER_CONTROLLER_H_

#include <PxPhysicsAPI.h>

#include <glm/glm.hpp>

class CharacterController {
 private:
  physx::PxScene* scene_;
  physx::PxCapsuleController* controller_;

  physx::PxVec3 velocity_;

 public:
  explicit CharacterController(physx::PxScene* scene,
                               physx::PxCapsuleController* controller);

  inline ~CharacterController() = default;
  // CharacterController does not own scene and controller.
  // So it will not release them.

  bool IsGrounded();

  void Move(glm::vec3 disp, float delta_time);
  void Jump(glm::vec3 jump_power, float delta_time);

  inline glm::vec3 position() const {
    auto data = controller_->getPosition();
    return {data.x, data.y, data.z};
  }
};

#endif