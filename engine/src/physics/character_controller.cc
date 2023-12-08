#include "physics/character_controller.h"

namespace {
class QueryFilterCallback : public physx::PxQueryFilterCallback {
 public:
  QueryFilterCallback(physx::PxActor* actor) : actor_(actor) {}
  physx::PxQueryHitType::Enum preFilter(
      const physx::PxFilterData& filterData, const physx::PxShape* shape,
      const physx::PxRigidActor* actor,
      physx::PxHitFlags& queryFlags) override {
    if (actor == actor_) {
      return physx::PxQueryHitType::eNONE;
    }
    return physx::PxQueryHitType::eBLOCK;
  }
  physx::PxQueryHitType::Enum postFilter(
      const physx::PxFilterData& filterData, const physx::PxQueryHit& hit,
      const physx::PxShape* shape, const physx::PxRigidActor* actor) override {
    return physx::PxQueryHitType::eNONE;
  }

 private:
  physx::PxActor* actor_;
};
}  // namespace

CharacterController::CharacterController(physx::PxScene* scene,
                                         physx::PxCapsuleController* controller)
    : scene_(scene), controller_(controller), velocity_(0) {}

bool CharacterController::IsTouched(bool down, float distance_offset) {
  auto position = controller_->getPosition();
  float distance = controller_->getHeight() / 2 + controller_->getRadius() +
                   controller_->getContactOffset() + distance_offset;

  physx::PxSweepBuffer callback;
  physx::PxSphereGeometry sweep_geom(controller_->getRadius());
  physx::PxQueryFilterData query_filter_data;
  query_filter_data.flags |= physx::PxQueryFlag::ePREFILTER;
  QueryFilterCallback query_filter_callback(controller_->getActor());

  float mul = down ? -1 : 1;
  bool status = scene_->sweep(
      sweep_geom, {(float)position.x, (float)position.y, (float)position.z},
      mul * controller_->getUpDirection(), distance, callback,
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
    since_grouned_ = 0;
  } else {
    // disp does not make any effects here since the character is in the air
    if (since_head_hit_ > 0.5 && IsHeadHit()) {
      glm::vec3 glm_velocity = {velocity_.x, velocity_.y, velocity_.z};
      glm::vec3 glm_gravity = {
          scene_->getGravity().x,
          scene_->getGravity().y,
          scene_->getGravity().z,
      };
      glm_velocity += glm_gravity * glm::dot(glm::normalize(glm_velocity),
                                             glm::normalize(-glm_gravity));
      velocity_ = {glm_velocity.x, glm_velocity.y, glm_velocity.z};
      since_head_hit_ = 0;
    } else {
      velocity_ += scene_->getGravity() * delta_time;
      since_head_hit_ += delta_time;
    }
    if (since_grouned_ > 0.1) {
      disp = velocity_ * delta_time;
    } else {
      disp += velocity_ * delta_time;
    }
    controller_->move(disp, min_distance, delta_time, filters);
    since_grouned_ += delta_time;
  }
}

void CharacterController::Jump(glm::vec3 jump_power, float delta_time) {
  float min_distance = 0;
  physx::PxControllerFilters filters;

  if (IsGrounded()) {
    velocity_ = {jump_power.x, jump_power.y, jump_power.z};
    auto disp = velocity_ * delta_time;
    controller_->move(disp, min_distance, delta_time, filters);
  }
}