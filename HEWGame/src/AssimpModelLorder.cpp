

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

// [h
std::shared_ptr<ModelDX> AssimpModelLorder::Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache)
{
#ifdef _DEBUG
	std::string errorStr = "";
#endif

	// assimp̐ݒ
	Assimp::Importer importer;
	int flag = 0;
	flag |= aiProcess_Triangulate;
	flag |= aiProcess_FlipUVs;
	//flag |= aiProcess_MakeLeftHanded;

	// assimpœǂݍ
	const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
	if (!pScene) {
#ifdef _DEBUG
		errorStr = importer.GetErrorString();
#endif
		return nullptr;
	}

	// ǂݍݎ̐ݒۑ
	

	// fBNg̓ǂݎ
	std::string directory = path;
	auto strIt = directory.begin();
	while (strIt != directory.end()) {
		if (*strIt == '/')
			*strIt = '\\';
		++strIt;
	}
	directory = directory.substr(0, directory.find_last_of('\\') + 1);

	// bV쐬
	std::vector<std::shared_ptr<MeshDX>> meshes = MakeMesh(pScene);
	// }eA̍쐬
	        std::vector<std::shared_ptr<MaterialDX>> materials = MakeMaterial(pScene, directory, device, textureCache);


	return make_shared<ModelDX>(meshes, materials);
}

// bV̍쐬
std::vector<std::shared_ptr<MeshDX>> AssimpModelLorder::MakeMesh(const aiScene* ptr)
{
	// O
	aiVector3D zero3(0.0f, 0.0f, 0.0f);
	aiColor4D one4(1.0f, 1.0f, 1.0f, 1.0f);
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);


	std::vector<std::shared_ptr<MeshDX>> meshes;

	std::vector<MeshDX::Vertex> vertices;


	// bV̍쐬
	meshes.resize(pScene->mNumMeshes);
	for (unsigned int i = 0; i < meshes.size(); ++i)
	{
		Vertices vertices;
		Indices indices;

		// _ݐ̗̈p
		vertices.resize(pScene->mMeshes[i]->mNumVertices);

		// _f[^̏
		for (unsigned int j = 0; j < vertices.size(); ++j) {
			// ff[^l̎擾
			aiVector3D pos = pScene->mMeshes[i]->mVertices[j]; // Positioñf[^͕K݂
			aiVector3D normal = pScene->mMeshes[i]->HasNormals() ? // K݂킯ł͂Ȃ̂ő݂邩mF
				pScene->mMeshes[i]->mNormals[j] : zero3;
			aiVector3D uv = pScene->mMeshes[i]->HasTextureCoords(0) ?
				pScene->mMeshes[i]->mTextureCoords[0][j] : zero3;
			aiColor4D color = pScene->mMeshes[i]->HasVertexColors(0) ?
				pScene->mMeshes[i]->mColors[0][j] : one4;

			// lݒ
			vertices[j] = {
				DirectX::XMFLOAT3(pos.x, pos.y, pos.z),
				DirectX::XMFLOAT3(normal.x, normal.y, normal.z),
				DirectX::XMFLOAT2(uv.x, uv.y),
				DirectX::XMFLOAT4(color.r, color.g, color.b, color.a)
			};
		}

		// CfbNX̏ݐ̗p
		// mNumFaces̓|S̐\(P|S3CfbNX
		indices.resize(pScene->mMeshes[i]->mNumFaces * 3);

		// CfbNX̏
		for (unsigned int j = 0; j < pScene->mMeshes[i]->mNumFaces; ++j) {
			// ff[^l̎擾
			aiFace face = pScene->mMeshes[i]->mFaces[j];
			// l̐ݒ
			int idx = j * 3;
			indices[idx + 0] = face.mIndices[0];
			indices[idx + 1] = face.mIndices[1];
			indices[idx + 2] = face.mIndices[2];
		}

		// }eAԍ
		unsigned long materialIndex = pScene->mMeshes[i]->mMaterialIndex;
		
		// _obt@ɕKvȃf[^ݒ
		MeshDX::Description desc = {};
		desc.pVtx = vertices.data();
		desc.vtxSize = sizeof(MeshDX::Vertex);
		desc.vtxCount = vertices.size();
		desc.pIdx = indices.data();
		desc.idxSize = sizeof(unsigned long);
		desc.idxCount = indices.size();
		desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// _obt@쐬
		meshes[i] = make_shared<MeshDX>(desc, materialIndex);
	}

	return meshes;
}

// }eA̍쐬
std::vector<std::shared_ptr<MaterialDX>> AssimpModelLorder::MakeMaterial(const aiScene* ptr, const std::string& directory, ID3D11Device* device, TextureCache& textureCache)
{
	// O
	aiColor3D color(0.0f, 0.0f, 0.0f);
	float shininess = 0.0f;
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);

	std::vector<std::shared_ptr<MaterialDX>> materials;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 specular;

	

	// }eA̍쐬
	materials.resize(pScene->mNumMaterials);
	for (unsigned int i = 0; i < materials.size(); ++i)
	{
		//--- e}eAp[^[̓ǂݎ
		// gU̓ǂݎ
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



		// eNX`ǂݍݏ
		aiString path;
		std::shared_ptr<Texture2D> texture;


	

		// eNX`̃pXǂݍ
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



		// fƓKwT
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

		// eNX`̓Ǎ
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
