#include "engine/plugin.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "animation_graph/animation_graph_component.hpp"
#include "animation_graph/animation_graph_registry.hpp"
#include "animation_runtime/animation_runtime_system.hpp"
#include "ecs/components/name.hpp"
#include "ecs/components/transform.hpp"
#include "engine/camera.hpp"
#include "engine/component_registry.hpp"
#include "engine/input.hpp"
#include "engine/log.hpp"
#include "engine/module_registry.hpp"
#include "engine/physics.hpp"
#include "engine/prefab.hpp"
#include "engine/query_terms.hpp"
#include "engine/scene.hpp"
#include "engine/system_registration.hpp"
#include "engine/structured_data.hpp"
#include "engine/transform_math.hpp"
#include "engine/type_key.hpp"
#include "engine/world.hpp"
#include "engine/world_manager.hpp"
#include "engine/world_types.hpp"
#include "input_runtime/input_runtime_service.hpp"
#include "input_runtime/input_runtime_system.hpp"
#include "io/file_system.hpp"
#include "runtime/runtime_registration.hpp"
#include "state_machine_runtime/state_machine_component.hpp"
#include "state_machine_runtime/state_machine_registry.hpp"
#include "state_machine_runtime/state_machine_runtime_system.hpp"
#include "terrain_runtime/terrain_component.hpp"

namespace engine::sandbox {
namespace {

struct SandboxGameplayModule {};

struct SandboxGameplayState {
    bool inputMapsLoaded = false;
    bool inputMapsLogged = false;
    bool physicsMissingLogged = false;
};

struct SpawnPoint {
    std::string prefab = "asset://prefabs/meshy_player.prefab.json";
    bool spawnOnStart = true;
};

struct PlayerController {
    std::string controlMode = "shooter";
    float walkSpeed = 1.4f;
    float sprintSpeed = 6.0f;
    float jumpSpeed = 3.43f;
    float turnSpeed = 12.0f;
};

struct CharacterMotor {
    float radius = 0.3f;
    float cylinderHalfHeight = 0.6f;
    float gravity = 9.80665f;
    float maxSlopeAngleRadians = 0.87266463f;
    float stepHeight = 0.35f;
    Vec3 velocity{};
    bool grounded = false;
    PhysicsCharacterHandle character;
};

struct Possessed {
    std::uint32_t playerIndex = 0;
};

struct SpawnRuntimeState {
    bool spawned = false;
    bool missingServicesLogged = false;
    bool waitingForSpawnPointLogged = false;
};

struct EmptyViews {
    static std::vector<ViewSpec> build() {
        return {};
    }
};

template <typename ComponentT>
bool hasSceneComponent(const SceneEntityDesc& entity, std::string_view componentId) {
    return findSceneComponent(entity, componentId) != nullptr;
}

template <typename ComponentT>
bool removeSceneComponentById(SceneEntityDesc& entity, std::string_view componentId, std::string&) {
    return removeSceneComponent(entity, componentId);
}

void readVec3(const StructuredObjectView& object, Vec3& value) {
    if (!object.valid()) {
        return;
    }
    value.x = static_cast<float>(object.real("x", value.x));
    value.y = static_cast<float>(object.real("y", value.y));
    value.z = static_cast<float>(object.real("z", value.z));
}

void writeVec3(StructuredMutableObject object, const Vec3& value) {
    object.setFloat("x", value.x);
    object.setFloat("y", value.y);
    object.setFloat("z", value.z);
}

bool loadStructuredComponent(const SceneEntityDesc& entity, std::string_view componentId, StructuredDocument& document, std::string& error) {
    const SceneComponentDesc* sceneComponent = findSceneComponent(entity, componentId);
    if (sceneComponent == nullptr) {
        error = "scene component is missing";
        return false;
    }
    return StructuredDocument::loadFromString(sceneComponent->valueJson, document, error);
}

bool loadSpawnPoint(const SceneEntityDesc& entity, void* component, std::string& error) {
    StructuredDocument document;
    if (!loadStructuredComponent(entity, "SpawnPoint", document, error)) {
        return false;
    }
    SpawnPoint& value = *static_cast<SpawnPoint*>(component);
    const StructuredObjectView root = document.rootObject();
    value.prefab = root.string("prefab", value.prefab);
    value.spawnOnStart = root.boolean("spawnOnStart", value.spawnOnStart);
    return true;
}

bool saveSpawnPoint(SceneEntityDesc& entity, const void* component, std::string& error) {
    const SpawnPoint& value = *static_cast<const SpawnPoint*>(component);
    StructuredMutableDocument document;
    StructuredMutableObject root = document.rootObject();
    root.setString("prefab", value.prefab);
    root.setBool("spawnOnStart", value.spawnOnStart);
    SceneComponentDesc& sceneComponent = ensureSceneComponent(entity, "SpawnPoint");
    sceneComponent.componentId = "SpawnPoint";
    sceneComponent.valueJson = document.writeToString(error);
    return !sceneComponent.valueJson.empty() || error.empty();
}

bool loadPlayerController(const SceneEntityDesc& entity, void* component, std::string& error) {
    StructuredDocument document;
    if (!loadStructuredComponent(entity, "PlayerController", document, error)) {
        return false;
    }
    PlayerController& value = *static_cast<PlayerController*>(component);
    const StructuredObjectView root = document.rootObject();
    value.controlMode = root.string("controlMode", value.controlMode);
    value.walkSpeed = static_cast<float>(root.real("walkSpeed", value.walkSpeed));
    value.sprintSpeed = static_cast<float>(root.real("sprintSpeed", value.sprintSpeed));
    value.jumpSpeed = static_cast<float>(root.real("jumpSpeed", value.jumpSpeed));
    value.turnSpeed = static_cast<float>(root.real("turnSpeed", value.turnSpeed));
    return true;
}

bool savePlayerController(SceneEntityDesc& entity, const void* component, std::string& error) {
    const PlayerController& value = *static_cast<const PlayerController*>(component);
    StructuredMutableDocument document;
    StructuredMutableObject root = document.rootObject();
    root.setString("controlMode", value.controlMode);
    root.setFloat("walkSpeed", value.walkSpeed);
    root.setFloat("sprintSpeed", value.sprintSpeed);
    root.setFloat("jumpSpeed", value.jumpSpeed);
    root.setFloat("turnSpeed", value.turnSpeed);
    SceneComponentDesc& sceneComponent = ensureSceneComponent(entity, "PlayerController");
    sceneComponent.componentId = "PlayerController";
    sceneComponent.valueJson = document.writeToString(error);
    return !sceneComponent.valueJson.empty() || error.empty();
}

bool loadCharacterMotor(const SceneEntityDesc& entity, void* component, std::string& error) {
    StructuredDocument document;
    if (!loadStructuredComponent(entity, "CharacterMotor", document, error)) {
        return false;
    }
    CharacterMotor& value = *static_cast<CharacterMotor*>(component);
    const StructuredObjectView root = document.rootObject();
    value.radius = static_cast<float>(root.real("radius", value.radius));
    value.cylinderHalfHeight = static_cast<float>(root.real("cylinderHalfHeight", value.cylinderHalfHeight));
    value.gravity = static_cast<float>(root.real("gravity", value.gravity));
    value.maxSlopeAngleRadians = static_cast<float>(root.real("maxSlopeAngleRadians", value.maxSlopeAngleRadians));
    value.stepHeight = static_cast<float>(root.real("stepHeight", value.stepHeight));
    return true;
}

bool saveCharacterMotor(SceneEntityDesc& entity, const void* component, std::string& error) {
    const CharacterMotor& value = *static_cast<const CharacterMotor*>(component);
    StructuredMutableDocument document;
    StructuredMutableObject root = document.rootObject();
    root.setFloat("radius", value.radius);
    root.setFloat("cylinderHalfHeight", value.cylinderHalfHeight);
    root.setFloat("gravity", value.gravity);
    root.setFloat("maxSlopeAngleRadians", value.maxSlopeAngleRadians);
    root.setFloat("stepHeight", value.stepHeight);
    SceneComponentDesc& sceneComponent = ensureSceneComponent(entity, "CharacterMotor");
    sceneComponent.componentId = "CharacterMotor";
    sceneComponent.valueJson = document.writeToString(error);
    return !sceneComponent.valueJson.empty() || error.empty();
}

bool loadPossessed(const SceneEntityDesc& entity, void* component, std::string& error) {
    StructuredDocument document;
    if (!loadStructuredComponent(entity, "Possessed", document, error)) {
        return false;
    }
    Possessed& value = *static_cast<Possessed*>(component);
    value.playerIndex = static_cast<std::uint32_t>(document.rootObject().uintv("playerIndex", value.playerIndex));
    return true;
}

bool savePossessed(SceneEntityDesc& entity, const void* component, std::string& error) {
    const Possessed& value = *static_cast<const Possessed*>(component);
    StructuredMutableDocument document;
    document.rootObject().setUInt("playerIndex", value.playerIndex);
    SceneComponentDesc& sceneComponent = ensureSceneComponent(entity, "Possessed");
    sceneComponent.componentId = "Possessed";
    sceneComponent.valueJson = document.writeToString(error);
    return !sceneComponent.valueJson.empty() || error.empty();
}

using CharacterControllerQuery = Query<
    With<Transform, PlayerController, CharacterMotor, Possessed>,
    Read<Name>,
    ReadWrite<Transform>,
    Read<WorldTransform>,
    Read<PlayerController>,
    ReadWrite<CharacterMotor>,
    Read<Possessed>,
    Optional<Name, animation_graph::AnimationGraphComponent, state_machine::StateMachineComponent>
>;

using SpawnPointQuery = Query<With<Transform, SpawnPoint>, Read<Transform>, Read<SpawnPoint>>;
bool isShooterControlMode(std::string_view mode) {
    return mode == "shooter" || mode == "aim" || mode == "fps" || mode == "tps";
}

bool isArcadeControlMode(std::string_view mode) {
    return mode == "arcade" || mode == "adventure" || mode == "movement";
}

Quat yawRotation(float yaw) {
    const float halfYaw = yaw * 0.5f;
    return Quat{
        .w = std::cos(halfYaw),
        .x = 0.0f,
        .y = std::sin(halfYaw),
        .z = 0.0f,
    };
}

Quat multiply(Quat a, Quat b) {
    return Quat{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

Quat axisAngle(Vec3 axis, float angle) {
    const float half = angle * 0.5f;
    const float s = std::sin(half);
    return Quat{std::cos(half), axis.x * s, axis.y * s, axis.z * s};
}

Quat yawPitchRotation(float yaw, float pitch) {
    return multiply(axisAngle(Vec3{0.0f, 1.0f, 0.0f}, yaw), axisAngle(Vec3{1.0f, 0.0f, 0.0f}, pitch));
}

Vec3 rotateVector(const Quat& rotation, const Vec3& value) {
    const Vec3 qv{rotation.x, rotation.y, rotation.z};
    const Vec3 uv{
        qv.y * value.z - qv.z * value.y,
        qv.z * value.x - qv.x * value.z,
        qv.x * value.y - qv.y * value.x,
    };
    const Vec3 uuv{
        qv.y * uv.z - qv.z * uv.y,
        qv.z * uv.x - qv.x * uv.z,
        qv.x * uv.y - qv.y * uv.x,
    };
    return Vec3{
        value.x + ((uv.x * rotation.w) + uuv.x) * 2.0f,
        value.y + ((uv.y * rotation.w) + uuv.y) * 2.0f,
        value.z + ((uv.z * rotation.w) + uuv.z) * 2.0f,
    };
}

Vec3 normalized(Vec3 value) {
    const float len = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (len <= 0.00001f) {
        return {};
    }
    return Vec3{value.x / len, value.y / len, value.z / len};
}

float distanceSquared(Vec3 left, Vec3 right) {
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    const float dz = left.z - right.z;
    return dx * dx + dy * dy + dz * dz;
}

Vec2 normalized(Vec2 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y);
    if (length <= 0.0001f) {
        return {};
    }
    if (length <= 1.0f) {
        return value;
    }
    return Vec2{value.x / length, value.y / length};
}

Vec3 horizontalMoveDirectionFromCameraYaw(float cameraYaw, Vec2 move) {
    const Quat yaw = yawRotation(cameraYaw);
    const Vec3 cameraForward = rotateVector(yaw, Vec3{0.0f, 0.0f, -1.0f});
    const Vec3 cameraRight = rotateVector(yaw, Vec3{1.0f, 0.0f, 0.0f});
    return normalized(Vec3{
        cameraRight.x * move.x + cameraForward.x * move.y,
        0.0f,
        cameraRight.z * move.x + cameraForward.z * move.y,
    });
}

float yawFromHorizontalDirection(Vec3 direction) {
    direction.y = 0.0f;
    const Vec3 horizontal = normalized(direction);
    if (horizontal.x == 0.0f && horizontal.z == 0.0f) {
        return 0.0f;
    }
    return std::atan2(horizontal.x, horizontal.z);
}

float yawFromRotation(const Quat& rotation) {
    return yawFromHorizontalDirection(rotateVector(rotation, Vec3{0.0f, 0.0f, 1.0f}));
}

struct CameraMovementBasis {
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    bool valid = false;
};

CameraMovementBasis movementBasisFromCamera(EcsWorld& world, const input::InputActionService& actions) {
    CameraMovementBasis basis;
    const EntityId cameraEntity = world.findCameraByRole("play_main");
    if (cameraEntity == 0) {
        return basis;
    }

    const WorldTransform* cameraWorldTransform = static_cast<const WorldTransform*>(
        world.tryGetComponent(typeKey<WorldTransform>(), cameraEntity));
    if (cameraWorldTransform == nullptr) {
        return basis;
    }

    basis.forward = rotateVector(cameraWorldTransform->transform.rotation, Vec3{0.0f, 0.0f, -1.0f});
    basis.forward.y = 0.0f;
    basis.forward = normalized(basis.forward);
    if (basis.forward.x == 0.0f && basis.forward.z == 0.0f) {
        return basis;
    }
    basis.right = rotateVector(cameraWorldTransform->transform.rotation, Vec3{1.0f, 0.0f, 0.0f});
    basis.right.y = 0.0f;
    basis.right = normalized(basis.right);
    basis.yaw = yawFromHorizontalDirection(basis.forward);
    basis.valid = true;
    return basis;
}

Vec3 horizontalMoveDirectionFromCameraBasis(const CameraMovementBasis& basis, Vec2 move) {
    return normalized(Vec3{
        basis.right.x * move.x + basis.forward.x * move.y,
        0.0f,
        basis.right.z * move.x + basis.forward.z * move.y,
    });
}

float wrapRadians(float value) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;
    while (value > kPi) {
        value -= kTwoPi;
    }
    while (value < -kPi) {
        value += kTwoPi;
    }
    return value;
}

float approachAngle(float current, float target, float alpha) {
    return current + wrapRadians(target - current) * std::clamp(alpha, 0.0f, 1.0f);
}

float approachAngleBySpeed(float current, float target, float maxStep) {
    const float delta = wrapRadians(target - current);
    const float step = std::max(0.0f, maxStep);
    if (std::fabs(delta) <= step) {
        return target;
    }
    return current + std::clamp(delta, -step, step);
}

bool snapTransformToGround(PhysicsWorld& physics, Transform& transform) {
    PhysicsQueryHit hit;
    if (!physics.raycast(
            PhysicsRaycastDesc{
                .origin = Vec3{transform.position.x, transform.position.y + 500.0f, transform.position.z},
                .direction = Vec3{0.0f, -1.0f, 0.0f},
                .maxDistance = 1200.0f,
            },
            hit)) {
        return false;
    }
    transform.position.y = hit.position.y;
    return true;
}

float characterCenterOffset(const CharacterMotor& motor) {
    return std::max(0.0f, motor.cylinderHalfHeight) + std::max(0.0f, motor.radius);
}

Vec3 characterCenterFromRoot(const Vec3& rootPosition, const CharacterMotor& motor) {
    return Vec3{
        rootPosition.x,
        rootPosition.y + characterCenterOffset(motor),
        rootPosition.z,
    };
}

Vec3 characterRootFromCenter(const Vec3& centerPosition, const CharacterMotor& motor) {
    return Vec3{
        centerPosition.x,
        centerPosition.y - characterCenterOffset(motor),
        centerPosition.z,
    };
}

std::string entityDisplayName(const SceneEntityDesc& entity) {
    if (const SceneComponentDesc* nameComponent = findSceneComponent(entity, "Name"); nameComponent != nullptr) {
        StructuredDocument document;
        std::string error;
        if (StructuredDocument::loadFromString(nameComponent->valueJson, document, error)) {
            const std::string value = document.rootObject().string("value");
            if (!value.empty()) {
                return value;
            }
        }
    }
    return entity.id.empty() ? std::string("entity") : entity.id;
}

bool applySpawnedComponent(
    EcsWorld& world,
    EntityId entity,
    const SceneEntityDesc& sceneEntity,
    const SceneComponentDesc& sceneComponent,
    const ComponentRegistry& components,
    std::string& error
) {
    const ComponentDescriptor* descriptor = components.findBySceneId(sceneComponent.componentId);
    if (descriptor == nullptr || !descriptor->scene.load || descriptor->runtime.createDefault == nullptr) {
        error = "component is not spawnable: " + sceneComponent.componentId;
        return false;
    }

    ComponentValuePtr componentValue = descriptor->runtime.createDefault();
    if (!componentValue || !descriptor->scene.load(sceneEntity, componentValue.get(), error)) {
        return false;
    }
    if (!world.setComponent(descriptor->type, entity, componentValue.get(), descriptor)) {
        error = "failed to attach spawned component: " + sceneComponent.componentId;
        return false;
    }
    if (descriptor->type == typeKey<Camera>()) {
        const Camera& camera = *static_cast<const Camera*>(componentValue.get());
        if (!camera.role.empty()) {
            world.registerCameraRole(entity, camera.role);
        }
    }
    return true;
}

Quat readQuat(const StructuredObjectView& object, Quat fallback) {
    if (!object.valid()) {
        return fallback;
    }
    return Quat{
        static_cast<float>(object.real("w", fallback.w)),
        static_cast<float>(object.real("x", fallback.x)),
        static_cast<float>(object.real("y", fallback.y)),
        static_cast<float>(object.real("z", fallback.z)),
    };
}

Transform readTransformComponent(const SceneEntityDesc& entity) {
    Transform transform;
    const SceneComponentDesc* component = findSceneComponent(entity, "Transform");
    if (component == nullptr) {
        return transform;
    }

    StructuredDocument document;
    std::string error;
    if (!StructuredDocument::loadFromString(component->valueJson, document, error)) {
        return transform;
    }
    const StructuredObjectView root = document.rootObject();
    readVec3(root.object("position"), transform.position);
    transform.rotation = readQuat(root.object("rotation"), transform.rotation);
    readVec3(root.object("scale"), transform.scale);
    return transform;
}

Transform composeTransform(const Transform& parent, const Transform& local) {
    const Vec3 scaled{
        local.position.x * parent.scale.x,
        local.position.y * parent.scale.y,
        local.position.z * parent.scale.z,
    };
    const Vec3 rotated = rotateVector(parent.rotation, scaled);
    return Transform{
        .position = Vec3{
            parent.position.x + rotated.x,
            parent.position.y + rotated.y,
            parent.position.z + rotated.z,
        },
        .rotation = multiply(parent.rotation, local.rotation),
        .scale = Vec3{
            parent.scale.x * local.scale.x,
            parent.scale.y * local.scale.y,
            parent.scale.z * local.scale.z,
        },
    };
}

bool writeTransformComponent(SceneEntityDesc& entity, const Transform& transform, std::string& error) {
    SceneComponentDesc& transformComponent = ensureSceneComponent(entity, "Transform");
    StructuredMutableDocument document;
    StructuredMutableObject root = document.rootObject();
    writeVec3(root.addObject("position"), transform.position);
    StructuredMutableObject rotation = root.addObject("rotation");
    rotation.setFloat("w", transform.rotation.w);
    rotation.setFloat("x", transform.rotation.x);
    rotation.setFloat("y", transform.rotation.y);
    rotation.setFloat("z", transform.rotation.z);
    writeVec3(root.addObject("scale"), transform.scale);
    transformComponent.componentId = "Transform";
    transformComponent.valueJson = document.writeToString(error);
    return !transformComponent.valueJson.empty() || error.empty();
}

std::string sceneEntityKey(const SceneEntityDesc& entity, std::size_t index) {
    if (!entity.id.empty()) {
        return entity.id;
    }
    return "#" + std::to_string(index);
}

EntityId findSpawnedSceneEntity(
    const std::vector<std::pair<std::string, EntityId>>& createdEntities,
    std::string_view sceneId
) {
    for (const auto& [candidateSceneId, entity] : createdEntities) {
        if (candidateSceneId == sceneId) {
            return entity;
        }
    }
    return 0;
}

bool hierarchyParentId(const SceneEntityDesc& entity, std::string& parentSceneId, std::string& error) {
    const SceneComponentDesc* hierarchy = findSceneComponent(entity, "Hierarchy");
    if (hierarchy == nullptr) {
        parentSceneId.clear();
        return false;
    }

    StructuredDocument document;
    if (!StructuredDocument::loadFromString(hierarchy->valueJson, document, error)) {
        parentSceneId.clear();
        return true;
    }
    parentSceneId = document.rootObject().string("parent");
    return true;
}

bool applySpawnedPrefabHierarchy(
    EcsWorld& world,
    std::span<const SceneEntityDesc> entities,
    const std::vector<std::pair<std::string, EntityId>>& createdEntities,
    std::string& error
) {
    if (entities.empty()) {
        return true;
    }

    for (std::size_t index = 0; index < entities.size(); ++index) {
        const SceneEntityDesc& entityDesc = entities[index];
        const EntityId child = createdEntities[index].second;
        std::string parentSceneId;
        const bool hasHierarchy = hierarchyParentId(entityDesc, parentSceneId, error);
        if (!error.empty()) {
            return false;
        }
        if (!hasHierarchy || parentSceneId.empty()) {
            continue;
        }

        const EntityId parent = findSpawnedSceneEntity(createdEntities, parentSceneId);
        if (child == 0 || parent == 0 || parent == child) {
            error = "spawned prefab hierarchy parent not found: " + parentSceneId;
            return false;
        }
        if (!world.setParent(child, parent)) {
            error = "failed to set spawned prefab hierarchy parent: " + parentSceneId;
            return false;
        }
    }
    return true;
}

void writeWorldTransformToLocal(EcsWorld& world, EntityId entity, Transform& localTransform, const Transform& worldTransform) {
    const EntityId parent = world.parentOf(entity);
    if (const WorldTransform* parentWorld = parent != 0 ? world.tryGetWorldTransform(parent) : nullptr) {
        localTransform = localTransformFromWorldTransform(parentWorld->transform, worldTransform);
    } else {
        localTransform = worldTransform;
    }
}

bool spawnPrefabAt(
    EcsWorld& world,
    FileSystem& files,
    const ComponentRegistry& components,
    std::string_view prefabPath,
    const Transform& spawnTransform,
    std::string& error
) {
    PrefabAsset prefab;
    if (!loadPrefabAssetDocument(files.resolve(std::string(prefabPath)), prefab, error)) {
        return false;
    }
    if (prefab.entities.empty()) {
        error = "prefab has no entities";
        return false;
    }

    std::vector<SceneEntityDesc> entities = prefab.entities;
    if (!writeTransformComponent(entities.front(), spawnTransform, error)) {
        return false;
    }

    std::vector<std::pair<std::string, EntityId>> createdEntities;
    createdEntities.reserve(entities.size());
    for (std::size_t index = 0; index < entities.size(); ++index) {
        const SceneEntityDesc& entityDesc = entities[index];
        const EntityId entity = world.createEntity(entityDisplayName(entityDesc));
        if (entity == 0) {
            error = "failed to create spawned entity";
            return false;
        }
        createdEntities.emplace_back(sceneEntityKey(entityDesc, index), entity);
    }

    for (std::size_t index = 0; index < entities.size(); ++index) {
        const SceneEntityDesc& entityDesc = entities[index];
        const EntityId entity = createdEntities[index].second;
        for (const SceneComponentDesc& component : entityDesc.components) {
            if (component.componentId == "Hierarchy") {
                continue;
            }
            if (!applySpawnedComponent(world, entity, entityDesc, component, components, error)) {
                return false;
            }
        }
    }

    if (!applySpawnedPrefabHierarchy(world, entities, createdEntities, error)) {
        return false;
    }
    world.updateWorldTransforms();
    return true;
}

PhysicsCharacterHandle ensurePhysicsCharacter(
    EcsWorld& world,
    PhysicsWorld& physics,
    CharacterMotor& motor,
    EntityId entity,
    Transform& localTransform,
    const WorldTransform* currentWorldTransform
) {
    if (motor.character) {
        return motor.character;
    }

    Transform spawnWorldTransform = currentWorldTransform != nullptr ? currentWorldTransform->transform : localTransform;
    snapTransformToGround(physics, spawnWorldTransform);
    writeWorldTransformToLocal(world, entity, localTransform, spawnWorldTransform);
    world.updateWorldTransforms();
    motor.character = physics.createCharacter(PhysicsCharacterDesc{
        .radius = motor.radius,
        .cylinderHalfHeight = motor.cylinderHalfHeight,
        .position = characterCenterFromRoot(spawnWorldTransform.position, motor),
        .rotation = spawnWorldTransform.rotation,
        .maxSlopeAngleRadians = motor.maxSlopeAngleRadians,
        .stepHeight = motor.stepHeight,
        .userData = entity,
    });
    return motor.character;
}

void loadInputMapsOnce(SystemContext& ctx, input::InputActionService& actions) {
    SandboxGameplayState& state = ctx.ecs.ensureWorldResource<SandboxGameplayState>();
    if (state.inputMapsLoaded) {
        return;
    }

    FileSystem* files = ctx.tryService<FileSystem>();
    if (files == nullptr) {
        if (!state.inputMapsLogged) {
            state.inputMapsLogged = true;
            ENGINE_LOG_WARN("Sandbox gameplay: FileSystem service is missing, input maps were not loaded");
        }
        return;
    }

    state.inputMapsLoaded = true;

    constexpr std::string_view kMaps[] = {
        "engine://input/keyboard_mouse.json",
        "engine://input/gamepad_xbox.json",
        "engine://input/touch_basic.json",
    };

    for (std::string_view map : kMaps) {
        std::string error;
        if (!actions.loadActionMap(*files, map, error)) {
            ENGINE_LOG_WARN("Sandbox gameplay: input map load failed: {} ({})", map, error);
        }
    }
}

class SandboxInputMapLoadSystem final : public SystemBase<SandboxInputMapLoadSystem, EmptyViews> {
public:
    void run(SystemContext& ctx) {
        input::InputActionService* actions = ctx.tryService<input::InputActionService>();
        if (actions == nullptr) {
            return;
        }
        loadInputMapsOnce(ctx, *actions);
    }
};

std::string_view locomotionStateFromMove(Vec2 move) {
    if (std::fabs(move.x) <= 0.001f && std::fabs(move.y) <= 0.001f) {
        return "Idle";
    }
    if (std::fabs(move.x) > std::fabs(move.y)) {
        return move.x < 0.0f ? std::string_view{"StrafeLeft"} : std::string_view{"StrafeRight"};
    }
    return move.y < 0.0f ? std::string_view{"MoveBackward"} : std::string_view{"Move"};
}

void applyMovementAnimationToEntity(EcsWorld& world, EntityId entity, std::string_view locomotionState, bool moving, float speed) {
    if (auto* stateMachine = static_cast<state_machine::StateMachineComponent*>(
            world.tryGetMutableComponent(typeKey<state_machine::StateMachineComponent>(), entity))) {
        state_machine::setParameter(
            *stateMachine,
            "speed",
            state_machine::makeFloatValue(moving ? speed : 0.0f)
        );
        world.markComponentChanged(typeKey<state_machine::StateMachineComponent>(), entity);
    } else if (auto* graph = static_cast<animation_graph::AnimationGraphComponent*>(
                   world.tryGetMutableComponent(typeKey<animation_graph::AnimationGraphComponent>(), entity))) {
        if (graph->state != locomotionState) {
            graph->state = std::string(locomotionState);
            world.markComponentChanged(typeKey<animation_graph::AnimationGraphComponent>(), entity);
        }
    }

    for (EntityId child : world.childrenOf(entity)) {
        applyMovementAnimationToEntity(world, child, locomotionState, moving, speed);
    }
}

struct SandboxMovingGuard {
    static bool test(
        state_machine::StateMachineGuardContext& ctx,
        std::span<const state_machine::StateMachineArgument>
    ) {
        const state_machine::StateMachineValue* speed = state_machine::findParameter(ctx.component, "speed");
        return speed != nullptr && speed->kind == state_machine::StateMachineValueKind::Float && speed->floatValue > 0.01f;
    }
};

state_machine::StateMachineActionCall setGraphStateAction(state_machine::StateMachineRegistry& registry, std::string state) {
    return state_machine::StateMachineActionCall{
        .action = registry.intern("animation_graph.set_state"),
        .args = {
            state_machine::StateMachineArgument{
                .name = registry.intern("state"),
                .value = state_machine::makeStringValue(std::move(state)),
            },
        },
    };
}

void registerSandboxAnimationGraphs(animation_graph::AnimationGraphRegistry& graphs) {
    animation_graph::AnimationGraphDefinition graph;
    graph.name = graphs.intern("sandbox.ual_locomotion");
    graph.initialState = graphs.intern("Idle");
    graph.states.push_back(animation_graph::AnimationGraphState{
        .name = graphs.intern("Idle"),
        .clipAsset = "asset://characters/ual1/ual1.clips/Idle_Loop.animation.json",
        .loop = true,
        .playing = true,
        .playbackSpeed = 1.0f,
    });
    graph.states.push_back(animation_graph::AnimationGraphState{
        .name = graphs.intern("Move"),
        .clipAsset = "asset://characters/ual1/ual1.clips/Jog_Fwd_Loop.animation.json",
        .loop = true,
        .playing = true,
        .playbackSpeed = 1.0f,
    });
    graphs.registerGraph(std::move(graph));
}

void registerSandboxStateMachines(state_machine::StateMachineRegistry& machines) {
    machines.guard<SandboxMovingGuard>("sandbox.is_moving");

    const state_machine::StateMachineSymbol movingGuard = machines.intern("sandbox.is_moving");

    state_machine::StateMachineState idle;
    idle.name = machines.intern("Idle");
    idle.enterActions.push_back(setGraphStateAction(machines, "Idle"));
    idle.transitions.push_back(state_machine::StateMachineTransition{
        .target = machines.intern("Move"),
        .guards = {
            state_machine::StateMachineGuardCall{.guard = movingGuard},
        },
    });

    state_machine::StateMachineState move;
    move.name = machines.intern("Move");
    move.enterActions.push_back(setGraphStateAction(machines, "Move"));
    move.transitions.push_back(state_machine::StateMachineTransition{
        .target = machines.intern("Idle"),
        .guards = {
            state_machine::StateMachineGuardCall{.guard = movingGuard, .invert = true},
        },
    });

    state_machine::StateMachineDefinition machine;
    machine.name = machines.intern("sandbox.ual_locomotion");
    machine.initialState = machines.intern("Idle");
    machine.states.push_back(std::move(idle));
    machine.states.push_back(std::move(move));
    machines.registerMachine(std::move(machine));
}

void registerSandboxGameplayServices(WorldManager& worlds) {
    registerSandboxAnimationGraphs(worlds.ensureService<animation_graph::AnimationGraphRegistry>());
    registerSandboxStateMachines(worlds.ensureService<state_machine::StateMachineRegistry>());
}

class SandboxSpawnSystem final : public SystemBase<SandboxSpawnSystem, SystemViews<SpawnPointQuery>> {
public:
    void run(SystemContext& ctx) {
        SpawnRuntimeState& state = ctx.ecs.ensureWorldResource<SpawnRuntimeState>();
        if (state.spawned) {
            return;
        }

        FileSystem* files = ctx.tryService<FileSystem>();
        const ComponentRegistry* components = ctx.tryService<ComponentRegistry>();
        PhysicsWorld* physics = ctx.tryService<PhysicsWorld>();
        if (files == nullptr || components == nullptr) {
            if (!state.missingServicesLogged) {
                state.missingServicesLogged = true;
                ENGINE_LOG_WARN(
                    "Sandbox gameplay spawn: missing services world={}:{} files={} components={}",
                    ctx.world.world.index,
                    ctx.world.world.generation,
                    files != nullptr ? "yes" : "no",
                    components != nullptr ? "yes" : "no");
            }
            return;
        }

        bool sawSpawnPoint = false;
        for (auto row : ctx.ecs.view<SpawnPointQuery>()) {
            sawSpawnPoint = true;
            const SpawnPoint& spawn = row.template read<SpawnPoint>();
            if (!spawn.spawnOnStart || spawn.prefab.empty()) {
                ENGINE_LOG_INFO(
                    "Sandbox gameplay spawn: skip spawn point entity={} spawnOnStart={} prefab='{}'",
                    row.entity(),
                    spawn.spawnOnStart ? "true" : "false",
                    spawn.prefab);
                continue;
            }
            Transform spawnTransform = row.template read<Transform>();
            if (physics != nullptr) {
                (void)snapTransformToGround(*physics, spawnTransform);
            }
            const std::size_t beforeEntities = ctx.ecs.entityCount();
            std::string error;
            if (!spawnPrefabAt(ctx.ecs, *files, *components, spawn.prefab, spawnTransform, error)) {
                ENGINE_LOG_ERROR("Sandbox gameplay: prefab spawn failed: {}", error);
                return;
            }
            state.spawned = true;
            const EntityId playCamera = ctx.ecs.findCameraByRole("play_main");
            const Name* cameraName = playCamera != 0 ? ctx.ecs.tryGetName(playCamera) : nullptr;
            ENGINE_LOG_INFO(
                "Sandbox gameplay spawn: prefab='{}' world={}:{} spawnEntity={} entitiesBefore={} entitiesAfter={} playMainCamera={} cameraName='{}' position=({}, {}, {})",
                spawn.prefab,
                ctx.world.world.index,
                ctx.world.world.generation,
                row.entity(),
                beforeEntities,
                ctx.ecs.entityCount(),
                playCamera,
                cameraName != nullptr ? cameraName->value : std::string(""),
                spawnTransform.position.x,
                spawnTransform.position.y,
                spawnTransform.position.z);
            return;
        }
        if (!sawSpawnPoint && !state.waitingForSpawnPointLogged) {
            state.waitingForSpawnPointLogged = true;
            ENGINE_LOG_WARN(
                "Sandbox gameplay spawn: no SpawnPoint rows in world={}:{} entities={}",
                ctx.world.world.index,
                ctx.world.world.generation,
                ctx.ecs.entityCount());
        }
    }
};

class SandboxCharacterControllerSystem final
    : public SystemBase<SandboxCharacterControllerSystem, SystemViews<CharacterControllerQuery>> {
public:
    void run(SystemContext& ctx) {
        input::InputActionService* actions = ctx.tryService<input::InputActionService>();
        if (actions == nullptr) {
            return;
        }
        PhysicsWorld* physics = ctx.tryService<PhysicsWorld>();
        SandboxGameplayState& gameplayState = ctx.ecs.ensureWorldResource<SandboxGameplayState>();
        if (physics == nullptr) {
            if (!gameplayState.physicsMissingLogged) {
                gameplayState.physicsMissingLogged = true;
                ENGINE_LOG_WARN("Sandbox gameplay: PhysicsWorld service is missing, player physics is disabled");
            }
            return;
        }

        const Vec2 move = normalized(actions->axis2D("Move"));
        const bool moving = std::fabs(move.x) > 0.001f || std::fabs(move.y) > 0.001f;
        const bool jump = actions->wasPressed("Jump");
        const bool sprint = actions->isDown("Sprint");

        ctx.ecs.updateWorldTransforms();
        const CameraMovementBasis movementBasis = movementBasisFromCamera(ctx.ecs, *actions);
        bool changedWorldTransform = false;
        for (auto row : ctx.ecs.view<CharacterControllerQuery>()) {
            Transform& transform = row.template write<Transform>();
            const WorldTransform* worldTransform = row.template tryRead<WorldTransform>();
            CharacterMotor& motor = row.template write<CharacterMotor>();
            const PlayerController& controller = row.template read<PlayerController>();
            const PhysicsCharacterHandle character = ensurePhysicsCharacter(
                ctx.ecs,
                *physics,
                motor,
                row.entity(),
                transform,
                worldTransform);
            if (!character) {
                continue;
            }

            PhysicsCharacterState current = physics->getCharacterState(character);
            const Vec3 desiredDirection = movementBasis.valid
                ? horizontalMoveDirectionFromCameraBasis(movementBasis, move)
                : horizontalMoveDirectionFromCameraYaw(0.0f, move);
            const float speed = sprint ? controller.sprintSpeed : controller.walkSpeed;
            motor.velocity.x = desiredDirection.x * speed;
            motor.velocity.z = desiredDirection.z * speed;
            if (current.grounded) {
                motor.velocity.y = jump ? controller.jumpSpeed : 0.0f;
            } else {
                motor.velocity.y -= motor.gravity * ctx.world.frame.deltaTime;
            }

            const Vec3 displacement{
                motor.velocity.x * ctx.world.frame.deltaTime,
                motor.velocity.y * ctx.world.frame.deltaTime,
                motor.velocity.z * ctx.world.frame.deltaTime,
            };
            current = physics->moveCharacter(character, PhysicsCharacterMoveDesc{
                .displacement = displacement,
                .velocity = motor.velocity,
                .deltaTime = ctx.world.frame.deltaTime,
            });
            motor.velocity = current.velocity;
            if (!moving) {
                motor.velocity.x = 0.0f;
                motor.velocity.z = 0.0f;
            }
            if (current.grounded && !jump) {
                motor.velocity.y = 0.0f;
            }
            motor.grounded = current.grounded;
            Transform desiredWorldTransform = worldTransform != nullptr ? worldTransform->transform : transform;
            Vec3 nextPosition = characterRootFromCenter(current.position, motor);
            if (!moving && current.grounded && !jump && worldTransform != nullptr) {
                constexpr float kIdlePositionDeadZoneMeters = 0.02f;
                const Vec3 previousPosition = worldTransform->transform.position;
                if (distanceSquared(previousPosition, nextPosition) <= kIdlePositionDeadZoneMeters * kIdlePositionDeadZoneMeters) {
                    nextPosition = previousPosition;
                    physics->setCharacterPosition(character, characterCenterFromRoot(nextPosition, motor));
                }
            }
            desiredWorldTransform.position = nextPosition;
            const bool shooterControl = isShooterControlMode(controller.controlMode);
            if (shooterControl && moving && movementBasis.valid) {
                const float currentYaw = yawFromRotation(desiredWorldTransform.rotation);
                const float targetYaw = yawFromHorizontalDirection(movementBasis.forward);
                desiredWorldTransform.rotation = yawRotation(approachAngleBySpeed(
                    currentYaw,
                    targetYaw,
                    std::max(0.0f, controller.turnSpeed) * ctx.world.frame.deltaTime));
            } else if (moving) {
                const float currentYaw = yawFromRotation(desiredWorldTransform.rotation);
                const float targetYaw = yawFromHorizontalDirection(desiredDirection);
                desiredWorldTransform.rotation = yawRotation(approachAngleBySpeed(
                    currentYaw,
                    targetYaw,
                    std::max(0.0f, controller.turnSpeed) * ctx.world.frame.deltaTime));
            }
            writeWorldTransformToLocal(ctx.ecs, row.entity(), transform, desiredWorldTransform);
            ctx.ecs.markComponentChanged(typeKey<CharacterMotor>(), row.entity());
            ctx.ecs.markComponentChanged(typeKey<Transform>(), row.entity());
            changedWorldTransform = true;

            applyMovementAnimationToEntity(ctx.ecs, row.entity(), locomotionStateFromMove(move), moving, speed);
        }
        if (changedWorldTransform) {
            ctx.ecs.updateWorldTransforms();
        }
    }
};

void registerSandboxGameplayComponents(ComponentRegistry& registry) {
    registry.add<SpawnPoint>()
        .named("SpawnPoint")
        .sceneId("SpawnPoint")
        .category("Gameplay")
        .scenePresence([](const SceneEntityDesc& entity) { return hasSceneComponent<SpawnPoint>(entity, "SpawnPoint"); })
        .sceneAdd([](SceneEntityDesc& entity, std::string& error) {
            SpawnPoint component;
            return saveSpawnPoint(entity, &component, error);
        })
        .sceneRemove([](SceneEntityDesc& entity, std::string& error) { return removeSceneComponentById<SpawnPoint>(entity, "SpawnPoint", error); })
        .sceneLoad(loadSpawnPoint)
        .sceneSave(saveSpawnPoint)
        .field("prefab", &SpawnPoint::prefab)
        .fieldDisplayName("Prefab")
        .fieldControl("AssetReference")
        .assetReference("prefab")
        .field("spawnOnStart", &SpawnPoint::spawnOnStart)
        .fieldDisplayName("Spawn On Start");

    registry.add<PlayerController>()
        .named("PlayerController")
        .sceneId("PlayerController")
        .category("Gameplay")
        .scenePresence([](const SceneEntityDesc& entity) { return hasSceneComponent<PlayerController>(entity, "PlayerController"); })
        .sceneAdd([](SceneEntityDesc& entity, std::string& error) {
            PlayerController component;
            return savePlayerController(entity, &component, error);
        })
        .sceneRemove([](SceneEntityDesc& entity, std::string& error) { return removeSceneComponentById<PlayerController>(entity, "PlayerController", error); })
        .sceneLoad(loadPlayerController)
        .sceneSave(savePlayerController)
        .field("controlMode", &PlayerController::controlMode)
        .fieldDisplayName("Control Mode")
        .field("walkSpeed", &PlayerController::walkSpeed)
        .fieldDisplayName("Walk Speed")
        .fieldNumericRange(0.1, 20.0)
        .field("sprintSpeed", &PlayerController::sprintSpeed)
        .fieldDisplayName("Sprint Speed")
        .fieldNumericRange(0.1, 40.0)
        .field("jumpSpeed", &PlayerController::jumpSpeed)
        .fieldDisplayName("Jump Speed")
        .fieldNumericRange(0.0, 30.0)
        .field("turnSpeed", &PlayerController::turnSpeed)
        .fieldDisplayName("Turn Speed")
        .fieldNumericRange(0.0, 40.0);

    registry.add<CharacterMotor>()
        .named("CharacterMotor")
        .sceneId("CharacterMotor")
        .category("Gameplay")
        .scenePresence([](const SceneEntityDesc& entity) { return hasSceneComponent<CharacterMotor>(entity, "CharacterMotor"); })
        .sceneAdd([](SceneEntityDesc& entity, std::string& error) {
            CharacterMotor component;
            return saveCharacterMotor(entity, &component, error);
        })
        .sceneRemove([](SceneEntityDesc& entity, std::string& error) { return removeSceneComponentById<CharacterMotor>(entity, "CharacterMotor", error); })
        .sceneLoad(loadCharacterMotor)
        .sceneSave(saveCharacterMotor)
        .field("radius", &CharacterMotor::radius)
        .fieldDisplayName("Capsule Radius")
        .fieldNumericRange(0.05, 2.0)
        .field("cylinderHalfHeight", &CharacterMotor::cylinderHalfHeight)
        .fieldDisplayName("Capsule Half Height")
        .fieldNumericRange(0.05, 3.0)
        .field("gravity", &CharacterMotor::gravity)
        .fieldDisplayName("Gravity")
        .fieldNumericRange(0.0, 50.0)
        .field("stepHeight", &CharacterMotor::stepHeight)
        .fieldDisplayName("Step Height")
        .fieldNumericRange(0.0, 2.0);

    registry.add<Possessed>()
        .named("Possessed")
        .sceneId("Possessed")
        .category("Gameplay")
        .scenePresence([](const SceneEntityDesc& entity) { return hasSceneComponent<Possessed>(entity, "Possessed"); })
        .sceneAdd([](SceneEntityDesc& entity, std::string& error) {
            Possessed component;
            return savePossessed(entity, &component, error);
        })
        .sceneRemove([](SceneEntityDesc& entity, std::string& error) { return removeSceneComponentById<Possessed>(entity, "Possessed", error); })
        .sceneLoad(loadPossessed)
        .sceneSave(savePossessed)
        .field("playerIndex", &Possessed::playerIndex)
        .fieldDisplayName("Player Index");

}

void registerSandboxGameplaySystems(SystemRegistry& systems, ModuleRegistry& modules) {
    systems.add<SandboxInputMapLoadSystem>()
        .named("sandbox.input_maps.load")
        .domains(WorldDomain::PlayScene)
        .after(runtimeSimulationStageSystemType())
        .before(input::inputRuntimeUpdateSystemType())
        .serviceWrite<input::InputActionService>()
        .serviceRead<FileSystem>()
        .allowParallel(false)
        .mainThreadOnly(true);

    systems.add<SandboxSpawnSystem>()
        .named("sandbox.spawn")
        .domains(WorldDomain::PlayScene)
        .after(runtimeSimulationStageSystemType())
        .after(terrain::terrainPhysicsSyncSystemType())
        .before<SandboxCharacterControllerSystem>()
        .serviceRead<FileSystem>()
        .serviceRead<ComponentRegistry>()
        .serviceRead<PhysicsWorld>()
        .allowParallel(false)
        .mainThreadOnly(true);

    systems.add<SandboxCharacterControllerSystem>()
        .named("sandbox.character_controller")
        .domains(WorldDomain::PlayScene)
        .after(runtimeSimulationStageSystemType())
        .after<SandboxSpawnSystem>()
        .after(input::inputRuntimeUpdateSystemType())
        .after(terrain::terrainPhysicsSyncSystemType())
        .before(state_machine::stateMachineUpdateSystemType())
        .before(animation::animationAnimatorResolveSystemType())
        .before(runtimeRenderExtractStageSystemType())
        .serviceWrite<input::InputActionService>()
        .serviceWrite<PhysicsWorld>()
        .serviceRead<state_machine::StateMachineRegistry>()
        .serviceRead<FileSystem>()
        .allowParallel(false)
        .mainThreadOnly(true);

    modules.add<SandboxGameplayModule>()
        .named("sandbox.gameplay")
        .system<SandboxInputMapLoadSystem>()
        .system<SandboxSpawnSystem>()
        .system<SandboxCharacterControllerSystem>();
}

void appendDefaultSandboxGameplayWorldModules(std::vector<TypeKey>& modules) {
    const TypeKey module = typeKey<SandboxGameplayModule>();
    if (std::find(modules.begin(), modules.end(), module) == modules.end()) {
        modules.push_back(module);
    }
}

const PluginDescriptor kSandboxGameplayPlugin{
    .id = "sandbox.gameplay",
    .moduleKind = PluginModuleKind::Project,
    .delivery = PluginDelivery::Static,
    .contributions = PluginContributions{
        .registerComponents = &registerSandboxGameplayComponents,
        .registerServices = &registerSandboxGameplayServices,
        .registerSystems = &registerSandboxGameplaySystems,
        .registerDefaultWorldModules = &appendDefaultSandboxGameplayWorldModules,
    },
};

const StaticPluginRegistrar kSandboxGameplayRegistrar(kSandboxGameplayPlugin);

}  // namespace
}  // namespace engine::sandbox
