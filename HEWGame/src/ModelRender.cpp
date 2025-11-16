#include "ModelRender.h"
#include "DrawMatrix.h"
#include "ShaderList.h"

ModelRender::ModelRender()
{
}

ModelRender::~ModelRender()
{
}

void ModelRender::Draw(ModelDX& a_model, ICamera& a_camera)
{
	DirectX::XMFLOAT4X4 fMat; // s—ñ‚ÌŠi”[æ
	DirectX::XMFLOAT4X4 wvp[3];


	wvp[1] = a_camera.GetViewMatrix();
	wvp[2] = a_camera.GetProjectionMatrix();

	// •ÏŠ·s—ñ‚Ì“]’u
	DrawMatrix::CreateWorldMatrix(wvp[0], float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 170.0f * RAD, 0.0f });


	// ƒVƒF[ƒ_[‚Ì“o˜^
	ShaderList::SetWVP(wvp);
	VertexShader* pVTX = ShaderList::GetVS(ShaderList::VS_WORLD);
	PixelShader* pPXS = ShaderList::GetPS(ShaderList::PS_LAMBERT);

	pVTX->Bind();
	pPXS->Bind();

	// •`‰æˆ—
	for (int i = 0; i < a_model.GetMeshCount(); i++)
	{
		std::shared_ptr<MeshDX> mesh = a_model.GetMesh(i).lock();
		if (!mesh) continue;

		
		if (i < a_model.GetMeshCount())
		{
			std::shared_ptr<MaterialDX> material = a_model.GetMaterial(mesh->GetMaterialID()).lock();

			ShaderList::SetMaterial(*material);
		}

		MeshDraw(*mesh);

	}

}

void ModelRender::MeshDraw(MeshDX& a_mesh)
{
		ID3D11DeviceContext* pContext = GetContext();
	UINT stride = sizeof(MeshDX::Vertex);
	UINT offset = 0;

	ID3D11Buffer* vertex = a_mesh.GetVertecBuffer();
	ID3D11Buffer* index = a_mesh.GetIndexBuffer();

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->IASetVertexBuffers(0, 1, &vertex, &stride, &offset);

	
	DXGI_FORMAT format;
	switch (sizeof(unsigned long))
	{
	case 4: format = DXGI_FORMAT_R32_UINT; break;
	case 2: format = DXGI_FORMAT_R16_UINT; break;
	}
	pContext->IASetIndexBuffer(index, format, 0);
	pContext->DrawIndexed(a_mesh.GetIndexCount(), 0, 0);
	
}
