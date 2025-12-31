// プレイヤー操作の入力を解釈する
// プレイヤー更新がこの結果を参照する

#pragma once

#include <functional>
#include <string>

namespace CameraSpace
{
    class PerspectiveCamera;
}

namespace Core
{
    class Input;
}

namespace Entities
{
    class Player;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class PlayerController
{
public:
    using LogFn = std::function<void(const std::string&)>;

    struct SpeedInput
    {
        Core::Input& input;
        Entities::Player* player = nullptr;
        float baseHorizontalSpeed = 0.0f;
        float baseVerticalSpeed = 0.0f;
        float speedMultiplier = 1.0f;
    };

    struct SpeedResult
    {
        float speedMultiplier = 1.0f;
    };

    struct BoundsInput
    {
        Entities::Player* player = nullptr;
        CameraSpace::PerspectiveCamera* camera = nullptr;
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        float deltaSeconds = 0.0f;
    };

    enum class PostGameAction
    {
        None,
        ConfirmRestart,
        RetryIgnored,
    };

    PlayerController(LogFn log, const char* logPrefix);

    SpeedResult HandleSpeedInput(const SpeedInput& input) const;
    bool HandleManualRetryInput(Core::Input& input, float elapsedSeconds, float peakHeight) const;
    PostGameAction HandlePostGameInput(Core::Input& input, float peakHeight, int score) const;
    void UpdatePlayerBounds(const BoundsInput& input);

private:
    LogFn m_log;
    const char* m_logPrefix = "";
    float m_cachedPlayerMinX = 0.0f;
    float m_cachedPlayerMaxX = 0.0f;
    bool m_cachedPlayerBoundsValid = false;
    float m_playerBoundsLogCooldownSeconds = 0.0f;
};
