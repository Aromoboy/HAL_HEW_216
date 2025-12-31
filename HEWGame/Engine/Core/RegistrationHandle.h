// 更新登録の結果を保持して解除を行う
// 更新ノードとスケジューラがこれを使う

#pragma once

#include <cstdint>
#include <utility>

namespace Core
{
    class UpdateScheduler;

    class RegistrationHandle
    {
    public:
        using NodeId = std::uint64_t;

        RegistrationHandle() = default;
        ~RegistrationHandle();

        RegistrationHandle(const RegistrationHandle&) = delete;
        RegistrationHandle& operator=(const RegistrationHandle&) = delete;

        RegistrationHandle(RegistrationHandle&& other) noexcept;
        RegistrationHandle& operator=(RegistrationHandle&& other) noexcept;

        bool IsValid() const;

        void Reset();
        void Release();

    private:
        friend class UpdateScheduler;

        RegistrationHandle(UpdateScheduler* scheduler, NodeId nodeId);

        UpdateScheduler* m_scheduler = nullptr;
        NodeId m_nodeId = 0;
    };
}
