
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "AssetManager.h"

#include "WICTextureLoader.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str(), 0, srv.GetAddressOf())
			

constexpr int GCD(int a, int b) { return b == 0 ? a : GCD(b, a % b); }


// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	camera = 0;

	// Seed random
	srand((unsigned int)time(0));

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object

	// Delete any one-off objects
	delete sky;
	delete camera;
	delete arial;
	delete spriteBatch;
	delete renderer;

	// Delete singletons
	delete& Input::GetInstance();
	delete& AssetManager::GetInstance();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize the input manager with the window's handle
	Input::GetInstance().Initialize(this->hWnd);
	AssetManager::GetInstance().Initialize(GetExePath(), GetExePath_Wide(), device, context);

	// Asset loading and entity creation
	AssetManager::GetInstance().Load();
	LoadAssetsAndCreateEntities();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set up lights initially
	lightCount = 64;
	GenerateLights();

	// Make our camera
	camera = new Camera(
		0, 0, -10,	// Position
		3.0f,		// Move speed
		1.0f,		// Mouse look
		this->width / (float)this->height); // Aspect ratio

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Pick a style
	ImGui::StyleColorsDark();

	// Setup Platform/renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());

	renderer = new Renderer(device, context, swapChain, backBufferRTV, depthStencilView, width, height, sky, lights);
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Set up the sprite batch and load the sprite font
	spriteBatch = new SpriteBatch(context.Get());
	arial = new SpriteFont(device.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/arial.spritefont").c_str());


	// Create the sky using a DDS cube map
	/*sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\SunnyCubeMap.dds").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context);*/

	AssetManager& assets = AssetManager::GetInstance();

	// Create the sky using 6 images
	sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\right.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\left.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\up.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\down.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\front.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\back.png").c_str(),
		assets.GetMesh("cube"),
		assets.GetVertexShader("SkyVS"),
		assets.GetPixelShader("SkyPS"),
		assets.samplerOptions,
		device,
		context);


	// Save assets needed for drawing point lights
	// (Since these are just copies of the pointers,
	//  we won't need to directly delete them as 
	//  the original pointers will be cleaned up)
	lightMesh = assets.GetMesh("sphere");
	lightVS = assets.GetVertexShader("VertexShader");
	lightPS = assets.GetPixelShader("SolidColorPS");
}


// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < lightCount)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update our projection matrix to match the new aspect ratio
	if (camera)
		camera->UpdateProjectionMatrix(this->width / (float)this->height);

	renderer->PostResize(width, height, backBufferRTV, depthStencilView);
}

void DisplayTransformData(Transform* t) {
	XMFLOAT3 pos = t->GetPosition();
	ImGui::DragFloat3("Position", &pos.x);
	t->SetPosition(pos.x, pos.y, pos.z);

	XMFLOAT3 pyr = t->GetPitchYawRoll();
	ImGui::DragFloat3("Pitch/Yaw/Roll", &pyr.x);
	t->SetRotation(pyr.x, pyr.y, pyr.z);

	XMFLOAT3 scale = t->GetScale();
	ImGui::DragFloat3("Scale", &scale.x);
	t->SetScale(scale.x, scale.y, scale.z);

	t->MarkChildTransformDirty();

	XMFLOAT4X4 worldMatrix = t->GetWorldMatrix();
	ImGui::BulletText("World Matrix:");
	if (ImGui::BeginTable("World Table", 4, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoHostExtendX)) {
		for (int row = 0; row < 4; row++) {
			ImGui::TableNextRow();
			for (int column = 0; column < 4; column++) {
				ImGui::TableSetColumnIndex(column);
				ImGui::Text("[%d,%d] %.2f", column, row, worldMatrix.m[column][row]);
			}
		}
		ImGui::EndTable();
	}

	ImGui::Separator();

	// Parent and Child panels can be duplicated, which means ImGui ids are duplicated. 
	// It currently runs fine, but should figure a way to make ids unique.

	// Can't use name
	// Don't know how deep in the tree we are -- would have to make a variables that stores what has been displayed. Seems messy/bloated.
	// Could just randomly generate a number

	Transform* parent = t->GetParent();
	if (parent != nullptr) {
		std::string parent_label = "Parent - " + parent->GetAttachedEntity()->GetName();
		if (ImGui::TreeNode(parent_label.c_str())) {
			DisplayTransformData(parent);

			ImGui::TreePop();
			ImGui::Separator();
		}
	}

	unsigned int child_count = t->GetChildCount();
	if (child_count > 0) {
		std::string group_label = "Children: " + std::to_string(child_count);
		if (ImGui::TreeNode(group_label.c_str())) {
			for (unsigned int i = 0; i < child_count; i++) {
				Transform* child = t->GetChild(i);
				std::string label = "Child " + std::to_string(i) + " - " + child->GetAttachedEntity()->GetName();
				if (ImGui::TreeNode(label.c_str())) {
					DisplayTransformData(child);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
			ImGui::TreePop();
			ImGui::Separator();
		}
	}
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Update the camera
	camera->Update(deltaTime);

	float wave = sinf(totalTime);

	AssetManager& assets = AssetManager::GetInstance();
	auto entities = AssetManager::GetInstance().GetEntities();
	
	entities["cobSpherePBR"]->GetTransform()->Rotate(0, 0.01f, 0);
	entities["floorSpherePBR"]->GetTransform()->Rotate(0.01f, 0, 0);
	entities["floorSpherePBR"]->GetTransform()->SetScale(1 + wave / 2, 1 + wave / 2, 1 + wave / 2);
	entities["paintSpherePBR"]->GetTransform()->Rotate(0, 0, 0.01f);
	entities["bronzeSpherePBR"]->GetTransform()->Rotate(0, -0.01f, 0);

	entities["cobSphere"]->GetTransform()->SetPosition(2 + wave * 2, 2 + wave * 2, 2 + wave * 2);

	// Check individual input
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();

	// Reset input manager's gui state so we don't taint our own input
	input.SetGuiKeyboardCapture(false);
	input.SetGuiMouseCapture(false);

	// Set io info
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->width;
	io.DisplaySize.y = (float)this->height;
	io.KeyCtrl = input.KeyDown(VK_CONTROL);
	io.KeyShift = input.KeyDown(VK_SHIFT);
	io.KeyAlt = input.KeyDown(VK_MENU);
	io.MousePos.x = (float)input.GetMouseX();
	io.MousePos.y = (float)input.GetMouseY();
	io.MouseDown[0] = input.MouseLeftDown();
	io.MouseDown[1] = input.MouseRightDown();
	io.MouseDown[2] = input.MouseMiddleDown();
	io.MouseWheel = input.GetMouseWheel();
	input.GetKeyArray(io.KeysDown, 256);

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	input.SetGuiKeyboardCapture(io.WantCaptureKeyboard);
	input.SetGuiMouseCapture(io.WantCaptureMouse);

	// ImGui::ShowDemoWindow();

	ImGui::Begin("Config");
	if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("FPS: %i", (int)io.Framerate);
		if (ImGui::TreeNode("Window Size")) {
			ImGui::BulletText("Width: %d", this->width);
			ImGui::BulletText("Height: %d", this->height);
			const int gcd = GCD(this->width, this->height);
			ImGui::BulletText("Aspect Ratio: %d:%d (%f)", this->width / gcd, this->height / gcd, this->width / (float)this->height);
			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader("Scene Info", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::CollapsingHeader("Entities")) {
			ImGui::Text("Amount: %d", assets.GetEntities().size());
			static std::string current_index = "";
			GameEntity* current_entity = assets.GetEntity(current_index);
			if (ImGui::BeginCombo("EntitySelect", current_index.c_str())) {
				for (auto& p : assets.GetEntities()) {
					const bool isSelected = (current_index == p.first);
					if (ImGui::Selectable(p.first.c_str(), isSelected)) {
						current_index = p.first;
						current_entity = p.second;
					}

					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::TreeNode("Transform")) {
				DisplayTransformData(current_entity->GetTransform());

				ImGui::Separator();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Material##Entity")) {
				Material* m = current_entity->GetMaterial();
				TextureBundle* textures = m->GetSRVs();

				static std::string current_index_tex = textures->name;
				TextureBundle* current_texture = assets.GetBundle(textures->name);
				std::string preview = current_texture == nullptr ? "Custom" : current_texture->name;
				if (ImGui::BeginCombo("Texture Group", preview.c_str())) {
					bool inList = false;
					for (auto& p : AssetManager::GetInstance().GetBundles()) {
						const bool isSelected = (current_index_tex == p.first);
						inList = inList || isSelected;
						if (ImGui::Selectable(p.first.c_str(), isSelected)) {
							current_index_tex = p.first;
							m->SetSRVs(p.second);
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::Selectable("Custom##Entity", !inList);

					ImGui::EndCombo();
				}

				if (ImGui::TreeNode("Albedo##Entity")) {
					//ImGui::Button("Change##AlbedoTexture");
					ImGui::Image(textures->albedo.Get(), ImVec2(200, 200));

					ImGui::Separator();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Normal##Entity")) {
					//ImGui::Button("Change##NormalTexture");
					ImGui::Image(textures->normal.Get(), ImVec2(200, 200));

					ImGui::Separator();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Roughness##Entity")) {
					//ImGui::Button("Change##RoughnessTexture");
					ImGui::Image(textures->roughness.Get(), ImVec2(200, 200));

					ImGui::Separator();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Metalness##Entity")) {
					//ImGui::Button("Change##MetalnessTexture");
					ImGui::Image(textures->metalness.Get(), ImVec2(200, 200));

					ImGui::Separator();
					ImGui::TreePop();
				}

				ImGui::Separator();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Mesh##Entity")) {
				Mesh* m = current_entity->GetMesh();
				static std::string current_mesh = assets.GetMesh(m->name) != nullptr ? m->name : std::string();
				if (ImGui::BeginCombo("Mesh##EntityLabel", current_mesh.c_str())) {
					for (const auto& p : assets.GetMeshes()) {
						const bool isSelected = (current_mesh == p.first);
						if (ImGui::Selectable(p.first.c_str(), isSelected)) {
							current_mesh = p.first;
							current_entity->SetMesh(p.second);
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}
				ImGui::TreePop();

				ImGui::Text("\tIndex Count: %d", m->GetIndexCount());
			}
		}

		if (ImGui::CollapsingHeader("Lights")) {
			ImGui::Text("Amount: %d", this->lightCount);
			for (int i = 0; i < this->lightCount; i++) {
				Light l = this->lights[i];
				int lightType = this->lights[i].Type;
				const std::string str_label = "Light " + std::to_string(i);
				if (ImGui::TreeNode(str_label.c_str())) {
					const std::string str_barLabel = "LightInfo##" + str_label;
					if (ImGui::BeginTabBar(str_barLabel.c_str())) {
						const std::string str_overviewLabel = "Overview##" + str_label;
						if (ImGui::BeginTabItem(str_overviewLabel.c_str())) {
							const int types[] = { LIGHT_TYPE_DIRECTIONAL, LIGHT_TYPE_POINT, LIGHT_TYPE_SPOT };
							static int cur_type_idx = 0;
							const std::string str_typeLabel = "Type##" + std::to_string(i);
							if (ImGui::BeginCombo(str_typeLabel.c_str(), TypeToString(types[cur_type_idx]))) {
								for (int n = 0; n < IM_ARRAYSIZE(types); n++) {
									const bool isSelected = (cur_type_idx == n);
									if (ImGui::Selectable(TypeToString(types[n]), isSelected)) {
										cur_type_idx = n;
										this->lights[i].Type = types[n];
									}

									if (isSelected) {
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}

							const std::string str_intensityLabel = "Intensity##" + str_label;
							ImGui::SliderFloat(str_intensityLabel.c_str(), &this->lights[i].Intensity, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

							if (this->lights[i].Type != LIGHT_TYPE_DIRECTIONAL) {
								const std::string str_rangeLabel = "Range##" + str_label;
								ImGui::SliderFloat(str_rangeLabel.c_str(), &this->lights[i].Range, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
							}

							if (lightType == LIGHT_TYPE_SPOT) {
								const std::string str_falloffLabel = "Spot Falloff##" + str_label;
								ImGui::SliderFloat(str_falloffLabel.c_str(), &this->lights[i].SpotFalloff, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
							}

							ImGui::EndTabItem();
						}
						const std::string str_orientationLabel = "Orientation##" + str_label;
						if (ImGui::BeginTabItem(str_orientationLabel.c_str())) {
							if (lightType != LIGHT_TYPE_POINT) {
								const std::string str_directionLabel = "Direction##" + str_label;
								ImGui::DragFloat3(str_directionLabel.c_str(), &this->lights[i].Direction.x);
							}

							if (lightType != LIGHT_TYPE_DIRECTIONAL) {
								const std::string str_positionLabel = "Position##" + str_label;
								ImGui::DragFloat3(str_positionLabel.c_str(), &this->lights[i].Position.x);
							}

							ImGui::EndTabItem();
						}

						const std::string str_colorLabel = "Color##" + str_label;
						if (ImGui::BeginTabItem(str_colorLabel.c_str())) {
							ImGui::ColorPicker3(str_colorLabel.c_str(), &this->lights[i].Color.x);

							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
					ImGui::Separator();
					ImGui::TreePop();
				}
			}
		}
	}

	ImGui::End();

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	renderer->Render(camera);
}
