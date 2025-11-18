// ============================================================================
// 概要: AssimpModelLorder に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef ___ASSIMP_MODE_LORDER___
#define ___ASSIMP_MODE_LORDER___

#include <memory>
#include <string>

#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
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

	~AssimpModelLorder();

	virtual std::shared_ptr<ModelDX> Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache) override;

private:
	
	using Vertices = std::vector <MeshDX::Vertex>;
	using Indices = std::vector<unsigned long>;

	std::vector<std::shared_ptr<MeshDX>> MakeMesh(const aiScene*);

	std::vector<std::shared_ptr<MaterialDX>> MakeMaterial(const aiScene*, const std::string& directory, ID3D11Device* device, TextureCache& textureCache);



};


#endif
