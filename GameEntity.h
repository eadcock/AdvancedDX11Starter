#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Material.h"
#include "Transform.h"
#include "Camera.h"
#include "SimpleShader.h"

class GameEntity
{
public:
	GameEntity(std::string name, Mesh* mesh, Material* material);

	Mesh* GetMesh();
	Material* GetMaterial();
	Transform* GetTransform();
	std::string GetName();

	void SetName(std::string n) { this->name = n; }
	void SetMesh(Mesh* m) { this->mesh = m; }

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* camera);

private:
	std::string name;
	Mesh* mesh;
	Material* material;
	Transform transform = Transform(this);
};

