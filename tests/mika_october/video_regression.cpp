// ミカ：実装本体から抽出したデコーダーと既存MP4で表示時刻を検証する
#include "video_decoder_under_test.inc"

#include <cstdio>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    LONGLONG Advance(MfState& decoder, float dt, LONGLONG previous = -1)
    {
        auto sample = decoder.ReadDueSample(dt);
        if (!sample)
        {
            return previous;
        }
        LONGLONG timestamp = -1;
        Check(SUCCEEDED(sample->GetSampleTime(&timestamp)), "sample timestamp unavailable");
        Check(static_cast<double>(timestamp) <= decoder.clockTime * kMfTimeBase + 1.0,
            "future frame presented early");
        return timestamp;
    }

    LONGLONG CatchUp(MfState& decoder, LONGLONG displayed)
    {
        for (int frame = 0; frame < 100 && !decoder.finished; ++frame)
        {
            displayed = Advance(decoder, 0.0f, displayed);
            if (decoder.endOfStream || (decoder.nextSample && decoder.nextSampleTime > decoder.clockTime * kMfTimeBase))
            {
                return displayed;
            }
        }
        Check(decoder.finished, "catch-up did not converge");
        return displayed;
    }

    void CheckRenderRate(float updatesPerSecond, const char* path = "HEWGame/Assets/Video/PowerUp.mp4")
    {
        MfState decoder;
        Check(decoder.Open(path), "rate test source open failed");
        Check(decoder.fps > 0.0f, "input frame rate unavailable");
        LONGLONG displayed = Advance(decoder, 0.0f);
        Check(displayed == 0, "initial sample missing");
        const double frameDuration = kMfTimeBase / static_cast<double>(decoder.fps);
        for (int frame = 0; frame < static_cast<int>(updatesPerSecond * 2.0f); ++frame)
        {
            displayed = Advance(decoder, 1.0f / updatesPerSecond, displayed);
            const double lag = decoder.clockTime * kMfTimeBase - displayed;
            Check(lag <= frameDuration + 2.0, "playback slowed below wall-clock rate");
        }
        const double beforePause = decoder.clockTime;
        decoder.paused = true;
        Check(!decoder.ReadDueSample(2.0f), "paused decoder produced a frame");
        Check(decoder.clockTime == beforePause, "pause advanced playback clock");
        decoder.paused = false;
        Advance(decoder, 1.0f / updatesPerSecond, displayed);
        std::printf("PASS: %.2ffps video at %.0ffps update and pause\n", decoder.fps, updatesPerSecond);
    }

    void CheckHitchAndBoundedReads()
    {
        MfState decoder;
        Check(decoder.Open("HEWGame/Assets/Video/Fade.mp4"), "Fade open failed");
        LONGLONG displayed = Advance(decoder, 0.0f);
        // ミカ：古いキーフレームを再現し保持済み一枚と最大八枚の新規読込だけで前進することを確認する
        decoder.catchingUp = true;
        displayed = Advance(decoder, 1.0f, displayed);
        const double frameDuration = kMfTimeBase / static_cast<double>(decoder.fps);
        Check(displayed > 0 && displayed <= frameDuration * 9.0 + 2.0,
            "decode budget exceeded after hitch");
        int iterations = 0;
        while (!(decoder.nextSample && decoder.nextSampleTime > decoder.clockTime * kMfTimeBase))
        {
            const LONGLONG previous = displayed;
            displayed = Advance(decoder, 0.0f, displayed);
            Check(displayed >= previous, "catch-up restarted at an older keyframe");
            Check(displayed - previous <= frameDuration * 8.0 + 2.0, "decode budget exceeded");
            Check(++iterations < 10, "bounded catch-up stalled");
        }
        displayed = Advance(decoder, 1.3f, displayed);
        displayed = CatchUp(decoder, displayed);
        Check(decoder.clockTime * kMfTimeBase - displayed <= frameDuration + 2.0,
            "real seek after hitch remained behind");
        std::puts("PASS: bounded reads, monotonic keyframe catch-up, real seek");
    }

    void CheckLoopAndEnd()
    {
        MfState looping;
        Check(looping.Open("HEWGame/Assets/Video/Fade.mp4"), "loop source open failed");
        Check(looping.duration > 0, "duration unavailable");
        Advance(looping, 0.3f);
        const float jump = static_cast<float>(3.0 * looping.duration / kMfTimeBase + 0.2);
        const double expected = std::fmod(looping.clockTime + jump,
            static_cast<double>(looping.duration) / kMfTimeBase);
        auto displayed = Advance(looping, jump);
        displayed = CatchUp(looping, displayed);
        Check(std::abs(looping.clockTime - expected) < 0.000001, "loop discarded residual time");
        Check(!looping.finished && displayed >= 0, "loop stopped playback");

        MfState ending;
        Check(ending.Open("HEWGame/Assets/Video/Fade.mp4"), "EOS source open failed");
        ending.autoLoop = false;
        const double seconds = static_cast<double>(ending.duration) / kMfTimeBase;
        displayed = Advance(ending, static_cast<float>(seconds - 0.01));
        displayed = CatchUp(ending, displayed);
        Check(!ending.finished, "EOS finished before last frame duration");
        const LONGLONG finalFrame = displayed;
        displayed = Advance(ending, 0.02f, displayed);
        for (int frame = 0; frame < 100 && !ending.finished; ++frame)
        {
            displayed = Advance(ending, 0.0f, displayed);
        }
        Check(ending.finished && displayed >= finalFrame, "non-looping EOS did not finish");
        Check(!ending.ReadDueSample(1.0f), "finished playback restarted");
        std::puts("PASS: multiple loops preserve time, EOS waits for last frame");

        MfState jumpToEnd;
        Check(jumpToEnd.Open("HEWGame/Assets/Video/Fade.mp4"), "jump-to-end source open failed");
        jumpToEnd.autoLoop = false;
        displayed = Advance(jumpToEnd, 0.0f);
        displayed = Advance(jumpToEnd, static_cast<float>(seconds + 1.0), displayed);
        displayed = CatchUp(jumpToEnd, displayed);
        Check(jumpToEnd.finished && displayed > jumpToEnd.duration - 2 * kMfTimeBase / jumpToEnd.fps,
            "seek past EOS skipped the final frame");
        std::puts("PASS: jump beyond EOS presents final frame");

        MfState unknownDuration;
        Check(unknownDuration.Open("HEWGame/Assets/Video/Fade.mp4"), "unknown duration source open failed");
        unknownDuration.duration = 0;
        displayed = Advance(unknownDuration, 4.5f);
        displayed = CatchUp(unknownDuration, displayed);
        Check(unknownDuration.duration > 4 * kMfTimeBase, "EOS failed to derive missing duration");
        displayed = Advance(unknownDuration, 0.0f, displayed);
        displayed = CatchUp(unknownDuration, displayed);
        Check(unknownDuration.clockTime > 0.2 && unknownDuration.clockTime < 0.3,
            "unknown duration loop lost residual time");
        Check(!unknownDuration.finished && displayed >= 0, "unknown duration loop stopped");
        std::puts("PASS: EOS derives missing duration before loop seek");
    }
}

int main()
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult) || FAILED(MFStartup(MF_VERSION)))
    {
        std::puts("FAIL: Media Foundation initialization");
        return 1;
    }
    int result = 0;
    try
    {
        CheckRenderRate(30.0f);
        CheckRenderRate(60.0f);
        CheckRenderRate(15.0f, "HEWGame/Assets/Video/Fade.mp4");
        CheckHitchAndBoundedReads();
        CheckLoopAndEnd();
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL: %s\n", error.what());
        result = 1;
    }
    MFShutdown();
    CoUninitialize();
    return result;
}
