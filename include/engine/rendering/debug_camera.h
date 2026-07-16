#pragma once
#include "engine/core/result.h"
#include <array>
namespace engine {
struct CameraInput { float forward=0,right=0,up=0; float mouse_x=0,mouse_y=0; bool boost=false; };
class DebugCamera final {
public:
    void apply(const CameraInput& input,float seconds);
    [[nodiscard]] Result<void> set_perspective(float vertical_fov_radians,float aspect,float near_plane,float far_plane);
    [[nodiscard]] std::array<float,3> position()const{return position_;}
    [[nodiscard]] float yaw()const{return yaw_;} [[nodiscard]] float pitch()const{return pitch_;}
    void set_pose(const std::array<float, 3>& position, float yaw, float pitch);
    [[nodiscard]] std::array<float,3> forward()const;
    [[nodiscard]] std::array<float,16> view_projection()const;
    [[nodiscard]] std::array<float,16> view_matrix()const;
    [[nodiscard]] std::array<float,16> projection_matrix()const;
private:
    std::array<float,3> position_{0,3,-8}; float yaw_=0,pitch_=0; float fov_=1.04719755f,aspect_=16.0f/9.0f,near_=0.1f,far_=2000; float speed_=6,sensitivity_=0.0025f;
};
}
