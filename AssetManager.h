#pragma once
#include "TextureBundle.h"
#include "Material.h"
#include "Mesh.h"
#include "SimpleShader.h"
#include "GameEntity.h"
#include "Sky.h"

#include <filesystem>
#include <unordered_map>
#include <string>
#include <concepts>
#include <boost/any.hpp>

class AssetManager
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static AssetManager& GetInstance()
	{
		if (!instance)
		{
			instance = new AssetManager();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	AssetManager(AssetManager const&) = delete;
	void operator=(AssetManager const&) = delete;

private:
	static AssetManager* instance;
	AssetManager() {};
#pragma endregion

public:
	~AssetManager();

	void Initialize(std::string path, std::wstring wide_path, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void Load();

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clamplerOptions;

	Sky* sky;

	TextureBundle* GetBundle(std::string tag);
	std::unordered_map<std::string, TextureBundle*> GetBundles() { return textureBundles; }
	int GetBundleCount();

	Material* GetMaterial(std::string tag);
	std::unordered_map<std::string, Material*> GetMaterials() { return materials; }
	int GetMaterialCount() { return materials.size(); }

	Mesh* GetMesh(std::string tag);
	std::unordered_map<std::string, Mesh*> GetMeshes() { return meshes; }
	int GetMeshCount() { return meshes.size(); }

	ISimpleShader* GetShader(std::string tag);
	SimpleVertexShader* GetVertexShader(std::string tag);
	SimplePixelShader* GetPixelShader(std::string tag);

	GameEntity* GetEntity(std::string tag);
	std::unordered_map<std::string, GameEntity*> GetEntities() { return entities; }
private:
	std::wstring wide_path;
	std::string path;

	Microsoft::WRL::ComPtr<ID3D11Device>		device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>	context;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
	std::unordered_map<std::string, TextureBundle*> textureBundles;

	std::unordered_map<std::string, Material*> materials;
	std::unordered_map<std::string, Mesh*> meshes;
	std::unordered_map<std::string, ISimpleShader*> shaders;
	std::unordered_map<std::string, GameEntity*> entities;

	void LoadTextureBundles(std::vector<std::filesystem::directory_entry> bundlePaths);
	void LoadMaterials(std::vector<std::filesystem::directory_entry> materialPaths);
	void LoadEntities(std::vector<std::filesystem::directory_entry> entityPaths);
};
