#include "TextureBundle.h"

TextureBundle::TextureBundle()
{
	albedo = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	normal = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	roughness = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
	metalness = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>();
}

TextureBundle::TextureBundle(std::string name) : TextureBundle::TextureBundle() {
	this->name = name;
}
