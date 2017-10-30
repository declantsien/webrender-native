/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H
#define MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H

#include "RenderTextureHost.h"

namespace mozilla {
namespace gfx {
class SourceSurfaceSharedDataWrapper;
}

namespace wr {

class RenderSharedSurfaceTextureHost final : public RenderTextureHost
{
public:
  explicit RenderSharedSurfaceTextureHost(gfx::SourceSurfaceSharedDataWrapper* aSurface);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;

private:
  ~RenderSharedSurfaceTextureHost() override;

  RefPtr<gfx::SourceSurfaceSharedDataWrapper> mSurface;
  gfx::DataSourceSurface::MappedSurface mMap;
  bool mLocked;
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H
