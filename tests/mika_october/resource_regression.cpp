#include "PlayerData.h"
#include "SoundSystem.h"
#include "TextureDX.h"
#include "Main.h"
#include "Debug.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#ifdef NDEBUG
#error ミカ：回帰テストはNDEBUGを指定せずビルドする
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "ole32.lib")

// ミカ：保存と読込の実装だけを検証し、ゲーム画面や実際の音声出力を起動しない
namespace DebugLog { void DebugMessage(const std::string&) {} }
HRESULT GetComInitResult() { return E_FAIL; }

const std::vector<PowerUpEffectConfig::Entry>& PowerUpEffectConfig::Entries()
{
    static const std::vector<Entry> entries = []
    {
        Entry entry;
        entry.type = PowerUpType::SpeedUp;
        entry.key = "SpeedUp";
        return std::vector<Entry>{entry};
    }();
    return entries;
}
std::optional<PowerUpEffectConfig::Entry> PowerUpEffectConfig::FindByKey(std::string_view key)
{
    return key == "SpeedUp" ? std::optional<Entry>{Entries().front()} : std::nullopt;
}
PowerUpEffectResult PowerUpEffectConfig::ComputeEffect(PowerUpType, int) { return {}; }

std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void Write(const std::filesystem::path& path, const std::string& data)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    output.close();
    assert(output);
}

void CheckSaves()
{
    const std::filesystem::path path = "Assets/Data/PlayerData.cfg";
    PlayerData save;
    PlayerData::Data entry;
    entry.nPoints = 1234;
    entry.levels[PowerUpType::SpeedUp] = 3;
    save.SetEntries({entry});
    assert(save.SaveData());
    const auto original = Read(path);
    assert(original.find("entry=1234,SpeedUp=3") != std::string::npos);
    PlayerData loaded;
    assert(loaded.LoadData());
    assert(loaded.GetEntries().front().nPoints == 1234);
    assert(loaded.GetEntries().front().levels.at(PowerUpType::SpeedUp) == 3);

    // ミカ：置換が拒否されても元の保存内容を失わず、一時ファイルも残さない
    HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(locked != INVALID_HANDLE_VALUE);
    entry.nPoints = 5678;
    save.SetEntries({entry});
    assert(!save.SaveData());
    assert(Read(path) == original);
    assert(CloseHandle(locked));
    for (const auto& item : std::filesystem::directory_iterator("Assets/Data"))
    {
        assert(item.path().filename() == path.filename());
    }
    assert(save.SaveData());
    assert(Read(path).find("entry=5678") != std::string::npos);

    // ミカ：不明な項目や空ファイルを読み込んだ後も初期値で上書きしない
    for (const std::string invalid : {std::string("[PlayerData]\nentry=800,SpeedUp=2,Unknown=1\n"), std::string{}})
    {
        Write(path, invalid);
        PlayerData damaged;
        assert(!damaged.LoadData());
        damaged.SetEntries({PlayerData::Data{}});
        assert(!damaged.SaveData());
        assert(Read(path) == invalid);
    }
    std::filesystem::remove(path);
    PlayerData firstRun;
    assert(!firstRun.LoadData());
    firstRun.SetEntries({entry});
    assert(firstRun.SaveData());
    std::cout << "PASS save roundtrip, failed replacement, damaged-file protection, first run\n";
}

void WriteWave(const std::string& path, uint32_t formatSize, uint32_t declaredData,
    uint32_t actualData, uint16_t channels = 1)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const auto u16 = [&](uint16_t value) { out.write(reinterpret_cast<const char*>(&value), 2); };
    const auto u32 = [&](uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); };
    out.write("RIFF", 4); u32(20 + formatSize + declaredData); out.write("WAVEfmt ", 8);
    u32(formatSize); u16(WAVE_FORMAT_PCM); u16(channels); u32(44100);
    u32(88200); u16(2); u16(16);
    for (uint32_t i = 16; i < formatSize; ++i) out.put('\0');
    out.write("data", 4); u32(declaredData);
    for (uint32_t i = 0; i < actualData; ++i) out.put('\0');
    out.close();
    assert(out);
}

void CheckWaveFiles()
{
    SoundSystem sound;
    WriteWave("valid.wav", 16, 8, 8);
    WaveData valid;
    assert(sound.LoadWaveFile("valid.wav", &valid));
    assert(valid.m_size == 8 && valid.m_wavFormat.nChannels == 1);
    assert(sound.PlayWabeSound("valid.wav", &valid, false) == -1);
    valid.Cleanup();

    // ミカ：読込後にファイルハンドルが残っていないことも検証する
    HANDLE exclusive = CreateFileA("valid.wav", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(exclusive != INVALID_HANDLE_VALUE);
    assert(CloseHandle(exclusive));
    WriteWave("oversized-fmt.wav", 40, 8, 8);
    WriteWave("truncated.wav", 16, 32, 2);
    WriteWave("zero-channels.wav", 16, 8, 8, 0);
    WriteWave("partial-frame.wav", 16, 3, 3);
    for (const char* filename : {"oversized-fmt.wav", "truncated.wav", "zero-channels.wav", "partial-frame.wav"})
    {
        WaveData invalid;
        const bool accepted = sound.LoadWaveFile(filename, &invalid);
        if (accepted) std::cerr << "Unexpected WAV acceptance: " << filename << " bytes=" << invalid.m_size << std::endl;
        assert(!accepted);
        assert(invalid.m_soundBuffer == nullptr && invalid.m_size == 0);
        assert(!sound.LoadWaveFile(filename, &invalid));
        invalid.Cleanup();
        assert(sound.LoadWaveFile("valid.wav", &invalid));
        invalid.Cleanup();
    }
    std::cout << "PASS WAV bounds, truncation/retry, format validation, handle release, unavailable audio\n";
}

void CheckTextures()
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, &level, &context)));
    Texture2D texture;
    const uint8_t rgba[] = {255, 0, 0, 255};
    assert(texture.LoadFromMemory(device.Get(), rgba, sizeof(rgba), 1, 1));
    auto* original = texture.Get();
    assert(original);
    assert(!texture.LoadFromMemory(device.Get(), rgba, 3, 1, 1));
    assert(!texture.LoadFromMemory(device.Get(), rgba, sizeof(rgba), UINT_MAX, UINT_MAX));
    assert(!texture.LoadFromMemory(device.Get(), rgba, sizeof(rgba), 1, 0));
    assert(!texture.LoadFromMemory(nullptr, rgba, sizeof(rgba), 1, 1));
    const uint8_t invalid[] = {'B', 'M', 0, 0};
    assert(!texture.LoadFromMemory(device.Get(), invalid, sizeof(invalid)));
    assert(texture.Get() == original);

    // ミカ：実際のWICデコードも通し、失敗した再読込が既存テクスチャを壊さないことを確認する
    std::vector<uint8_t> bmp(58, 0);
    const auto put16 = [&](size_t offset, uint16_t value) { std::memcpy(bmp.data() + offset, &value, 2); };
    const auto put32 = [&](size_t offset, uint32_t value) { std::memcpy(bmp.data() + offset, &value, 4); };
    bmp[0] = 'B'; bmp[1] = 'M'; put32(2, 58); put32(10, 54); put32(14, 40);
    put32(18, 1); put32(22, 1); put16(26, 1); put16(28, 24); put32(34, 4);
    bmp[56] = 255;
    assert(texture.LoadFromMemory(device.Get(), bmp.data(), bmp.size()));
    assert(texture.Get());
    assert(texture.LoadFromMemory(device.Get(), bmp.data(), bmp.size(), static_cast<unsigned int>(bmp.size()), 0));
    std::cout << "PASS texture raw bounds, invalid decode, valid WIC decode, preserved resource\n";
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto originalDirectory = std::filesystem::current_path();
    const auto fixture = std::filesystem::absolute(argv[1]);
    assert(fixture != originalDirectory && fixture.filename().string().starts_with("resource-fixture-"));
    std::filesystem::create_directories(fixture / "Assets/Data");
    std::filesystem::current_path(fixture);
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    CheckSaves();
    CheckWaveFiles();
    CheckTextures();
    CoUninitialize();
    std::filesystem::current_path(originalDirectory);
}
