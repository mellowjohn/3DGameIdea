#pragma once

#include "engine/core/error.h"
#include "engine/rendering/viewport_picking.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

namespace engine {

/**
 * Foliage / instance GPU frustum cull (TICKET-0276 / TICKET-0278).
 *
 * Compute shader tests each instance origin (sphere) against the camera frustum,
 * compacts visible instance indices, then ExecuteIndirect draws only survivors.
 * Resources are double-buffered to match the swapchain frame ring so ClearUAV /
 * Dispatch cannot race the prior frame's GPU work after Present stops draining
 * the fence (DEC-0047).
 *
 * Typical frame use:
 *   1. set_frame_slot(frame_index)
 *   2. dispatch_cull(...)
 *   3. restore foliage graphics PSO / roots / IA
 *   4. bind_visible_indices_root_srv(...)
 *   5. execute_prepared_draw(...)
 */
class GpuInstanceCullPass {
public:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    static constexpr UINT k_frame_slots = 2;

    [[nodiscard]] Result<void> create(ID3D12Device* device);
    [[nodiscard]] Result<void> ensure_capacity(ID3D12Device* device, UINT max_instances);
    [[nodiscard]] bool has_capacity(UINT max_instances) const;
    [[nodiscard]] UINT capacity_instances() const { return capacity_instances_; }

    [[nodiscard]] bool ready() const { return command_signature_ != nullptr && compute_pso_ != nullptr; }

    /** Select which ring slot to write this frame (swapchain backbuffer index). */
    void set_frame_slot(UINT frame_index) { frame_slot_ = frame_index % k_frame_slots; }

    /** Compute cull + fill indirect InstanceCount. Leaves visible-indices in SRV state.
     *  Returns false if the cull could not be prepared (caller must DrawInstanced). */
    [[nodiscard]] bool dispatch_cull(ID3D12GraphicsCommandList* command_list,
        ID3D12Resource* instance_rows_buffer, const Frustum& frustum, UINT instance_offset,
        UINT instance_count, float sphere_radius_m, UINT vertex_count_per_instance,
        UINT start_vertex_location);

    /** SetGraphicsRootShaderResourceView for compacted uint indices (register t1). */
    void bind_visible_indices_root_srv(ID3D12GraphicsCommandList* command_list, UINT root_parameter_index) const;

    /** ExecuteIndirect using args filled by dispatch_cull; returns indices buffer to UAV. */
    void execute_prepared_draw(ID3D12GraphicsCommandList* command_list);

private:
    struct FrameSlot {
        ComPtr<ID3D12Resource> visible_indices;
        ComPtr<ID3D12Resource> visible_count;
        ComPtr<ID3D12Resource> indirect_args;
        ComPtr<ID3D12Resource> cull_cb_upload;
        ComPtr<ID3D12Resource> draw_args_upload;
        ComPtr<ID3D12DescriptorHeap> uav_heap_shader;
        ComPtr<ID3D12DescriptorHeap> uav_heap_cpu;

        D3D12_CPU_DESCRIPTOR_HANDLE visible_indices_uav_cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE visible_indices_uav_gpu{};
        D3D12_CPU_DESCRIPTOR_HANDLE visible_indices_uav_cpu_clear{};
        D3D12_CPU_DESCRIPTOR_HANDLE visible_count_uav_cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE visible_count_uav_gpu{};
        D3D12_CPU_DESCRIPTOR_HANDLE visible_count_uav_cpu_clear{};

        void* cull_cb_mapped = nullptr;
        void* draw_args_mapped = nullptr;
        bool prepared = false;
    };

    [[nodiscard]] Result<void> create_compute_pipeline(ID3D12Device* device);
    [[nodiscard]] Result<void> create_command_signature(ID3D12Device* device);
    [[nodiscard]] Result<void> recreate_instance_buffers(ID3D12Device* device, UINT max_instances);
    [[nodiscard]] Result<void> create_slot_upload_buffers(ID3D12Device* device, FrameSlot& slot);
    [[nodiscard]] Result<void> recreate_slot_instance_buffers(
        ID3D12Device* device, FrameSlot& slot, UINT max_instances);

    ComPtr<ID3D12RootSignature> compute_root_;
    ComPtr<ID3D12PipelineState> compute_pso_;
    ComPtr<ID3D12CommandSignature> command_signature_;

    FrameSlot slots_[k_frame_slots]{};
    UINT capacity_instances_ = 0;
    UINT descriptor_stride_ = 0;
    UINT frame_slot_ = 0;
};

} // namespace engine
