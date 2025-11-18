// ============================================================================
// 概要: Scene に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef __SCENE_H__
#define __SCENE_H__

#include "CameraManager.h"
#include "Core/UpdateScheduler.h"

#include <memory>

namespace Core
{
        class StepTimer;
        class Input;
        struct UpdateContext;
        class IUpdatable;
}

namespace SceneSpace
{
class Scene
{
public:
void RootUpdate(Core::StepTimer& timer, Core::Input& input);
void RootDraw();

Scene();
virtual ~Scene();
virtual void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) = 0;

protected:
virtual void ConfigureUpdates();
virtual void Update(Core::UpdateContext& ctx) = 0;

void Register(Core::IUpdatable& node);
void Unregister(Core::IUpdatable& node);

Core::UpdateScheduler& GetUpdateScheduler();
Core::Input* GetInput();
const Core::Input* GetInput() const;
CameraSpace::CameraManager& GetCameraManager();
const CameraSpace::CameraManager& GetCameraManager() const;
std::shared_ptr<CameraSpace::ICamera> AddCamera(std::shared_ptr<CameraSpace::ICamera> camera);
void SetActiveCamera(const std::shared_ptr<CameraSpace::ICamera>& camera);
std::shared_ptr<CameraSpace::ICamera> GetActiveCamera() const;

private:
Core::UpdateScheduler m_scheduler;
bool m_isConfigured;
Core::Input* m_input;
CameraSpace::CameraManager m_cameraManager;
bool m_warnedAboutMissingCameraForDraw;
};
}

#endif // 備考: __SCENE_H__ に関する説明を日本語で補足している。
