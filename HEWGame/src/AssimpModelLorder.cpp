// ============================================================================
// 概要: AssimpModelLorder の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================



#include "Debug.h"
#ifdef _DEBUG
#include "Geometory.h"
#include <string>
#include "math.h"
#endif

#include "AssimpModelLorder.h"
#include <DirectXMath.h>
#include "TextureCache.h"
#include <locale>
#include <codecvt>


AssimpModelLorder::AssimpModelLorder()
{
}

AssimpModelLorder::~AssimpModelLorder()
{
}

// 備考: [h に関する説明を日本語で補足している。
std::shared_ptr<ModelDX> AssimpModelLorder::Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache)
{
#ifdef _DEBUG
	std::string errorStr = "";
#endif

	// 備考: assimp̐ݒ に関する説明を日本語で補足している。
	Assimp::Importer importer;
	int flag = 0;
	flag |= aiProcess_Triangulate;
	flag |= aiProcess_FlipUVs;
	// 備考: flag |= aiProcess_MakeLeftHanded; に関する説明を日本語で補足している。

	// 備考: assimpœǂݍ に関する説明を日本語で補足している。
	const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
	if (!pScene) {
#ifdef _DEBUG
		errorStr = importer.GetErrorString();
#endif
		return nullptr;
	}

	// ǂݍݎ̐ݒۑ
	

	// 備考: fBNg̓ǂݎ に関する説明を日本語で補足している。
	std::string directory = path;
	auto strIt = directory.begin();
	while (strIt != directory.end()) {
		if (*strIt == '/')
			*strIt = '\\';
		++strIt;
	}
	directory = directory.substr(0, directory.find_last_of('\\') + 1);

	// 備考: bV쐬 に関する説明を日本語で補足している。
	std::vector<std::shared_ptr<MeshDX>> meshes = MakeMesh(pScene);
	// 備考: }eA̍쐬 に関する説明を日本語で補足している。
	        std::vector<std::shared_ptr<MaterialDX>> materials = MakeMaterial(pScene, directory, device, textureCache);


	return make_shared<ModelDX>(meshes, materials);
}

// 備考: bV̍쐬 に関する説明を日本語で補足している。
std::vector<std::shared_ptr<MeshDX>> AssimpModelLorder::MakeMesh(const aiScene* ptr)
{
	// 備考: O に関する説明を日本語で補足している。
	aiVector3D zero3(0.0f, 0.0f, 0.0f);
	aiColor4D one4(1.0f, 1.0f, 1.0f, 1.0f);
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);


	std::vector<std::shared_ptr<MeshDX>> meshes;

	std::vector<MeshDX::Vertex> vertices;


	// 備考: bV̍쐬 に関する説明を日本語で補足している。
	meshes.resize(pScene->mNumMeshes);
	for (unsigned int i = 0; i < meshes.size(); ++i)
	{
		Vertices vertices;
		Indices indices;

		// 備考: _ݐ̗̈p に関する説明を日本語で補足している。
		vertices.resize(pScene->mMeshes[i]->mNumVertices);

		// 備考: _f[^̏ に関する説明を日本語で補足している。
		for (unsigned int j = 0; j < vertices.size(); ++j) {
			// ff[^l̎擾
			aiVector3D pos = pScene->mMeshes[i]->mVertices[j]; // 備考: Positioñf[^͕K݂ に関する説明を日本語で補足している。
			aiVector3D normal = pScene->mMeshes[i]->HasNormals() ? // K݂킯ł͂Ȃ̂ő݂邩mF
				pScene->mMeshes[i]->mNormals[j] : zero3;
			aiVector3D uv = pScene->mMeshes[i]->HasTextureCoords(0) ?
				pScene->mMeshes[i]->mTextureCoords[0][j] : zero3;
			aiColor4D color = pScene->mMeshes[i]->HasVertexColors(0) ?
				pScene->mMeshes[i]->mColors[0][j] : one4;

			// 備考: lݒ に関する説明を日本語で補足している。
			vertices[j] = {
				DirectX::XMFLOAT3(pos.x, pos.y, pos.z),
				DirectX::XMFLOAT3(normal.x, normal.y, normal.z),
				DirectX::XMFLOAT2(uv.x, uv.y),
				DirectX::XMFLOAT4(color.r, color.g, color.b, color.a)
			};
		}

		// 備考: CfbNX̏ݐ̗p に関する説明を日本語で補足している。
		// 備考: mNumFaces̓|S̐\(P|S3CfbNX に関する説明を日本語で補足している。
		indices.resize(pScene->mMeshes[i]->mNumFaces * 3);

		// 備考: CfbNX̏ に関する説明を日本語で補足している。
		for (unsigned int j = 0; j < pScene->mMeshes[i]->mNumFaces; ++j) {
			// ff[^l̎擾
			aiFace face = pScene->mMeshes[i]->mFaces[j];
			// 備考: l̐ݒ に関する説明を日本語で補足している。
			int idx = j * 3;
			indices[idx + 0] = face.mIndices[0];
			indices[idx + 1] = face.mIndices[1];
			indices[idx + 2] = face.mIndices[2];
		}

		// 備考: }eAԍ に関する説明を日本語で補足している。
		unsigned long materialIndex = pScene->mMeshes[i]->mMaterialIndex;
		
		// 備考: _obt@ɕKvȃf[^ݒ に関する説明を日本語で補足している。
		MeshDX::Description desc = {};
		desc.pVtx = vertices.data();
		desc.vtxSize = sizeof(MeshDX::Vertex);
		desc.vtxCount = vertices.size();
		desc.pIdx = indices.data();
		desc.idxSize = sizeof(unsigned long);
		desc.idxCount = indices.size();
		desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// 備考: _obt@쐬 に関する説明を日本語で補足している。
		meshes[i] = make_shared<MeshDX>(desc, materialIndex);
	}

	return meshes;
}

// 備考: }eA̍쐬 に関する説明を日本語で補足している。
std::vector<std::shared_ptr<MaterialDX>> AssimpModelLorder::MakeMaterial(const aiScene* ptr, const std::string& directory, ID3D11Device* device, TextureCache& textureCache)
{
	// 備考: O に関する説明を日本語で補足している。
	aiColor3D color(0.0f, 0.0f, 0.0f);
	float shininess = 0.0f;
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);

	std::vector<std::shared_ptr<MaterialDX>> materials;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 specular;

	

	// 備考: }eA̍쐬 に関する説明を日本語で補足している。
	materials.resize(pScene->mNumMaterials);
	for (unsigned int i = 0; i < materials.size(); ++i)
	{
		// 備考: --- e}eAp[^[̓ǂݎ に関する説明を日本語で補足している。
		// 備考: gU̓ǂݎ に関する説明を日本語で補足している。
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
			diffuse = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
		else
			diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		// ̓ǂݎ
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
			ambient = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
		else
			ambient = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		// ˌ̓ǂݎ
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
			specular = DirectX::XMFLOAT4(color.r, color.g, color.b, 0.0f);
		else
			specular = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		// ˌ̋ǂݎ
		if (pScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
			specular.w = shininess;



		// 備考: eNX`ǂݍݏ に関する説明を日本語で補足している。
		aiString path;
		std::shared_ptr<Texture2D> texture;


	

		// 備考: eNX`̃pXǂݍ に関する説明を日本語で補足している。
		if (pScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) != AI_SUCCESS) 
		{
			continue;
		}

		// ̂܂ܓǂݍ

		
		texture = textureCache.Get(path.C_Str());
		if (texture) 
		{
			materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);
			continue; 
		}



		// 備考: fƓKwT に関する説明を日本語で補足している。
		// pXt@Ĉݎ擾
		std::string fullPath = path.C_Str();
		std::string::iterator strIt = fullPath.begin();
		while (strIt != fullPath.end()) {
			if (*strIt == '/')
				*strIt = '\\';
			++strIt;
		}
		size_t find = fullPath.find_last_of("\\");
		std::string fileName = fullPath;
		if (find != std::string::npos)
			fileName = fileName.substr(find + 1);

		                std::string filePathCmp = directory + fileName;

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::wstring wFilePath = converter.from_bytes(filePathCmp);

		// 備考: eNX`̓Ǎ に関する説明を日本語で補足している。
		texture = textureCache.Load(fileName, device, wFilePath );
		if (texture) 
		{
			materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);
			continue; 
		}

		materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);

#ifdef _DEBUG
		std::string errorStr = "";
#endif


#ifdef _DEBUG
		errorStr += path.C_Str();
#endif
	}



	return materials;
}
