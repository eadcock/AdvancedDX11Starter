#include "AssetManager.h"

#include "WICTextureLoader.h"
#include "DXCore.h"

#include <fstream>
#include <nlohmann/json.hpp>

AssetManager* AssetManager::instance;

constexpr auto ASSET_PATH = ".\\Assets";
constexpr auto DEFINITIONS_PATH = ".\\Definitions";

#define LoadTexture(srv) DirectX::CreateWICTextureFromFile(device.Get(), context.Get(), (wide_path + L"\\..\\..\\" + p.path().wstring()).c_str(), 0, srv.GetAddressOf())

#define LoadShader(type, file) new type(device.Get(), context.Get(), (wide_path + L"\\" + file).c_str())

#define RemoveExtension(s) s.erase(s.find_last_of("."), std::string::npos)

AssetManager::~AssetManager()
{
	for (auto& p : textureBundles) delete p.second;
	for (auto& p : materials) delete p.second;
	for (auto& p : meshes) delete p.second;
	for (auto& p : shaders) delete p.second;
}

void AssetManager::LoadTextureBundles(std::vector<std::filesystem::directory_entry> bundlePaths) {
	for (auto& p : bundlePaths) {
		std::ifstream i(p.path());
		nlohmann::json d;
		i >> d;

		textureBundles[d["name"].get<std::string>()] = new TextureBundle();
		std::string name = d["name"].get<std::string>();
		textureBundles[name]->name = name;
		if (d.contains("location")) {
			std::string location = d["location"].get<std::string>();
			std::string root = std::string(ASSET_PATH) + "\\" + location + "\\" + d["name"].get<std::string>().c_str();
			textureBundles[d["name"].get<std::string>()]->albedo = textures[root + "_albedo"];
			textureBundles[d["name"].get<std::string>()]->normal = textures[root + "_normals"];
			textureBundles[d["name"].get<std::string>()]->roughness = textures[root + "_roughness"];
			textureBundles[d["name"].get<std::string>()]->metalness = textures[root + "_metal"];
		}
		else {
			textureBundles[d["name"].get<std::string>()]->albedo = textures[std::string(ASSET_PATH) + "\\" + d["albedo"].get<std::string>()];
			textureBundles[d["name"].get<std::string>()]->normal = textures[std::string(ASSET_PATH) + "\\" + d["normal"].get<std::string>()];
			textureBundles[d["name"].get<std::string>()]->roughness = textures[std::string(ASSET_PATH) + "\\" + d["roughness"].get<std::string>()];
			textureBundles[d["name"].get<std::string>()]->metalness = textures[std::string(ASSET_PATH) + "\\" + d["metal"].get<std::string>()];
		}
	}
	
}

void AssetManager::LoadMaterials(std::vector<std::filesystem::directory_entry> materialPaths) {
	for (auto& p : materialPaths) {
		std::ifstream i(p.path());
		nlohmann::json d;

		i >> d;

		std::string name;
		if (d["name"].is_string()) {
			name = d["name"].get<std::string>();
		}
		else
		{
			name = p.path().filename().string();
			RemoveExtension(name);
		}

		TextureBundle* bundle;
		if (d["textures"].is_string()) {
			std::string textureName = d["textures"].get<std::string>();
			bundle = textureBundles[textureName];
		}
		else {
			bundle = new TextureBundle();
			bundle->albedo = textures[d["textures"]["albedo"].get<std::string>()];
			bundle->normal = textures[d["textures"]["normal"].get<std::string>()];
			bundle->roughness = textures[d["textures"]["roughness"].get<std::string>()];
			bundle->metalness = textures[d["textures"]["metalness"].get<std::string>()];
		}

		float color[4];
		d["color"].get_to(color);
		float uv[2];
		d["uvScale"].get_to(uv);
		SimpleVertexShader* vs = GetVertexShader(d["shader"]["vertex"].get<std::string>());
		SimplePixelShader* ps = GetPixelShader(d["shader"]["pixel"].get<std::string>());
		materials[name] = new Material(
			vs,
			ps,
			DirectX::XMFLOAT4(color),
			d["shininess"].get<float>(),
			DirectX::XMFLOAT2(uv),
			bundle,
			samplerOptions
		);

		bundle = nullptr;
	}
}


void AssetManager::LoadEntities(std::vector<std::filesystem::directory_entry> entityPaths) {
	for (auto& p : entityPaths) {
		std::ifstream i(p.path());
		nlohmann::json d;

		i >> d;

		std::string name;
		if (d["name"].is_string()) {
			name = d["name"].get<std::string>();
		}
		else
		{
			name = p.path().filename().string();
			RemoveExtension(name);
		}

		float position[3];
		d["position"].get_to(position);
		float scale[3];
		d["scale"].get_to(scale);
		float rotation[3];
		d["rotation"].get_to(rotation);

		entities[name] = new GameEntity(
			name,
			meshes[d["mesh"].get<std::string>()],
			materials[d["material"].get<std::string>()]
		);

		entities[name]->GetTransform()->SetPosition(position[0], position[1], position[2]);
		entities[name]->GetTransform()->SetScale(scale[0], scale[1], scale[2]);
		entities[name]->GetTransform()->SetRotation(rotation[0], rotation[1], rotation[2]);

		if (!d["parent"].is_null()) {
			std::string parent_str = d["parent"].get<std::string>();
			if (entities.contains(parent_str)) {
				if (entities[parent_str]->GetTransform()->IndexOfChild(entities[name]->GetTransform()) == -1) {
					entities[parent_str]->GetTransform()->AddChild(entities[name]->GetTransform());
				}
			}
		}

		if (!d["children"].is_null()) {
			for (int i = 0; i < d["children"].size(); i++) {
				std::string child_str = d["children"][i].get<std::string>();
				if (entities.contains(child_str) && entities[child_str]->GetTransform()->GetParent() != entities[name]->GetTransform()) {
					entities[child_str]->GetTransform()->SetParent(entities[name]->GetTransform());
				}
			}
		}
	}
}

void AssetManager::Load()
{
	shaders["VertexShader"] = LoadShader(SimpleVertexShader, L"VertexShader.cso");
	shaders["PixelShader"] = LoadShader(SimplePixelShader, L"PixelShader.cso");
	shaders["PixelShaderPBR"] = LoadShader(SimplePixelShader, L"PixelShaderPBR.cso");
	shaders["SolidColorPS"] = LoadShader(SimplePixelShader, L"SolidColorPS.cso");
	shaders["SkyPS"] = LoadShader(SimplePixelShader, L"SkyPS.cso");
	shaders["SkyVS"] = LoadShader(SimpleVertexShader, L"SkyVS.cso");

	for (auto& p : std::filesystem::recursive_directory_iterator(ASSET_PATH)) {
		if (p.path().extension().compare(".png") == 0) {
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			LoadTexture(srv);

			std::string s = p.path().string();
			RemoveExtension(s);
			textures[s] = srv.Get();
		}
		else if (p.path().extension().compare(".obj") == 0) {
			std::string s = p.path().filename().string();
			RemoveExtension(s);
			meshes[s] = new Mesh(s, (path + "\\..\\..\\" + p.path().string()).c_str(), device);
		}
	}

	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());

	std::vector<std::filesystem::directory_entry> bundlePaths;
	std::vector<std::filesystem::directory_entry> materialPaths;
	std::vector<std::filesystem::directory_entry> entityPaths;
	for (auto& p : std::filesystem::recursive_directory_iterator(DEFINITIONS_PATH)) {
		if (p.path().extension().compare(".bundle") == 0) {
			bundlePaths.push_back(p);
		}
		else if (p.path().extension().compare(".material") == 0) {
			materialPaths.push_back(p);
		}
		else if (p.path().extension().compare(".ge") == 0) {
			entityPaths.push_back(p);
		}
	}

	LoadTextureBundles(bundlePaths);
	LoadMaterials(materialPaths);
	LoadEntities(entityPaths);
}

void AssetManager::Initialize(std::string path, std::wstring wide_path, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	this->path = path;
	this->wide_path = wide_path;
	this->device = device;
	this->context = context;
}

int AssetManager::GetBundleCount()
{
	return textureBundles.size();
}

Material* AssetManager::GetMaterial(std::string tag)
{
	if (materials.contains(tag)) {
		return materials[tag];
	}
	return nullptr;
}

Mesh* AssetManager::GetMesh(std::string tag)
{
	if (meshes.contains(tag)) {
		return meshes[tag];
	}
	return nullptr;
}

TextureBundle* AssetManager::GetBundle(std::string tag)
{
	if (textureBundles.contains(tag)) {
		return textureBundles[tag];
	}

	return nullptr;
}

ISimpleShader* AssetManager::GetShader(std::string tag) {
	if (shaders.contains(tag)) {
		return shaders[tag];
	}
	return nullptr;
}

SimpleVertexShader* AssetManager::GetVertexShader(std::string tag)
{
	if (shaders.contains(tag)) {
		return dynamic_cast<SimpleVertexShader*>(shaders[tag]);
	}
	return nullptr;
}

SimplePixelShader* AssetManager::GetPixelShader(std::string tag)
{
	if (shaders.contains(tag)) {
		return dynamic_cast<SimplePixelShader*>(shaders[tag]);
	}
	return nullptr;
}

GameEntity* AssetManager::GetEntity(std::string tag)
{
	if (entities.contains(tag)) {
		return entities[tag];
	}
	return nullptr;
}
