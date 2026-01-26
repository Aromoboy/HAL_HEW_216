#include "Sweets/SweetsSpawner.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

#include "Config/ConfigSectionReader.h"
#include "Diagnostics/Logger.h"
#include "SceneGame/GameplayConfig.h"
#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsFactory.h"
#include "Sweets/SweetsRegistry.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[SweetsManager] ";
    constexpr float kDefaultSpawnIntervalSeconds = 2.5f;
    constexpr float kDefaultBurstIntervalSeconds = 0.5f;
    constexpr int kDefaultSimultaneousSpawnCount = 1;
    constexpr int kDefaultSimultaneousLiveTarget = kDefaultSimultaneousSpawnCount;
    constexpr int kDefaultLiveSweetsCap = 24;
    constexpr float kMinimumBurstIntervalSeconds = 0.05f;
    constexpr float kMinimumSpawnIntervalSeconds = 0.1f;
    constexpr bool kAutoIncludeMissingWeightKinds = true;
    constexpr std::size_t kMaxSpawnQueueEntries = 16;
    constexpr float kDefaultSpawnHeightOffset = 8.0f;
    std::string TrimToken(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }

        while (end != begin)
        {
            auto prev = end;
            --prev;
            if (!std::isspace(static_cast<unsigned char>(*prev)))
            {
                break;
            }
            end = prev;
        }

        return std::string(begin, end);
    }

    std::vector<::SweetsSpace::WeightedSweetsType> ParseWeights(const std::string& raw, bool* outError)
    {
        std::vector<::SweetsSpace::WeightedSweetsType> weights;
        auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
        std::string token;
        std::istringstream stream(raw);

        while (std::getline(stream, token, ','))
        {
            token = TrimToken(token);
            if (token.empty())
            {
                continue;
            }

            float weight = 1.0f;
            std::string kind = token;
            const auto colon = token.find(':');
            if (colon != std::string::npos)
            {
                kind = TrimToken(token.substr(0, colon));
                std::string weightToken = TrimToken(token.substr(colon + 1));
                if (!weightToken.empty())
                {
                    try
                    {
                        weight = std::stof(weightToken);
                    }
                    catch (const std::exception& ex)
                    {
                        if (outError)
                        {
                            *outError = true;
                        }

                        std::ostringstream oss;
                        oss << kLogPrefix << "重量トークン '" << weightToken
                            << "' (元='" << raw << "') のパースに失敗しちゃった: " << ex.what() << "\n";
                        DebugTrace(oss.str());
                        return {};
                    }
                }
            }

            if (!(weight > 0.0f))
            {
                continue;
            }

            const auto resolvedType = manifest.Resolve(kind);
            if (!resolvedType)
            {
                if (outError)
                {
                    *outError = true;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "重量指定 '" << raw << "' に知らないスイーツキー '" << kind
                    << "' が混じってたよ \n";
                DebugTrace(oss.str());
                continue;
            }

            weights.push_back({ *resolvedType, weight });
        }

        if (weights.empty() && outError)
        {
            *outError = true;
        }

        return weights;
    }

    bool TryParseFloat(const std::string& token, float* out)
    {
        try
        {
            *out = std::stof(token);
            return std::isfinite(*out);
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool ParseFloatList(const std::string& value, std::vector<float>& outValues)
    {
        outValues.clear();
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            token = TrimToken(token);
            if (token.empty())
            {
                continue;
            }

            float parsed = 0.0f;
            if (!TryParseFloat(token, &parsed))
            {
                return false;
            }
            outValues.push_back(parsed);
        }

        return !outValues.empty();
    }

    bool ParseFloat3Value(const std::string& value, float3& outValue)
    {
        std::vector<float> components;
        if (!ParseFloatList(value, components))
        {
            return false;
        }

        if (components.size() == 1)
        {
            outValue = float3{ components[0], components[0], components[0] };
            return true;
        }

        if (components.size() == 2)
        {
            outValue = float3{ components[0], components[1], components[1] };
            return true;
        }

        outValue = float3{ components[0], components[1], components[2] };
        return true;
    }

    std::string NormalizeConfigKey(const std::string& value)
    {
        std::string normalized = SweetsSpace::NormalizeKey(value);
        normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](char ch)
            {
                return ch == '_' || ch == '.';
            }),
            normalized.end());
        return normalized;
    }
}

namespace SweetsSpace
{
    SweetsSpawner::SweetsSpawner(SweetsFactory& factory, SweetsRegistry& registry, SpawnCallback spawnCallback)
        : m_factory(&factory)
        , m_registry(&registry)
        , m_spawnCallback(std::move(spawnCallback))
        , m_spawnTimer(kDefaultSpawnIntervalSeconds)
        , m_spawnHeightOffset(kDefaultSpawnHeightOffset)
        , m_activeBurstInterval(kDefaultBurstIntervalSeconds)
        , m_simultaneousLiveTarget(kDefaultSimultaneousLiveTarget)
        , m_batchStackTarget(kDefaultSimultaneousSpawnCount)
        , m_random(std::random_device{}())
        , m_liveSweetsCap(kDefaultLiveSweetsCap)
    {
        ApplyDefaultSpawnConfig();
    }

    void SweetsSpawner::RunSpawnScheduler(float deltaSeconds)
    {
        const int liveSweets = CountLiveSweets();
        const bool liveCapActive = (m_liveSweetsCap > 0);
        const bool spawnBlockedByCap = liveCapActive && liveSweets >= m_liveSweetsCap;

        m_spawnPausedDueToLiveSweets = spawnBlockedByCap;
        m_lastLiveSweetsCount = liveSweets;

        if (m_spawnTuningConfig.weights.empty())
        {
            ApplyDefaultSpawnConfig();
            if (m_spawnTuningConfig.weights.empty())
            {
                return;
            }
        }

        if (!(deltaSeconds > 0.0f))
        {
            return;
        }
        const auto& config = m_spawnTuningConfig;
        const float interval = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        const float intervalMultiplier = std::max(m_spawnIntervalMultiplier, 0.01f);
        const float adjustedInterval = std::max(kMinimumSpawnIntervalSeconds, interval / intervalMultiplier);
        m_activeBurstInterval = std::max(config.burstIntervalSeconds, kMinimumBurstIntervalSeconds);

        const int batchTarget = std::max(1, config.simultaneousSpawnCount + m_spawnCountBonus);
        const int simultaneousTarget = std::max(1, config.simultaneousLiveTarget + m_spawnCountBonus);

        m_simultaneousLiveTarget = simultaneousTarget;
        m_batchStackTarget = batchTarget;

        if (!m_batchCoolingDown && m_stackedInBatch >= m_batchStackTarget)
        {
            m_batchCoolingDown = true;
            m_spawnTimer = 0.0f;
            m_batchInitialSpawnDone = false;
            m_activeBurstPlanned = 0;
            m_currentBurstRemaining = 0;
            m_pendingTopUpCount = 0;
            m_spawnQueue.clear();
        }

        if (m_batchCoolingDown)
        {
            m_spawnTimer += deltaSeconds;
            const float remainingCooldown = std::max(0.0f, adjustedInterval - m_spawnTimer);
            if (remainingCooldown > 0.0f)
            {
                return;
            }

            m_batchCoolingDown = false;
            m_stackedInBatch = 0;
            m_batchInitialSpawnDone = false;
            m_spawnTimer = 0.0f;
        }

        if (spawnBlockedByCap)
        {
            m_burstTimer = std::min(m_activeBurstInterval, m_burstTimer + deltaSeconds);
            return;
        }

        const int pendingRequests = static_cast<int>(m_spawnQueue.size()) + m_currentBurstRemaining;
        const int projectedLive = liveSweets + pendingRequests;
        const int remainingToStack = std::max(0, m_batchStackTarget - m_stackedInBatch);
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);

        if (desiredLiveTarget <= 0)
        {
            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            return;
        }

        const bool forceSeedNewBatch = (!m_batchInitialSpawnDone && m_stackedInBatch == 0);
        const bool liveAtTarget = liveSweets >= desiredLiveTarget;
        if (liveAtTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
            }

            if (m_pendingTopUpCount > 0)
            {
                m_pendingTopUpCount = 0;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            return;
        }

        if (liveAtTarget && forceSeedNewBatch)
        {
            if (m_pendingTopUpCount > 0)
            {
                m_pendingTopUpCount = 0;
            }
        }

        if (projectedLive >= desiredLiveTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            m_pendingTopUpCount = 0;
            return;
        }

        const bool delayBurst = (m_pendingTopUpCount > 0);
        const int deficit = desiredLiveTarget - projectedLive;
        const int pendingTopUpCount = delayBurst ? std::min(m_pendingTopUpCount, deficit) : 0;
        const int requestedCount = forceSeedNewBatch ? desiredLiveTarget : (delayBurst ? std::max(1, pendingTopUpCount) : deficit);
        const int availableQueueSlots = static_cast<int>(kMaxSpawnQueueEntries - m_spawnQueue.size());
        const int clampedRequestedCount = std::max(0, std::min(requestedCount, availableQueueSlots));
        m_activeBurstPlanned = std::max(desiredLiveTarget, clampedRequestedCount);

        const int enqueued = EnqueueSpawns(clampedRequestedCount);
        m_burstTimer = delayBurst ? 0.0f : m_activeBurstInterval;
        if (forceSeedNewBatch)
        {
            m_pendingTopUpCount = 0;
        }
        else if (delayBurst && enqueued > 0)
        {
            m_pendingTopUpCount = std::max(0, m_pendingTopUpCount - enqueued);
        }
        m_batchInitialSpawnDone = true;

        EmitSpawnRequests(deltaSeconds);
        m_lastLiveSweetsCount = CountLiveSweets();
    }

    bool SweetsSpawner::LoadSpawnTuningConfig(const std::string& path)
    {
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(path, GameplayConfig::kSectionSpawnTuning, lines))
        {
            ApplyDefaultSpawnConfig();
            std::ostringstream oss;
            oss << kLogPrefix << "スポーン調整の設定 '" << GameplayConfig::kSectionSpawnTuning
                << "' が見つからないから、デフォルトに戻すね ('" << path << "')\n";
            DebugTrace(oss.str());
            return false;
        }

        SpawnTuningConfig config{};
        bool parseError = false;
        bool hasWeights = false;
        bool hasSpawnInterval = false;
        bool hasBurstInterval = false;
        bool hasSimultaneous = false;
        bool hasLiveTarget = false;

        for (const auto& entry : lines)
        {
            std::string key = NormalizeConfigKey(entry.key);
            const std::string value = TrimToken(entry.value);
            if (key.empty())
            {
                continue;
            }

            if (key == "spawnintervalseconds" || key == "spawninterval")
            {
                float parsed = 0.0f;
                if (TryParseFloat(value, &parsed))
                {
                    config.spawnIntervalSeconds = parsed;
                    hasSpawnInterval = true;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (key == "burstintervalseconds" || key == "burstinterval")
            {
                float parsed = 0.0f;
                if (TryParseFloat(value, &parsed))
                {
                    config.burstIntervalSeconds = parsed;
                    hasBurstInterval = true;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (key == "simultaneousspawncount" || key == "simultaneous")
            {
                try
                {
                    config.simultaneousSpawnCount = std::stoi(value);
                    hasSimultaneous = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (key == "simultaneouslivetarget" || key == "livetarget")
            {
                try
                {
                    config.simultaneousLiveTarget = std::stoi(value);
                    hasLiveTarget = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (key == "weights")
            {
                bool weightError = false;
                config.weights = ParseWeights(value, &weightError);
                if (weightError)
                {
                    parseError = true;
                }
                else
                {
                    hasWeights = true;
                }
            }
            else if (key == "spawnoriginoffset")
            {
                float3 offset{};
                if (ParseFloat3Value(value, offset))
                {
                    if (!m_spawnOriginsPrimary.empty())
                    {
                        for (auto& origin : m_spawnOriginsPrimary)
                        {
                            origin.x += offset.x;
                            origin.y += offset.y;
                            origin.z += offset.z;
                        }
                    }
                    if (!m_spawnOriginsSecondary.empty())
                    {
                        for (auto& origin : m_spawnOriginsSecondary)
                        {
                            origin.x += offset.x;
                            origin.y += offset.y;
                            origin.z += offset.z;
                        }
                    }
                }
                else
                {
                    parseError = true;
                }
            }
        }

        if (!hasWeights)
        {
            std::ostringstream warn;
            warn << kLogPrefix << "spawn.tuning に weight がないのでデフォルトを入れるね\n";
            DebugTrace(warn.str());
            config.weights = BuildDefaultWeights();
        }
        else
        {
            config.weights = NormalizeWeights(config.weights);
        }

        if (!hasSpawnInterval)
        {
            config.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
        }
        if (!hasBurstInterval)
        {
            config.burstIntervalSeconds = kDefaultBurstIntervalSeconds;
        }
        if (!hasSimultaneous)
        {
            config.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
        }
        if (!hasLiveTarget)
        {
            config.simultaneousLiveTarget = config.simultaneousSpawnCount;
        }

        config.spawnIntervalSeconds = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        config.burstIntervalSeconds = std::max(config.burstIntervalSeconds, kMinimumBurstIntervalSeconds);
        config.simultaneousSpawnCount = std::max(1, config.simultaneousSpawnCount);
        config.simultaneousLiveTarget = std::max(1, config.simultaneousLiveTarget);

        std::unordered_set<std::string> registeredKindsNormalized;
        if (m_factory)
        {
            for (const auto& entry : m_factory->GetSpawners())
            {
                registeredKindsNormalized.insert(NormalizeKey(entry.first));
            }
        }

        std::unordered_map<SweetsType, float, SweetsTypeHash> defaultWeightLookup;
        for (const auto& entry : BuildDefaultWeights())
        {
            defaultWeightLookup[entry.kind] = entry.weight;
        }

        EnsureRegisteredWeights(config, registeredKindsNormalized, defaultWeightLookup);
        m_spawnTuningConfig = config;
        ForceSpawnRefresh();

        std::ostringstream oss;
        oss << kLogPrefix << "'" << path << "' からスポーン調整を読んだよ (parseError="
            << std::boolalpha << parseError << ")\n";
        DebugTrace(oss.str());
        LogSpawnParameters(m_spawnTuningConfig);

        return !parseError;
    }

    void SweetsSpawner::ForceSpawnRefresh()
    {
        m_spawnTimer = 0.0f;
        m_burstTimer = 0.0f;
        m_activeBurstPlanned = 0;
        m_stackedInBatch = 0;
        m_batchInitialSpawnDone = false;
        m_batchCoolingDown = false;
        m_activeBurstInterval = std::max(m_spawnTuningConfig.burstIntervalSeconds, kMinimumBurstIntervalSeconds);
        m_batchStackTarget = std::max(1, m_spawnTuningConfig.simultaneousSpawnCount + m_spawnCountBonus);
        m_simultaneousLiveTarget = std::max(1, m_spawnTuningConfig.simultaneousLiveTarget + m_spawnCountBonus);
        m_pendingTopUpCount = 0;
        if (!m_spawnQueue.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "保留中のスポーンキューをクリアするね (" << m_spawnQueue.size() << ") \n";
            DebugTrace(oss.str());
        }
        m_spawnQueue.clear();
        m_currentBurstRemaining = 0;
    }

    void SweetsSpawner::SeedInitialSpawns()
    {
        if (m_spawnPerPointInitial <= 0)
        {
            return;
        }

        const int spawned = SpawnInitialPerPoint(m_spawnPerPointInitial);
        if (spawned > 0)
        {
            m_burstTimer = m_activeBurstInterval;
        }
    }

    void SweetsSpawner::SetSpawnOrigins(
        const std::vector<float3>& primary,
        const std::vector<float3>& secondary,
        std::optional<float> spawnHeightOffset)
    {
        m_spawnOriginsPrimary = primary;
        m_spawnOriginsSecondary = secondary;
        if (spawnHeightOffset)
        {
            m_spawnHeightOffset = *spawnHeightOffset;
        }
        m_warnedMissingSpawnOrigins = false;
        ResetSpawnOriginTracking();
    }

    void SweetsSpawner::OffsetSpawnOriginsY(float deltaY)
    {
        if (deltaY == 0.0f)
        {
            return;
        }

        for (auto& origin : m_spawnOriginsPrimary)
        {
            origin.y += deltaY;
        }

        for (auto& origin : m_spawnOriginsSecondary)
        {
            origin.y += deltaY;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "[SpawnOrigin] guideline advanced offsetY=" << deltaY;
        if (!m_spawnOriginsPrimary.empty())
        {
            oss << " primaryY=" << m_spawnOriginsPrimary.front().y;
        }
        else
        {
            oss << " primaryY=none";
        }
        if (!m_spawnOriginsSecondary.empty())
        {
            oss << " secondaryY=" << m_spawnOriginsSecondary.front().y;
        }
        else
        {
            oss << " secondaryY=none";
        }
        oss << "\n";
        DebugTrace(oss.str());
    }

    void SweetsSpawner::SetSpawnIntervalMultiplier(float multiplier)
    {
        m_spawnIntervalMultiplier = std::isfinite(multiplier) ? multiplier : 1.0f;
        if (m_spawnIntervalMultiplier <= 0.0f)
        {
            m_spawnIntervalMultiplier = 1.0f;
        }
    }

    void SweetsSpawner::SetSpawnCountBonus(int bonusCount)
    {
        m_spawnCountBonus = bonusCount;
    }

    void SweetsSpawner::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_randomRotationMinDegrees = minDegrees;
        m_randomRotationMaxDegrees = maxDegrees;
    }

    void SweetsSpawner::SetLiveSweetsCap(int cap)
    {
        const int sanitizedCap = std::max(0, cap);
        const bool changed = (sanitizedCap != m_liveSweetsCap);
        m_liveSweetsCap = sanitizedCap;
        const bool capActive = (m_liveSweetsCap > 0);

        std::ostringstream oss;
        oss << kLogPrefix << "同時出現上限は " << (sanitizedCap > 0 ? "設定" : "無効")
            << " (cap=" << sanitizedCap << ", queued=" << m_spawnQueue.size()
            << ", liveCount=" << m_lastLiveSweetsCount << ") にしたよ\n";
        DebugTrace(oss.str());

        if (changed && m_spawnPausedDueToLiveSweets && (!capActive || m_lastLiveSweetsCount < m_liveSweetsCap))
        {
            m_spawnPausedDueToLiveSweets = false;
        }
    }

    void SweetsSpawner::SetSpawnPointCap(int cap)
    {
        const int sanitizedCap = std::max(0, cap);
        m_spawnPerPointCap = sanitizedCap;

        std::ostringstream oss;
        oss << kLogPrefix << "スポーン地点ごとの上限は " << (sanitizedCap > 0 ? "設定" : "無効")
            << " (cap=" << sanitizedCap << ", primary=" << m_spawnOriginsPrimary.size()
            << ", secondary=" << m_spawnOriginsSecondary.size() << ") にしたよ\n";
        DebugTrace(oss.str());
    }

    void SweetsSpawner::SetSpawnPerPointInitial(int count)
    {
        const int sanitizedCount = std::max(0, count);
        m_spawnPerPointInitial = sanitizedCount;

        std::ostringstream oss;
        oss << kLogPrefix << "初期スポーン数(1地点あたり)は "
            << (sanitizedCount > 0 ? "設定" : "無効")
            << " (initial=" << sanitizedCount << ") にしたよ\n";
        DebugTrace(oss.str());
    }

    bool SweetsSpawner::RequestSupplementalSpawn(int count, const char* context)
    {
        if (count <= 0)
        {
            return false;
        }

        if (m_spawnTuningConfig.weights.empty())
        {
            return false;
        }

        const int liveSweets = CountLiveSweets();
        const int pendingRequests = static_cast<int>(m_spawnQueue.size()) + m_currentBurstRemaining;
        int allowedCount = count;
        if (m_liveSweetsCap > 0)
        {
            allowedCount = std::min(allowedCount, std::max(0, m_liveSweetsCap - liveSweets - pendingRequests));
        }

        const int availableQueueSlots = static_cast<int>(kMaxSpawnQueueEntries - m_spawnQueue.size());
        allowedCount = std::min(allowedCount, std::max(0, availableQueueSlots));

        if (allowedCount <= 0)
        {
            return false;
        }

        const int enqueued = EnqueueSupplementalSpawns(allowedCount);
        if (enqueued > 0)
        {
            m_activeBurstPlanned = std::max(m_activeBurstPlanned, enqueued);
            if (context)
            {
                std::ostringstream oss;
                oss << kLogPrefix << context << " でスイーツ補充を依頼したよ count=" << enqueued
                    << " live=" << liveSweets
                    << " queued=" << m_spawnQueue.size()
                    << " burstRemaining=" << m_currentBurstRemaining << "\n";
                DebugTrace(oss.str());
            }
            return true;
        }

        return false;
    }

    int SweetsSpawner::CountLiveSweets() const
    {
        if (!m_registry)
        {
            return 0;
        }

        const std::array<SweetsState, 3> liveStates = {
            SweetsState::Active,
            SweetsState::Ground,
            SweetsState::Carried,
        };

        int total = 0;
        for (const auto state : liveStates)
        {
            const auto it = m_registry->StateMap().find(state);
            if (it != m_registry->StateMap().end())
            {
                for (const auto& sweets : it->second)
                {
                    if (sweets && !sweets->IsPendingDestroy())
                    {
                        ++total;
                    }
                }
            }
        }

        return total;
    }

    int SweetsSpawner::ComputeDesiredLiveTarget(int remainingToStack) const
    {
        if (remainingToStack <= 0)
        {
            return 0;
        }

        int desired = std::max(1, std::min(m_simultaneousLiveTarget, remainingToStack));
        if (m_liveSweetsCap > 0)
        {
            desired = std::min(desired, m_liveSweetsCap);
        }
        return desired;
    }

    int SweetsSpawner::GetBatchStackTarget() const
    {
        return m_batchStackTarget;
    }

    int SweetsSpawner::GetStackedInBatch() const
    {
        return m_stackedInBatch;
    }

    void SweetsSpawner::OnStateChanged(const std::shared_ptr<ActiveSweets>& sweets, SweetsState previousState, SweetsState newState)
    {
        (void)sweets;
        if (newState == SweetsState::Stacked && previousState != SweetsState::Stacked)
        {
            ++m_stackedInBatch;

            std::ostringstream oss;
            oss << kLogPrefix << "積み上げ進捗=" << m_stackedInBatch
                << '/' << m_batchStackTarget << " tag='" << (sweets ? sweets->GetTag() : "<null>") << "' だよ \n";
            DebugTrace(oss.str());
        }
    }

    void SweetsSpawner::OnSweetsRemoved(const std::shared_ptr<ActiveSweets>& sweets, int remainingToStack,
        int liveAfterRemoval, const char* context)
    {
        UnregisterSpawnOrigin(sweets);
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);
        if (!m_batchCoolingDown && desiredLiveTarget > 0 && liveAfterRemoval < desiredLiveTarget)
        {
            m_pendingTopUpCount = std::max(m_pendingTopUpCount, desiredLiveTarget - liveAfterRemoval);

            std::ostringstream oss;
            oss << kLogPrefix << context << " で目標より生存スイーツが減ったから、あとで補充するね  live="
                << liveAfterRemoval << " target=" << desiredLiveTarget << " remainingToStack=" << remainingToStack
                << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
                << " pendingTopUp=" << m_pendingTopUpCount
                << " queue=" << m_spawnQueue.size()
                << " burstInterval=" << std::fixed << std::setprecision(2) << m_activeBurstInterval << "\n";
            DebugTrace(oss.str());
        }
    }

    int SweetsSpawner::EnqueueSpawns(int count)
    {
        if (count <= 0)
        {
            return 0;
        }

        const int burstTarget = (m_activeBurstPlanned > 0) ? m_activeBurstPlanned : count;
        const int availableQueueSlots = static_cast<int>(kMaxSpawnQueueEntries - m_spawnQueue.size());
        const int allowedCount = std::max(0, std::min({ count, burstTarget, availableQueueSlots }));

        int enqueued = 0;
        for (int i = 0; i < allowedCount; ++i)
        {
            if (m_spawnQueue.size() >= kMaxSpawnQueueEntries)
            {
                break;
            }

            const std::string chosenKind = ChooseKindFromActiveConfig();
            const std::string resolvedKind = m_factory ? m_factory->ResolveKindKey(chosenKind) : chosenKind;
            const std::string kind = NormalizeKey(resolvedKind);
            if (kind.empty())
            {
                break;
            }

            m_spawnQueue.push_back(kind);
            ++enqueued;
        }

        if (enqueued != allowedCount)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スポーンキューへの積み込みが足りなかったよ enqueued=" << enqueued
                << " expected=" << allowedCount << " queueSize=" << m_spawnQueue.size() << ".\n";
            DebugTrace(oss.str());
        }

        m_currentBurstRemaining = enqueued;
        return enqueued;
    }

    int SweetsSpawner::EnqueueSupplementalSpawns(int count)
    {
        if (count <= 0)
        {
            return 0;
        }

        const int burstTarget = (m_activeBurstPlanned > 0) ? m_activeBurstPlanned : count;
        const int availableQueueSlots = static_cast<int>(kMaxSpawnQueueEntries - m_spawnQueue.size());
        const int allowedCount = std::max(0, std::min({ count, burstTarget, availableQueueSlots }));

        int enqueued = 0;
        for (int i = 0; i < allowedCount; ++i)
        {
            if (m_spawnQueue.size() >= kMaxSpawnQueueEntries)
            {
                break;
            }

            const std::string chosenKind = ChooseKindFromActiveConfig();
            const std::string resolvedKind = m_factory ? m_factory->ResolveKindKey(chosenKind) : chosenKind;
            const std::string kind = NormalizeKey(resolvedKind);
            if (kind.empty())
            {
                break;
            }

            m_spawnQueue.push_back(kind);
            ++enqueued;
        }

        if (enqueued != allowedCount)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "補充リクエストの積み込みが足りなかったよ enqueued=" << enqueued
                << " expected=" << allowedCount << " queueSize=" << m_spawnQueue.size() << ".\n";
            DebugTrace(oss.str());
        }

        if (m_currentBurstRemaining < 0)
        {
            m_currentBurstRemaining = 0;
        }

        m_currentBurstRemaining += enqueued;
        return enqueued;
    }

    void SweetsSpawner::EmitSpawnRequests(float deltaSeconds)
    {
        if (m_spawnQueue.empty() || !m_spawnCallback)
        {
            return;
        }

        m_burstTimer += deltaSeconds;
        while (!m_spawnQueue.empty() && m_burstTimer >= m_activeBurstInterval)
        {
            const std::string requestedKind = m_spawnQueue.front();
            const std::string resolvedKind = m_factory ? m_factory->ResolveKindKey(requestedKind) : requestedKind;
            const std::string kind = NormalizeKey(resolvedKind);
            const auto originSelection = NextSpawnOrigin();
            if (!originSelection)
            {
                break;
            }

            m_burstTimer -= m_activeBurstInterval;
            m_spawnQueue.pop_front();

            if (m_currentBurstRemaining > 0)
            {
                --m_currentBurstRemaining;
            }

            Transform transform = BuildSpawnTransform(kind, *originSelection);
            const auto spawned = m_spawnCallback(kind, transform);
            if (spawned)
            {
                RegisterSpawnOrigin(spawned, *originSelection);
            }
        }

        if (m_currentBurstRemaining < 0)
        {
            m_currentBurstRemaining = 0;
        }
    }

    std::string SweetsSpawner::ChooseKindFromActiveConfig()
    {
        const auto& config = m_spawnTuningConfig;
        if (config.weights.empty())
        {
            return {};
        }

        float totalWeight = 0.0f;
        for (const auto& entry : config.weights)
        {
            totalWeight += entry.weight;
        }

        if (!(totalWeight > 0.0f))
        {
            return {};
        }

        std::uniform_real_distribution<float> dist(0.0f, totalWeight);
        float roll = dist(m_random);
        std::optional<SweetsType> chosenType;
        for (const auto& entry : config.weights)
        {
            if (roll <= entry.weight)
            {
                chosenType = entry.kind;
                break;
            }
            roll -= entry.weight;
        }

        if (!chosenType)
        {
            chosenType = config.weights.back().kind;
        }

        const auto* descriptor = SweetsPrototypeManifest::Instance().Find(*chosenType);
        if (!descriptor)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スポーン候補選びでスイーツタイプ '" << ToString(*chosenType)
                << "' のデスクリプタを見つけられなかったよ \n";
            DebugTrace(oss.str());
            return {};
        }

        return NormalizeKey(descriptor->key);
    }

    void SweetsSpawner::LogSpawnParameters(const SpawnTuningConfig& config) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Spawn] interval=" << std::fixed << std::setprecision(2) << config.spawnIntervalSeconds
            << " burstInterval=" << std::fixed << std::setprecision(2) << config.burstIntervalSeconds
            << " simultaneous=" << config.simultaneousSpawnCount
            << " liveTarget=" << config.simultaneousLiveTarget
            << " weights=";

        if (config.weights.empty())
        {
            oss << "(none)";
        }
        else
        {
            bool first = true;
            for (const auto& entry : config.weights)
            {
                oss << (first ? ' ' : ',') << ToString(entry.kind)
                    << ':' << std::fixed << std::setprecision(2) << entry.weight;
                first = false;
            }
        }

        oss << "\n";
        DebugTrace(oss.str());
    }

    std::vector<WeightedSweetsType> SweetsSpawner::NormalizeWeights(const std::vector<WeightedSweetsType>& weights) const
    {
        float total = 0.0f;
        for (const auto& entry : weights)
        {
            total += entry.weight;
        }

        if (!(total > 0.0f))
        {
            return weights;
        }

        std::vector<WeightedSweetsType> normalized;
        normalized.reserve(weights.size());
        for (const auto& entry : weights)
        {
            normalized.push_back({ entry.kind, entry.weight / total });
        }
        return normalized;
    }

    void SweetsSpawner::ApplyDefaultSpawnConfig()
    {
        std::vector<WeightedSweetsType> baseWeights = BuildDefaultWeights();
        if (baseWeights.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "デフォルトのスポーン設定を作れないよ スイーツが何も登録されてない \n");
            return;
        }

        SpawnTuningConfig config{};
        config.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
        config.burstIntervalSeconds = kDefaultBurstIntervalSeconds;
        config.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
        config.simultaneousLiveTarget = kDefaultSimultaneousLiveTarget;
        config.weights = baseWeights;
        m_spawnTuningConfig = config;
        ForceSpawnRefresh();

        std::ostringstream oss;
        oss << kLogPrefix << "デフォルトのスポーン設定を適用したよ\n";
        DebugTrace(oss.str());
        LogSpawnParameters(m_spawnTuningConfig);
    }

    std::vector<WeightedSweetsType> SweetsSpawner::BuildDefaultWeights() const
    {
        std::vector<WeightedSweetsType> weights;
        if (!m_factory)
        {
            return weights;
        }
        weights.reserve(m_factory->GetSpawners().size());
        auto& manifest = SweetsPrototypeManifest::Instance();
        for (const auto& entry : m_factory->GetSpawners())
        {
            float spawnWeight = 1.0f;
            auto presetIt = m_factory->GetFactoryPresets().find(entry.first);
            if (presetIt != m_factory->GetFactoryPresets().end())
            {
                spawnWeight = presetIt->second.spawnWeight;
            }
            if (spawnWeight <= 0.0f)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << entry.first
                    << "' のデフォルトウェイトが " << spawnWeight << "<= 0 だったのでスキップするね \n";
                DebugTrace(warn.str());
                continue;
            }

            const auto resolvedType = manifest.Resolve(entry.first);
            if (!resolvedType)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "知らないスイーツキー '" << entry.first
                    << "' のデフォルトウェイトはスキップするよ \n";
                DebugTrace(warn.str());
                continue;
            }

            std::ostringstream resolved;
            resolved << kLogPrefix << "[SpawnWeights] key='" << entry.first << "' -> type="
                << ToString(*resolvedType) << " weight=" << spawnWeight << "\n";
            Diagnostics::LogInfo(resolved.str());

            weights.push_back({ *resolvedType, spawnWeight });
        }
        return weights;
    }

    std::optional<SweetsSpawner::SpawnOriginSelection> SweetsSpawner::NextSpawnOrigin()
    {
        const bool hasPrimary = !m_spawnOriginsPrimary.empty();
        const bool hasSecondary = !m_spawnOriginsSecondary.empty();
        if (!hasPrimary && !hasSecondary)
        {
            if (!m_warnedMissingSpawnOrigins)
            {
                DebugTrace(std::string(kLogPrefix) + "スポーン元リストが空だから、ワールド原点を使うね \n");
                m_warnedMissingSpawnOrigins = true;
            }
            return SpawnOriginSelection{ float3{ 0.0f, 0.0f, 0.0f }, "fallback", SpawnOriginLane::Fallback, 0 };
        }

        auto selectFromLane = [this](std::vector<float3>& origins, std::vector<int>& liveCounts,
            std::size_t& cursor, const char* label, SpawnOriginLane lane) -> std::optional<SpawnOriginSelection>
        {
            if (origins.empty())
            {
                return std::nullopt;
            }

            const std::size_t laneSize = origins.size();
            if (liveCounts.size() != laneSize)
            {
                liveCounts.assign(laneSize, 0);
            }

            std::optional<std::size_t> chosenIndex;
            if (m_spawnPerPointCap <= 0)
            {
                cursor = cursor % laneSize;
                chosenIndex = cursor;
            }
            else
            {
                for (std::size_t offset = 0; offset < laneSize; ++offset)
                {
                    const std::size_t candidate = (cursor + offset) % laneSize;
                    if (liveCounts[candidate] < m_spawnPerPointCap)
                    {
                        chosenIndex = candidate;
                        break;
                    }
                }
            }

            if (!chosenIndex)
            {
                return std::nullopt;
            }

            cursor = (*chosenIndex + 1) % laneSize;
            const float3 origin = origins[*chosenIndex];

            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnOrigin] lane=" << label
                << " index=" << *chosenIndex << "/" << origins.size()
                << " position=(" << origin.x << ',' << origin.y << ',' << origin.z << ")\n";
            DebugTrace(oss.str());

            return std::optional<SpawnOriginSelection>{ SpawnOriginSelection{ origin, label, lane, *chosenIndex } };
        };

        if (hasPrimary && hasSecondary)
        {
            const bool preferPrimary = m_nextSpawnUsesPrimary;
            m_nextSpawnUsesPrimary = !m_nextSpawnUsesPrimary;
            auto selection = preferPrimary
                ? selectFromLane(m_spawnOriginsPrimary, m_spawnOriginLiveCountsPrimary, m_spawnOriginCursorPrimary,
                    "primary", SpawnOriginLane::Primary)
                : selectFromLane(m_spawnOriginsSecondary, m_spawnOriginLiveCountsSecondary, m_spawnOriginCursorSecondary,
                    "secondary", SpawnOriginLane::Secondary);
            if (!selection)
            {
                selection = preferPrimary
                    ? selectFromLane(m_spawnOriginsSecondary, m_spawnOriginLiveCountsSecondary, m_spawnOriginCursorSecondary,
                        "secondary", SpawnOriginLane::Secondary)
                    : selectFromLane(m_spawnOriginsPrimary, m_spawnOriginLiveCountsPrimary, m_spawnOriginCursorPrimary,
                        "primary", SpawnOriginLane::Primary);
            }
            return selection;
        }

        if (hasPrimary)
        {
            return selectFromLane(m_spawnOriginsPrimary, m_spawnOriginLiveCountsPrimary, m_spawnOriginCursorPrimary,
                "primary", SpawnOriginLane::Primary);
        }

        return selectFromLane(m_spawnOriginsSecondary, m_spawnOriginLiveCountsSecondary, m_spawnOriginCursorSecondary,
            "secondary", SpawnOriginLane::Secondary);
    }

    void SweetsSpawner::ResetSpawnOriginTracking()
    {
        m_spawnOriginLiveCountsPrimary.assign(m_spawnOriginsPrimary.size(), 0);
        m_spawnOriginLiveCountsSecondary.assign(m_spawnOriginsSecondary.size(), 0);
        m_spawnOriginAssignments.clear();
    }

    void SweetsSpawner::RegisterSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets, const SpawnOriginSelection& selection)
    {
        if (!sweets)
        {
            return;
        }

        if (selection.lane == SpawnOriginLane::Primary)
        {
            if (selection.index < m_spawnOriginLiveCountsPrimary.size())
            {
                ++m_spawnOriginLiveCountsPrimary[selection.index];
                m_spawnOriginAssignments[sweets.get()] = { selection.lane, selection.index };
            }
        }
        else if (selection.lane == SpawnOriginLane::Secondary)
        {
            if (selection.index < m_spawnOriginLiveCountsSecondary.size())
            {
                ++m_spawnOriginLiveCountsSecondary[selection.index];
                m_spawnOriginAssignments[sweets.get()] = { selection.lane, selection.index };
            }
        }
    }

    void SweetsSpawner::UnregisterSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        const auto it = m_spawnOriginAssignments.find(sweets.get());
        if (it == m_spawnOriginAssignments.end())
        {
            return;
        }

        const SpawnOriginAssignment assignment = it->second;
        if (assignment.lane == SpawnOriginLane::Primary)
        {
            if (assignment.index < m_spawnOriginLiveCountsPrimary.size())
            {
                m_spawnOriginLiveCountsPrimary[assignment.index] = std::max(0, m_spawnOriginLiveCountsPrimary[assignment.index] - 1);
            }
        }
        else if (assignment.lane == SpawnOriginLane::Secondary)
        {
            if (assignment.index < m_spawnOriginLiveCountsSecondary.size())
            {
                m_spawnOriginLiveCountsSecondary[assignment.index] = std::max(0, m_spawnOriginLiveCountsSecondary[assignment.index] - 1);
            }
        }

        m_spawnOriginAssignments.erase(it);
    }

    Transform SweetsSpawner::BuildSpawnTransform(const std::string& normalizedKind, const SpawnOriginSelection& originSelection)
    {
        Transform transform{};
        transform.position = originSelection.origin;
        static bool s_flipSpawnSign = false;
        s_flipSpawnSign = !s_flipSpawnSign;
        std::uniform_real_distribution<float> xJitterDist(1.0f, 1.5f);
        const float xJitter = (s_flipSpawnSign ? 1.0f : -1.0f) * xJitterDist(m_random);
        transform.position.x += xJitter;
        transform.position.y += m_spawnHeightOffset;
        transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        transform.scale = float3{ 1.0f, 1.0f, 1.0f };

        bool hasFactoryPreset = false;
        float3 rotationMinDegrees = m_randomRotationMinDegrees;
        float3 rotationMaxDegrees = m_randomRotationMaxDegrees;

        if (m_factory)
        {
            auto presetIt = m_factory->GetFactoryPresets().find(normalizedKind);
            if (presetIt != m_factory->GetFactoryPresets().end())
            {
                transform.scale = presetIt->second.scale;
                rotationMinDegrees = presetIt->second.rotationMinDegrees;
                rotationMaxDegrees = presetIt->second.rotationMaxDegrees;
                hasFactoryPreset = true;
            }
            else if (m_warnedMissingFactoryPresetKinds.insert(normalizedKind).second)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << normalizedKind
                    << "' の工場プリセットが見つからないから、スケールはデフォルト(1,1,1)にしとくね\n";
                DebugTrace(warn.str());
            }
        }

        const auto computeDeterministicRotation = [](float minDeg, float maxDeg)
        {
            const float lo = std::min(minDeg, maxDeg);
            const float hi = std::max(minDeg, maxDeg);
            return (lo + hi) * 0.5f;
        };

        const float sampledPitch = computeDeterministicRotation(rotationMinDegrees.x, rotationMaxDegrees.x);
        const float sampledYaw = computeDeterministicRotation(rotationMinDegrees.y, rotationMaxDegrees.y);
        const float sampledRoll = computeDeterministicRotation(rotationMinDegrees.z, rotationMaxDegrees.z);
        transform.rotation = float3{ sampledPitch, sampledYaw, sampledRoll };

        std::ostringstream oss;
        oss << kLogPrefix << "スポーン回転を決めたよ pitch/yaw/roll(deg)="
            << sampledPitch << ',' << sampledYaw << ',' << sampledRoll
            << " rotationSource=" << (hasFactoryPreset ? "preset" : "global")
            << " scale=(" << transform.scale.x << ',' << transform.scale.y << ',' << transform.scale.z << ')'
            << " origin=" << originSelection.origin.x << ',' << originSelection.origin.y << ',' << originSelection.origin.z
            << " lane=" << originSelection.laneLabel << "\n";
        DebugTrace(oss.str());

        return transform;
    }

    int SweetsSpawner::SpawnInitialPerPoint(int perPoint)
    {
        if (perPoint <= 0)
        {
            return 0;
        }

        const std::size_t primaryCount = m_spawnOriginsPrimary.size();
        const std::size_t secondaryCount = m_spawnOriginsSecondary.size();
        const int totalPoints = static_cast<int>(primaryCount + secondaryCount);
        if (totalPoints <= 0)
        {
            return 0;
        }

        int perPointCount = perPoint;
        if (m_spawnPerPointCap > 0)
        {
            perPointCount = std::min(perPointCount, m_spawnPerPointCap);
        }

        int allowedTotal = std::numeric_limits<int>::max();
        if (m_liveSweetsCap > 0)
        {
            allowedTotal = std::max(0, m_liveSweetsCap - CountLiveSweets());
        }
        if (allowedTotal <= 0 || perPointCount <= 0)
        {
            return 0;
        }

        const auto spawnAt = [this](const float3& origin, const char* label, SpawnOriginLane lane, std::size_t index) -> bool
        {
            const std::string chosenKind = ChooseKindFromActiveConfig();
            const std::string resolvedKind = m_factory ? m_factory->ResolveKindKey(chosenKind) : chosenKind;
            const std::string kind = NormalizeKey(resolvedKind);
            if (kind.empty())
            {
                return false;
            }

            const SpawnOriginSelection selection{ origin, label, lane, index };
            Transform transform = BuildSpawnTransform(kind, selection);
            const auto spawned = m_spawnCallback ? m_spawnCallback(kind, transform) : nullptr;
            if (spawned)
            {
                RegisterSpawnOrigin(spawned, selection);
                return true;
            }

            return false;
        };

        int spawnedCount = 0;
        for (int iteration = 0; iteration < perPointCount && allowedTotal > 0; ++iteration)
        {
            for (std::size_t index = 0; index < primaryCount && allowedTotal > 0; ++index)
            {
                if (m_spawnPerPointCap > 0 && index < m_spawnOriginLiveCountsPrimary.size())
                {
                    if (m_spawnOriginLiveCountsPrimary[index] >= m_spawnPerPointCap)
                    {
                        continue;
                    }
                }

                if (spawnAt(m_spawnOriginsPrimary[index], "primary", SpawnOriginLane::Primary, index))
                {
                    ++spawnedCount;
                    --allowedTotal;
                }
            }

            for (std::size_t index = 0; index < secondaryCount && allowedTotal > 0; ++index)
            {
                if (m_spawnPerPointCap > 0 && index < m_spawnOriginLiveCountsSecondary.size())
                {
                    if (m_spawnOriginLiveCountsSecondary[index] >= m_spawnPerPointCap)
                    {
                        continue;
                    }
                }

                if (spawnAt(m_spawnOriginsSecondary[index], "secondary", SpawnOriginLane::Secondary, index))
                {
                    ++spawnedCount;
                    --allowedTotal;
                }
            }
        }

        if (spawnedCount > 0)
        {
            m_lastLiveSweetsCount = CountLiveSweets();
            std::ostringstream oss;
            oss << kLogPrefix << "初期スポーンを実行したよ perPoint=" << perPointCount
                << " spawned=" << spawnedCount << " points=" << totalPoints
                << " live=" << m_lastLiveSweetsCount << "\n";
            DebugTrace(oss.str());
        }

        return spawnedCount;
    }

    void SweetsSpawner::EnsureRegisteredWeights(SpawnTuningConfig& config,
        const std::unordered_set<std::string>& registeredKindsNormalized,
        const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const
    {
        auto& manifest = SweetsPrototypeManifest::Instance();

        std::unordered_set<std::string> presentNormalized;
        for (const auto& weight : config.weights)
        {
            const auto* descriptor = manifest.Find(weight.kind);
            if (!descriptor)
            {
                continue;
            }

            presentNormalized.insert(NormalizeKey(descriptor->key));
        }

        std::vector<SweetsType> missingTypes;
        missingTypes.reserve(registeredKindsNormalized.size());
        for (const auto& normalizedKind : registeredKindsNormalized)
        {
            if (presentNormalized.find(normalizedKind) != presentNormalized.end())
            {
                continue;
            }

            const auto resolvedType = manifest.Resolve(normalizedKind);
            if (resolvedType)
            {
                missingTypes.push_back(*resolvedType);
            }
        }

        if (missingTypes.empty())
        {
            return;
        }

        std::sort(missingTypes.begin(), missingTypes.end(), [](SweetsType lhs, SweetsType rhs)
        {
            return static_cast<int>(lhs) < static_cast<int>(rhs);
        });

        std::ostringstream warn;
        warn << kLogPrefix << "[Spawn] 登録済みの種類に weight が足りないよ:";
        for (auto missing : missingTypes)
        {
            warn << ' ' << ToString(missing);
        }
        warn << " (registered=" << registeredKindsNormalized.size()
            << " configured=" << presentNormalized.size() << ")\n";
        DebugTrace(warn.str());

        if (!kAutoIncludeMissingWeightKinds)
        {
            return;
        }

        for (const auto missing : missingTypes)
        {
            float fallbackWeight = 1.0f;
            const auto defaultIt = defaultWeightLookup.find(missing);
            if (defaultIt != defaultWeightLookup.end() && defaultIt->second > 0.0f)
            {
                fallbackWeight = defaultIt->second;
            }

            config.weights.push_back({ missing, fallbackWeight });
            presentNormalized.insert(NormalizeKey(ToString(missing)));

            std::ostringstream added;
            added << kLogPrefix << "[Spawn] 不足してた種類 '" << ToString(missing)
                << "' をフォールバック weight=" << std::fixed << std::setprecision(2) << fallbackWeight
                << " で自動追加したよ (autoInclude=" << std::boolalpha << kAutoIncludeMissingWeightKinds << ")\n";
            DebugTrace(added.str());
        }
    }
}
