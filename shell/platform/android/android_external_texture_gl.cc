// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_external_texture_gl.h"

#include <GLES/glext.h>

#include "flutter/common/threads.h"
#include "flutter/shell/platform/android/platform_view_android_jni.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrTexture.h"

namespace shell {

AndroidExternalTextureGL::AndroidExternalTextureGL(
    const fml::jni::JavaObjectWeakGlobalRef& surfaceTexture)
    : surface_texture_(surfaceTexture) {}

AndroidExternalTextureGL::~AndroidExternalTextureGL() = default;

void AndroidExternalTextureGL::OnGrContextCreated() {
  ASSERT_IS_GPU_THREAD;
  state_ = AttachmentState::uninitialized;
}

void AndroidExternalTextureGL::MarkNewFrameAvailable() {
  ASSERT_IS_GPU_THREAD;
  new_frame_ready_ = true;
}

sk_sp<SkImage> AndroidExternalTextureGL::MakeSkImage(int width,
                                                     int height,
                                                     GrContext* grContext) {
  ASSERT_IS_GPU_THREAD;
  if (state_ == AttachmentState::detached) {
    return nullptr;
  }
  if (state_ == AttachmentState::uninitialized) {
    glGenTextures(1, &texture_name_);
    Attach(static_cast<jint>(texture_name_));
    state_ = AttachmentState::attached;
  }
  if (new_frame_ready_) {
    Update();
    new_frame_ready_ = false;
  }
  GrGLTextureInfo textureInfo = {GL_TEXTURE_EXTERNAL_OES, texture_name_};
  GrBackendTexture backendTexture(width, height, kRGBA_8888_GrPixelConfig,
                                  textureInfo);
  return SkImage::MakeFromTexture(grContext, backendTexture,
                                  kTopLeft_GrSurfaceOrigin,
                                  SkAlphaType::kPremul_SkAlphaType, nullptr);
}

void AndroidExternalTextureGL::OnGrContextDestroyed() {
  ASSERT_IS_GPU_THREAD;
  if (state_ == AttachmentState::attached) {
    Detach();
  }
  state_ = AttachmentState::detached;
}

void AndroidExternalTextureGL::Attach(jint textureName) {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  fml::jni::ScopedJavaLocalRef<jobject> surfaceTexture =
      surface_texture_.get(env);
  if (!surfaceTexture.is_null()) {
    SurfaceTextureAttachToGLContext(env, surfaceTexture.obj(), textureName);
  }
}

void AndroidExternalTextureGL::Update() {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  fml::jni::ScopedJavaLocalRef<jobject> surfaceTexture =
      surface_texture_.get(env);
  if (!surfaceTexture.is_null()) {
    SurfaceTextureUpdateTexImage(env, surfaceTexture.obj());
  }
}

void AndroidExternalTextureGL::Detach() {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  fml::jni::ScopedJavaLocalRef<jobject> surfaceTexture =
      surface_texture_.get(env);
  if (!surfaceTexture.is_null()) {
    SurfaceTextureDetachFromGLContext(env, surfaceTexture.obj());
  }
}

}  // namespace shell
