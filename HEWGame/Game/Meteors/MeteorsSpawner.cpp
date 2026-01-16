#include "Meteors/MeteorsSpawner.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>

#include "Config/ConfigSectionReader.h"
#include "Diagnostics/Logger.h"
#include "Meteors/BaseMeteors.h"
#include "Meteors/MeteorsFactory.h"
#include "Meteors/MeteorsRegistry.h"
#include "SceneGame/GameplayConfig.h"

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

    constexpr const char* kLogPrefix = "[MeteorsManager] ";
    constexpr float kDefaultSpawnIntervalSeconds = 2.5f;
    constexpr float kDefaultBurstIntervalSeconds = 0.5f;
    constexpr int kDefaultSimultaneousSpawnCount = 1;
    constexpr int kDefaultSimultaneousLiveTarget = kDefaultSimultaneousSpawnCount;
    constexpr int kDefaultLiveMeteorsCap = 24;
    constexpr float kMinimumBurstIntervalSeconds = 0.05f;
    constexpr float kMinimumSpawnIntervalSeconds = 0.1f;
    constexpr bool kAutoIncludeMissingWeightKinds = true;
    constexpr std::size_t kMaxSpawnQueueEntries = 16;
    constexpr float kDefaultSpawnHeightOffset = 8.0f;
    constexpr float kAutoSpawnIntervalFactor = 0.9f;
    constexpr float kAutoBurstIntervalFactor = 0.9f;
    constexpr int kAutoSimultaneousIncrement = 1;
    constexpr int kAutoSimultaneousCap = 6;
    constexpr float kMinimumAutoGrowthFactor = 0.01f;
    constexpr float kDefaultGrowthIntervalSeconds = 20.0f;
    constexpr int kDefaultGrowthStep = 1;
    constexpr float kDefaultAutoGrowthFactor = 0.9f;
    constexpr int kDefaultAutoSimultaneousIncrement = 1;
    constexpr int kDefaultAutoSimultaneousCap = 6;

    float SanitizeGrowthFactor(float value, float fallback = kDefaultAutoGrowthFactor)
    {
        if (!(value > 0.0f))
        {
            return fallback;
        }
        return std::max(kMinimumAutoGrowthFactor, value);
    }

    int SanitizeSimultaneousIncrement(int value, int fallback = kDefaultAutoSimultaneousIncrement)
    {
        return (value >= 0) ? value : fallback;
    }

    int SanitizeSimultaneousCap(int value, int fallback = kDefaultAutoSimultaneousCap)
    {
        return (value > 0) ? value : fallback;
    }

    float SanitizeGrowthIntervalSeconds(float value, float fallback = kDefaultGrowthIntervalSeconds)
    {
        if (!(value > 0.0f))
        {
            return fallback;
        }
        return value;
    }

    int SanitizeGrowthStep(int value, int fallback = kDefaultGrowthStep)
    {
        return (value >= 0) ? value : fallback;
    }

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

    std::vector<::MeteorsSpace::WeightedMeteorsType> ParseWeights(const std::string& raw, bool* outError)
    {
        std::vector<::MeteorsSpace::WeightedMeteorsType> weights;
        auto& manifest = MeteorsSpace::MeteorsPrototypeManifest::Instance();
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
}

namespace MeteorsSpace
{
    MeteorsSpawner::MeteorsSpawner(MeteorsFactory& factory, MeteorsRegistry& registry, SpawnCallback spawnCallback)
        : m_factory(&factory)
        , m_registry(&registry)
        , m_spawnCallback(std::move(spawnCallback))
        , m_spawnTimer(kDefaultSpawnIntervalSeconds)
        , m_spawnHeightOffset(kDefaultSpawnHeightOffset)
        , m_activeBurstInterval(kDefaultBurstIntervalSeconds)
        , m_simultaneousLiveTarget(kDefaultSimultaneousLiveTarget)
        , m_batchStackTarget(kDefaultSimultaneousSpawnCount)
        , m_autoSpawnIntervalFactor(kAutoSpawnIntervalFactor)
        , m_autoBurstIntervalFactor(kAutoBurstIntervalFactor)
        , m_autoSimultaneousIncrement(kAutoSimultaneousIncrement)
        , m_autoSimultaneousCap(kAutoSimultaneousCap)
        , m_growthIntervalSeconds(kDefaultGrowthIntervalSeconds)
        , m_growthStep(kDefaultGrowthStep)
        , m_random(std::random_device{}())
        , m_liveMeteorsCap(kDefaultLiveMeteorsCap)
    {
        ApplyDefaultPhaseConfigs();
    }

    void MeteorsSpawner::RunSpawnScheduler(float deltaSeconds)
    {
        const int liveMeteors = CountLiveMeteors();
        const bool liveCapActive = (m_liveMeteorsCap > 0);
        const bool spawnBlockedByCap = liveCapActive && liveMeteors >= m_liveMeteorsCap;

        m_spawnPausedDueToLiveMeteors = spawnBlockedByCap;
        m_lastLiveMeteorsCount = liveMeteors;

        if (m_phaseConfigs.empty())
        {
            return;
        }

        if (m_activePhaseConfigIndex < 0 || m_activePhaseConfigIndex >= static_cast<int>(m_phaseConfigs.size()))
        {
            OnPhaseConfigurationChanged(m_currentPhase);
            if (m_activePhaseConfigIndex < 0 || m_activePhaseConfigIndex >= static_cast<int>(m_phaseConfigs.size()))
            {
                return;
            }
        }

        if (!(deltaSeconds > 0.0f))
        {
            return;
        }
        const auto& config = m_phaseConfigs[m_activePhaseConfigIndex];

        if (m_growthIntervalSeconds > 0.0f && m_growthStep > 0)
        {
            m_growthElapsedSeconds += deltaSeconds;
            const int newSteps = static_cast<int>(m_growthElapsedSeconds / m_growthIntervalSeconds);
            if (newSteps > 0)
            {
                m_growthElapsedSeconds -= m_growthIntervalSeconds * static_cast<float>(newSteps);
                m_growthStepsApplied += newSteps;
            }
        }
        const float interval = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        m_activeBurstInterval = std::max(config.burstIntervalSeconds, kMinimumBurstIntervalSeconds);

        const int batchTarget = std::max(1, config.simultaneousSpawnCount);
        const int simultaneousTarget = std::max(1, config.simultaneousLiveTarget);
        const int growthBonus = std::max(0, m_growthStepsApplied * m_growthStep);

        m_simultaneousLiveTarget = std::max(1, simultaneousTarget + growthBonus);
        m_batchStackTarget = std::max(1, batchTarget + growthBonus);

        if (!m_batchCoolingDown && m_stackedInBatch >= m_batchStackTarget)
        {
            m_batchCoolingDown = true;
            m_spawnTimer = 0.0f;
            m_batchInitialSpawnDone = false;
            m_activeBurstPlanned = 0;
            m_currentBurstRemaining = 0;
            m_pendingTopUpAfterDrop = false;
            m_spawnQueue.clear();
        }

        if (m_batchCoolingDown)
        {
            m_spawnTimer += deltaSeconds;
            const float remainingCooldown = std::max(0.0f, interval - m_spawnTimer);
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
        const int projectedLive = liveMeteors + pendingRequests;
        const int remainingToStack = std::max(0, m_batchStackTarget - m_stackedInBatch);
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);

        if (desiredLiveTarget <= 0)
        {
            EmitSpawnRequests(deltaSeconds);
            m_lastLiveMeteorsCount = CountLiveMeteors();
            return;
        }

        const bool forceSeedNewBatch = (!m_batchInitialSpawnDone && m_stackedInBatch == 0);
        const bool liveAtTarget = liveMeteors >= desiredLiveTarget;
        if (liveAtTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
            }

            if (m_pendingTopUpAfterDrop)
            {
                m_pendingTopUpAfterDrop = false;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveMeteorsCount = CountLiveMeteors();
            return;
        }

        if (liveAtTarget && forceSeedNewBatch)
        {
            if (m_pendingTopUpAfterDrop)
            {
                m_pendingTopUpAfterDrop = false;
            }
        }

        if (projectedLive >= desiredLiveTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveMeteorsCount = CountLiveMeteors();
            m_pendingTopUpAfterDrop = false;
            return;
        }

        const bool delayBurst = m_pendingTopUpAfterDrop;
        const int deficit = desiredLiveTarget - projectedLive;
        const int requestedCount = forceSeedNewBatch ? desiredLiveTarget : (delayBurst ? 1 : deficit);
        m_activeBurstPlanned = std::max(desiredLiveTarget, requestedCount);

        EnqueueSpawns(requestedCount);
        m_burstTimer = delayBurst ? 0.0f : m_activeBurstInterval;
        m_pendingTopUpAfterDrop = false;
        m_batchInitialSpawnDone = true;

        EmitSpawnRequests(deltaSeconds);
        m_lastLiveMeteorsCount = CountLiveMeteors();
    }

    bool MeteorsSpawner::LoadPhaseSpawnConfig(const std::string& path)
    {
        m_phaseConfigs.clear();
        m_growthIntervalSeconds = kDefaultGrowthIntervalSeconds;
        m_growthStep = kDefaultGrowthStep;
        bool parseError = false;
        std::unordered_set<std::string> registeredKindsNormalized;
        if (m_factory)
        {
            for (const auto& entry : m_factory->GetSpawners())
            {
                registeredKindsNormalized.insert(NormalizeKey(entry.first));
            }
        }

        std::unordered_map<MeteorsType, float, MeteorsTypeHash> defaultWeightLookup;
        for (const auto& entry : BuildDefaultWeights())
        {
            defaultWeightLookup[entry.kind] = entry.weight;
        }

        std::ifstream file(path);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スポーン設定 '" << path << "' を開けなかったよ\n";
            DebugTrace(oss.str());
            return false;
        }

        std::vector<Config::SectionLine> lines;
        const bool hasSection = Config::ReadSectionLines(path, GameplayConfig::kSectionSpawnPhases, lines);

        int phaseIndex = 0;
        PhaseSpawnConfig config{};
        std::optional<int> currentPhaseLabel;
        bool hasActivePhase = false;
        bool hasWeights = false;
        bool hasSpawnInterval = false;
        bool hasBurstInterval = false;
        bool hasSimultaneous = false;
        bool hasLiveTarget = false;
        bool hasInitialPerLane = false;
        bool hasPhaseStartBonus = false;
        bool hasMeteorSeekSpeed = false;
        bool hasMeteorSeekSpread = false;

        const auto resetPhaseState = [&]()
        {
            config = PhaseSpawnConfig{};
            currentPhaseLabel.reset();
            hasActivePhase = false;
            hasWeights = false;
            hasSpawnInterval = false;
            hasBurstInterval = false;
            hasSimultaneous = false;
            hasLiveTarget = false;
            hasInitialPerLane = false;
            hasPhaseStartBonus = false;
            hasMeteorSeekSpeed = false;
            hasMeteorSeekSpread = false;
        };

        const auto finalizePhase = [&]()
        {
            if (!hasActivePhase)
            {
                return;
            }

            if (!hasSpawnInterval && !hasBurstInterval && !hasSimultaneous && !hasWeights && !hasLiveTarget
                && !hasInitialPerLane && !hasPhaseStartBonus && !hasMeteorSeekSpeed && !hasMeteorSeekSpread)
            {
                return;
            }

            const int phaseLabel = currentPhaseLabel.value_or(phaseIndex);
            if (!hasWeights)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "phase " << phaseLabel << " に weight がないのでデフォルトを入れるね\n";
                DebugTrace(warn.str());
                config.weights = BuildDefaultWeights();
            }
            else
            {
                config.weights = NormalizeWeights(config.weights);
            }

            if (!hasLiveTarget)
            {
                config.simultaneousLiveTarget = config.simultaneousSpawnCount;
            }

            EnsureRegisteredWeights(config, phaseLabel, registeredKindsNormalized, defaultWeightLookup);
            m_phaseConfigs.push_back(config);
            ++phaseIndex;
        };

        resetPhaseState();

        for (const auto& entry : lines)
        {
            if (entry.key.empty())
            {
                continue;
            }

            const std::string normalizedKey = NormalizeKey(entry.key);
            if (normalizedKey == "phase")
            {
                finalizePhase();
                resetPhaseState();
                hasActivePhase = true;
                try
                {
                    currentPhaseLabel = std::stoi(entry.value);
                }
                catch (const std::exception&)
                {
                    parseError = true;
                    currentPhaseLabel.reset();
                }
                continue;
            }

            if (!hasActivePhase)
            {
                hasActivePhase = true;
            }

            if (normalizedKey == "spawn_interval" || normalizedKey == "interval")
            {
                try
                {
                    config.spawnIntervalSeconds = std::stof(entry.value);
                    hasSpawnInterval = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "burst_interval" || normalizedKey == "burstinterval")
            {
                try
                {
                    config.burstIntervalSeconds = std::stof(entry.value);
                    hasBurstInterval = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "simultaneous")
            {
                try
                {
                    config.simultaneousSpawnCount = std::stoi(entry.value);
                    hasSimultaneous = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "live_target" || normalizedKey == "livetarget")
            {
                try
                {
                    config.simultaneousLiveTarget = std::stoi(entry.value);
                    hasLiveTarget = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "initial_per_lane" || normalizedKey == "initialperlane")
            {
                try
                {
                    config.initialSpawnPerLane = std::stoi(entry.value);
                    hasInitialPerLane = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "phase_start_bonus_per_lane" || normalizedKey == "phasestartbonusperlane")
            {
                try
                {
                    config.phaseStartBonusPerLane = std::stoi(entry.value);
                    hasPhaseStartBonus = true;
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "meteor_seek_speed" || normalizedKey == "meteorseekspeed")
            {
                float speed = 0.0f;
                if (TryParseFloat(entry.value, &speed))
                {
                    config.meteorSeekSpeed = std::max(0.0f, speed);
                    hasMeteorSeekSpeed = true;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "meteor_seek_spread_deg" || normalizedKey == "meteor_seek_spread_degrees"
                || normalizedKey == "meteorseekspreaddeg" || normalizedKey == "meteorseekspreaddegrees")
            {
                float spread = 0.0f;
                if (TryParseFloat(entry.value, &spread))
                {
                    config.meteorSeekSpreadDegrees = std::max(0.0f, spread);
                    hasMeteorSeekSpread = true;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "weights")
            {
                bool weightError = false;
                config.weights = ParseWeights(entry.value, &weightError);
                if (weightError)
                {
                    parseError = true;
                }
                else
                {
                    hasWeights = true;
                }
            }
            else if (normalizedKey == "auto_spawn_interval_factor")
            {
                float factor = 0.0f;
                if (TryParseFloat(entry.value, &factor))
                {
                    m_autoSpawnIntervalFactor = factor;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "auto_burst_interval_factor")
            {
                float factor = 0.0f;
                if (TryParseFloat(entry.value, &factor))
                {
                    m_autoBurstIntervalFactor = factor;
                }
                else
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "auto_simultaneous_increment")
            {
                try
                {
                    m_autoSimultaneousIncrement = std::stoi(entry.value);
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "auto_simultaneous_cap")
            {
                try
                {
                    m_autoSimultaneousCap = std::stoi(entry.value);
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "growth_interval_seconds" || normalizedKey == "growthintervalseconds")
            {
                float intervalSeconds = 0.0f;
                if (TryParseFloat(entry.value, &intervalSeconds))
                {
                    m_growthIntervalSeconds = SanitizeGrowthIntervalSeconds(intervalSeconds);
                }
                else
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "growth_step" || normalizedKey == "growthstep")
            {
                try
                {
                    m_growthStep = SanitizeGrowthStep(std::stoi(entry.value));
                }
                catch (const std::exception&)
                {
                    parseError = true;
                }
            }
            else if (normalizedKey == "spawn_origin_offset")
            {
                float3 offset{};
                if (ParseFloat3Value(entry.value, offset))
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
            else if (normalizedKey == "camera_spawn_offset" || normalizedKey == "spawn_camera_offset"
                || normalizedKey == "cameraspawnoffset" || normalizedKey == "spawncameraoffset")
            {
                float offset = 0.0f;
                if (TryParseFloat(entry.value, &offset))
                {
                    m_spawnCameraOffset = std::max(0.0f, offset);
                }
                else
                {
                    parseError = true;
                }
            }
        }

        finalizePhase();

        const float loadedAutoSpawnFactor = m_autoSpawnIntervalFactor;
        const float loadedAutoBurstFactor = m_autoBurstIntervalFactor;
        const int loadedAutoSimultaneousIncrement = m_autoSimultaneousIncrement;
        const int loadedAutoSimultaneousCap = m_autoSimultaneousCap;
        if (parseError)
        {
            std::ostringstream autoOss;
            autoOss << kLogPrefix << "スポーンの auto 設定に失敗したかもしれないよ"
                << " intervalFactor=" << loadedAutoSpawnFactor
                << " burstFactor=" << loadedAutoBurstFactor
                << " simultaneousIncrement=" << loadedAutoSimultaneousIncrement
                << " simultaneousCap=" << loadedAutoSimultaneousCap << "\n";
            DebugTrace(autoOss.str());
        }
        LogAutoGenerationPolicy("config-load");

        if (!hasSection)
        {
            std::ostringstream warn;
            warn << kLogPrefix << "'" << path << "' に section '" << GameplayConfig::kSectionSpawnPhases
                << "' が見つからないか空だったよ\n";
            DebugTrace(warn.str());
        }

        std::ostringstream oss;
        oss << kLogPrefix << "'" << path << "' section '" << GameplayConfig::kSectionSpawnPhases << "' から "
            << m_phaseConfigs.size() << " 個のスポーンフェーズを読んだよ (lines=" << lines.size()
            << ", parseError=" << std::boolalpha << parseError << ")\n";
        DebugTrace(oss.str());
        return !parseError;
    }

    void MeteorsSpawner::ForceSpawnRefresh()
    {
        m_spawnTimer = 0.0f;
        m_burstTimer = 0.0f;
        m_activeBurstPlanned = 0;
        m_activeBurstInterval = kDefaultBurstIntervalSeconds;
        m_stackedInBatch = 0;
        m_batchInitialSpawnDone = false;
        m_batchCoolingDown = false;
        m_batchStackTarget = std::max(1, m_batchStackTarget);
        m_simultaneousLiveTarget = std::max(1, m_simultaneousLiveTarget);
        m_growthElapsedSeconds = 0.0f;
        m_growthStepsApplied = 0;
        m_pendingTopUpAfterDrop = false;
        if (!m_spawnQueue.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "保留中のスポーンキューをクリアするね (" << m_spawnQueue.size() << ") \n";
            DebugTrace(oss.str());
        }
        m_spawnQueue.clear();
        m_currentBurstRemaining = 0;
    }

    void MeteorsSpawner::SetSpawnOrigins(
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
    }

    void MeteorsSpawner::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_randomRotationMinDegrees = minDegrees;
        m_randomRotationMaxDegrees = maxDegrees;
    }

    void MeteorsSpawner::SetLiveMeteorsCap(int cap)
    {
        const int sanitizedCap = std::max(0, cap);
        const bool changed = (sanitizedCap != m_liveMeteorsCap);
        m_liveMeteorsCap = sanitizedCap;
        const bool capActive = (m_liveMeteorsCap > 0);

        std::ostringstream oss;
        oss << kLogPrefix << "同時出現上限は " << (sanitizedCap > 0 ? "設定" : "無効")
            << " (cap=" << sanitizedCap << ", queued=" << m_spawnQueue.size()
            << ", liveCount=" << m_lastLiveMeteorsCount << ") にしたよ\n";
        DebugTrace(oss.str());

        if (changed && m_spawnPausedDueToLiveMeteors && (!capActive || m_lastLiveMeteorsCount < m_liveMeteorsCap))
        {
            m_spawnPausedDueToLiveMeteors = false;
        }
    }

    void MeteorsSpawner::SetCameraBounds(float minX, float maxX, float minY, float maxY)
    {
        if (!(std::isfinite(minX) && std::isfinite(maxX) && std::isfinite(minY) && std::isfinite(maxY)))
        {
            return;
        }

        m_hasCameraBounds = true;
        m_cameraMinX = std::min(minX, maxX);
        m_cameraMaxX = std::max(minX, maxX);
        m_cameraMinY = std::min(minY, maxY);
        m_cameraMaxY = std::max(minY, maxY);
    }

    void MeteorsSpawner::NotifyPhaseChanged(int newPhase)
    {
        m_currentPhase = std::max(0, newPhase);
        OnPhaseConfigurationChanged(m_currentPhase);
    }

    int MeteorsSpawner::CountLiveMeteors() const
    {
        if (!m_registry)
        {
            return 0;
        }

        const std::array<MeteorsState, 3> liveStates = {
            MeteorsState::Active,
            MeteorsState::Ground,
            MeteorsState::Carried,
        };

        int total = 0;
        for (const auto state : liveStates)
        {
            const auto it = m_registry->StateMap().find(state);
            if (it != m_registry->StateMap().end())
            {
                for (const auto& meteors : it->second)
                {
                    if (meteors && !meteors->IsPendingDestroy())
                    {
                        ++total;
                    }
                }
            }
        }

        return total;
    }

    int MeteorsSpawner::ComputeDesiredLiveTarget(int remainingToStack) const
    {
        if (remainingToStack <= 0)
        {
            return 0;
        }

        return std::max(1, std::min(m_simultaneousLiveTarget, remainingToStack));
    }

    int MeteorsSpawner::GetBatchStackTarget() const
    {
        return m_batchStackTarget;
    }

    int MeteorsSpawner::GetStackedInBatch() const
    {
        return m_stackedInBatch;
    }

    void MeteorsSpawner::OnStateChanged(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState previousState, MeteorsState newState)
    {
        (void)meteors;
        if (newState == MeteorsState::Stacked && previousState != MeteorsState::Stacked)
        {
            ++m_stackedInBatch;

            std::ostringstream oss;
            oss << kLogPrefix << "積み上げ進捗=" << m_stackedInBatch
                << '/' << m_batchStackTarget << " tag='" << (meteors ? meteors->GetTag() : "<null>") << "' だよ \n";
            DebugTrace(oss.str());
        }
    }

    void MeteorsSpawner::OnMeteorsRemoved(int remainingToStack, int liveAfterRemoval, const char* context)
    {
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);
        if (!m_batchCoolingDown && desiredLiveTarget > 0 && liveAfterRemoval < desiredLiveTarget)
        {
            m_pendingTopUpAfterDrop = true;

            std::ostringstream oss;
            oss << kLogPrefix << context << " で目標より生存スイーツが減ったから、あとで補充するね  live="
                << liveAfterRemoval << " target=" << desiredLiveTarget << " remainingToStack=" << remainingToStack
                << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
                << " queue=" << m_spawnQueue.size()
                << " burstInterval=" << std::fixed << std::setprecision(2) << m_activeBurstInterval << "\n";
            DebugTrace(oss.str());
        }
    }

    const std::vector<PhaseSpawnConfig>& MeteorsSpawner::GetPhaseConfigs() const
    {
        return m_phaseConfigs;
    }

    int MeteorsSpawner::GetActivePhaseConfigIndex() const
    {
        return m_activePhaseConfigIndex;
    }

    void MeteorsSpawner::EnqueueSpawns(int count)
    {
        if (count <= 0)
        {
            return;
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
    }

    void MeteorsSpawner::EmitSpawnRequests(float deltaSeconds)
    {
        if (m_spawnQueue.empty() || !m_spawnCallback)
        {
            return;
        }

        m_burstTimer += deltaSeconds;
        while (!m_spawnQueue.empty() && m_burstTimer >= m_activeBurstInterval)
        {
            m_burstTimer -= m_activeBurstInterval;
            const std::string requestedKind = m_spawnQueue.front();
            const std::string resolvedKind = m_factory ? m_factory->ResolveKindKey(requestedKind) : requestedKind;
            const std::string kind = NormalizeKey(resolvedKind);
            m_spawnQueue.pop_front();

            if (m_currentBurstRemaining > 0)
            {
                --m_currentBurstRemaining;
            }

            const SpawnOriginSelection originSelection = NextSpawnOrigin();
            Transform transform = BuildSpawnTransform(kind, originSelection);
            m_spawnCallback(kind, transform);
        }

        if (m_currentBurstRemaining < 0)
        {
            m_currentBurstRemaining = 0;
        }
    }

    std::string MeteorsSpawner::ChooseKindFromActiveConfig()
    {
        if (m_phaseConfigs.empty())
        {
            return {};
        }

        if (m_activePhaseConfigIndex < 0 || m_activePhaseConfigIndex >= static_cast<int>(m_phaseConfigs.size()))
        {
            return {};
        }

        const auto& config = m_phaseConfigs[m_activePhaseConfigIndex];
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
        std::optional<MeteorsType> chosenType;
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

        const auto* descriptor = MeteorsPrototypeManifest::Instance().Find(*chosenType);
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

    void MeteorsSpawner::OnPhaseConfigurationChanged(int newPhase)
    {
        const int sanitizedPhase = std::max(0, newPhase);
        const int maxExistingIndex = std::max(0, static_cast<int>(m_phaseConfigs.size()) - 1);
        EnsurePhaseConfigAvailable(std::min(sanitizedPhase, maxExistingIndex));
        if (m_phaseConfigs.empty())
        {
            return;
        }

        const int loopModulo = static_cast<int>(m_phaseConfigs.size());
        int target = sanitizedPhase;
        if (loopModulo > 0 && target >= loopModulo)
        {
            const int loopIndex = target % loopModulo;
            std::ostringstream loopLog;
            loopLog << kLogPrefix << "[Spawn] phase=" << sanitizedPhase << " -> reused config index=" << loopIndex
                << " (loopModulo=" << loopModulo << ")\n";
            DebugTrace(loopLog.str());
            target = loopIndex;
        }

        if (target >= static_cast<int>(m_phaseConfigs.size()))
        {
            return;
        }

        if (m_activePhaseConfigIndex == target)
        {
            return;
        }

        const auto& activeConfig = m_phaseConfigs[target];
        m_activePhaseConfigIndex = target;
        ForceSpawnRefresh();
        const float interval = std::max(activeConfig.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        m_spawnTimer = std::max(m_spawnTimer, interval);
        m_activeBurstInterval = std::max(activeConfig.burstIntervalSeconds, kMinimumBurstIntervalSeconds);
        m_batchStackTarget = std::max(1, activeConfig.simultaneousSpawnCount);
        m_simultaneousLiveTarget = std::max(1, activeConfig.simultaneousLiveTarget);
        m_batchInitialSpawnDone = false;
        m_batchCoolingDown = false;
        m_stackedInBatch = 0;
        const int activeLaneCount = (m_spawnOriginsPrimary.empty() ? 0 : 1) + (m_spawnOriginsSecondary.empty() ? 0 : 1);
        const int phaseStartBonusPerLane = std::max(0, activeConfig.phaseStartBonusPerLane);
        const int initialPerLane = std::max(0, activeConfig.initialSpawnPerLane);
        const int phaseStartBonusCount = (phaseStartBonusPerLane > 0 && activeLaneCount > 0)
            ? phaseStartBonusPerLane * activeLaneCount
            : 0;
        const int initialSpawnCount = (initialPerLane > 0 && activeLaneCount > 0)
            ? initialPerLane * activeLaneCount
            : 0;
        const int phaseStartTotal = phaseStartBonusCount + initialSpawnCount;
        if (phaseStartTotal > 0)
        {
            m_activeBurstPlanned = std::max(m_activeBurstPlanned, phaseStartTotal);
            if (phaseStartBonusCount > 0)
            {
                EnqueueSpawns(phaseStartBonusCount);
            }
            if (initialSpawnCount > 0)
            {
                EnqueueSpawns(initialSpawnCount);
            }
            m_burstTimer = m_activeBurstInterval;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "フェーズ変更で次のバッチを仕込んだよ interval="
            << std::fixed << std::setprecision(2) << interval << " burstInterval=" << m_activeBurstInterval
            << " plannedSimultaneous=" << activeConfig.simultaneousSpawnCount
            << " liveTarget=" << m_simultaneousLiveTarget << ".\n";
        DebugTrace(oss.str());
        LogActiveSpawnParameters(activeConfig, target);
    }

    void MeteorsSpawner::EnsurePhaseConfigAvailable(int phaseIndex)
    {
        if (phaseIndex < 0)
        {
            phaseIndex = 0;
        }

        if (m_phaseConfigs.empty())
        {
            ApplyDefaultPhaseConfigs();
        }

        if (m_phaseConfigs.empty())
        {
            return;
        }

        while (static_cast<int>(m_phaseConfigs.size()) <= phaseIndex)
        {
            const int previousIndex = static_cast<int>(m_phaseConfigs.size()) - 1;
            PhaseSpawnConfig generated = GenerateAutoPhaseConfig(m_phaseConfigs.back(), previousIndex);
            const int generatedIndex = static_cast<int>(m_phaseConfigs.size());
            m_phaseConfigs.push_back(generated);
            std::ostringstream oss;
            oss << kLogPrefix << "[Spawn] phase=" << generatedIndex << " (auto-generated from " << previousIndex << ") interval="
                << std::fixed << std::setprecision(2) << generated.spawnIntervalSeconds
                << " burstInterval=" << generated.burstIntervalSeconds
                << " simultaneous=" << generated.simultaneousSpawnCount
                << " liveTarget=" << generated.simultaneousLiveTarget << "\n";
            DebugTrace(oss.str());
            LogActiveSpawnParameters(generated, generatedIndex);
        }
    }

    void MeteorsSpawner::LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Spawn] phase=" << phaseIndex;
        if (config.autoGenerated)
        {
            oss << " (auto-generated";
            if (config.generatedFromPhase >= 0)
            {
                oss << " from " << config.generatedFromPhase;
            }
            oss << ")";
        }
        oss
            << " interval=" << std::fixed << std::setprecision(2) << config.spawnIntervalSeconds
            << " burstInterval=" << std::fixed << std::setprecision(2) << config.burstIntervalSeconds
            << " simultaneous=" << config.simultaneousSpawnCount
            << " liveTarget=" << config.simultaneousLiveTarget
            << " initialPerLane=" << config.initialSpawnPerLane
            << " phaseStartBonusPerLane=" << config.phaseStartBonusPerLane
            << " seekSpeed=" << std::fixed << std::setprecision(2) << config.meteorSeekSpeed
            << " seekSpreadDeg=" << std::fixed << std::setprecision(2) << config.meteorSeekSpreadDegrees
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

    PhaseSpawnConfig MeteorsSpawner::GenerateAutoPhaseConfig(const PhaseSpawnConfig& previous, int previousIndex) const
    {
        PhaseSpawnConfig generated = previous;
        generated.autoGenerated = true;
        generated.generatedFromPhase = previousIndex;
        const float sanitizedIntervalFactor = SanitizeGrowthFactor(m_autoSpawnIntervalFactor);
        const float sanitizedBurstFactor = SanitizeGrowthFactor(m_autoBurstIntervalFactor);
        generated.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds,
            previous.spawnIntervalSeconds * sanitizedIntervalFactor);
        generated.burstIntervalSeconds = std::max(kMinimumBurstIntervalSeconds,
            previous.burstIntervalSeconds * sanitizedBurstFactor);

        const int sanitizedIncrement = SanitizeSimultaneousIncrement(m_autoSimultaneousIncrement);
        const int sanitizedCap = SanitizeSimultaneousCap(m_autoSimultaneousCap);
        const int nextSimultaneous = std::min(sanitizedCap,
            previous.simultaneousSpawnCount + sanitizedIncrement);
        const int nextLiveTarget = std::min(sanitizedCap,
            previous.simultaneousLiveTarget + sanitizedIncrement);
        generated.simultaneousSpawnCount = std::max(1, nextSimultaneous);
        generated.simultaneousLiveTarget = std::max(1, nextLiveTarget);
        generated.weights = NormalizeWeights(previous.weights);
        if (generated.weights.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "[Spawn] auto-generationに使える weight が無かったからデフォルトで補うね\n");
            generated.weights = NormalizeWeights(BuildDefaultWeights());
        }
        return generated;
    }

    std::vector<WeightedMeteorsType> MeteorsSpawner::NormalizeWeights(const std::vector<WeightedMeteorsType>& weights) const
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

        std::vector<WeightedMeteorsType> normalized;
        normalized.reserve(weights.size());
        for (const auto& entry : weights)
        {
            normalized.push_back({ entry.kind, entry.weight / total });
        }
        return normalized;
    }

    void MeteorsSpawner::LogAutoGenerationPolicy(const char* context) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Spawn] auto-generation policy (" << context << ") intervalFactor=" << m_autoSpawnIntervalFactor
            << " burstFactor=" << m_autoBurstIntervalFactor
            << " simultaneousIncrement=" << m_autoSimultaneousIncrement
            << " simultaneousCap=" << m_autoSimultaneousCap << "\n";
        DebugTrace(oss.str());
    }

    void MeteorsSpawner::ApplyDefaultPhaseConfigs()
    {
        m_phaseConfigs.clear();
        std::vector<WeightedMeteorsType> baseWeights = BuildDefaultWeights();
        if (baseWeights.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "デフォルトのスポーン設定を作れないよ スイーツが何も登録されてない \n");
            return;
        }

        PhaseSpawnConfig phase0;
        phase0.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
        phase0.burstIntervalSeconds = kDefaultBurstIntervalSeconds;
        phase0.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
        phase0.simultaneousLiveTarget = kDefaultSimultaneousLiveTarget;
        phase0.weights = baseWeights;

        PhaseSpawnConfig phase1 = phase0;
        phase1.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.75f);

        PhaseSpawnConfig phase2 = phase0;
        phase2.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.5f);
        phase2.simultaneousSpawnCount = 2;
        phase2.simultaneousLiveTarget = 2;

        m_phaseConfigs.push_back(phase0);
        m_phaseConfigs.push_back(phase1);
        m_phaseConfigs.push_back(phase2);

        m_autoSpawnIntervalFactor = SanitizeGrowthFactor(kDefaultAutoGrowthFactor);
        m_autoBurstIntervalFactor = SanitizeGrowthFactor(kDefaultAutoGrowthFactor);
        m_autoSimultaneousIncrement = SanitizeSimultaneousIncrement(kDefaultAutoSimultaneousIncrement);
        m_autoSimultaneousCap = SanitizeSimultaneousCap(kDefaultAutoSimultaneousCap);
        m_activePhaseConfigIndex = -1;
        ForceSpawnRefresh();

        std::ostringstream oss;
        oss << kLogPrefix << "デフォルトのスポーン設定を適用したよ (" << m_phaseConfigs.size() << " phases) \n";
        DebugTrace(oss.str());
        LogAutoGenerationPolicy("defaults");
    }

    std::vector<WeightedMeteorsType> MeteorsSpawner::BuildDefaultWeights() const
    {
        std::vector<WeightedMeteorsType> weights;
        if (!m_factory)
        {
            return weights;
        }
        weights.reserve(m_factory->GetSpawners().size());
        auto& manifest = MeteorsPrototypeManifest::Instance();
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

    MeteorsSpawner::SpawnOriginSelection MeteorsSpawner::NextSpawnOrigin()
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
            return SpawnOriginSelection{ float3{ 0.0f, 0.0f, 0.0f }, "fallback" };
        }

        auto selectFromLane = [](std::vector<float3>& origins, std::size_t& cursor, const char* label)
        {
            if (origins.empty())
            {
                return SpawnOriginSelection{ float3{ 0.0f, 0.0f, 0.0f }, label };
            }

            cursor = cursor % origins.size();
            const std::size_t chosenIndex = cursor;
            cursor = (cursor + 1) % origins.size();
            const float3 origin = origins[chosenIndex];

            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnOrigin] lane=" << label
                << " index=" << chosenIndex << "/" << origins.size()
                << " position=(" << origin.x << ',' << origin.y << ',' << origin.z << ")\n";
            DebugTrace(oss.str());

            return SpawnOriginSelection{ origin, label };
        };

        if (hasPrimary && hasSecondary)
        {
            SpawnOriginSelection selection = m_nextSpawnUsesPrimary
                ? selectFromLane(m_spawnOriginsPrimary, m_spawnOriginCursorPrimary, "primary")
                : selectFromLane(m_spawnOriginsSecondary, m_spawnOriginCursorSecondary, "secondary");
            m_nextSpawnUsesPrimary = !m_nextSpawnUsesPrimary;
            return selection;
        }

        if (hasPrimary)
        {
            return selectFromLane(m_spawnOriginsPrimary, m_spawnOriginCursorPrimary, "primary");
        }

        return selectFromLane(m_spawnOriginsSecondary, m_spawnOriginCursorSecondary, "secondary");
    }

    Transform MeteorsSpawner::BuildSpawnTransform(const std::string& normalizedKind, const SpawnOriginSelection& originSelection)
    {
        Transform transform{};
        transform.position = originSelection.origin;
        static bool s_flipSpawnSign = false;
        s_flipSpawnSign = !s_flipSpawnSign;
        std::uniform_real_distribution<float> xJitterDist(1.0f, 1.5f);
        const float xJitter = (s_flipSpawnSign ? 1.0f : -1.0f) * xJitterDist(m_random);
        transform.position.x += xJitter;
        transform.position.y += m_spawnHeightOffset;
        if (m_hasCameraBounds && m_spawnCameraOffset > 0.0f)
        {
            const float midpointX = (m_cameraMinX + m_cameraMaxX) * 0.5f;
            const bool spawnLeft = transform.position.x <= midpointX;
            transform.position.x = spawnLeft
                ? (m_cameraMinX - m_spawnCameraOffset)
                : (m_cameraMaxX + m_spawnCameraOffset);
        }
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

    void MeteorsSpawner::EnsureRegisteredWeights(PhaseSpawnConfig& config, int phaseIndex,
        const std::unordered_set<std::string>& registeredKindsNormalized,
        const std::unordered_map<MeteorsType, float, MeteorsTypeHash>& defaultWeightLookup) const
    {
        auto& manifest = MeteorsPrototypeManifest::Instance();

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

        std::vector<MeteorsType> missingTypes;
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

        std::sort(missingTypes.begin(), missingTypes.end(), [](MeteorsType lhs, MeteorsType rhs)
        {
            return static_cast<int>(lhs) < static_cast<int>(rhs);
        });

        std::ostringstream warn;
        warn << kLogPrefix << "[Spawn] フェーズ " << phaseIndex
            << " で登録済みの種類に weight が足りないよ:";
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
            added << kLogPrefix << "[Spawn] フェーズ " << phaseIndex
                << " に不足してた種類 '" << ToString(missing)
                << "' をフォールバック weight=" << std::fixed << std::setprecision(2) << fallbackWeight
                << " で自動追加したよ (autoInclude=" << std::boolalpha << kAutoIncludeMissingWeightKinds << ")\n";
            DebugTrace(added.str());
        }
    }
}
