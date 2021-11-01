#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "Sky.h"
#include "GameEntity.h"
#include "AssetManager.h"

enum RenderTargetType
{
	SCENE_COLORS_NO_AMBIENT,
	SCENE_AMBIENT,
	SCENE_NORMALS,
	SCENE_DEPTHS,
	SSAO_RESULTS,
	SSAO_BLUR,
	REFRACTION_SILHOUETTE,
	FINAL_COMPOSITE,

	// Count is always the last one!
	RENDER_TARGET_TYPE_COUNT
};

// This needs to match the expected per-frame vertex shader data
struct VSPerFrameData
{
	DirectX::XMFLOAT4X4 ViewMatrix;
	DirectX::XMFLOAT4X4 ProjectionMatrix;
};

// This needs to match the expected per-frame pixel shader data
struct PSPerFrameData
{
	Light Lights[MAX_LIGHTS];
	int LightCount;
	DirectX::XMFLOAT3 CameraPosition;
	int TotalSpecIBLMipLevels;
};

using namespace DirectX;

class Renderer
{
private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;

	unsigned int windowWidth;
	unsigned int windowHeight;

	std::vector<Light>& lights;

	// Per-frame constant buffers and data
	Microsoft::WRL::ComPtr<ID3D11Buffer> psPerFrameConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vsPerFrameConstantBuffer;
	PSPerFrameData psPerFrameData;
	VSPerFrameData vsPerFrameData;

	// Refraction related
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> refractionSilhouetteDepthState;
	bool useRefractionSilhouette;
	bool refractionFromNormalMap;
	float indexOfRefraction;
	float refractionScale;

	// Render targets
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetRTVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> renderTargetSRVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];

	

public:
	Renderer(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV,
		unsigned int windowWidth,
		unsigned int windowHeight,
		std::vector<Light>& lights
	);

	void PreResize();

	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV
	);

	void Render(Camera* camera);


	bool GetUseRefractionSilhouette();
	bool GetRefractionFromNormalMap();
	float GetIndexOfRefraction();
	float GetRefractionScale();

	void SetUseRefractionSilhouette(bool silhouette);
	void SetRefractionFromNormalMap(bool fromNormals);
	void SetIndexOfRefraction(float index);
	void SetRefractionScale(float scale);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetRenderTargetSRV(RenderTargetType type);

	void CreateRenderTarget(unsigned int width, unsigned int height, Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
	

private:
	void DrawPointLights(Camera* camera);
};

