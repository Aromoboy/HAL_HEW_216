#include "CameraRig.h"
#include "DirectXMath.h"


void CameraRig::OnUpdate()
{
	// UpdateContextÇ™èoÇΩÇÁÇªÇ¡ÇøÇ…èëÇ´ä∑Ç¶ÇÈ
	float3 pos;
	float3 scale;
	float3 rotation;

	float3 epos;
	float3 escale;
	float3 erotation;

	
	DirectX::XMMATRIX matrix = DirectX::XMMatrixRotationRollPitchYaw(erotation.x, erotation.y, erotation.z);

	matrix* DirectX::XMMatrixTranslation(offset.x, offset.y, offset.z);

	matrix *  DirectX::XMMatrixRotationRollPitchYaw(0.0f, pitch, yaw);

	matrix* DirectX::XMMatrixTranslation(epos.x, epos.y, epos.y);


	// ïœä∑ÇµÇΩç¿ïWÇë„ì¸
	DirectX::XMFLOAT4X4 store;

	DirectX::XMStoreFloat4x4(&store, matrix);


	pos.x = store.m[3][0];
	pos.y = store.m[3][1];
	pos.z = store.m[3][2];	

	
	store.m[3][1];
	

	rotation.x = atan2f(store.m[1][2], store.m[1][1]);
	rotation.y = atan2f(store.m[2][0], store.m[0][0]);
	rotation.z = atan2f(store.m[0][1], store.m[0][0]);

}
