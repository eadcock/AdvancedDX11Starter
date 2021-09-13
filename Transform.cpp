#include "Transform.h"
#include "GameEntity.h"

using namespace DirectX;

Transform::Transform() {
	// Start with an identity matrix and basic transform data
	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());

	position = XMFLOAT3(0, 0, 0);
	pitchYawRoll = XMFLOAT3(0, 0, 0);
	scale = XMFLOAT3(1, 1, 1);

	parent = nullptr;
	children = std::vector<Transform*>();

	// No need to recalc yet
	matricesDirty = false;
}

Transform::Transform(GameEntity* attached)
{
	this->attachedEntity = attached;
	Transform();
}

void Transform::MoveAbsolute(float x, float y, float z)
{
	position.x += x;
	position.y += y;
	position.z += z;
	MarkChildTransformDirty();
}

void Transform::MoveRelative(float x, float y, float z)
{
	// Create a direction vector from the params
	// and a rotation quaternion
	XMVECTOR movement = XMVectorSet(x, y, z, 0);
	XMVECTOR rotQuat = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));

	// Rotate the movement by the quaternion
	XMVECTOR dir = XMVector3Rotate(movement, rotQuat);

	// Add and store, and invalidate the matrices
	XMStoreFloat3(&position, XMLoadFloat3(&position) + dir);
	MarkChildTransformDirty();
}

void Transform::Rotate(float p, float y, float r)
{
	pitchYawRoll.x += p;
	pitchYawRoll.y += y;
	pitchYawRoll.z += r;
	MarkChildTransformDirty();
}

void Transform::Scale(float x, float y, float z)
{
	scale.x *= x;
	scale.y *= y;
	scale.z *= z;
	MarkChildTransformDirty();
}

void Transform::SetPosition(float x, float y, float z)
{
	position.x = x;
	position.y = y;
	position.z = z;
	MarkChildTransformDirty();
}

void Transform::SetRotation(float p, float y, float r)
{
	pitchYawRoll.x = p;
	pitchYawRoll.y = y;
	pitchYawRoll.z = r;
	MarkChildTransformDirty();
}

void Transform::SetScale(float x, float y, float z)
{
	scale.x = x;
	scale.y = y;
	scale.z = z;
	MarkChildTransformDirty();
}

DirectX::XMFLOAT3 Transform::GetPosition() { return position; }

DirectX::XMFLOAT3 Transform::GetPitchYawRoll() { return pitchYawRoll; }

DirectX::XMFLOAT3 Transform::GetScale() { return scale; }


DirectX::XMFLOAT4X4 Transform::GetWorldMatrix()
{
	UpdateMatrices();
	return worldMatrix;
}

DirectX::XMFLOAT4X4 Transform::GetWorldInverseTransposeMatrix()
{
	UpdateMatrices();
	return worldInverseTransposeMatrix;
}

// Thanks stack overflow
void ExtractPitchYawRollFromXMMatrix(float* pitchOut, float* yawOut, float* rollOut, XMMATRIX m) {
	XMFLOAT4X4 values;
	XMStoreFloat4x4(&values, XMMatrixTranspose(m));
	*pitchOut = (float)asin(-values._23);
	*yawOut = (float)atan2(values._13, values._33);
	*rollOut = (float)atan2(values._21, values._22);
}

void Transform::CorrectPosition() {
	XMFLOAT4X4 f_wm = parent->GetWorldMatrix();
	XMMATRIX wm = XMLoadFloat4x4(&f_wm);
	wm = XMMatrixInverse(nullptr, wm);
	XMVECTOR iTrans, iScale, iRot;
	XMMatrixDecompose(&iScale, &iRot, &iTrans, wm); 

	XMVECTOR v_scale = XMLoadFloat3(&scale);
	v_scale *= iScale;

	XMVECTOR v_trans = XMLoadFloat3(&position);
	v_trans = v_trans * iScale + iTrans;

	XMVECTOR v_rot = XMLoadFloat3(&pitchYawRoll);
	v_rot += iRot;

	XMStoreFloat3(&scale, v_scale);
	XMStoreFloat3(&pitchYawRoll, v_rot);
	XMStoreFloat3(&position, v_trans);
}

void Transform::AddChild(Transform* child)
{
	if (child != nullptr && IndexOfChild(child) == -1) {
		children.push_back(child);

		child->parent = this;

		child->CorrectPosition();

		child->MarkChildTransformDirty();
	}
}

void Transform::RemoveChild(Transform* child)
{
	if (child != nullptr && IndexOfChild(child) > 0) {
		child->parent = nullptr;

		children.erase(children.begin() + IndexOfChild(child));

		child->MarkChildTransformDirty();
	}
}

Transform* Transform::GetChild(unsigned int index)
{
	if (index < children.size()) {
		return children[index];
	}
	return 0;
}

int Transform::IndexOfChild(Transform* child)
{
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child) {
			return i;
		}
	}
	return -1;
}

unsigned int Transform::GetChildCount()
{
	return children.size();
}

Transform* Transform::GetParent()
{
	return this->parent;
}

void Transform::SetParent(Transform* newParent)
{
	if (newParent != nullptr) {
		if (parent != nullptr) {
			parent->RemoveChild(this);
		}
		parent = newParent;
		parent->AddChild(this);
	}
}

void Transform::UpdateMatrices()
{
	// Are the matrices out of date (dirty)?
	if (matricesDirty)
	{
		// Create the three transformation pieces
		XMMATRIX trans = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
		XMMATRIX rot = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
		XMMATRIX sc = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

		// Combine and store the world
		XMMATRIX wm = sc * rot * trans;
		if (parent != nullptr) {
			XMFLOAT4X4 pm = parent->GetWorldMatrix();
			wm *= XMLoadFloat4x4(&pm);
		}
		XMStoreFloat4x4(&worldMatrix, wm);

		// Invert and transpose, too
		XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(wm)));

		// All set
		matricesDirty = false;
	}
}

void Transform::MarkChildTransformDirty()
{
	matricesDirty = true;
	for (auto& c : children) {
		c->MarkChildTransformDirty();
	}
}

GameEntity* Transform::GetAttachedEntity()
{
	return attachedEntity;
}
