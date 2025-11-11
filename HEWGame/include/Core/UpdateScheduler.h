#pragma once

#include <utility>
#include <vector>

#include "Core/IUpdatable.h"

namespace Core
{
        class UpdateScheduler
        {
        public:
                void Register(IUpdatable& node);
                void Unregister(IUpdatable& node);
                void AddDependency(IUpdatable& before, IUpdatable& after);
                void ClearDependencies();
                void Tick(const UpdateContext& ctx);

        private:
                using Dependency = std::pair<IUpdatable*, IUpdatable*>;

                static bool NodeLess(const IUpdatable* lhs, const IUpdatable* rhs);

                std::vector<IUpdatable*> m_nodes;
                std::vector<Dependency> m_dependencies;
        };
}

