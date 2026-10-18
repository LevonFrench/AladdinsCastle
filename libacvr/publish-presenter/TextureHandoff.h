// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "WinGL.h"
#include "Presenter.h"
#include <cstdint>
#include <mutex>
#ifdef ACVR_VIEWER_TEST
inline void (*ACVR_TestConsumerDone)()=nullptr;
#endif

// One slot owns a complete stereo pair. Both GPU ownership transfers finish
// before releasing its mutex. In particular, Publish cannot overwrite or resize
// until Consume's glFinish has completed every read of these shared textures.
class TextureHandoff
{
public:
  struct Info { unsigned w=0,h=0; std::uint64_t serial=0; acvr_publish::ProjectionPacket projection; };
  Info peek() { std::lock_guard<std::mutex> lock(m_mutex); return m_info; }
  void publish(const GLuint source[2], unsigned w, unsigned h,const acvr_publish::ProjectionPacket& projection={})
  {
    std::lock_guard<std::mutex> lock(m_mutex); // also waits for consumer completion
    if(m_done){glWaitSync(m_done,0,GL_TIMEOUT_IGNORED);glDeleteSync(m_done);m_done=nullptr;}
    if (!m_tex[0] || m_info.w!=w || m_info.h!=h) {
      destroy();
      glGenTextures(2,m_tex); glGenFramebuffers(2,m_fbo);
      for (int eye=0;eye<2;++eye) {
        glBindTexture(GL_TEXTURE_2D,m_tex[eye]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,m_fbo[eye]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_tex[eye],0);
      }
    }
    const GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST), srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB);
    for(int eye=0;eye<2;++eye) {
      glBindFramebuffer(GL_READ_FRAMEBUFFER,source[eye]); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_fbo[eye]);
      glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(scissor) glEnable(GL_SCISSOR_TEST); if(srgb) glEnable(GL_FRAMEBUFFER_SRGB);
    if(m_ready)glDeleteSync(m_ready);
    m_ready=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
    glFinish(); // retain Model 2's producer completion behavior
    m_info.w=w; m_info.h=h;m_info.projection=projection; ++m_info.serial;
  }
  template<class Copy> bool consume(unsigned w,unsigned h,Copy copy,std::uint64_t &serial)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_info.serial || m_info.w!=w || m_info.h!=h) return false;
    if(m_ready)glWaitSync(m_ready,0,GL_TIMEOUT_IGNORED);
    // A scope guard completes even if a copy callback reports an error/throws.
    struct Complete { GLsync* done; ~Complete() {
      if(*done)glDeleteSync(*done);
      *done=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
      glFinish();
#ifdef ACVR_VIEWER_TEST
      if(ACVR_TestConsumerDone) ACVR_TestConsumerDone();
#endif
    } } completion{&m_done};
    if(!copy(m_tex,m_info)) return false;
    serial=m_info.serial;
    return true; // completion runs BEFORE lock is released: consumer-done handoff
  }
  void reset() { std::lock_guard<std::mutex> lock(m_mutex); glFinish(); destroy(); m_info={}; }
private:
  void destroy() {
    if(m_ready)glDeleteSync(m_ready);if(m_done)glDeleteSync(m_done);m_ready=m_done=nullptr;
    glDeleteFramebuffers(2,m_fbo); glDeleteTextures(2,m_tex);
    m_fbo[0]=m_fbo[1]=m_tex[0]=m_tex[1]=0;
  }
  std::mutex m_mutex;
  Info m_info;
  GLsync m_ready=nullptr,m_done=nullptr;
  GLuint m_tex[2]{},m_fbo[2]{};
};
