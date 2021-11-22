#include "Emitter.h"

Emitter::Emitter(int maxParticles, int particlesPerSecond, float lifetime, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, SimpleVertexShader* vs, SimplePixelShader* ps, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture)
	: maxParticles(maxParticles), particlesPerSecond(particlesPerSecond), lifetime(lifetime), context(context), vs(vs), ps(ps), texture(texture)
{
	secondsPerParticle = 1.0f / particlesPerSecond;
	timeSinceEmitted = 0.0f;
	livingParticleCount = 0;
	indexFirstAlive = 0;
	indexFirstDead = 0;

	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	unsigned int* indices = new unsigned int[maxParticles * 6];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	// Regular (static) index buffer
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
	delete[] indices; // Sent to GPU already

	// Make a dynamic buffer to hold all particle data on GPU
	// Note: We'll be overwriting this every frame with new lifetime data
	D3D11_BUFFER_DESC allParticleBufferDesc = {};
	allParticleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	allParticleBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	allParticleBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	allParticleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	allParticleBufferDesc.StructureByteStride = sizeof(Particle);
	allParticleBufferDesc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&allParticleBufferDesc, 0, particleDataBuffer.GetAddressOf());

	// Create an SRV that points to a structured buffer of particles
	// so we can grab this data in a vertex shader
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleDataBuffer.Get(), &srvDesc, particleDataSRV.GetAddressOf());
}

Emitter::~Emitter()
{
	delete[] particles;
}

void Emitter::Update(float dt, float currentTime)
{
	if (livingParticleCount > 0) {
		if (indexFirstAlive < indexFirstDead) {
			for (int i = indexFirstAlive; i < indexFirstDead; i++) {
				UpdateSingleParticle(currentTime, i);
			}
		}
		else if (indexFirstDead < indexFirstAlive) {
			for (int i = indexFirstAlive; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);

			for (int i = 0; i < indexFirstDead; i++)
				UpdateSingleParticle(currentTime, i);
		}
		else {
			for (int i = 0; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);
		}
	}
	timeSinceEmitted += dt;

	while (timeSinceEmitted > secondsPerParticle)
	{
		EmitParticle(currentTime);
		timeSinceEmitted -= secondsPerParticle;
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	
	if (indexFirstAlive < indexFirstDead)
	{
		// Only copy from FirstAlive -> FirstDead
		memcpy(
			mapped.pData, // Destination = start of particle buffer
			particles + indexFirstAlive, // Source = particle array, offset to first living particle
			sizeof(Particle) * livingParticleCount); // Amount = number of particles (measured in BYTES!)
	}
	else
	{
		// Copy from 0 -> FirstDead 
		memcpy(
			mapped.pData, // Destination = start of particle buffer
			particles, // Source = start of particle array
			sizeof(Particle) * indexFirstDead); // Amount = particles up to first dead (measured in BYTES!)

		// ALSO copy from FirstAlive -> End
		memcpy(
			(void*)((Particle*)mapped.pData + indexFirstDead), // Destination = particle buffer, AFTER the data we copied in previous memcpy()
			particles + indexFirstAlive,  // Source = particle array, offset to first living particle
			sizeof(Particle) * (maxParticles - indexFirstAlive)); // Amount = number of living particles at end of array (measured in BYTES!)
	}

	// Unmap now that we're done copying
	context->Unmap(particleDataBuffer.Get(), 0);
}

void Emitter::Draw(Camera* camera, float currentTime)
{
	// Set up buffers - note that we're NOT using a vertex buffer!
	// When we draw, we'll calculate the number of vertices we expect
	// to have given how many particles are currently alive.  We'll
	// construct the actual vertex data on the fly in the shader.
	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* nullBuffer = 0;
	context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Shader setup
	vs->SetShader();
	ps->SetShader();

	// SRVs - Particle data in VS and texture in PS
	vs->SetShaderResourceView("ParticleData", particleDataSRV);
	ps->SetShaderResourceView("Texture", texture);

	// Vertex data
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat("currentTime", currentTime);
	vs->CopyAllBufferData();

	// Now that all of our data is in the beginning of the particle buffer,
	// we can simply draw the correct amount of living particle indices.
	// Each particle = 4 vertices = 6 indices for a quad
	context->DrawIndexed(livingParticleCount * 6, 0, 0);
}

void Emitter::UpdateSingleParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;

	// Update and check for death
	if (age >= lifetime)
	{
		// Recent death, so retire by moving alive count (and wrap)
		indexFirstAlive++;
		indexFirstAlive %= maxParticles;
		livingParticleCount--;
	}
}

void Emitter::EmitParticle(float currentTime)
{
	// Any left to spawn?
	if (livingParticleCount == maxParticles)
		return;

	// Which particle is spawning?
	int spawnedIndex = indexFirstDead;

	// Update the spawn time of the first dead particle
	particles[spawnedIndex].EmitTime = currentTime;
	particles[spawnedIndex].StartPosition = DirectX::XMFLOAT3(0, 0, 0);

	// Here is where you could make particle spawning more interesting
	// by adjusting the starting position and other starting values
	// using random numbers.

	// Increment the first dead particle (since it's now alive)
	indexFirstDead++;
	indexFirstDead %= maxParticles; // Wrap

	// One more living particle
	livingParticleCount++;
}
