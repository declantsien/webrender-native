/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DCLAYER_TREE_H
#define MOZILLA_GFX_DCLAYER_TREE_H

#include <dxgiformat.h>
#include <unordered_map>
#include <vector>
#include <windows.h>

#include "Colorspaces.h"
#include "GLTypes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/layers/OverlayInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoProcessor;
struct ID3D11VideoProcessorEnumerator;
struct ID3D11VideoProcessorOutputView;
struct IDCompositionColorMatrixEffect;
struct IDCompositionFilterEffect;
struct IDCompositionTableTransferEffect;
struct IDCompositionDevice2;
struct IDCompositionDevice3;
struct IDCompositionSurface;
struct IDCompositionTarget;
struct IDCompositionVisual2;
struct IDXGIDecodeSwapChain;
struct IDXGIResource;
struct IDXGISwapChain1;
struct IDCompositionVirtualSurface;

namespace mozilla {

namespace gl {
class GLContext;
}

namespace wr {

// The size of the virtual surface. This is large enough such that we
// will never render a surface larger than this.
#define VIRTUAL_SURFACE_SIZE (1024 * 1024)

class DCTile;
class DCSurface;
class DCSurfaceVideo;
class DCSurfaceHandle;
class RenderTextureHost;
class RenderDcompSurfaceTextureHost;

struct GpuOverlayInfo {
  bool mSupportsOverlays = false;
  bool mSupportsHardwareOverlays = false;
  DXGI_FORMAT mOverlayFormatUsed = DXGI_FORMAT_B8G8R8A8_UNORM;
  DXGI_FORMAT mOverlayFormatUsedHdr = DXGI_FORMAT_R10G10B10A2_UNORM;
  UINT mNv12OverlaySupportFlags = 0;
  UINT mYuy2OverlaySupportFlags = 0;
  UINT mBgra8OverlaySupportFlags = 0;
  UINT mRgb10a2OverlaySupportFlags = 0;
};

// -

struct ColorManagementChain {
  RefPtr<IDCompositionColorMatrixEffect> srcRgbFromSrcYuv;
  RefPtr<IDCompositionTableTransferEffect> srcLinearFromSrcTf;
  RefPtr<IDCompositionColorMatrixEffect> dstLinearFromSrcLinear;
  RefPtr<IDCompositionTableTransferEffect> dstTfFromDstLinear;
  RefPtr<IDCompositionFilterEffect> last;

  static ColorManagementChain From(IDCompositionDevice3& dcomp,
                                   const color::ColorProfileConversionDesc&);

  ~ColorManagementChain();
};

// -

enum class DCompOverlayTypes : uint8_t {
  NO_OVERLAY = 0,
  HARDWARE_DECODED_VIDEO = 1 << 0,
  SOFTWARE_DECODED_VIDEO = 1 << 1,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(DCompOverlayTypes)

// -

/**
 * DCLayerTree manages direct composition layers.
 * It does not manage gecko's layers::Layer.
 */
class DCLayerTree {
 public:
  static UniquePtr<DCLayerTree> Create(gl::GLContext* aGL, EGLConfig aEGLConfig,
                                       ID3D11Device* aDevice,
                                       ID3D11DeviceContext* aCtx, HWND aHwnd,
                                       nsACString& aError);

  static void Shutdown();

  explicit DCLayerTree(gl::GLContext* aGL, EGLConfig aEGLConfig,
                       ID3D11Device* aDevice, ID3D11DeviceContext* aCtx,
                       IDCompositionDevice2* aCompositionDevice);
  ~DCLayerTree();

  void SetDefaultSwapChain(IDXGISwapChain1* aSwapChain);
  void MaybeUpdateDebug();
  void MaybeCommit();
  void WaitForCommitCompletion();
  void DisableNativeCompositor();

  // Interface for wr::Compositor
  void CompositorBeginFrame();
  void CompositorEndFrame();
  void Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
            wr::DeviceIntRect aDirtyRect, wr::DeviceIntRect aValidRect);
  void Unbind();
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aVirtualOffset,
                     wr::DeviceIntSize aTileSize, bool aIsOpaque);
  void CreateExternalSurface(wr::NativeSurfaceId aId, bool aIsOpaque);
  void DestroySurface(NativeSurfaceId aId);
  void CreateTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY);
  void DestroyTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY);
  void AttachExternalImage(wr::NativeSurfaceId aId,
                           wr::ExternalImageId aExternalImage);
  void AddSurface(wr::NativeSurfaceId aId,
                  const wr::CompositorSurfaceTransform& aTransform,
                  wr::DeviceIntRect aClipRect,
                  wr::ImageRendering aImageRendering);

  gl::GLContext* GetGLContext() const { return mGL; }
  EGLConfig GetEGLConfig() const { return mEGLConfig; }
  ID3D11Device* GetDevice() const { return mDevice; }
  IDCompositionDevice2* GetCompositionDevice() const {
    return mCompositionDevice;
  }
  ID3D11VideoDevice* GetVideoDevice() const { return mVideoDevice; }
  ID3D11VideoContext* GetVideoContext() const { return mVideoContext; }
  ID3D11VideoProcessor* GetVideoProcessor() const { return mVideoProcessor; }
  ID3D11VideoProcessorEnumerator* GetVideoProcessorEnumerator() const {
    return mVideoProcessorEnumerator;
  }
  bool EnsureVideoProcessor(const gfx::IntSize& aInputSize,
                            const gfx::IntSize& aOutputSize);

  DCSurface* GetSurface(wr::NativeSurfaceId aId) const;

  // Get or create an FBO with depth buffer suitable for specified dimensions
  GLuint GetOrCreateFbo(int aWidth, int aHeight);

  bool SupportsHardwareOverlays();
  DXGI_FORMAT GetOverlayFormatForSDR();

  bool SupportsSwapChainTearing();

  void SetUsedOverlayTypeInFrame(DCompOverlayTypes aTypes);

 protected:
  bool Initialize(HWND aHwnd, nsACString& aError);
  bool InitializeVideoOverlaySupport();
  bool MaybeUpdateDebugCounter();
  bool MaybeUpdateDebugVisualRedrawRegions();
  void DestroyEGLSurface();
  GLuint CreateEGLSurfaceForCompositionSurface(
      wr::DeviceIntRect aDirtyRect, wr::DeviceIntPoint* aOffset,
      RefPtr<IDCompositionSurface> aCompositionSurface,
      wr::DeviceIntPoint aSurfaceOffset);
  void ReleaseNativeCompositorResources();
  layers::OverlayInfo GetOverlayInfo();

  RefPtr<gl::GLContext> mGL;
  EGLConfig mEGLConfig;

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mCtx;

  RefPtr<IDCompositionDevice2> mCompositionDevice;
  RefPtr<IDCompositionTarget> mCompositionTarget;
  RefPtr<IDCompositionVisual2> mRootVisual;
  RefPtr<IDCompositionVisual2> mDefaultSwapChainVisual;

  RefPtr<ID3D11VideoDevice> mVideoDevice;
  RefPtr<ID3D11VideoContext> mVideoContext;
  RefPtr<ID3D11VideoProcessor> mVideoProcessor;
  RefPtr<ID3D11VideoProcessorEnumerator> mVideoProcessorEnumerator;
  gfx::IntSize mVideoInputSize;
  gfx::IntSize mVideoOutputSize;

  bool mDebugCounter;
  bool mDebugVisualRedrawRegions;

  Maybe<RefPtr<IDCompositionSurface>> mCurrentSurface;

  // The EGL image that is bound to the D3D texture provided by
  // DirectComposition.
  EGLImage mEGLImage;

  // The GL render buffer ID that maps the EGLImage to an RBO for attaching to
  // an FBO.
  GLuint mColorRBO;

  struct SurfaceIdHashFn {
    std::size_t operator()(const wr::NativeSurfaceId& aId) const {
      return HashGeneric(wr::AsUint64(aId));
    }
  };

  std::unordered_map<wr::NativeSurfaceId, UniquePtr<DCSurface>, SurfaceIdHashFn>
      mDCSurfaces;

  // A list of layer IDs as they are added to the visual tree this frame.
  std::vector<wr::NativeSurfaceId> mCurrentLayers;

  // The previous frame's list of layer IDs in visual order.
  std::vector<wr::NativeSurfaceId> mPrevLayers;

  // Information about a cached FBO that is retained between frames.
  struct CachedFrameBuffer {
    int width;
    int height;
    GLuint fboId;
    GLuint depthRboId;
    int lastFrameUsed;
  };

  // A cache of FBOs, containing a depth buffer allocated to a specific size.
  // TODO(gw): Might be faster as a hashmap? The length is typically much less
  // than 10.
  nsTArray<CachedFrameBuffer> mFrameBuffers;
  int mCurrentFrame = 0;

  bool mPendingCommit;

  static color::ColorProfileDesc QueryOutputColorProfile();

  mutable Maybe<color::ColorProfileDesc> mOutputColorProfile;

  DCompOverlayTypes mUsedOverlayTypesInFrame = DCompOverlayTypes::NO_OVERLAY;
  int mSlowCommitCount = 0;

 public:
  const color::ColorProfileDesc& OutputColorProfile() const {
    if (!mOutputColorProfile) {
      mOutputColorProfile = Some(QueryOutputColorProfile());
    }
    return *mOutputColorProfile;
  }

 protected:
  static StaticAutoPtr<GpuOverlayInfo> sGpuOverlayInfo;
};

/**
 Represents a single picture cache slice. Each surface contains some
 number of tiles. An implementation may choose to allocate individual
 tiles to render in to (as the current impl does), or allocate a large
 single virtual surface to draw into (e.g. the DirectComposition virtual
 surface API in future).
 */
class DCSurface {
 public:
  const bool mIsVirtualSurface;

  explicit DCSurface(wr::DeviceIntSize aTileSize,
                     wr::DeviceIntPoint aVirtualOffset, bool aIsVirtualSurface,
                     bool aIsOpaque, DCLayerTree* aDCLayerTree);
  virtual ~DCSurface();

  bool Initialize();
  void CreateTile(int32_t aX, int32_t aY);
  void DestroyTile(int32_t aX, int32_t aY);

  IDCompositionVisual2* GetVisual() const { return mVisual; }
  DCTile* GetTile(int32_t aX, int32_t aY) const;

  struct TileKey {
    TileKey(int32_t aX, int32_t aY) : mX(aX), mY(aY) {}

    int32_t mX;
    int32_t mY;
  };

  wr::DeviceIntSize GetTileSize() const { return mTileSize; }
  wr::DeviceIntPoint GetVirtualOffset() const { return mVirtualOffset; }

  IDCompositionVirtualSurface* GetCompositionSurface() const {
    return mVirtualSurface;
  }

  void UpdateAllocatedRect();
  void DirtyAllocatedRect();

  // Implement these if the inherited surface supports attaching external image.
  virtual void AttachExternalImage(wr::ExternalImageId aExternalImage) {
    MOZ_RELEASE_ASSERT(true, "Not support attaching external image");
  }
  virtual void PresentExternalSurface(gfx::Matrix& aTransform) {
    MOZ_RELEASE_ASSERT(true, "Not support presenting external surface");
  }

  virtual DCSurfaceVideo* AsDCSurfaceVideo() { return nullptr; }
  virtual DCSurfaceHandle* AsDCSurfaceHandle() { return nullptr; }

 protected:
  DCLayerTree* mDCLayerTree;

  struct TileKeyHashFn {
    std::size_t operator()(const TileKey& aId) const {
      return HashGeneric(aId.mX, aId.mY);
    }
  };

  // The visual for this surface. No content is attached to here, but tiles
  // that belong to this surface are added as children. In this way, we can
  // set the clip and scroll offset once, on this visual, to affect all
  // children.
  //
  // However when using a virtual surface, it is directly attached to this
  // visual and the tiles do not own visuals.
  //
  // Whether mIsVirtualSurface is enabled is decided at DCSurface creation
  // time based on the pref gfx.webrender.dcomp-use-virtual-surfaces
  RefPtr<IDCompositionVisual2> mVisual;

  wr::DeviceIntSize mTileSize;
  bool mIsOpaque;
  bool mAllocatedRectDirty;
  std::unordered_map<TileKey, UniquePtr<DCTile>, TileKeyHashFn> mDCTiles;
  wr::DeviceIntPoint mVirtualOffset;
  RefPtr<IDCompositionVirtualSurface> mVirtualSurface;
};

/**
 * A wrapper surface which can contain either a DCVideo or a DCSurfaceHandle.
 */
class DCExternalSurfaceWrapper : public DCSurface {
 public:
  DCExternalSurfaceWrapper(bool aIsOpaque, DCLayerTree* aDCLayerTree)
      : DCSurface(wr::DeviceIntSize{}, wr::DeviceIntPoint{},
                  false /* virtual surface */, false /* opaque */,
                  aDCLayerTree),
        mIsOpaque(aIsOpaque) {}
  ~DCExternalSurfaceWrapper() = default;

  void AttachExternalImage(wr::ExternalImageId aExternalImage) override;

  void PresentExternalSurface(gfx::Matrix& aTransform) override;

  DCSurfaceVideo* AsDCSurfaceVideo() override {
    return mSurface ? mSurface->AsDCSurfaceVideo() : nullptr;
  }

  DCSurfaceHandle* AsDCSurfaceHandle() override {
    return mSurface ? mSurface->AsDCSurfaceHandle() : nullptr;
  }

 private:
  DCSurface* EnsureSurfaceForExternalImage(wr::ExternalImageId aExternalImage);

  UniquePtr<DCSurface> mSurface;
  const bool mIsOpaque;
  Maybe<ColorManagementChain> mCManageChain;
};

class DCSurfaceVideo : public DCSurface {
 public:
  DCSurfaceVideo(bool aIsOpaque, DCLayerTree* aDCLayerTree);

  void AttachExternalImage(wr::ExternalImageId aExternalImage) override;
  bool CalculateSwapChainSize(gfx::Matrix& aTransform);
  void PresentVideo();

  DCSurfaceVideo* AsDCSurfaceVideo() override { return this; }

 protected:
  virtual ~DCSurfaceVideo();

  DXGI_FORMAT GetSwapChainFormat();
  bool CreateVideoSwapChain();
  bool CallVideoProcessorBlt();
  void ReleaseDecodeSwapChainResources();

  RefPtr<ID3D11VideoProcessorOutputView> mOutputView;
  RefPtr<IDXGIResource> mDecodeResource;
  RefPtr<IDXGISwapChain1> mVideoSwapChain;
  RefPtr<IDXGIDecodeSwapChain> mDecodeSwapChain;
  HANDLE mSwapChainSurfaceHandle = 0;
  gfx::IntSize mVideoSize;
  gfx::IntSize mSwapChainSize;
  DXGI_FORMAT mSwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
  bool mIsDRM = false;
  bool mFailedYuvSwapChain = false;
  RefPtr<RenderTextureHost> mRenderTextureHost;
  RefPtr<RenderTextureHost> mPrevTexture;
  int mSlowPresentCount = 0;
  bool mFirstPresent = true;
  const UINT mSwapChainBufferCount;
};

/**
 * A DC surface contains a IDCompositionSurface that is directly constructed by
 * a handle. This is used by the Media Foundataion media engine, which would
 * store the decoded video content in the surface.
 */
class DCSurfaceHandle : public DCSurface {
 public:
  DCSurfaceHandle(bool aIsOpaque, DCLayerTree* aDCLayerTree);
  ~DCSurfaceHandle() = default;

  void AttachExternalImage(wr::ExternalImageId aExternalImage) override;
  void PresentSurfaceHandle();

  DCSurfaceHandle* AsDCSurfaceHandle() override { return this; }

 protected:
  HANDLE GetSurfaceHandle() const;
  IDCompositionSurface* EnsureSurface();

  RefPtr<RenderDcompSurfaceTextureHost> mDcompTextureHost;
};

class DCTile {
 public:
  gfx::IntRect mValidRect;

  DCLayerTree* mDCLayerTree;
  // Indicates that when the first BeginDraw occurs on the surface it must be
  // full size - required by dcomp on non-virtual surfaces.
  bool mNeedsFullDraw;

  explicit DCTile(DCLayerTree* aDCLayerTree);
  ~DCTile();
  bool Initialize(int aX, int aY, wr::DeviceIntSize aSize,
                  bool aIsVirtualSurface, bool aIsOpaque,
                  RefPtr<IDCompositionVisual2> mSurfaceVisual);
  RefPtr<IDCompositionSurface> Bind(wr::DeviceIntRect aValidRect);
  IDCompositionVisual2* GetVisual() { return mVisual; }

 protected:
  // Size in pixels of this tile, some may be unused.  Set by Initialize.
  wr::DeviceIntSize mSize;
  // Whether the tile is composited as opaque (ignores alpha) or transparent.
  // Set by Initialize.
  bool mIsOpaque;
  // Some code paths differ based on whether parent surface is virtual.
  bool mIsVirtualSurface;
  // Visual that displays the composition surface, or NULL if the tile belongs
  // to a virtual surface.
  RefPtr<IDCompositionVisual2> mVisual;
  // Surface for the visual, or NULL if the tile has not had its first Bind or
  // belongs to a virtual surface.
  RefPtr<IDCompositionSurface> mCompositionSurface;

  RefPtr<IDCompositionSurface> CreateCompositionSurface(wr::DeviceIntSize aSize,
                                                        bool aIsOpaque);
};

static inline bool operator==(const DCSurface::TileKey& a0,
                              const DCSurface::TileKey& a1) {
  return a0.mX == a1.mX && a0.mY == a1.mY;
}

}  // namespace wr
}  // namespace mozilla

#endif
