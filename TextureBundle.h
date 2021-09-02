#pragma once

#include <d3d11.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects

struct TextureBundle {
	TextureBundle();
	TextureBundle(const char* name);
	const char* name;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughness;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalness;
};