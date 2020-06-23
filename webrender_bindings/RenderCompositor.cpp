/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositor.h"

#include "GLContext.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/webrender/RenderCompositorOGL.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef XP_WIN
#  include "mozilla/webrender/RenderCompositorANGLE.h"
#endif

#if defined(MOZ_WAYLAND) || defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/webrender/RenderCompositorEGL.h"
#endif

#ifdef XP_MACOSX
#  include "mozilla/webrender/RenderCompositorNative.h"
#endif

namespace mozilla {
namespace wr {

void wr_compositor_add_surface(void* aCompositor, wr::NativeSurfaceId aId,
                               wr::DeviceIntPoint aPosition,
                               wr::DeviceIntRect aClipRect) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->AddSurface(aId, aPosition, aClipRect);
}

void wr_compositor_begin_frame(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->CompositorBeginFrame();
}

void wr_compositor_bind(void* aCompositor, wr::NativeTileId aId,
                        wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
                        wr::DeviceIntRect aDirtyRect,
                        wr::DeviceIntRect aValidRect) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->Bind(aId, aOffset, aFboId, aDirtyRect, aValidRect);
}

void wr_compositor_create_surface(void* aCompositor, wr::NativeSurfaceId aId,
                                  wr::DeviceIntPoint aVirtualOffset,
                                  wr::DeviceIntSize aTileSize, bool aIsOpaque) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->CreateSurface(aId, aVirtualOffset, aTileSize, aIsOpaque);
}

void wr_compositor_create_tile(void* aCompositor, wr::NativeSurfaceId aId,
                               int32_t aX, int32_t aY) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->CreateTile(aId, aX, aY);
}

void wr_compositor_destroy_tile(void* aCompositor, wr::NativeSurfaceId aId,
                                int32_t aX, int32_t aY) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->DestroyTile(aId, aX, aY);
}

void wr_compositor_destroy_surface(void* aCompositor, NativeSurfaceId aId) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->DestroySurface(aId);
}

void wr_compositor_end_frame(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->CompositorEndFrame();
}

void wr_compositor_enable_native_compositor(void* aCompositor, bool aEnable) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->EnableNativeCompositor(aEnable);
}

CompositorCapabilities wr_compositor_get_capabilities(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  return compositor->GetCompositorCapabilities();
}

void wr_compositor_unbind(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->Unbind();
}

void wr_compositor_deinit(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->DeInit();
}

void wr_compositor_map_tile(void* aCompositor, wr::NativeTileId aId,
                            wr::DeviceIntRect aDirtyRect,
                            wr::DeviceIntRect aValidRect, void** aData,
                            int32_t* aStride) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->MapTile(aId, aDirtyRect, aValidRect, aData, aStride);
}

void wr_compositor_unmap_tile(void* aCompositor) {
  RenderCompositor* compositor = static_cast<RenderCompositor*>(aCompositor);
  compositor->UnmapTile();
}

/* static */
UniquePtr<RenderCompositor> RenderCompositor::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
#ifdef XP_WIN
  if (gfx::gfxVars::UseWebRenderANGLE()) {
    return RenderCompositorANGLE::Create(std::move(aWidget));
  }
#endif

#if defined(MOZ_WAYLAND) || defined(MOZ_WIDGET_ANDROID)
  UniquePtr<RenderCompositor> eglCompositor =
      RenderCompositorEGL::Create(aWidget);
  if (eglCompositor) {
    return eglCompositor;
  }
#endif

#if defined(MOZ_WIDGET_ANDROID)
  // RenderCompositorOGL is not used on android
  return nullptr;
#elif defined(XP_MACOSX)
  // Mac uses NativeLayerCA
  return RenderCompositorNativeOGL::Create(std::move(aWidget));
#else
  return RenderCompositorOGL::Create(std::move(aWidget));
#endif
}

RenderCompositor::RenderCompositor(RefPtr<widget::CompositorWidget>&& aWidget)
    : mWidget(aWidget) {}

RenderCompositor::~RenderCompositor() = default;

bool RenderCompositor::MakeCurrent() {
  gl::GLContext* context = gl();
  return !context || context->MakeCurrent();
}

bool RenderCompositor::IsContextLost() {
  // XXX Add glGetGraphicsResetStatus handling for checking rendering context
  // has not been lost
  return false;
}

}  // namespace wr
}  // namespace mozilla
