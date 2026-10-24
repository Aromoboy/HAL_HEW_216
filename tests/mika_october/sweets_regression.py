from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def method(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


CONTACTS = "HEWGame/Game/Sweets/SweetsContacts.cpp"
SPAWNER = "HEWGame/Game/Sweets/SweetsSpawner.cpp"

HARNESS = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using b2BodyId = int;
using b2ShapeId = int;
using b2ContactId = int;
using b2JointId = int;
struct b2Vec2 { float x{}, y{}; };
b2Vec2 b2Neg(b2Vec2 a) { return {-a.x, -a.y}; }
b2Vec2 b2MulSV(float s, b2Vec2 a) { return {s*a.x, s*a.y}; }
b2Vec2 b2Add(b2Vec2 a, b2Vec2 b) { return {a.x+b.x, a.y+b.y}; }
std::unordered_map<int, b2Vec2> velocities;
float b2Body_GetMass(int) { return 2.0f; }
b2Vec2 b2Body_GetLinearVelocity(int id) { return velocities[id]; }
void b2Body_SetLinearVelocity(int id, b2Vec2 v) { velocities[id] = v; }
struct b2ContactData { int shapeIdA{}, shapeIdB{}; struct { int pointCount{1}; } manifold; };
#define B2_ID_EQUALS(a, b) ((a) == (b))
#define B2_IS_NULL(a) ((a) == 0)
bool b2Body_IsValid(int id) { return id > 0; }
bool b2Shape_IsValid(int id) { return id > 0; }
bool b2Joint_IsValid(int id) { return id > 0; }
int b2Shape_GetBody(int id) { return id; }
std::unordered_map<int, std::vector<b2ContactData>> worldContacts;
int contactReads{};
std::uint64_t worldRevision{};
int b2Body_GetContactCapacity(int id) { return static_cast<int>(worldContacts[id].size()); }
int b2Body_GetContactData(int id, b2ContactData* out, int) {
    ++contactReads;
    std::copy(worldContacts[id].begin(), worldContacts[id].end(), out);
    return static_cast<int>(worldContacts[id].size());
}
namespace Entities {
struct PhysicalEntity {
    virtual ~PhysicalEntity() = default;
    int id{};
    bool fast{};
    std::string tag{"fruit"};
    int GetBox2DBodyId() const { return id; }
    const std::string& GetTag() const { return tag; }
};
}
struct ActiveSweets : Entities::PhysicalEntity {
    bool destroyed{}, carried{true}, cooldown{}, support{true};
    bool IsPendingDestroy() const { return destroyed; }
    bool WasEverCarried() const { return carried; }
};
namespace Systems {
struct ContactEventInfo { int contactId{}, shapeIdA{}, shapeIdB{};
    struct { b2Vec2 normal; } manifold; bool hasManifold{}; float normalImpulse{}; };
struct PhysicsMachine {
    std::vector<Entities::PhysicalEntity*> bodies;
    int creates{};
    mutable int graphScans{};
    std::uint64_t GetContactRevision() const { return worldRevision; }
    const auto& GetBodies() const { ++graphScans; return bodies; }
    int CreateWeldJoint(Entities::PhysicalEntity&, Entities::PhysicalEntity&, b2Vec2,
        float, float, float, float, bool) { ++worldRevision; return ++creates; }
};
}
bool IsCakeTag(const std::string& tag) { return tag == "cake"; }
bool IsCreamTag(const std::string& tag) { return tag == "cream"; }
bool IsBlockedTag(const std::string&) { return false; }
constexpr float kGlueCooldownSeconds = 2.0f;
class SweetsContacts {
public:
    struct ContactAnchorData { b2Vec2 point; int shapeIdA{}, shapeIdB{}; };
    struct PendingGlue { ActiveSweets* cream{}; ActiveSweets* fruit{}; Entities::PhysicalEntity* cake{};
        int contactId{}, shapeIdA{}, shapeIdB{}; };
    struct CreamWeldEntry { ActiveSweets* cream{}; ActiveSweets* fruit{}; int jointId{}; b2Vec2 anchorWorld; };
    struct { float linearHertz{}, linearDampingRatio{}, angularHertz{}, angularDampingRatio{};
        bool collideConnected{}; float breakImpulse{4.0f}; } m_glueTuning;
    bool m_creamEnabled{true};
    std::unordered_map<ActiveSweets*, float> m_glueCooldown;
    int breaks{};
    Systems::PhysicsMachine* m_physicsMachine{};
    bool* m_warnedMissingPhysicsMachine{};
    std::unordered_map<int, PendingGlue> m_pendingGlue;
    std::unordered_map<int, CreamWeldEntry> m_creamWeldJoints;
    std::unordered_map<ActiveSweets*, int> m_creamAnchorJoints;
    std::unordered_map<ActiveSweets*, b2Vec2> m_creamAnchorPoints;
    std::unordered_map<ActiveSweets*, std::unordered_map<ActiveSweets*, std::size_t>> m_fruitToCreams;
    std::unordered_set<int> liveContacts;
    struct ContactGraphNode { std::vector<ActiveSweets*> neighbors; bool visited{}; };
    mutable bool m_cakeAnchorsDirty{true};
    mutable std::uint64_t m_cachedContactRevision{};
    mutable std::unordered_map<ActiveSweets*, ContactGraphNode> m_contactGraph;
    mutable std::unordered_set<ActiveSweets*> m_cakeAnchoredSweets;
    mutable std::vector<ActiveSweets*> m_contactQueue;
    mutable std::vector<b2ContactData> m_contactScratch;
    const std::unordered_set<ActiveSweets*>& GetCakeAnchoredSweets() const;
    void QueueGlue(ActiveSweets*, ActiveSweets*, Entities::PhysicalEntity*, const Systems::ContactEventInfo&);
    void ProcessPendingGlue();
    void OnContactBegin(Entities::PhysicalEntity*, Entities::PhysicalEntity*, const Systems::ContactEventInfo&);
    void UpdateContactStateBegin(ActiveSweets*, Entities::PhysicalEntity*) {}
    void RemoveCreamWeldJointsForSweets(ActiveSweets* fruit, const char*) {
        ++breaks; m_fruitToCreams.erase(fruit);
    }
    Entities::PhysicalEntity* GetEntityForShape(int id) const {
        for (auto* body : m_physicsMachine->bodies) if (body->id == id) return body;
        return nullptr;
    }
    bool TryGetContactAnchorData(int contact, int a, int b, ContactAnchorData& out) const {
        out.shapeIdA = a; out.shapeIdB = b; return liveContacts.contains(contact);
    }
    bool IsGlueCooldownActive(const ActiveSweets& fruit) const { return fruit.cooldown; }
    bool IsGlueSpeedTooFast(const Entities::PhysicalEntity& body) const { return body.fast; }
    bool HasSupportingContact(const ActiveSweets& body, void*) const { return body.support; }
    bool DoesShapeBelongToEntity(const Entities::PhysicalEntity& body, int id) const { return body.id == id; }
    void ApplyGluePushOut(Entities::PhysicalEntity&, bool, const ContactAnchorData&, b2Vec2&) {}
    void CleanupInvalidAnchor(ActiveSweets*) {}
    void CleanupInvalidJoint(int, CreamWeldEntry&) {}
    void LogGlueReject(const char*, const ActiveSweets*, const Entities::PhysicalEntity*) const {}
    void LogGlueCreateFruitToCream(const ActiveSweets*, const ActiveSweets*, b2Vec2) const {}
    void LogGlueCreateCreamToCake(const ActiveSweets*, const Entities::PhysicalEntity*, b2Vec2) const {}
};
constexpr size_t kMaxSpawnQueueEntries = 256;
const char* kLogPrefix = "";
void DebugTrace(const std::string&) {}
class SweetsSpawner {
public:
    enum class SpawnOriginLane { Primary, Secondary, Fallback };
    struct SpawnOriginAssignment { SpawnOriginLane lane; size_t index; };
    std::unordered_map<const ActiveSweets*, SpawnOriginAssignment> m_spawnOriginAssignments;
    std::vector<int> m_spawnOriginLiveCountsPrimary, m_spawnOriginLiveCountsSecondary;
    struct { std::vector<int> weights{1}; } m_spawnTuningConfig;
    std::deque<int> m_spawnQueue;
    int m_currentBurstRemaining{}, m_liveSweetsCap{10}, live{}, m_activeBurstPlanned{};
    int CountLiveSweets() const { return live; }
    int EnqueueSupplementalSpawns(int count) {
        for (int i = 0; i < count; ++i) m_spawnQueue.push_back(1);
        m_currentBurstRemaining += count; return count;
    }
    bool RequestSupplementalSpawn(int, const char*);
    bool UnregisterSpawnOrigin(const std::shared_ptr<ActiveSweets>&);
    void ReleaseSpawnOrigin(const std::shared_ptr<ActiveSweets>&);
};
'''

TESTS = r'''
void connect(int a, int b) {
    ++worldRevision;
    worldContacts[a].push_back({a,b});
    worldContacts[b].push_back({a,b});
}
int main() {
    Systems::PhysicsMachine physics;
    SweetsContacts contacts;
    contacts.m_physicsMachine = &physics;
    constexpr int count = 2000;
    std::vector<ActiveSweets> fruit(count);
    for (int i = 0; i < count; ++i) {
        fruit[i].id = i + 1; physics.bodies.push_back(&fruit[i]);
    }
    Entities::PhysicalEntity cake; cake.id = count + 1; cake.tag = "cake";
    physics.bodies.push_back(&cake);
    connect(1, cake.id);
    for (int i = 1; i < 1000; ++i) connect(i, i + 1);
    connect(1001, 1002); connect(1002, 1003); connect(1003, 1001);
    contacts.m_creamWeldJoints[1] = {&fruit[999], &fruit[1003], 1, {}};
    contacts.m_creamWeldJoints[2] = {&fruit[999], &fruit[1004], 0, {}};
    auto anchored = contacts.GetCakeAnchoredSweets();
    assert(anchored.size() == 1001);
    assert(anchored.contains(&fruit[999]) && anchored.contains(&fruit[1003]));
    assert(!anchored.contains(&fruit[1000]) && !anchored.contains(&fruit[1004]));
    assert(contactReads == 1003);
    assert(contacts.GetCakeAnchoredSweets().size() == 1001 && contactReads == 1003);
    fruit[500].destroyed = true;
    ++worldRevision;
    anchored = contacts.GetCakeAnchoredSweets();
    assert(anchored.size() == 500 && !anchored.contains(&fruit[500]) && !anchored.contains(&fruit[999]));
    fruit[500].destroyed = false;
    worldContacts.clear(); contacts.m_creamWeldJoints.clear();
    ++worldRevision;
    assert(contacts.GetCakeAnchoredSweets().empty());
    contacts.m_creamAnchorJoints[&fruit[1200]] = 1;
    contacts.m_creamWeldJoints[3] = {&fruit[1200], &fruit[1201], 1, {}};
    ++worldRevision;
    assert(contacts.GetCakeAnchoredSweets().size() == 2);
    fruit[1200].destroyed = true;
    ++worldRevision;
    assert(contacts.GetCakeAnchoredSweets().empty());
    fruit[1200].destroyed = false;

    contacts.m_creamAnchorJoints.clear(); contacts.m_creamWeldJoints.clear();
    auto& cream = fruit[0]; auto& target = fruit[1];
    connect(cream.id, target.id);
    cream.fast = true; contacts.liveContacts.insert(10);
    contacts.QueueGlue(&cream, &target, nullptr, {10,1,2});
    contacts.QueueGlue(&cream, &target, nullptr, {10,1,2});
    contacts.ProcessPendingGlue();
    assert(physics.creates == 0 && contacts.m_pendingGlue.size() == 1);
    cream.fast = false; target.cooldown = true;
    contacts.ProcessPendingGlue();
    assert(physics.creates == 0 && contacts.m_pendingGlue.size() == 1);
    target.cooldown = false;
    contacts.ProcessPendingGlue();
    assert(physics.creates == 0 && contacts.m_pendingGlue.size() == 1);
    connect(target.id, fruit[2].id); connect(fruit[2].id, cake.id);
    contacts.ProcessPendingGlue();
    assert(physics.creates == 1 && contacts.m_pendingGlue.empty());
    contacts.QueueGlue(&cream, &target, nullptr, {10,1,2});
    contacts.ProcessPendingGlue(); assert(physics.creates == 1);

    const int retainedJoint = contacts.m_creamWeldJoints.at(10).jointId;
    worldContacts.clear();
    ++worldRevision;
    assert(contacts.GetCakeAnchoredSweets().empty());
    contacts.ProcessPendingGlue();
    assert(contacts.m_creamWeldJoints.at(10).jointId == retainedJoint);
    assert(contacts.m_fruitToCreams.at(&target).contains(&cream));

    contacts.liveContacts.insert(11); cream.fast = true;
    connect(cream.id, cake.id);
    contacts.QueueGlue(&cream, nullptr, &cake, {11,1,cake.id});
    contacts.ProcessPendingGlue(); assert(physics.creates == 1);
    cream.fast = false; contacts.ProcessPendingGlue();
    assert(physics.creates == 2 && contacts.m_creamAnchorJoints.contains(&cream));
    contacts.liveContacts.insert(12); target.fast = true;
    contacts.QueueGlue(&cream, &target, nullptr, {12,1,2});
    contacts.ProcessPendingGlue(); assert(contacts.m_pendingGlue.size() == 1);
    contacts.liveContacts.erase(12); target.fast = false;
    contacts.ProcessPendingGlue();
    assert(physics.creates == 2 && contacts.m_pendingGlue.empty());
    cream.carried = false;
    contacts.QueueGlue(&cream, &target, nullptr, {13,1,2});
    assert(contacts.m_pendingGlue.empty());

    SweetsContacts floating;
    floating.m_physicsMachine = &physics;
    floating.liveContacts = {30,31};
    worldContacts.clear();
    ++worldRevision;
    fruit[10].carried = true;
    connect(fruit[10].id, fruit[11].id); connect(fruit[10].id, fruit[12].id);
    floating.QueueGlue(&fruit[10], &fruit[11], nullptr, {30,fruit[10].id,fruit[11].id});
    floating.QueueGlue(&fruit[10], &fruit[12], nullptr, {31,fruit[10].id,fruit[12].id});
    const int scansBefore = physics.graphScans;
    floating.ProcessPendingGlue();
    assert(physics.graphScans == scansBefore + 1 && floating.m_pendingGlue.size() == 2);
    assert(physics.creates == 2);

    SweetsContacts impact;
    impact.m_physicsMachine = &physics;
    ++impact.m_fruitToCreams[&target][&cream];
    Systems::ContactEventInfo hit{20, 3, target.id};
    hit.hasManifold = true; hit.normalImpulse = 20.0f; hit.manifold.normal = {1.0f, 0.0f};
    impact.OnContactBegin(&fruit[2], &target, hit);
    assert(impact.breaks == 0);
    hit.manifold.normal = {0.0f, -1.0f}; hit.normalImpulse = 3.0f;
    impact.OnContactBegin(&fruit[2], &target, hit);
    assert(impact.breaks == 0);
    hit.normalImpulse = 20.0f;
    velocities[target.id] = {-1.0f, -3.0f};
    impact.OnContactBegin(&fruit[2], &target, hit);
    assert(impact.breaks == 1 && velocities[target.id].x == -1.0f && velocities[target.id].y == -3.0f);

    SweetsSpawner spawner;
    spawner.live = 6; spawner.m_spawnQueue = {1,1}; spawner.m_currentBurstRemaining = 2;
    assert(spawner.RequestSupplementalSpawn(2, nullptr));
    assert(spawner.m_spawnQueue.size() == 4);
    assert(!spawner.RequestSupplementalSpawn(1, nullptr));
    auto removed = std::make_shared<ActiveSweets>();
    spawner.m_spawnOriginAssignments[removed.get()] = {SweetsSpawner::SpawnOriginLane::Primary, 0};
    spawner.m_spawnOriginLiveCountsPrimary = {1};
    spawner.ReleaseSpawnOrigin(removed);
    spawner.ReleaseSpawnOrigin(removed);
    assert(spawner.m_spawnOriginAssignments.empty() && spawner.m_spawnOriginLiveCountsPrimary[0] == 0);
    assert(spawner.m_spawnQueue.size() == 4);
    std::cout << "PASS: anchor graph, linear contact visits, indirect cake support, floating retry, retained welds, no duplicate impact, spawn pending capacity, origin release\n";
}
'''


def main():
    signatures = [
        "const std::unordered_set<ActiveSweets*>& SweetsContacts::GetCakeAnchoredSweets() const",
        "void SweetsContacts::QueueGlue(",
        "void SweetsContacts::ProcessPendingGlue()",
        "void SweetsContacts::OnContactBegin(",
    ]
    source = HARNESS + "\n".join(method(CONTACTS, signature) for signature in signatures)
    source += method(SPAWNER, "bool SweetsSpawner::RequestSupplementalSpawn(")
    source += method(SPAWNER, "bool SweetsSpawner::UnregisterSpawnOrigin(")
    source += method(SPAWNER, "void SweetsSpawner::ReleaseSpawnOrigin(") + TESTS
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    with tempfile.TemporaryDirectory(prefix="sweets_regression_") as temp:
        directory = Path(temp)
        (directory / "sweets.cpp").write_text(source, encoding="utf-8")
        command = f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 sweets.cpp /Fe:sweets.exe'
        (directory / "build.cmd").write_text(command, encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, check=True)
        subprocess.run([str(directory / "sweets.exe")], cwd=directory, check=True)


if __name__ == "__main__":
    main()
