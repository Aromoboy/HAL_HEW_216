#pragma once
#include"Sweets.h"

namespace SweetsSpace
{
    class Cake : public BaseSweets
    {
    public:
        Cake(std::shared_ptr<ModelDX> model);
    };
}

