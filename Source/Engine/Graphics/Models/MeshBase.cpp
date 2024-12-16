// Copyright (c) 2012-2024 Wojciech Figat. All rights reserved.

#include "MeshBase.h"
#include "Engine/Core/Log.h"
#include "Engine/Content/Assets/ModelBase.h"
#include "Engine/Core/Math/Transform.h"
#include "Engine/Graphics/GPUBuffer.h"
#include "Engine/Graphics/GPUContext.h"
#include "Engine/Graphics/GPUDevice.h"
#include "Engine/Renderer/DrawCall.h"
#include "Engine/Scripting/ManagedCLR/MCore.h"

namespace
{
#if !COMPILE_WITHOUT_CSHARP
    template<typename IndexType>
    bool UpdateTriangles(MeshBase* mesh, int32 triangleCount, const MArray* trianglesObj)
    {
        const auto model = mesh->GetModelBase();
        ASSERT(model && model->IsVirtual() && trianglesObj);

        // Get buffer data
        ASSERT(MCore::Array::GetLength(trianglesObj) / 3 >= triangleCount);
        auto ib = MCore::Array::GetAddress<IndexType>(trianglesObj);

        return mesh->UpdateTriangles(triangleCount, ib);
    }
#endif
}

MeshBase::~MeshBase()
{
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[0]);
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[1]);
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[2]);
    SAFE_DELETE_GPU_RESOURCE(_indexBuffer);
}

bool MeshBase::HasVertexColors() const
{
    return _vertexBuffers[2] != nullptr && _vertexBuffers[2]->IsAllocated();
}

void MeshBase::SetMaterialSlotIndex(int32 value)
{
    if (value < 0 || value >= _model->MaterialSlots.Count())
    {
        LOG(Warning, "Cannot set mesh material slot to {0} while model has {1} slots.", value, _model->MaterialSlots.Count());
        return;
    }

    _materialSlotIndex = value;
}

void MeshBase::SetBounds(const BoundingBox& box)
{
    _box = box;
    BoundingSphere::FromBox(box, _sphere);
}

void MeshBase::Unload()
{
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[0]);
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[1]);
    SAFE_DELETE_GPU_RESOURCE(_vertexBuffers[2]);
    SAFE_DELETE_GPU_RESOURCE(_indexBuffer);
    _triangles = 0;
    _vertices = 0;
    _use16BitIndexBuffer = false;
    _cachedIndexBuffer.Resize(0);
    _cachedVertexBuffers[0].Clear();
    _cachedVertexBuffers[1].Clear();
    _cachedVertexBuffers[2].Clear();
}

bool MeshBase::UpdateTriangles(uint32 triangleCount, const void* ib, bool use16BitIndices)
{
    uint32 indicesCount = triangleCount * 3;
    uint32 ibStride = use16BitIndices ? sizeof(uint16) : sizeof(uint32);
    if (!_indexBuffer)
        _indexBuffer = GPUDevice::Instance->CreateBuffer(TEXT("DynamicMesh.IB"));
    if (_indexBuffer->Init(GPUBufferDescription::Index(ibStride, indicesCount, ib)))
    {
        _triangles = 0;
        return true;
    }

    // TODO: update collision proxy

    _triangles = triangleCount;
    _use16BitIndexBuffer = use16BitIndices;
    return false;
}

bool MeshBase::Intersects(const Ray& ray, const Matrix& world, Real& distance, Vector3& normal) const
{
    // Get bounding box of the mesh bounds transformed by the instance world matrix
    Vector3 corners[8];
    _box.GetCorners(corners);
    Vector3 tmp;
    Vector3::Transform(corners[0], world, tmp);
    Vector3 min = tmp;
    Vector3 max = tmp;
    for (int32 i = 1; i < 8; i++)
    {
        Vector3::Transform(corners[i], world, tmp);
        min = Vector3::Min(min, tmp);
        max = Vector3::Max(max, tmp);
    }
    const BoundingBox transformedBox(min, max);

    // Test ray on box
#if USE_PRECISE_MESH_INTERSECTS
    if (transformedBox.Intersects(ray, distance))
    {
        // Use exact test on raw geometry
        return _collisionProxy.Intersects(ray, world, distance, normal);
    }
    distance = 0;
    normal = Vector3::Up;
    return false;
#else
    return transformedBox.Intersects(ray, distance, normal);
#endif
}

bool MeshBase::Intersects(const Ray& ray, const Transform& transform, Real& distance, Vector3& normal) const
{
    // Get bounding box of the mesh bounds transformed by the instance world matrix
    Vector3 corners[8];
    _box.GetCorners(corners);
    Vector3 tmp;
    transform.LocalToWorld(corners[0], tmp);
    Vector3 min = tmp;
    Vector3 max = tmp;
    for (int32 i = 1; i < 8; i++)
    {
        transform.LocalToWorld(corners[i], tmp);
        min = Vector3::Min(min, tmp);
        max = Vector3::Max(max, tmp);
    }
    const BoundingBox transformedBox(min, max);

    // Test ray on box
#if USE_PRECISE_MESH_INTERSECTS
    if (transformedBox.Intersects(ray, distance))
    {
        // Use exact test on raw geometry
        return _collisionProxy.Intersects(ray, transform, distance, normal);
    }
    distance = 0;
    normal = Vector3::Up;
    return false;
#else
    return transformedBox.Intersects(ray, distance, normal);
#endif
}

bool MeshBase::DownloadDataGPU(MeshBufferType type, BytesContainer& result) const
{
    GPUBuffer* buffer = nullptr;
    switch (type)
    {
    case MeshBufferType::Index:
        buffer = _indexBuffer;
        break;
    case MeshBufferType::Vertex0:
        buffer = _vertexBuffers[0];
        break;
    case MeshBufferType::Vertex1:
        buffer = _vertexBuffers[1];
        break;
    case MeshBufferType::Vertex2:
        buffer = _vertexBuffers[2];
        break;
    }
    return buffer && buffer->DownloadData(result);
}

Task* MeshBase::DownloadDataGPUAsync(MeshBufferType type, BytesContainer& result) const
{
    GPUBuffer* buffer = nullptr;
    switch (type)
    {
    case MeshBufferType::Index:
        buffer = _indexBuffer;
        break;
    case MeshBufferType::Vertex0:
        buffer = _vertexBuffers[0];
        break;
    case MeshBufferType::Vertex1:
        buffer = _vertexBuffers[1];
        break;
    case MeshBufferType::Vertex2:
        buffer = _vertexBuffers[2];
        break;
    }
    return buffer ? buffer->DownloadDataAsync(result) : nullptr;
}

void MeshBase::GetDrawCallGeometry(DrawCall& drawCall) const
{
    drawCall.Geometry.IndexBuffer = _indexBuffer;
    drawCall.Geometry.VertexBuffers[0] = _vertexBuffers[0];
    drawCall.Geometry.VertexBuffers[1] = _vertexBuffers[1];
    drawCall.Geometry.VertexBuffers[2] = _vertexBuffers[2];
    drawCall.Geometry.VertexBuffersOffsets[0] = 0;
    drawCall.Geometry.VertexBuffersOffsets[1] = 0;
    drawCall.Geometry.VertexBuffersOffsets[2] = 0;
    drawCall.Draw.StartIndex = 0;
    drawCall.Draw.IndicesCount = _triangles * 3;
}

void MeshBase::Render(GPUContext* context) const
{
    if (!IsInitialized())
        return;
    context->BindVB(ToSpan(_vertexBuffers, 3));
    context->BindIB(_indexBuffer);
    context->DrawIndexed(_triangles * 3);
}

ScriptingObject* MeshBase::GetParentModel() const
{
    return _model;
}

#if !COMPILE_WITHOUT_CSHARP

bool MeshBase::UpdateTrianglesUInt(int32 triangleCount, const MArray* trianglesObj)
{
    return ::UpdateTriangles<uint32>(this, triangleCount, trianglesObj);
}

bool MeshBase::UpdateTrianglesUShort(int32 triangleCount, const MArray* trianglesObj)
{
    return ::UpdateTriangles<uint16>(this, triangleCount, trianglesObj);
}

#endif
