#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

#include "Vertex.h"


class Mesh
{
public:
	Mesh(std::string name, Vertex* vertArray, int numVerts, unsigned int* indexArray, int numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device);
	Mesh(std::string name, const char* objFile, Microsoft::WRL::ComPtr<ID3D11Device> device);
	~Mesh(void);

	Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() { return vb; }
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() { return ib; }
	int GetIndexCount() { return numIndices; }

	void SetBuffersAndDraw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	std::string name;

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
	int numIndices;

	void CreateBuffers(Vertex* vertArray, int numVerts, unsigned int* indexArray, int numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device);
	void CalculateTangents(Vertex* verts, int numVerts, unsigned int* indices, int numIndices);

};

