#include "engine/rendering/debug_camera.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
namespace engine { namespace { EngineError camera_error(std::string c,std::string m){return {std::move(c),Severity::Error,ErrorCategory::Validation,"camera",std::move(m),ENGINE_SOURCE_CONTEXT,{},"Use finite perspective parameters with 0 < near < far.",make_correlation_id()};} }
Result<void> DebugCamera::set_perspective(float f,float a,float n,float z){if(!std::isfinite(f)||!std::isfinite(a)||!std::isfinite(n)||!std::isfinite(z)||f<=0.1f||f>=3.0f||a<=0||n<=0||z<=n)return Result<void>::failure(camera_error("CAMERA-PERSPECTIVE-INVALID","Invalid perspective parameters"));fov_=f;aspect_=a;near_=n;far_=z;return Result<void>::success();}
std::array<float,3> DebugCamera::forward()const{const float cp=std::cos(pitch_);return {std::sin(yaw_)*cp,std::sin(pitch_),std::cos(yaw_)*cp};}
void DebugCamera::apply(const CameraInput&i,float dt){if(!(dt>0)||dt>0.25f)return;yaw_+=i.mouse_x*sensitivity_;pitch_=std::clamp(pitch_-i.mouse_y*sensitivity_,-1.55334f,1.55334f);auto f=forward();std::array<float,3> r{std::cos(yaw_),0,-std::sin(yaw_)};float s=speed_*(i.boost?4.0f:1.0f)*dt;for(int k=0;k<3;++k)position_[k]+=(f[k]*i.forward+r[k]*i.right)*s;position_[1]+=i.up*s;}
void DebugCamera::set_pose(const std::array<float, 3>& position, float yaw, float pitch) {
    position_ = position;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -1.55334f, 1.55334f);
}
std::array<float,16> DebugCamera::view_matrix()const{using namespace DirectX;auto f=forward();XMVECTOR eye=XMVectorSet(position_[0],position_[1],position_[2],1);std::array<float,16> out{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()),XMMatrixLookToLH(eye,XMVectorSet(f[0],f[1],f[2],0),XMVectorSet(0,1,0,0)));return out;}
std::array<float,16> DebugCamera::projection_matrix()const{using namespace DirectX;std::array<float,16> out{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()),XMMatrixPerspectiveFovLH(fov_,aspect_,near_,far_));return out;}
std::array<float,16> DebugCamera::view_projection()const{using namespace DirectX;auto v=view_matrix(),p=projection_matrix();std::array<float,16> out{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out.data()),XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(v.data()))*XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(p.data())));return out;}
}
