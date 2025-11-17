#ifndef ___PLAYER_H___
#define ___PLAYER_H___



#include "CollisionEntity.h"
#include "Input.h"
#include "ICamera.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "PhysicalEntity.h"
#include <memory>

void Test();

namespace SceneSpace 
{

    
    class Player : public SceneSpace::PhysicalEntity
    {
    public:
        ModelDX* GetRenderableModel() override;
        const ModelDX* GetRenderableModel() const override;

        // OnUpdateは各派生クラスで実際の処理を書く場所だ
    // Contextを受け取り、必要なシステム情報にアクセスできる
        

        Player(std::string a_tag, float3 pos, float3 rot, float3 scl,
            std::shared_ptr<Collision::ICollider> collide,
            float3 velocity = { 0.0f, 0.0f, 0.0f }, float3 force = { 0.0f, 0.0f, 0.0f }, float weight = 1.0f, float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);


        virtual void OnUpdate(Core::UpdateContext& ctx) override;

        virtual void Draw(ModelRender& render, CameraSpace::ICamera& camera);

        void HandleInput(Core::Input input);

    private:
        std::shared_ptr<ModelDX> model;

    };
}



#endif // !___PLAYER_H___
