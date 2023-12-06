#include "physics/character_controller.h"

CharacterController::CharacterController(physx::PxScene* scene,
                                         physx::PxCapsuleController* controller)
    : scene_(scene), controller_(controller), velocity_(0) {}

bool CharacterController::IsGrounded() {
  class QueryFilterCallback : public physx::PxQueryFilterCallback {
   public:
    QueryFilterCallback(physx::PxActor* actor) : actor_(actor) {}
    physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData,
                                          const physx::PxShape* shape,
                                          const physx::PxRigidActor* actor,
                                          physx::PxHitFlags& queryFlags) {
      if (actor == actor_) {
        return physx::PxQueryHitType::eNONE;
      }
      return physx::PxQueryHitType::eBLOCK;
    }
    physx::PxQueryHitType::Enum postFilter(
        const physx::PxFilterData& filterData, const physx::PxQueryHit& hit,
        const physx::PxShape* shape, const physx::PxRigidActor* actor) {
      return physx::PxQueryHitType::eNONE;
    }

   private:
    physx::PxActor* actor_;
  };

  auto position = controller_->getPosition();
  float distance = controller_->getHeight() / 2 + controller_->getRadius() +
                   controller_->getContactOffset() + 1e-2;

  physx::PxRaycastBuffer callback;
  physx::PxQueryFilterData query_filter_data;
  query_filter_data.flags |= physx::PxQueryFlag::ePREFILTER;
  QueryFilterCallback query_filter_callback(controller_->getActor());
  bool status = scene_->raycast(
      {(float)position.x, (float)position.y, (float)position.z},
      -controller_->getUpDirection(), distance, callback,
      physx::PxHitFlag::eDEFAULT, query_filter_data, &query_filter_callback);
  return status;
}

void CharacterController::Move(glm::vec3 glm_disp, float delta_time) {
  float min_distance = 0;
  physx::PxVec3 disp = {glm_disp.x, glm_disp.y, glm_disp.z};
  physx::PxControllerFilters filters;
  if (IsGrounded()) {
    velocity_ = physx::PxVec3(0);
    controller_->move(disp, min_distance, delta_time, filters);
  } else {
    // disp does not make any effects here since the character is in the air
    velocity_ += scene_->getGravity() * delta_time;
    disp = velocity_ * delta_time;
    controller_->move(disp, min_distance, delta_time, filters);
  }
}