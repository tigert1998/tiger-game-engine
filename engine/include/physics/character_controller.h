#ifndef PHYSICS_CHARACTER_CONTROLLER_H_
#define PHYSICS_CHARACTER_CONTROLLER_H_

#include <PxPhysicsAPI.h>

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

  void Move(physx::PxVec3 disp, float delta_time);
};

#endif