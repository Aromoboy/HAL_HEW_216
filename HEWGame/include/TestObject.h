#pragma once

#include "Transform.h"
#include "Vector.h"
#include <memory>
#include <string>

namespace Render
{
    class Model;
}

class TestObject
{
public:
        TestObject(std::string a_szModelKey, float3 a_fPosition, float3 a_fScale, float3 a_fRads);

        ~TestObject() = default;

        const float3 GetPosition();
        const float3 GetScale();
        const float3 GetfRads();
        const Transform& GetTransform() const;
        std::shared_ptr<Render::Model> GetModel() const;


private:
        std::shared_ptr<Render::Model> m_model;
        Transform m_transform{};



};
