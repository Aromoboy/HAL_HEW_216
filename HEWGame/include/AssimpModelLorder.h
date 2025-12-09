#ifndef ___ASSIMP_MODE_LORDER___
#define ___ASSIMP_MODE_LORDER___

#include <memory>
#include <string>

#include "AABB.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "TextureDX.h"

#include "TextureCache.h"

#include "IModelLoader.h"

#include "DirectXTex.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#if _MSC_VER >= 1930
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#elif _MSC_VER >= 1920
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc142-mtd.lib")
#else
#pragma comment(lib, "assimp-vc142-mt.lib")
#endif
#elif _MSC_VER >= 1910
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc141-mtd.lib")
#else
#pragma comment(lib, "assimp-vc141-mt.lib")
#endif
#endif



class AssimpModelLorder : public IModelLoader
{
public:
AssimpModelLorder();

~AssimpModelLorder() override = default;

    virtual std::shared_ptr<Render::Model> Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache) override;

private:
	
	using Vertices = std::vector <MeshDX::Vertex>;
	using Indices = std::vector<unsigned long>;
        // bV쐬
        std::vector<std::shared_ptr<MeshDX>> MakeMesh(const aiScene*, AABB& outBounds, bool& hasBounds);
        // }eA̍쐬
        std::vector<std::shared_ptr<MaterialDX>> MakeMaterial(const aiScene*, const std::string& directory, const std::string& pathWithoutExtension, ID3D11Device* device, TextureCache& textureCache);



};


#endif
