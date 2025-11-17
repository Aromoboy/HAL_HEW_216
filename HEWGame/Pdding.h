#pragma once
#include"Sweets.h"

namespace SweetsSpace
{
    class Pudding : public BaseSweets
    {
    public:
        Pudding(std::shared_ptr<ModelDX> model);
    };
}

