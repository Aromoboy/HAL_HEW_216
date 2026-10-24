from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PICKUP = "HEWGame/Game/Entities/PlayerPickupSystem.cpp"
PROGRESSION = "HEWGame/Game/GameFlow/GameplayProgression.cpp"
TYPES = "HEWGame/Game/Sweets/SweetsTypes.h"


def block(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def declarations(path):
    return "\n".join(line for line in (ROOT / path).read_text(encoding="utf-8-sig").splitlines()
                     if not line.startswith(("#include", "#pragma once"))) + "\n"


# ミカ：本番のメソッドと宣言を抽出し描画と物理登録の境界だけを置き換える
PRELUDE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef NDEBUG
#error Assertions must be enabled
#endif
struct float3 { float x = 0, y = 0, z = 0; };
struct AABB { float3 min, max; };
struct Transform { float3 position, rotation; };
struct b2Vec2 { float x, y; };
static std::unordered_map<int, b2Vec2> velocities;
static int velocityWrites = 0;
static int nextBody = 1;
void b2Body_SetLinearVelocity(int id, b2Vec2 velocity) { velocities[id] = velocity; ++velocityWrites; }
b2Vec2 b2Body_GetPosition(int) { return {0, 0}; }
bool b2Body_IsValid(int id) { return id != 0; }
namespace Entities {
struct PhysicalEntity {
    struct { int bodyId = 0; } m_physical;
    bool HasBox2DBody() const { return m_physical.bodyId != 0; }
    int GetBox2DBodyId() const { return m_physical.bodyId; }
    static b2Vec2 ToBox2DVector(float3 v) { return {v.x, v.y}; }
    void SetLinearVelocity(const float3& velocity);
    float3 GetLinearVelocity() const {
        const auto v = velocities[m_physical.bodyId]; return {v.x, v.y, 0};
    }
};
}
namespace SweetsSpace { class BaseSweets; using ActiveSweets = BaseSweets; }
'''

FAKES = r'''
namespace SweetsSpace {
class BaseSweets : public Entities::PhysicalEntity {
public:
    Transform transform{};
    AABB bounds{};
    bool dead = false;
    bool IsPendingDestroy() const { return dead; }
    void SyncColliderBounds() {}
    AABB GetAABB() const { return bounds; }
    Transform GetTransform() const { return transform; }
    void SetTransformViaPhysics(float3 position, float3 rotation) { transform = {position, rotation}; }
    void SetPositionViaPhysics(float3 position) { transform.position = position; }
    const char* GetTag() const { return "fruit"; }
};
class SweetsManager {
public:
    using SweetsList = std::vector<std::shared_ptr<ActiveSweets>>;
    std::array<SweetsList, 4> lists;
    std::unordered_set<ActiveSweets*> anchored;
    bool allowPickup = true, allowRelease = true;
    std::function<bool(const std::shared_ptr<ActiveSweets>&)> releaseOverride;
    int graphQueries = 0, crossings = 0, releaseAttempts = 0;
    SweetsList& GetByState(SweetsState state) { return lists[static_cast<size_t>(state)]; }
    const std::unordered_set<ActiveSweets*>& GetCakeAnchoredSweets() { ++graphQueries; return anchored; }
    bool TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets) {
        if (!allowPickup) return false;
        sweets->m_physical.bodyId = 0;
        return true;
    }
    bool ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets) {
        ++releaseAttempts;
        if (releaseOverride) return releaseOverride(sweets);
        if (!allowRelease) return false;
        assert(!sweets->HasBox2DBody());
        sweets->m_physical.bodyId = nextBody++;
        return true;
    }
    void OnSweetsPassedGuideline(const std::shared_ptr<ActiveSweets>&) { ++crossings; }
};
}
namespace Entities {
class Player {
public:
    Transform m_transform{};
    float3 velocity{};
    bool HasBox2DBody() const { return false; }
    int GetBox2DBodyId() const { return 0; }
    static float3 ToWorldPosition(b2Vec2 value) { return {value.x, value.y, 0}; }
    const Transform& GetTransform() const { return m_transform; }
    float3 GetLinearVelocity() const { return velocity; }
};
class PlayerModel {
public:
    int uncarryAnimations = 0;
    void PlayUnCarry(Player&) { ++uncarryAnimations; }
    void PlayStabAnimation(Player&) {}
};
}
namespace Core {
enum class ActionId { Drop, Confirm };
struct UpdateContext {};
class Input {
public:
    bool drop = false, confirm = false;
    bool IsTrigger(ActionId action) const { return action == ActionId::Drop ? drop : confirm; }
};
}
namespace Systems {
class PhysicsMachine {
public:
    bool allowRegister = true;
    void RegisterBody(SweetsSpace::ActiveSweets& sweets) {
        if (allowRegister) sweets.m_physical.bodyId = nextBody++;
    }
};
}
namespace SweetsSpace {
class SweetsRegistry {
public:
    std::unordered_map<std::shared_ptr<ActiveSweets>, SweetsState> lookup;
    auto& Lookup() { return lookup; }
    std::optional<SweetsState> SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState next) {
        const auto previous = lookup.at(sweets); lookup[sweets] = next; return previous;
    }
};
class SweetsContacts {
public:
    int changes = 0, removals = 0;
    void OnStateChanged(ActiveSweets*, SweetsState, SweetsState) { ++changes; }
    void RemoveCreamWeldJointsForSweets(ActiveSweets*, const char*) { ++removals; }
};
class SweetsLifecycle {
public:
    SweetsRegistry* m_registry = nullptr;
    SweetsContacts* m_contacts = nullptr;
    Systems::PhysicsMachine* m_physicsMachine = nullptr;
    bool* m_warnedMissingPhysicsMachine = nullptr;
    std::unordered_map<std::shared_ptr<ActiveSweets>, float> m_grabRespawnTimers;
    bool ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets);
};
const char* StateToString(SweetsState) { return "state"; }
void DebugTrace(const std::string&) {}
constexpr const char* kLogPrefix = "[Sweets] ";
}
namespace GameFlow {
class PhaseManager {
public:
    struct PhaseUpdateResult { bool phaseAdvanced = false; };
    int phase = 0;
    float observedHeight = 0;
    int GetCurrentPhase() const { return phase; }
    PhaseUpdateResult UpdateHeight(float height) {
        observedHeight = height;
        const int next = static_cast<int>(height / 10);
        const bool advanced = next > phase;
        phase = std::max(phase, next);
        return {advanced};
    }
};
}
struct SoundManagerEX {
    static inline int calls = 0;
    static SoundManagerEX* GetInstance() { static SoundManagerEX instance; return &instance; }
    void PlaySoundEX(const char*, bool, float) { ++calls; }
};
void PlayerDebugTrace(const std::string&) {}
constexpr const char* kPlayerLogPrefix = "[Player] ";
constexpr float kCarryHorizontalOffset = 1, kCarryVerticalOffset = -0.2f, kCarryForwardOffset = 0.5f;
constexpr int kDefaultMaxCarry = 1;
constexpr float kDefaultPickupHalfExtent = 0.5f, kDefaultPickupMaxDistance = 0.5f;
constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();
'''

TESTS = r'''
using namespace SweetsSpace;
using namespace Entities;
static std::shared_ptr<ActiveSweets> fruit(float x, float y, float z = 0, float extent = 0.1f) {
    auto value = std::make_shared<ActiveSweets>();
    value->transform.position = {x, y, z};
    value->bounds = {{x-extent, y-extent, z-extent}, {x+extent, y+extent, z+extent}};
    return value;
}
static PlayerPickupSystem pickup(SweetsManager& manager) {
    PlayerPickupSystem value(&manager);
    value.m_pickupHalfWidth = 2.5f;
    value.m_pickupHalfHeight = 2.5f;
    value.m_pickupOriginOffset = {-0.8f, 0, 0};
    return value;
}
static void testPickup() {
    Player player;
    SweetsManager manager;
    auto farZ = fruit(2, 2, 500);
    manager.GetByState(SweetsState::Active) = {farZ};
    auto system = pickup(manager);
    system.m_pickupMaxDistance = std::numeric_limits<float>::quiet_NaN();
    system.m_pickupDepthThreshold = std::numeric_limits<float>::quiet_NaN();
    system.m_colliderFootprint = PlayerPickupSystem::ColliderFootprint{{1,1,std::numeric_limits<float>::infinity()}, {0,0,0}};
    system.TryPickupNearest(player);
    assert(system.m_lastPickupResult.Succeeded() && system.m_carrieds.back() == farZ);
    assert(system.m_lastPickupResult.distance > 2.8f);

    SweetsManager outside;
    outside.GetByState(SweetsState::Ground) = {fruit(3, 0)};
    auto box = pickup(outside);
    box.TryPickupNearest(player);
    assert(box.m_lastPickupResult.failReason == PickupFailReason::NoCandidates);
    auto edge = fruit(2.6f, 0, -500, 0.2f);
    outside.GetByState(SweetsState::Ground) = {edge};
    box.TryPickupNearest(player);
    assert(box.m_lastPickupResult.sweets == edge && box.m_lastPickupResult.Succeeded());

    SweetsManager ranking;
    auto dead = fruit(0, 0); dead->dead = true;
    auto carried = fruit(0.1f, 0);
    auto distant = fruit(1, 1);
    auto nearest = fruit(0.5f, 0, 1000);
    ranking.GetByState(SweetsState::Ground) = {nullptr, dead, carried, distant};
    ranking.GetByState(SweetsState::Stacked) = {nearest};
    auto choose = pickup(ranking);
    choose.m_carrieds.push_back(carried);
    choose.TryPickupNearest(player);
    assert(choose.m_lastPickupResult.sweets == nearest && choose.m_carrieds.size() == 2);

    SweetsManager rejected;
    rejected.allowPickup = false;
    rejected.GetByState(SweetsState::Active) = {fruit(0, 0)};
    auto refuse = pickup(rejected);
    const int beforeSound = SoundManagerEX::calls;
    refuse.TryPickupNearest(player);
    assert(refuse.m_carrieds.empty() && refuse.m_carriedRotations.empty());
    assert(refuse.m_lastPickupResult.failReason == PickupFailReason::StateTransitionFailed);
    assert(SoundManagerEX::calls == beforeSound);

    auto offset = pickup(manager);
    offset.m_pickupOffset = {Stage::PickupOffsetMode::FacingDirection, {1, 2, 100}};
    offset.m_facingSign = -1;
    offset.m_pickupOriginOffset = {0.8f, 0, 0};
    const auto mirrored = offset.MakePickupBox(player);
    assert(mirrored.center.x == -1 && mirrored.center.y == 2);
    assert(offset.IsInsidePickupBox(mirrored, {-3.5f, 4.5f, -1000}));
}
static void testDrop() {
    Player player; player.velocity = {3, -2, 0};
    SweetsManager manager;
    auto system = pickup(manager);
    auto sweets = fruit(0, 0);
    system.m_carrieds = {sweets};
    system.m_carriedRotations[sweets.get()] = {1, 2, 3};
    const int writes = velocityWrites, sounds = SoundManagerEX::calls;
    manager.allowRelease = false;
    assert(!system.DropCarriedSweets(player));
    assert(system.m_carrieds.size() == 1 && system.m_carriedRotations.contains(sweets.get()));
    assert(!sweets->HasBox2DBody() && velocityWrites == writes && SoundManagerEX::calls == sounds);
    system.m_sweetsManager = nullptr;
    assert(!system.DropCarriedSweets(player));
    assert(system.m_carrieds.size() == 1 && system.m_carriedRotations.contains(sweets.get()));
    assert(velocityWrites == writes && SoundManagerEX::calls == sounds);
    system.m_sweetsManager = &manager;
    manager.allowRelease = true;
    assert(system.DropCarriedSweets(player));
    assert(system.m_carrieds.empty() && system.m_carriedRotations.empty());
    assert(sweets->GetLinearVelocity().x == 3 && sweets->GetLinearVelocity().y == 4);
    assert(velocityWrites == writes + 1 && SoundManagerEX::calls == sounds + 1);
    auto rising = fruit(0, 0);
    system.m_carrieds = {rising};
    player.velocity = {-2, 6, 0};
    assert(system.DropCarriedSweets(player));
    assert(rising->GetLinearVelocity().x == -2 && rising->GetLinearVelocity().y == 6);
}
static void testHeight() {
    using namespace GameplayProgression;
    SweetsManager manager;
    auto airborne = fruit(0, 100);
    auto anchored = fruit(0, 12);
    auto carried = fruit(0, 200);
    auto dead = fruit(0, 300); dead->dead = true;
    manager.GetByState(SweetsState::Active) = {airborne};
    manager.GetByState(SweetsState::Ground) = {anchored, dead};
    manager.GetByState(SweetsState::Carried) = {carried};
    manager.anchored = {anchored.get(), dead.get(), carried.get()};
    GuidelineInput guideline{}; guideline.sweetsManager = &manager;
    GuidelineState line{}; line.heightMeters = 10; line.cooldownDurationSeconds = 1;
    const auto crossed = CheckGuidelineCrossing(guideline, line);
    assert(crossed.crossed && crossed.triggerHeight == anchored->bounds.max.y);
    assert(crossed.state.triggeredThisHeight.contains(anchored.get()));
    assert(manager.graphQueries == 1 && manager.crossings == 1);

    GameFlow::PhaseManager phases;
    TowerTelemetryInput input{}; input.sweetsManager = &manager; input.phaseManager = &phases; input.clearPhaseCount = 2;
    const auto first = UpdateTowerHeightTelemetry(input, {});
    assert(first.state.currentHeight == anchored->bounds.max.y && first.state.peakHeight < 13);
    assert(first.phaseAdvanced && first.state.phaseUpCount == 1 && !first.state.isGameClear);
    assert(phases.observedHeight == first.state.currentHeight && manager.graphQueries == 2);
    manager.anchored.erase(anchored.get());
    const auto fallen = UpdateTowerHeightTelemetry(input, first.state);
    assert(fallen.state.currentHeight == 0 && fallen.state.peakHeight == first.state.peakHeight);
    assert(fallen.state.maxHeight == first.state.maxHeight && !fallen.phaseAdvanced);
    assert(manager.graphQueries == 3);
    const auto unsupported = CheckGuidelineCrossing(guideline, line);
    assert(!unsupported.crossed && manager.graphQueries == 4);
    line.heightMeters = 400;
    const auto below = CheckGuidelineCrossing(guideline, line);
    assert(!below.crossed && manager.graphQueries == 4);
}
static void testReleaseFailureBoundary() {
    SweetsRegistry registry;
    SweetsContacts contacts;
    SweetsLifecycle lifecycle;
    Systems::PhysicsMachine physics;
    bool warned = false;
    lifecycle.m_registry = &registry;
    lifecycle.m_contacts = &contacts;
    lifecycle.m_warnedMissingPhysicsMachine = &warned;
    auto sweets = fruit(0, 0);
    registry.lookup[sweets] = SweetsState::Carried;
    lifecycle.m_grabRespawnTimers[sweets] = 2;
    SweetsManager manager;
    manager.releaseOverride = [&](const auto& value) { return lifecycle.ReleaseCarriedSweets(value); };
    auto system = pickup(manager);
    system.m_carrieds = {sweets};
    system.m_carriedRotations[sweets.get()] = {1, 2, 3};
    Player player; player.velocity = {-3, -1, 0};
    PlayerModel model;
    Core::Input input; input.drop = true;
    Core::UpdateContext context;
    const int sounds = SoundManagerEX::calls, writes = velocityWrites;
    system.HandleCarryInput(player, model, context, input);
    assert(registry.lookup.at(sweets) == SweetsState::Carried && lifecycle.m_grabRespawnTimers.at(sweets) == 2);
    assert(system.m_carrieds.size() == 1 && system.m_carriedRotations.contains(sweets.get()));
    assert(model.uncarryAnimations == 0 && SoundManagerEX::calls == sounds && velocityWrites == writes);
    assert(contacts.changes == 0 && contacts.removals == 0);
    lifecycle.m_physicsMachine = &physics;
    physics.allowRegister = false;
    system.HandleCarryInput(player, model, context, input);
    assert(registry.lookup.at(sweets) == SweetsState::Carried && lifecycle.m_grabRespawnTimers.at(sweets) == 2);
    assert(system.m_carrieds.size() == 1 && system.m_carriedRotations.contains(sweets.get()));
    assert(model.uncarryAnimations == 0 && SoundManagerEX::calls == sounds && velocityWrites == writes);
    assert(contacts.changes == 0 && contacts.removals == 0);
    physics.allowRegister = true;
    system.HandleCarryInput(player, model, context, input);
    assert(registry.lookup.at(sweets) == SweetsState::Active && lifecycle.m_grabRespawnTimers.empty());
    assert(system.m_carrieds.empty() && system.m_carriedRotations.empty());
    assert(model.uncarryAnimations == 1 && SoundManagerEX::calls == sounds + 1 && velocityWrites == writes + 1);
    assert(contacts.changes == 1 && contacts.removals == 1);
    assert(sweets->GetLinearVelocity().x == -3 && sweets->GetLinearVelocity().y == 4);
}
int main() {
    testPickup(); testDrop(); testHeight(); testReleaseFailureBoundary();
    std::cout << "PASS: XY rectangle/AABB/nearest, unused distance/Z, drop registration/velocity/rejection/animation, lifecycle missing/failed physics, cake-only guideline/height, current/peak preservation, bounded graph queries\n";
}
'''


def main():
    source = PRELUDE + "\nnamespace SweetsSpace {\n"
    for signature in ("enum class PickupResult", "enum class PickupFailReason", "struct PickupQueryResult", "enum class SweetsState"):
        source += block(TYPES, signature) + ";\n"
    source += "}\n" + FAKES
    source += declarations("HEWGame/Game/Stage/PickupConfig.h")
    source += declarations("HEWGame/Game/Entities/PlayerPickupSystem.h").replace("private:", "public:")
    source += declarations("HEWGame/Game/GameFlow/GameplayProgression.h")
    for signature in ("const char* PickupOffsetModeName", "const char* PickupResultName", "const char* PickupFailReasonName"):
        source += block(PICKUP, signature)
    source += "\nnamespace Entities {\n"
    source += block("HEWGame/Engine/Core/Entities/PhysicalEntity.cpp", "void PhysicalEntity::SetLinearVelocity(")
    for signature in ("PlayerPickupSystem::PlayerPickupSystem(", "float3 PlayerPickupSystem::ComputeCarryAnchor(",
                      "PlayerPickupSystem::PickupOrigin PlayerPickupSystem::ComputePickupOrigin(",
                      "void PlayerPickupSystem::HandleCarryInput(",
                      "void PlayerPickupSystem::TryPickupNearest(", "bool PlayerPickupSystem::DropCarriedSweets(",
                      "PlayerPickupSystem::PickupBox PlayerPickupSystem::MakePickupBox(", "bool PlayerPickupSystem::IsInsidePickupBox("):
        source += block(PICKUP, signature)
    source += "}\nnamespace GameplayProgression {\n"
    source += block(PROGRESSION, "GuidelineCheckResult CheckGuidelineCrossing(")
    source += block(PROGRESSION, "TowerTelemetryResult UpdateTowerHeightTelemetry(")
    source += "}\nnamespace SweetsSpace {\n"
    source += block("HEWGame/Game/Sweets/SweetsLifecycle.cpp", "bool SweetsLifecycle::ReleaseCarriedSweets(")
    source += "}\n" + TESTS
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    with tempfile.TemporaryDirectory(prefix="gameplay_rules_regression_") as temp:
        directory = Path(temp)
        (directory / "gameplay.cpp").write_text(source, encoding="utf-8")
        command = f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 gameplay.cpp /Fe:gameplay.exe'
        (directory / "build.cmd").write_text(command, encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, check=True)
        subprocess.run([str(directory / "gameplay.exe")], cwd=directory, check=True)


if __name__ == "__main__":
    main()
