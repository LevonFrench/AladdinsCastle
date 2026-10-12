// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QString>
#include <array>
#include <cstdint>
namespace ac {
struct CompositorMirror { unsigned int texture=0; std::uintptr_t sharedHandle=0; };
// Both production and CPU tests use this lifetime path. No source-texture fallback.
class CompositorReadbackOps {
public:
    virtual ~CompositorReadbackOps()=default;
    virtual bool acquire(unsigned int eye,CompositorMirror *mirror,QString *error)=0;
    virtual void lock(const CompositorMirror &mirror)noexcept=0;
    virtual bool read(const CompositorMirror &mirror,QImage *image,QString *error)=0;
    virtual void unlock(const CompositorMirror &mirror)noexcept=0;
    virtual bool release(const CompositorMirror &mirror)noexcept=0;
};
bool readCompositorEyes(CompositorReadbackOps &ops,std::array<QImage,2> *eyes,QString *error);
bool compositorDashboardMatches(const QString &requirement,bool visible);
bool validateCompositorDirectory(const QString &catalogRoot,const QString &directory,QString *error);
bool writeCompositorEyes(const QString &catalogRoot,const QString &directory,const std::array<QImage,2> &eyes,bool dashboardVisible,QString *error);
// Explicit one-shot native client. Requires an already-running service, no dashboard activation.
bool captureCompositorEyes(const QString &catalogRoot,const QString &directory,const QString &dashboardRequirement,QString *error);
}
