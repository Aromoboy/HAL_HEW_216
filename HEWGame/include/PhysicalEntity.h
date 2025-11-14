// PhysicalEntity.h

#ifndef __PHYSICALENTITY_H__
#define __PHYSICALENTITY_H__

#include "CollisionEntity.h"
#include "Physical.h"

namespace Entity
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
            Core::UpdatePhase phase = Core::UpdatePhase::Physics, int priority = 0);
        virtual ~PhysicalEntity();

        // PhysicalÇÃéQè∆Çï‘Ç∑
        Physics::Physical& GetPhysical();

    protected:
        Physics::Physical m_physical;
    };
}
#endif __PHYSICALENTITY_H__