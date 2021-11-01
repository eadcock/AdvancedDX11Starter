#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "SimpleShader.h"
#include "Camera.h"
#include "Lights.h"
#include "TextureBundle.h"
#include "Transform.h"

class Material
{
public:
	Material(
		SimpleVertexShader* vs, 
		SimplePixelShader* ps, 
		DirectX::XMFLOAT4 color, 
		float shininess, 
		DirectX::XMFLOAT2 uvScale, 
		TextureBundle* textures,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> clampler);
	~Material();

	void PrepareMaterial(Transform* transform, Camera* cam);
	void SetPerMaterialDataAndResources(bool copyToGPUNow = true);

	bool GetRefractive() { return refractive; }

	SimpleVertexShader* GetVS() { return vs; }
	SimplePixelShader* GetPS() { return ps; }

	void SetVS(SimpleVertexShader* vs) { this->vs = vs; }
	void SetPS(SimplePixelShader* ps) { this->ps = ps; }

	TextureBundle* GetSRVs() { return SRVs; }

	void SetSRVs(TextureBundle* b) { this->SRVs = b; }

private:
	SimpleVertexShader* vs;
	SimplePixelShader* ps;

	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT4 color;
	float shininess;
	bool refractive;

	TextureBundle* SRVs;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;
};

