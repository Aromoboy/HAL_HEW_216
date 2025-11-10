#pragma once
#include <memory>
#include <string>

#include "ModelDX.h"

// ローダーのインターフェース
class IModelLoader
{
public:
	IModelLoader() {};
	~IModelLoader() = default;

	virtual std::shared_ptr<ModelDX> Load(std::string path) = 0;
};

