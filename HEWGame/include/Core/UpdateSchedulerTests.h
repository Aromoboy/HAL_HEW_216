#pragma once

#if defined(ENABLE_UPDATE_SCHEDULER_TESTS)

namespace Core::Tests
{
        // Executes UpdateScheduler-focused test cases. Intended for test harness builds only.
        void RunUpdateSchedulerTests();
}

#endif // defined(ENABLE_UPDATE_SCHEDULER_TESTS)
