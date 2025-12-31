#pragma once

#include <utility>

namespace Core
{
    class UpdateScheduler;
    class IUpdatable;

    class RegistrationHandle
    {
    public:
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

        RegistrationHandle(UpdateScheduler* scheduler, IUpdatable* node);

        UpdateScheduler* m_scheduler = nullptr;
        IUpdatable* m_node = nullptr;
        bool m_active = false;
    };
}
