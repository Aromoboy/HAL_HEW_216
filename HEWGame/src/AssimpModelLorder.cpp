

#ifdef _DEBUG
#include "Geometory.h"
#include "Debug.h"
#include <string>
#include "math.h"
#endif

#include "AssimpModelLorder.h"
#include <DirectXMath.h>




AssimpModelLorder::AssimpModelLorder()
{
}

AssimpModelLorder::~AssimpModelLorder()
{
}

// ロード処理
std::shared_ptr<ModelDX> AssimpModelLorder::Load(std::string path)
{
#ifdef _DEBUG
	std::string errorStr = "";
#endif

	// assimpの設定
	Assimp::Importer importer;
	int flag = 0;
	flag |= aiProcess_Triangulate;
	flag |= aiProcess_FlipUVs;
	//flag |= aiProcess_MakeLeftHanded;

	// assimpで読み込み
	const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
	if (!pScene) {
#ifdef _DEBUG
		errorStr = importer.GetErrorString();
#endif
		return nullptr;
	}

	// 読み込み時の設定を保存
	

	// ディレクトリの読み取り
	std::string directory = path;
	auto strIt = directory.begin();
	while (strIt != directory.end()) {
		if (*strIt == '/')
			*strIt = '\\';
		++strIt;
	}
	directory = directory.substr(0, directory.find_last_of('\\') + 1);

	// メッシュ作成
	std::vector<std::shared_ptr<MeshDX>> meshes = MakeMesh(pScene);
	// マテリアルの作成
	std::vector<std::shared_ptr<MaterialDX>> materials = MakeMaterial(pScene, directory);


	return make_shared<ModelDX>(meshes, materials);
}

// メッシュの作成
std::vector<std::shared_ptr<MeshDX>> AssimpModelLorder::MakeMesh(const aiScene* ptr)
{
	// 事前準備
	aiVector3D zero3(0.0f, 0.0f, 0.0f);
	aiColor4D one4(1.0f, 1.0f, 1.0f, 1.0f);
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);


	std::vector<std::shared_ptr<MeshDX>> meshes;

	std::vector<MeshDX::Vertex> vertices;


	// メッシュの作成
	meshes.resize(pScene->mNumMeshes);
	for (unsigned int i = 0; i < meshes.size(); ++i)
	{
		Vertices vertices;
		Indices indices;

		// 頂点書き込み先の領域を用意
		vertices.resize(pScene->mMeshes[i]->mNumVertices);

		// 頂点データの書き込み
		for (unsigned int j = 0; j < vertices.size(); ++j) {
			// ☆モデルデータから値の取得
			aiVector3D pos = pScene->mMeshes[i]->mVertices[j]; // Positionのデータは必ず存在する
			aiVector3D normal = pScene->mMeshes[i]->HasNormals() ? // 必ず存在するわけではないので存在するか確認
				pScene->mMeshes[i]->mNormals[j] : zero3;
			aiVector3D uv = pScene->mMeshes[i]->HasTextureCoords(0) ?
				pScene->mMeshes[i]->mTextureCoords[0][j] : zero3;
			aiColor4D color = pScene->mMeshes[i]->HasVertexColors(0) ?
				pScene->mMeshes[i]->mColors[0][j] : one4;

			// ☆値を設定
			vertices[j] = {
				DirectX::XMFLOAT3(pos.x, pos.y, pos.z),
				DirectX::XMFLOAT3(normal.x, normal.y, normal.z),
				DirectX::XMFLOAT2(uv.x, uv.y),
				DirectX::XMFLOAT4(color.r, color.g, color.b, color.a)
			};
		}

		// インデックスの書き込み先の用意
		// mNumFacesはポリゴンの数を表す(１ポリゴンで3インデックス
		indices.resize(pScene->mMeshes[i]->mNumFaces * 3);

		// インデックスの書き込み
		for (unsigned int j = 0; j < pScene->mMeshes[i]->mNumFaces; ++j) {
			// ☆モデルデータから値の取得
			aiFace face = pScene->mMeshes[i]->mFaces[j];
			// ☆値の設定
			int idx = j * 3;
			indices[idx + 0] = face.mIndices[0];
			indices[idx + 1] = face.mIndices[1];
			indices[idx + 2] = face.mIndices[2];
		}

		// マテリアル番号
		unsigned long materialIndex = pScene->mMeshes[i]->mMaterialIndex;
		
		// ☆頂点バッファに必要なデータを設定
		MeshDX::Description desc = {};
		desc.pVtx = vertices.data();
		desc.vtxSize = sizeof(MeshDX::Vertex);
		desc.vtxCount = vertices.size();
		desc.pIdx = indices.data();
		desc.idxSize = sizeof(unsigned long);
		desc.idxCount = indices.size();
		desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// ☆頂点バッファ作成
		meshes[i] = make_shared<MeshDX>(desc, materialIndex);
	}

	return meshes;
}

// マテリアルの作成
std::vector<std::shared_ptr<MaterialDX>> AssimpModelLorder::MakeMaterial(const aiScene* ptr, std::string directory)
{
	// 事前準備
	aiColor3D color(0.0f, 0.0f, 0.0f);
	float shininess = 0.0f;
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);

	std::vector<std::shared_ptr<MaterialDX>> materials;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 specular;

	

	// マテリアルの作成
	materials.resize(pScene->mNumMaterials);
	for (unsigned int i = 0; i < materials.size(); ++i)
	{
		//--- 各種マテリアルパラメーターの読み取り
		// ☆拡散光の読み取り
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
			diffuse = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
		else
			diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		// ☆環境光の読み取り
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
			ambient = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
		else
			ambient = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		// ☆反射光の読み取り
		if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
			specular = DirectX::XMFLOAT4(color.r, color.g, color.b, 0.0f);
		else
			specular = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		// ☆反射光の強さを読み取り
		if (pScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
			specular.w = shininess;



		// テクスチャ読み込み処理
		HRESULT hr;
		aiString path;
		
		ID3D11ShaderResourceView* texture;

		// テクスチャのパス情報を読み込み
		texture = nullptr;
		if (pScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) != AI_SUCCESS) 
		{
			continue;
		}

		// そのまま読み込み
		hr = CreateTexture(path.C_Str(), &texture);
		if (SUCCEEDED(hr)) 
		{
			materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);
			continue; 
		}

		// ディレクトリと連結して探索
		hr = CreateTexture((directory + path.C_Str()).c_str(), &texture);
		if (SUCCEEDED(hr)) 
		{
			materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);
			continue; 
		}

		// モデルと同じ階層を探索
		// パスからファイル名のみ取得
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
		// テクスチャの読込
		hr = CreateTexture((directory + fileName).c_str(), &texture);
		if (SUCCEEDED(hr)) 
		{
			materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, texture);
			continue; 
		}

		// 念のため開放
		if (texture) 
		{
			texture->Release();
			texture = nullptr;
		}


		materials[i] = make_shared<MaterialDX>(diffuse, ambient, specular, nullptr);

#ifdef _DEBUG
		std::string errorStr = "";
#endif


#ifdef _DEBUG
		errorStr += path.C_Str();
#endif
	}



	return materials;
}


	
// テクスチャの作成
HRESULT AssimpModelLorder::CreateTexture(const char* fileName, ID3D11ShaderResourceView** a_outTexture)
{
	HRESULT hr = S_OK;

	// 文字変換
	wchar_t wPath[MAX_PATH];
	size_t wLen = 0;
	MultiByteToWideChar(0, 0, fileName, -1, wPath, MAX_PATH);

	// ファイル別読み込み
	DirectX::TexMetadata mdata;
	DirectX::ScratchImage image;
	if (strstr(fileName, ".tga"))
		hr = DirectX::LoadFromTGAFile(wPath, &mdata, image);
	else
		hr = DirectX::LoadFromWICFile(wPath, DirectX::WIC_FLAGS::WIC_FLAGS_IGNORE_SRGB, &mdata, image);
	if (FAILED(hr)) {
		return E_FAIL;
	}

	ID3D11ShaderResourceView* pSRV;

	// シェーダリソース生成
	hr = CreateShaderResourceView(GetDevice(), image.GetImages(), image.GetImageCount(), mdata, &pSRV);

	*a_outTexture = pSRV;
	return hr;
}

