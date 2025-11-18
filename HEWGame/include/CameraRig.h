// ============================================================================
// Tv: CameraRig ɊւC^[tF[Xf[^\錾Ainclude ėpł悤ɂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: include z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

#pragma once

#include <memory>

#include "BaseCamera.h"
#include "Core/UpdateNode.h"
#include "Entity.h"
#include "Vector.h"

namespace SceneSpace
{
        class Entity;
}

class CameraRig : public Core::UpdateNode
// Entity をフル定義に合わせたので、カメラのターゲットやアンカーを
// 直接参照できるようになった。Transform を通して位置取得ができる

{
public:
        explicit CameraRig(std::shared_ptr<CameraSpace::BaseCamera> camera = nullptr);

        void SetCamera(std::shared_ptr<CameraSpace::BaseCamera> camera);
        std::shared_ptr<CameraSpace::BaseCamera> GetCamera() const;

        void SetOffset(const float3& offset);
        const float3& GetOffset() const;

        void SetYaw(float yaw);
        float GetYaw() const;

        void SetPitch(float pitch);
        float GetPitch() const;

        void SetFollowSpeed(float followSpeed);
        float GetFollowSpeed() const;
        void SetTranslationLocked(bool locked);
        bool IsTranslationLocked() const;

        void SetYawPitchLocked(bool locked);
        bool IsYawPitchLocked() const;

        void Attach(const std::shared_ptr<SceneSpace::Entity>& target);

        void OnUpdate(Core::UpdateContext& ctx) override;

private:
        std::shared_ptr<CameraSpace::BaseCamera> m_camera;
        std::weak_ptr<SceneSpace::Entity> m_target;
        float3 m_offset;
        float m_yaw;
        float m_pitch;
        float m_followSpeed;
        bool m_warnedAboutExpiredTarget;
        bool m_lockTranslation;
        bool m_lockYawPitch;
};
