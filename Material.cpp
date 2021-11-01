#include "Material.h"

Material::Material(
	SimpleVertexShader* vs,
	SimplePixelShader* ps,
	DirectX::XMFLOAT4 color,
	float shininess,
	DirectX::XMFLOAT2 uvScale,
	TextureBundle* textures,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampler)
{
	this->vs = vs;
	this->ps = ps;
	this->color = color;
	this->shininess = shininess;
	this->SRVs = textures;
	this->sampler = sampler;
	this->uvScale = uvScale;
	this->clampSampler = clampler;
}


Material::~Material()
{
}

void Material::PrepareMaterial(Transform* transform, Camera* cam)
{
	// Turn shaders on
	vs->SetShader();
	ps->SetShader();

	// Set vertex shader data
	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
	vs->SetMatrix4x4("worldInverseTranspose", transform->GetWorldInverseTransposeMatrix());
	vs->SetMatrix4x4("view", cam->GetView());
	vs->SetMatrix4x4("projection", cam->GetProjection());
	vs->SetFloat2("uvScale", uvScale);
	vs->CopyAllBufferData();

	// Set pixel shader data
	ps->SetFloat4("Color", color); 
	ps->SetFloat("Shininess", shininess);
	ps->CopyBufferData("perMaterial");

	// Set SRVs
	ps->SetShaderResourceView("AlbedoTexture", SRVs->albedo);
	ps->SetShaderResourceView("NormalTexture", SRVs->normal);
	ps->SetShaderResourceView("RoughnessTexture", SRVs->roughness);
	ps->SetShaderResourceView("MetalTexture", SRVs->metalness);

	// Set sampler
	ps->SetSamplerState("BasicSampler", sampler);
}

void Material::SetPerMaterialDataAndResources(bool copyToGPUNow)
{
	// Set vertex shader per-material vars
	vs->SetFloat2("uvScale", uvScale);
	if (copyToGPUNow)
	{
		vs->CopyBufferData("perMaterial");
	}

	// Set pixel shader per-material vars
	ps->SetFloat4("Color", color);
	ps->SetFloat("Shininess", shininess);
	if (copyToGPUNow)
	{
		ps->CopyBufferData("perMaterial");
	}

	// Loop and set any other resources
	ps->SetShaderResourceView("AlbedoTexture", SRVs->albedo.Get());
	ps->SetShaderResourceView("NormalTexture", SRVs->normal.Get());
	ps->SetShaderResourceView("RoughnessTexture", SRVs->roughness.Get());
	ps->SetShaderResourceView("MetalnessTexture", SRVs->metalness.Get());
	ps->SetSamplerState("BasicSampler", sampler);
	ps->SetSamplerState("ClampSampler", clampSampler);
}
