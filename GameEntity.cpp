#include "GameEntity.h"

using namespace DirectX;

GameEntity::GameEntity(std::string name, Mesh* mesh, Material* material)
{
	// Save the data
	this->name = name;
	this->mesh = mesh;
	this->material = material;
}

Mesh* GameEntity::GetMesh() { return mesh; }
Material* GameEntity::GetMaterial() { return material; }
Transform* GameEntity::GetTransform() { return &transform; }
std::string GameEntity::GetName() { return name; }


void GameEntity::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* camera)
{
	// Tell the material to prepare for a draw
	material->PrepareMaterial(&transform, camera);

	// Draw the mesh
	mesh->SetBuffersAndDraw(context);
}
