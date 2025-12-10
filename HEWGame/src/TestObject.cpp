#include "TestObject.h"

#include "Debug.h"
#include "ModelCache.h"

namespace
{
        constexpr const char* kAnchorModelBasePath = "Assets/Model/LowPolyNature/Models/";
}

TestObject::TestObject(std::string a_szModelKey, float3 a_fPosition, float3 a_fScale, float3 a_fRads)
{
        m_transform.position = a_fPosition;
        m_transform.scale = a_fScale;
        m_transform.rotation = a_fRads;

        std::string modelName = std::string(kAnchorModelBasePath) + a_szModelKey + ".fbx";

        Render::ModelCache& cache = Render::ModelCache::Instance();
        m_model = cache.Get(modelName);

        if (!m_model)
        {
#ifdef _DEBUG
                DebugMessage(a_szModelKey + "が読み込めませんでした");
#endif // _DEBUG
        }
}

const float3 TestObject::GetPosition()
{
        return m_transform.position;
}

const float3 TestObject::GetScale()
{
        return m_transform.scale;
}

const float3 TestObject::GetfRads()
{
        return m_transform.rotation;
}

const Transform& TestObject::GetTransform() const
{
        return m_transform;
}

std::shared_ptr<Render::Model> TestObject::GetModel() const
{
        return m_model;
}

