#pragma once

#include <d3d11.h>
#include <string>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects

struct TextureBundle {
	TextureBundle();
	TextureBundle(std::string name);
	std::string name;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughness;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalness;
};