#include "TextureBundle.h"

TextureBundle::TextureBundle()
{
	albedo = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	normal = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	roughness = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	metalness = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
}

TextureBundle::TextureBundle(const char* name) : TextureBundle::TextureBundle() {
	this->name = name;
}
