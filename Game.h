#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "SpriteFont.h"
#include "SpriteBatch.h"
#include "Lights.h"
#include "Sky.h"
#include "Renderer.h"

#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>
#include <map>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

private:

	// Input and mesh swapping
	byte keys[256];
	byte prevKeys[256];

	// Keep track of "stuff" to clean up
	Camera* camera;

	// Lights
	std::vector<Light> lights;
	int lightCount;

	// These will be loaded along with other assets and
	// saved to these variables for ease of access
	Mesh* lightMesh;
	SimpleVertexShader* lightVS;
	SimplePixelShader* lightPS;

	// Text & ui
	DirectX::SpriteFont* arial;
	DirectX::SpriteBatch* spriteBatch;

	Renderer* renderer;

	// General helpers for setup and drawing
	void GenerateLights();

	// Initialization helper method
	void LoadAssetsAndCreateEntities();
};

