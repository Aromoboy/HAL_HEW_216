#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "Core/UpdateScheduler.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace DebugLog
{
    void DebugMessage(const std::string&) {}
}

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    class CountingInput final : public Core::Input
    {
    public:
        void OnUpdate(const Core::UpdateContext&) override
        {
            ++updates;
            if (shutdownDuringUpdate)
            {
                Shutdown();
            }
        }

        int updates = 0;
        bool shutdownDuringUpdate = false;
    };

    void TestResumePreservesFrameIdentity()
    {
        Core::StepTimer timer;
        timer.Tick(30.0f);
        const auto beforeResume = timer.FrameIndex();
        timer.SetFixedStep(0.02f, true);

        timer.ResetElapsedTime();
        Require(timer.FrameIndex() == beforeResume, "resume must preserve the frame index");
        Require(timer.DeltaTime() == 0.0f, "resume must clear the stale delta");
        Require(timer.Tick() == 0.0f, "the first resumed frame must exclude suspended time");
        Require(timer.FrameIndex() == beforeResume + 1, "the resumed frame must have a new identity");
        Require(timer.Tick() == 0.02f, "resume must preserve the fixed-step setting");

        timer.SetFixedStep(0.0f, false);
        timer.Tick(30.0f);
        timer.ResetElapsedTime();
        Require(timer.Tick() == 0.0f, "wall-clock mode must also discard the suspended interval");
    }

    void TestInputShutdownUnregistersBeforeSchedulerDestruction()
    {
        Core::StepTimer timer;
        CountingInput input;
        {
            Core::UpdateScheduler scheduler;
            input.SetRegistrationHandle(scheduler.Register(input));
            scheduler.Tick(Core::UpdateContext(timer.Tick(0.01f), timer));
            Require(input.updates == 1, "input must be registered before shutdown");

            input.Shutdown();
            input.Shutdown();
            scheduler.Tick(Core::UpdateContext(timer.Tick(0.01f), timer));
            Require(input.updates == 1, "shutdown must unregister input and be idempotent");
        }
        input.Shutdown();
    }

    void TestInputShutdownDuringUpdate()
    {
        Core::StepTimer timer;
        CountingInput input;
        {
            Core::UpdateScheduler scheduler;
            input.shutdownDuringUpdate = true;
            input.SetRegistrationHandle(scheduler.Register(input));
            scheduler.Tick(Core::UpdateContext(timer.Tick(0.01f), timer));
            scheduler.Tick(Core::UpdateContext(timer.Tick(0.01f), timer));
            Require(input.updates == 1, "shutdown during a tick must apply deferred unregistration");
        }
        input.Shutdown();
    }
}

int main()
{
    TestResumePreservesFrameIdentity();
    TestInputShutdownUnregistersBeforeSchedulerDestruction();
    TestInputShutdownDuringUpdate();
    std::cout << "PASS: timer resume and input scheduler lifetime\n";
    return EXIT_SUCCESS;
}
