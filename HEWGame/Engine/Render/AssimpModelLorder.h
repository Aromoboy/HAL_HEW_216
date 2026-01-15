#ifndef ___ASSIMP_MODE_LORDER___
#define ___ASSIMP_MODE_LORDER___

#include <memory>
#include <string>
#include <vector>

#include "AABB.h"
#include "Render/Model.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "TextureDX.h"
#include "DirectXMath.h"
#include "DirectX.h"
#include "TextureCache.h"

#include "IModelLoader.h"
#include "Animation.h"

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

    virtual std::shared_ptr<Render::Model> Load(
        const std::string& path,
        ID3D11Device* device,
        TextureCache& textureCache,
        const MaterialTuningSettings& materialTuningSettings) override;
	virtual std::shared_ptr<Animation> LoadAnimation(const std::string& path) override;

private:
	
	// 内部定数定義
	static const UINT	MAX_BONE = 200;	// １メッシュの最大ボーン数(ここを変更する場合.hlsl側の定義も変更する
	static const UINT	MAX_WEIGHT = 4;	// １頂点に割り当てられる最大ボーン数

	using Vertices = std::vector <MeshDX::Vertex>;
	using Indices = std::vector<unsigned long>;

	DirectX::XMMATRIX GetMatrixFromAssimpMatrix(aiMatrix4x4 M);
	NodeIndexDX FindNode(const char* name, const std::vector<NodeDX>&);

	void MakeNode(const aiScene* ptr, std::vector<NodeDX>& a_outNodes, Animation::AnimationArray& a_outAnimeTransform);
	// Model生成用関数
	std::vector<std::shared_ptr<MeshDX>> MakeMesh(const aiScene*, AABB& outBounds, bool& hasBounds, std::vector<NodeDX> a_nodes);
	std::vector<std::shared_ptr<MaterialDX>> MakeMaterial(
        const aiScene*,
        const std::string& directory,
        const std::string& modelFilename,
        ID3D11Device* device,
        TextureCache& textureCache,
        const MaterialTuningSettings& materialTuningSettings);

	void MakeVertexWeightHasBone(const void* ptr, AssimpModelLorder::Vertices& a_vertices, std::vector<NodeDX> a_nodes, std::vector<BoneDX>& a_bones);
	void MakeVertexWeight(const void* a_ptr, int a_meshIndex, std::vector<NodeDX>& a_nodes, AssimpModelLorder::Vertices& a_vertices, std::vector<BoneDX>& a_bones);
	void MakeVertexWeightFromNode(const void* scene, const void* ptr, AssimpModelLorder::Vertices& a_vertices, std::vector<NodeDX> a_nodes, std::vector<BoneDX>& a_bones);



};


#endif
