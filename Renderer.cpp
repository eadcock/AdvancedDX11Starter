#include "Renderer.h"
#include "AssetManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

Renderer::Renderer(Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain, 
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV, 
	unsigned int windowWidth, unsigned int windowHeight, 
	std::vector<Light>& lights) : lights(lights),
	refractionScale(0.1f),
	useRefractionSilhouette(false),
	refractionFromNormalMap(true),
	indexOfRefraction(0.5f)
{
	this->device = device;
	this->context = context;
	this->swapChain = swapChain;
	this->backBufferRTV = backBufferRTV;
	this->depthBufferDSV = depthBufferDSV;
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	// Initialize structs
	vsPerFrameData = {};
	psPerFrameData = {};

	// Grab two shaders on which to base per-frame cbuffers
	// Note: We're assuming ALL entity/material per-frame buffers are identical!
	//       And that they're all called "perFrame"
	AssetManager& assets = AssetManager::GetInstance();
	SimplePixelShader* ps = assets.GetPixelShader("PixelShaderPBR");
	SimpleVertexShader* vs = assets.GetVertexShader("VertexShader");

	// Struct to hold the descriptions from existing buffers
	D3D11_BUFFER_DESC bufferDesc = {};
	const SimpleConstantBuffer* scb = 0;

	// Make a new buffer that matches the existing PS per-frame buffer
	scb = ps->GetBufferInfo("perFrame");
	scb->ConstantBuffer.Get()->GetDesc(&bufferDesc);
	device->CreateBuffer(&bufferDesc, 0, psPerFrameConstantBuffer.GetAddressOf());

	// Make a new buffer that matches the existing PS per-frame buffer
	scb = vs->GetBufferInfo("perFrame");
	scb->ConstantBuffer.Get()->GetDesc(&bufferDesc);
	device->CreateBuffer(&bufferDesc, 0, vsPerFrameConstantBuffer.GetAddressOf());

	// Create render targets (just calling post resize which sets them all up)
	PostResize(windowWidth, windowHeight, backBufferRTV, depthBufferDSV);

	// Depth state for refraction silhouette
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // No depth writing
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&depthDesc, refractionSilhouetteDepthState.GetAddressOf());
}

void Renderer::PreResize()
{
	backBufferRTV.Reset();
	depthBufferDSV.Reset();
}

void Renderer::PostResize(unsigned int windowWidth, unsigned int windowHeight, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV)
{
	this->windowHeight = windowHeight;
	this->windowWidth = windowWidth;
	this->backBufferRTV = backBufferRTV;
	this->depthBufferDSV = depthBufferDSV; 


	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_NORMALS], renderTargetSRVs[RenderTargetType::SCENE_NORMALS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_DEPTHS], renderTargetSRVs[RenderTargetType::SCENE_DEPTHS], DXGI_FORMAT_R32_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::FINAL_COMPOSITE], renderTargetSRVs[RenderTargetType::FINAL_COMPOSITE]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_RESULTS], renderTargetSRVs[RenderTargetType::SSAO_RESULTS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_BLUR], renderTargetSRVs[RenderTargetType::SSAO_BLUR]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::REFRACTION_SILHOUETTE], renderTargetSRVs[RenderTargetType::REFRACTION_SILHOUETTE], DXGI_FORMAT_R8_UNORM);

}

void Renderer::Render(Camera* camera)
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthBufferDSV.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	AssetManager& assets = AssetManager::GetInstance();

	for (auto& rt : renderTargetRTVs) context->ClearRenderTargetView(rt.Get(), color);

	const int numTargets = 4;
	ID3D11RenderTargetView* targets[numTargets] = {};
	targets[0] = renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT].Get();
	targets[1] = renderTargetRTVs[RenderTargetType::SCENE_AMBIENT].Get();
	targets[2] = renderTargetRTVs[RenderTargetType::SCENE_NORMALS].Get();
	targets[3] = renderTargetRTVs[RenderTargetType::SCENE_DEPTHS].Get();
	context->OMSetRenderTargets(numTargets, targets, depthBufferDSV.Get());

	// Collect all per-frame data and copy to GPU
	{
		// vs ----
		vsPerFrameData.ViewMatrix = camera->GetView();
		vsPerFrameData.ProjectionMatrix = camera->GetProjection();
		context->UpdateSubresource(vsPerFrameConstantBuffer.Get(), 0, 0, &vsPerFrameData, 0, 0);

		// ps ----
		memcpy(&psPerFrameData.Lights, &lights[0], sizeof(Light) * lights.size());
		psPerFrameData.LightCount = lights.size();
		psPerFrameData.CameraPosition = camera->GetTransform()->GetPosition();
		psPerFrameData.TotalSpecIBLMipLevels = assets.sky->GetTotalSpecIBLMipLevels();
		context->UpdateSubresource(psPerFrameConstantBuffer.Get(), 0, 0, &psPerFrameData, 0, 0);
	}

	std::vector<GameEntity*> toDraw;
	for (auto& p : assets.GetEntities()) {
		toDraw.push_back(p.second);
	}
	std::sort(toDraw.begin(), toDraw.end(), [](const auto& e1, const auto& e2)
		{
			// Compare pointers to materials
			return e1->GetMaterial() < e2->GetMaterial();
		});

	// Collect all refractive entities for later
	std::vector<GameEntity*> refractiveEntities;

	// Draw all of the entities
	SimpleVertexShader* currentVS = 0;
	SimplePixelShader* currentPS = 0;
	Material* currentMaterial = 0;
	Mesh* currentMesh = 0;
	for (auto& ge : toDraw) {
		// Skip refractive materials for now
		if (ge->GetMaterial()->GetRefractive())
		{
			refractiveEntities.push_back(ge);
			continue;
		}
		// Track the current material and swap as necessary
		// (including swapping shaders)
		if (currentMaterial != ge->GetMaterial())
		{
			currentMaterial = ge->GetMaterial();

			// Swap vertex shader if necessary
			if (currentVS != currentMaterial->GetVS())
			{
				currentVS = currentMaterial->GetVS();
				currentVS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				// Note: Would be nice to have the option
				//       for SimpleShader to NOT auto-bind
				//       cbuffers - might add this feature
				context->VSSetConstantBuffers(0, 1, vsPerFrameConstantBuffer.GetAddressOf());
			}

			// Swap pixel shader if necessary
			if (currentPS != currentMaterial->GetPS())
			{
				currentPS = currentMaterial->GetPS();
				currentPS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				context->PSSetConstantBuffers(0, 1, psPerFrameConstantBuffer.GetAddressOf());

				// Set IBL textures now, too
				currentPS->SetShaderResourceView("IrradianceIBLMap", assets.sky->GetIrradianceIBL());
				currentPS->SetShaderResourceView("SpecularIBLMap", assets.sky->GetSpecularIBL());
				currentPS->SetShaderResourceView("BrdfLookUpMap", assets.sky->GetBrdfLookUpMap());
			}

			// Now that the material is set, we should
			// copy per-material data to its cbuffers
			currentMaterial->SetPerMaterialDataAndResources(true);
		}

		// Also track current mesh
		if (currentMesh != ge->GetMesh())
		{
			currentMesh = ge->GetMesh();

			// Bind new buffers
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			context->IASetVertexBuffers(0, 1, currentMesh->GetVertexBuffer().GetAddressOf(), &stride, &offset);
			context->IASetIndexBuffer(currentMesh->GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		}


		// Handle per-object data last (only VS at the moment)
		if (currentVS != 0)
		{
			Transform* trans = ge->GetTransform();
			currentVS->SetMatrix4x4("world", trans->GetWorldMatrix());
			currentVS->SetMatrix4x4("worldInverseTranspose", trans->GetWorldInverseTransposeMatrix());
			currentVS->CopyBufferData("perObject");
		}

		// Draw the entity
		if (currentMesh != 0)
		{
			context->DrawIndexed(currentMesh->GetIndexCount(), 0, 0);
		}
	}

	// Draw the sky
	assets.sky->Draw(camera);

	// Now refraction (if necessary)
	{
		// Loop and render the refractive objects to the silhouette texture (if use silhouettes)
		if (useRefractionSilhouette)
		{
			targets[0] = renderTargetRTVs[RenderTargetType::REFRACTION_SILHOUETTE].Get();
			context->OMSetRenderTargets(1, targets, depthBufferDSV.Get());

			// Depth state
			context->OMSetDepthStencilState(refractionSilhouetteDepthState.Get(), 0);

			// Grab the solid color shader
			SimplePixelShader* solidColorPS = assets.GetPixelShader("SolidColorPS");

			// Loop and draw each one
			for (auto ge : refractiveEntities)
			{
				// Get this material and sub the refraction PS for now
				Material* mat = ge->GetMaterial();
				SimplePixelShader* prevPS = mat->GetPS();
				mat->SetPS(solidColorPS);

				// Overall material prep
				mat->PrepareMaterial(ge->GetTransform(), camera);
				mat->SetPerMaterialDataAndResources(true);

				// Set up the refraction specific data
				solidColorPS->SetFloat3("Color", XMFLOAT3(1, 1, 1));
				solidColorPS->CopyBufferData("externalData");

				// Reset "per frame" buffer for VS
				context->VSSetConstantBuffers(0, 1, vsPerFrameConstantBuffer.GetAddressOf());

				// Draw
				ge->GetMesh()->SetBuffersAndDraw(context);

				// Reset this material's PS
				mat->SetPS(prevPS);
			}

			// Reset depth state
			context->OMSetDepthStencilState(0, 0);
		}

		// Loop and draw refractive objects
		{
			// Set up pipeline for refractive draw
			// Same target (back buffer), but now we need the depth buffer again
			targets[0] = backBufferRTV.Get();
			context->OMSetRenderTargets(1, targets, depthBufferDSV.Get());

			// Grab the refractive shader
			SimplePixelShader* refractionPS = assets.GetPixelShader("RefractionPS");

			// Loop and draw each one
			for (auto ge : refractiveEntities)
			{
				// Get this material and sub the refraction PS for now
				Material* mat = ge->GetMaterial();
				SimplePixelShader* prevPS = mat->GetPS();
				mat->SetPS(refractionPS);

				// Overall material prep
				mat->PrepareMaterial(ge->GetTransform(), camera);
				mat->SetPerMaterialDataAndResources(true);

				// Set up the refraction specific data
				refractionPS->SetFloat2("screenSize", XMFLOAT2((float)windowWidth, (float)windowHeight));
				refractionPS->SetMatrix4x4("viewMatrix", camera->GetView());
				refractionPS->SetMatrix4x4("projMatrix", camera->GetProjection());
				refractionPS->SetInt("useRefractionSilhouette", useRefractionSilhouette);
				refractionPS->SetInt("refractionFromNormalMap", refractionFromNormalMap);
				refractionPS->SetFloat("indexOfRefraction", indexOfRefraction);
				refractionPS->SetFloat("refractionScale", refractionScale);
				refractionPS->CopyBufferData("perObject");

				// Set textures
				refractionPS->SetShaderResourceView("ScreenPixels", renderTargetSRVs[RenderTargetType::FINAL_COMPOSITE].Get());
				refractionPS->SetShaderResourceView("RefractionSilhouette", renderTargetSRVs[RenderTargetType::REFRACTION_SILHOUETTE].Get());
				refractionPS->SetShaderResourceView("EnvironmentMap", assets.sky->GetEnvironmentSRV());


				// Reset "per frame" buffers
				context->VSSetConstantBuffers(0, 1, vsPerFrameConstantBuffer.GetAddressOf());
				context->PSSetConstantBuffers(0, 1, psPerFrameConstantBuffer.GetAddressOf());

				// Draw
				ge->GetMesh()->SetBuffersAndDraw(context);

				// Reset this material's PS
				mat->SetPS(prevPS);
			}
		}
	}

	// Draw the light sources
	DrawPointLights(camera);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());

	// Unbind all SRVs at the end of the frame so they're not still bound for input
	// when we begin the MRTs of the next frame
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	context->PSSetShaderResources(0, 16, nullSRVs);
}

void Renderer::DrawPointLights(Camera* camera)
{
	AssetManager& assets = AssetManager::GetInstance();
	SimpleVertexShader* lightVS = assets.GetVertexShader("VertexShader");
	SimplePixelShader* lightPS = assets.GetPixelShader("SolidColorPS");

	// Turn on these shaders
	lightVS->SetShader();
	lightPS->SetShader();

	// Set up vertex shader
	lightVS->SetMatrix4x4("view", camera->GetView());
	lightVS->SetMatrix4x4("projection", camera->GetProjection());

	for (const Light& light : lights)
	{
		// Only drawing points, so skip others
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		// (assuming range is between 5 - 10)
		float scale = light.Range / 10.0f;

		// Make the transform for this light
		XMMATRIX rotMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
		XMMATRIX worldMat = scaleMat * rotMat * transMat;

		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTrans;
		XMStoreFloat4x4(&world, worldMat);
		XMStoreFloat4x4(&worldInvTrans, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		// Set up the world matrix for this light
		lightVS->SetMatrix4x4("world", world);
		lightVS->SetMatrix4x4("worldInverseTranspose", worldInvTrans);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		lightPS->SetFloat3("Color", finalColor);

		// Copy data
		lightVS->CopyAllBufferData();
		lightPS->CopyAllBufferData();

		// Draw
		assets.GetMesh("sphere")->SetBuffersAndDraw(context);
	}
}

bool Renderer::GetUseRefractionSilhouette() { return useRefractionSilhouette; }
bool Renderer::GetRefractionFromNormalMap() { return refractionFromNormalMap; }
float Renderer::GetIndexOfRefraction() { return indexOfRefraction; }
float Renderer::GetRefractionScale() { return refractionScale; }

void Renderer::SetUseRefractionSilhouette(bool silhouette) { useRefractionSilhouette = silhouette; }
void Renderer::SetRefractionFromNormalMap(bool fromNormals) { refractionFromNormalMap = fromNormals; }
void Renderer::SetIndexOfRefraction(float index) { indexOfRefraction = index; }
void Renderer::SetRefractionScale(float scale) { refractionScale = scale; }

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRenderTargetSRV(RenderTargetType type)
{
	if (type < 0 || type >= RenderTargetType::RENDER_TARGET_TYPE_COUNT)
		return 0;

	return renderTargetSRVs[type];
}

void Renderer::CreateRenderTarget(
	unsigned int width,
	unsigned int height,
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
	DXGI_FORMAT colorFormat)
{
	// Make the texture
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = colorFormat;
	texDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf());

	// Make the render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0;                             // Which mip are we rendering into?
	rtvDesc.Format = texDesc.Format;                // Same format as texture
	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Create the shader resource view using default options 
	device->CreateShaderResourceView(
		rtTexture.Get(),     // Texture resource itself
		0,                   // Null description = default SRV options
		srv.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>
}