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
        float stageFloorY = 0.0f;
    };

    enum class PostGameAction
    {
        None,
        ConfirmRestart,
        RetryIgnored,
        RetryGame,     // ���������
        GoToPowerUp,   // ������ʂ�
        GoToTitle,     // �^�C�g����
    };

    enum class ResultScreenOption
    {
        Retry,       // ���������
        PowerUp,     // ������ʂɍs��
        ReturnTitle  // ���j���[�ɖ߂�
    };
    class ResultScreenState
    {
    public:
        ResultScreenOption currentOption = ResultScreenOption::Retry;
       void MoveUp()  { currentOption = static_cast<ResultScreenOption>((static_cast<int>(currentOption) + 2) % 3);}
       void MoveDown(){ currentOption = static_cast<ResultScreenOption>((static_cast<int>(currentOption) + 1) % 3);}
    };

    PlayerController(LogFn log, const char* logPrefix);

    SpeedResult HandleSpeedInput(const SpeedInput& input) const;
    bool HandleManualRetryInput(Core::Input& input, float elapsedSeconds, float peakHeight) const;
    PostGameAction HandlePostGameInput(Core::Input& input, float peakHeight, int score) const;
    PostGameAction HandlePostGameMenuInput(Core::Input& input,float peakHeight,int score,ResultScreenState& state) const;

    void UpdatePlayerBounds(const BoundsInput& input);

private:
    LogFn m_log;
    const char* m_logPrefix = "";
    float m_cachedPlayerMinX = 0.0f;
    float m_cachedPlayerMaxX = 0.0f;
    bool m_cachedPlayerBoundsValid = false;
    float m_playerBoundsLogCooldownSeconds = 0.0f;
};
