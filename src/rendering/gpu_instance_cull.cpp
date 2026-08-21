#include "engine/rendering/gpu_instance_cull.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace engine {
namespace {

EngineError cull_error(std::string code, std::string message, HRESULT hr = S_OK) {
    std::vector<std::string> causes;
    if (FAILED(hr)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "HRESULT 0x%08lX", static_cast<unsigned long>(hr));
        causes.push_back(buf);
    }
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Graphics, "gpu-instance-cull",
        std::move(message), ENGINE_SOURCE_CONTEXT, std::move(causes),
        "Check the D3D12 debug layer output and graphics driver.", make_correlation_id()};
}

struct alignas(256) CullConstants {
    float planes[6][4];
    UINT instance_offset = 0;
    UINT instance_count = 0;
    UINT float4s_per_instance = 4;
    float radius = 1.0f;
    UINT max_visible = 0;
    UINT pad0 = 0;
    UINT pad1 = 0;
    UINT pad2 = 0;
};

static_assert(sizeof(CullConstants) == 256, "Cull CB must be 256-byte aligned for D3D12");

} // namespace

Result<void> GpuInstanceCullPass::create_slot_upload_buffers(ID3D12Device* device, FrameSlot& slot) {
    D3D12_HEAP_PROPERTIES upload{};
    upload.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC cb_desc{};
    cb_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cb_desc.Width = sizeof(CullConstants);
    cb_desc.Height = 1;
    cb_desc.DepthOrArraySize = 1;
    cb_desc.MipLevels = 1;
    cb_desc.SampleDesc.Count = 1;
    cb_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    HRESULT hr = device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &cb_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&slot.cull_cb_upload));
    if (FAILED(hr))
        return Result<void>::failure(cull_error("GFX-GPU-CULL-CB", "Could not create cull constant buffer", hr));
    hr = slot.cull_cb_upload->Map(0, nullptr, &slot.cull_cb_mapped);
    if (FAILED(hr) || !slot.cull_cb_mapped)
        return Result<void>::failure(cull_error("GFX-GPU-CULL-CB-MAP", "Could not map cull constant buffer", hr));

    D3D12_RESOURCE_DESC args_upload = cb_desc;
    args_upload.Width = sizeof(D3D12_DRAW_ARGUMENTS);
    hr = device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &args_upload,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&slot.draw_args_upload));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-ARGS-UPLOAD", "Could not create draw-args upload buffer", hr));
    hr = slot.draw_args_upload->Map(0, nullptr, &slot.draw_args_mapped);
    if (FAILED(hr) || !slot.draw_args_mapped)
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-ARGS-MAP", "Could not map draw-args upload buffer", hr));
    return Result<void>::success();
}

Result<void> GpuInstanceCullPass::create(ID3D12Device* device) {
    if (!device)
        return Result<void>::failure(cull_error("GFX-GPU-CULL-DEVICE", "Device is null"));
    descriptor_stride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto pipeline = create_compute_pipeline(device);
    if (!pipeline)
        return pipeline;
    auto signature = create_command_signature(device);
    if (!signature)
        return signature;

    for (UINT i = 0; i < k_frame_slots; ++i) {
        auto upload = create_slot_upload_buffers(device, slots_[i]);
        if (!upload)
            return upload;
    }

    return ensure_capacity(device, 4096);
}

Result<void> GpuInstanceCullPass::create_compute_pipeline(ID3D12Device* device) {
    const char* cs_source = R"(
        cbuffer CullParams : register(b0) {
            float4 planes[6];
            uint instanceOffset;
            uint instanceCount;
            uint float4sPerInstance;
            float radius;
            uint maxVisible;
            uint pad0;
            uint pad1;
            uint pad2;
        };
        StructuredBuffer<float4> instanceRows : register(t0);
        RWStructuredBuffer<uint> visibleIndices : register(u0);
        RWStructuredBuffer<uint> visibleCount : register(u1);

        [numthreads(64, 1, 1)]
        void main(uint3 dtid : SV_DispatchThreadID) {
            if (dtid.x >= instanceCount) return;
            const uint instanceIndex = instanceOffset + dtid.x;
            const uint baseIndex = instanceIndex * float4sPerInstance;
            // Matches foliage VS: model = transpose(float4x4(rows[0..3])); origin = mul(model, 0).
            const float3 origin = float3(
                instanceRows[baseIndex + 0u].w,
                instanceRows[baseIndex + 1u].w,
                instanceRows[baseIndex + 2u].w);
            [unroll]
            for (uint p = 0; p < 6u; ++p) {
                const float dist = dot(planes[p].xyz, origin) + planes[p].w;
                if (dist < -radius) return;
            }
            uint slot;
            InterlockedAdd(visibleCount[0], 1u, slot);
            // Cap compacted writes + InstanceCount so a stale ClearUAV race cannot
            // ExecuteIndirect with a huge count (GPU hang / process AV).
            if (slot < maxVisible)
                visibleIndices[slot] = instanceIndex;
            else
                InterlockedMin(visibleCount[0], maxVisible);
        }
    )";

    ComPtr<ID3DBlob> cs;
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr = D3DCompile(cs_source, std::strlen(cs_source), "gpu_instance_cull", nullptr, nullptr, "main",
        "cs_5_1", flags, 0, &cs, &errors);
    if (FAILED(hr)) {
        return Result<void>::failure(cull_error("GFX-GPU-CULL-CS",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Cull CS compile failed", hr));
    }

    D3D12_DESCRIPTOR_RANGE uav_range{};
    uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uav_range.NumDescriptors = 2;
    uav_range.BaseShaderRegister = 0;
    uav_range.RegisterSpace = 0;
    uav_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[1].Descriptor.ShaderRegister = 0;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &uav_range;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters = 3;
    root_desc.pParameters = params;
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> root_errors;
    hr = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &root_errors);
    if (FAILED(hr)) {
        return Result<void>::failure(cull_error("GFX-GPU-CULL-ROOT",
            root_errors ? static_cast<const char*>(root_errors->GetBufferPointer())
                        : "Cull root signature serialize failed",
            hr));
    }
    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&compute_root_));
    if (FAILED(hr))
        return Result<void>::failure(cull_error("GFX-GPU-CULL-ROOT-CREATE", "Cull root signature create failed", hr));

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = compute_root_.Get();
    pso.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
    hr = device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&compute_pso_));
    if (FAILED(hr))
        return Result<void>::failure(cull_error("GFX-GPU-CULL-PSO", "Cull compute PSO create failed", hr));
    return Result<void>::success();
}

Result<void> GpuInstanceCullPass::create_command_signature(ID3D12Device* device) {
    D3D12_INDIRECT_ARGUMENT_DESC arg{};
    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC desc{};
    desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &arg;
    HRESULT hr = device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&command_signature_));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-CMDSIG", "CreateCommandSignature(Draw) failed", hr));
    return Result<void>::success();
}

Result<void> GpuInstanceCullPass::ensure_capacity(ID3D12Device* device, UINT max_instances) {
    if (!device)
        return Result<void>::failure(cull_error("GFX-GPU-CULL-DEVICE", "Device is null"));
    if (max_instances == 0)
        max_instances = 1;
    if (max_instances <= capacity_instances_ && slots_[0].visible_indices && slots_[0].visible_count
        && slots_[0].indirect_args)
        return Result<void>::success();
    // Grow with headroom so streaming does not recreate every few cells.
    const UINT grown = std::max(max_instances, capacity_instances_ == 0 ? max_instances : capacity_instances_ * 2u);
    return recreate_instance_buffers(device, grown);
}

bool GpuInstanceCullPass::has_capacity(UINT max_instances) const {
    return max_instances <= capacity_instances_ && slots_[0].visible_indices && slots_[0].visible_count
        && slots_[0].indirect_args;
}

Result<void> GpuInstanceCullPass::recreate_slot_instance_buffers(
    ID3D12Device* device, FrameSlot& slot, UINT max_instances) {
    slot.visible_indices.Reset();
    slot.visible_count.Reset();
    slot.indirect_args.Reset();
    slot.uav_heap_shader.Reset();
    slot.uav_heap_cpu.Reset();
    slot.prepared = false;

    D3D12_HEAP_PROPERTIES default_heap{};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC indices_desc{};
    indices_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indices_desc.Width = static_cast<UINT64>(max_instances) * sizeof(UINT);
    indices_desc.Height = 1;
    indices_desc.DepthOrArraySize = 1;
    indices_desc.MipLevels = 1;
    indices_desc.SampleDesc.Count = 1;
    indices_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    indices_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    HRESULT hr = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &indices_desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&slot.visible_indices));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-INDICES", "Could not create visible-indices buffer", hr));

    D3D12_RESOURCE_DESC count_desc = indices_desc;
    count_desc.Width = sizeof(UINT);
    hr = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &count_desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&slot.visible_count));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-COUNT", "Could not create visible-count buffer", hr));

    D3D12_RESOURCE_DESC args_desc = indices_desc;
    args_desc.Width = sizeof(D3D12_DRAW_ARGUMENTS);
    args_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &args_desc,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, nullptr, IID_PPV_ARGS(&slot.indirect_args));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-INDIRECT", "Could not create indirect-args buffer", hr));

    D3D12_DESCRIPTOR_HEAP_DESC shader_heap{};
    shader_heap.NumDescriptors = 2;
    shader_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    shader_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&shader_heap, IID_PPV_ARGS(&slot.uav_heap_shader));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-UAV-HEAP", "Could not create shader-visible UAV heap", hr));

    D3D12_DESCRIPTOR_HEAP_DESC cpu_heap = shader_heap;
    cpu_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&cpu_heap, IID_PPV_ARGS(&slot.uav_heap_cpu));
    if (FAILED(hr))
        return Result<void>::failure(
            cull_error("GFX-GPU-CULL-UAV-CPU-HEAP", "Could not create CPU UAV heap", hr));

    slot.visible_indices_uav_cpu = slot.uav_heap_shader->GetCPUDescriptorHandleForHeapStart();
    slot.visible_indices_uav_gpu = slot.uav_heap_shader->GetGPUDescriptorHandleForHeapStart();
    slot.visible_count_uav_cpu = slot.visible_indices_uav_cpu;
    slot.visible_count_uav_cpu.ptr += descriptor_stride_;
    slot.visible_count_uav_gpu = slot.visible_indices_uav_gpu;
    slot.visible_count_uav_gpu.ptr += descriptor_stride_;

    slot.visible_indices_uav_cpu_clear = slot.uav_heap_cpu->GetCPUDescriptorHandleForHeapStart();
    slot.visible_count_uav_cpu_clear = slot.visible_indices_uav_cpu_clear;
    slot.visible_count_uav_cpu_clear.ptr += descriptor_stride_;

    D3D12_UNORDERED_ACCESS_VIEW_DESC indices_uav{};
    indices_uav.Format = DXGI_FORMAT_UNKNOWN;
    indices_uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    indices_uav.Buffer.FirstElement = 0;
    indices_uav.Buffer.NumElements = max_instances;
    indices_uav.Buffer.StructureByteStride = sizeof(UINT);
    device->CreateUnorderedAccessView(
        slot.visible_indices.Get(), nullptr, &indices_uav, slot.visible_indices_uav_cpu);
    device->CreateUnorderedAccessView(
        slot.visible_indices.Get(), nullptr, &indices_uav, slot.visible_indices_uav_cpu_clear);

    D3D12_UNORDERED_ACCESS_VIEW_DESC count_uav = indices_uav;
    count_uav.Buffer.NumElements = 1;
    device->CreateUnorderedAccessView(
        slot.visible_count.Get(), nullptr, &count_uav, slot.visible_count_uav_cpu);
    device->CreateUnorderedAccessView(
        slot.visible_count.Get(), nullptr, &count_uav, slot.visible_count_uav_cpu_clear);

    return Result<void>::success();
}

Result<void> GpuInstanceCullPass::recreate_instance_buffers(ID3D12Device* device, UINT max_instances) {
    capacity_instances_ = 0;
    for (UINT i = 0; i < k_frame_slots; ++i) {
        auto slot = recreate_slot_instance_buffers(device, slots_[i], max_instances);
        if (!slot)
            return slot;
    }
    capacity_instances_ = max_instances;
    return Result<void>::success();
}

bool GpuInstanceCullPass::dispatch_cull(ID3D12GraphicsCommandList* command_list,
    ID3D12Resource* instance_rows_buffer, const Frustum& frustum, UINT instance_offset, UINT instance_count,
    float sphere_radius_m, UINT vertex_count_per_instance, UINT start_vertex_location) {
    FrameSlot& slot = slots_[frame_slot_];
    slot.prepared = false;
    if (!command_list || !instance_rows_buffer || !ready() || instance_count == 0 || !slot.cull_cb_mapped
        || !slot.draw_args_mapped || !slot.visible_indices || !slot.visible_count || !slot.indirect_args)
        return false;
    if (instance_count > capacity_instances_)
        return false;

    CullConstants cb{};
    for (int i = 0; i < 6; ++i) {
        cb.planes[i][0] = frustum.planes[static_cast<std::size_t>(i)].a;
        cb.planes[i][1] = frustum.planes[static_cast<std::size_t>(i)].b;
        cb.planes[i][2] = frustum.planes[static_cast<std::size_t>(i)].c;
        cb.planes[i][3] = frustum.planes[static_cast<std::size_t>(i)].d;
    }
    cb.instance_offset = instance_offset;
    cb.instance_count = instance_count;
    cb.float4s_per_instance = 4;
    cb.radius = std::max(sphere_radius_m, 0.25f);
    cb.max_visible = capacity_instances_;
    std::memcpy(slot.cull_cb_mapped, &cb, sizeof(cb));

    D3D12_DRAW_ARGUMENTS draw_args{};
    draw_args.VertexCountPerInstance = vertex_count_per_instance;
    draw_args.InstanceCount = 0;
    draw_args.StartVertexLocation = start_vertex_location;
    draw_args.StartInstanceLocation = 0;
    std::memcpy(slot.draw_args_mapped, &draw_args, sizeof(draw_args));

    D3D12_RESOURCE_BARRIER to_copy{};
    to_copy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to_copy.Transition.pResource = slot.indirect_args.Get();
    to_copy.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    to_copy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    to_copy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list->ResourceBarrier(1, &to_copy);
    command_list->CopyBufferRegion(
        slot.indirect_args.Get(), 0, slot.draw_args_upload.Get(), 0, sizeof(D3D12_DRAW_ARGUMENTS));

    // ClearUnorderedAccessViewUint requires the UAV heap to be bound first.
    ID3D12DescriptorHeap* uav_heaps[] = {slot.uav_heap_shader.Get()};
    command_list->SetDescriptorHeaps(1, uav_heaps);

    const UINT clear_values[4] = {0, 0, 0, 0};
    command_list->ClearUnorderedAccessViewUint(slot.visible_count_uav_gpu, slot.visible_count_uav_cpu_clear,
        slot.visible_count.Get(), clear_values, 0, nullptr);
    // ClearUAV is not a sync point — without this, Dispatch can race the clear
    // and InterlockedAdd onto a stale count (huge InstanceCount → GPU hang).
    D3D12_RESOURCE_BARRIER clear_barrier{};
    clear_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    clear_barrier.UAV.pResource = slot.visible_count.Get();
    command_list->ResourceBarrier(1, &clear_barrier);

    command_list->SetComputeRootSignature(compute_root_.Get());
    command_list->SetPipelineState(compute_pso_.Get());
    command_list->SetComputeRootConstantBufferView(0, slot.cull_cb_upload->GetGPUVirtualAddress());
    command_list->SetComputeRootShaderResourceView(1, instance_rows_buffer->GetGPUVirtualAddress());
    command_list->SetComputeRootDescriptorTable(2, slot.visible_indices_uav_gpu);

    const UINT groups = (instance_count + 63u) / 64u;
    command_list->Dispatch(groups, 1, 1);

    D3D12_RESOURCE_BARRIER uav_barriers[2]{};
    uav_barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav_barriers[0].UAV.pResource = slot.visible_indices.Get();
    uav_barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav_barriers[1].UAV.pResource = slot.visible_count.Get();
    command_list->ResourceBarrier(2, uav_barriers);

    D3D12_RESOURCE_BARRIER count_to_src{};
    count_to_src.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    count_to_src.Transition.pResource = slot.visible_count.Get();
    count_to_src.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    count_to_src.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    count_to_src.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list->ResourceBarrier(1, &count_to_src);
    command_list->CopyBufferRegion(slot.indirect_args.Get(), offsetof(D3D12_DRAW_ARGUMENTS, InstanceCount),
        slot.visible_count.Get(), 0, sizeof(UINT));

    D3D12_RESOURCE_BARRIER after_copy[3]{};
    after_copy[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    after_copy[0].Transition.pResource = slot.indirect_args.Get();
    after_copy[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    after_copy[0].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    after_copy[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    after_copy[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    after_copy[1].Transition.pResource = slot.visible_count.Get();
    after_copy[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    after_copy[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    after_copy[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    after_copy[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    after_copy[2].Transition.pResource = slot.visible_indices.Get();
    after_copy[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    after_copy[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    after_copy[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list->ResourceBarrier(3, after_copy);
    slot.prepared = true;
    return true;
}

void GpuInstanceCullPass::bind_visible_indices_root_srv(
    ID3D12GraphicsCommandList* command_list, UINT root_parameter_index) const {
    const FrameSlot& slot = slots_[frame_slot_];
    if (!command_list || !slot.visible_indices || !slot.prepared)
        return;
    command_list->SetGraphicsRootShaderResourceView(
        root_parameter_index, slot.visible_indices->GetGPUVirtualAddress());
}

void GpuInstanceCullPass::execute_prepared_draw(ID3D12GraphicsCommandList* command_list) {
    FrameSlot& slot = slots_[frame_slot_];
    if (!command_list || !slot.prepared || !command_signature_ || !slot.indirect_args || !slot.visible_indices)
        return;
    command_list->ExecuteIndirect(command_signature_.Get(), 1, slot.indirect_args.Get(), 0, nullptr, 0);

    D3D12_RESOURCE_BARRIER indices_back{};
    indices_back.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    indices_back.Transition.pResource = slot.visible_indices.Get();
    indices_back.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    indices_back.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    indices_back.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list->ResourceBarrier(1, &indices_back);
    slot.prepared = false;
}

} // namespace engine
