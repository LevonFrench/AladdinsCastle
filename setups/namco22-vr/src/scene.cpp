// SPDX-License-Identifier: GPL-3.0-only
#include "n22_scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace n22 {
namespace {
bool finite(float v) { return std::isfinite(v); }
bool finite(Vec3 v) { return finite(v.x) && finite(v.y) && finite(v.z); }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
bool camera_valid(const acvr_game_camera &c) {
    if (c.size < sizeof(c) || c.version != ACVR_STRUCT_VERSION ||
        !c.raster_width || !c.raster_height || !(c.flags & ACVR_CAMERA_VIEW_LOCAL) ||
        !finite(c.focal_x_px) || c.focal_x_px <= 0 ||
        !finite(c.focal_y_px) || c.focal_y_px <= 0 ||
        !finite(c.centre_x_px) || !finite(c.centre_y_px)) return false;
    for (int i=0;i<16;++i) {
        const float expected = (i%5 == 0) ? 1.f : 0.f;
        if (c.view_from_scene[i] != expected) return false;
    }
    for (float v:c.viewport_px) if (!finite(v)) return false;
    return finite(c.near_scene) && c.near_scene>0 && finite(c.far_scene) &&
        (c.far_scene==0 || c.far_scene>c.near_scene) && c.viewport_px[0]>=0 && c.viewport_px[1]>=0 &&
        c.viewport_px[2]>0 && c.viewport_px[3]>0 &&
        c.viewport_px[0]+c.viewport_px[2]<=static_cast<float>(c.raster_width) &&
        c.viewport_px[1]+c.viewport_px[3]<=static_cast<float>(c.raster_height);
}
struct V4 { double x,y,z,w; };
struct ClipVertex {V4 p;double u,v,brightness;};
V4 transform(const float *m,V4 p) {
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12]*p.w,
            m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13]*p.w,
            m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]*p.w,
            m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15]*p.w};
}
double plane(V4 v,int i) {
    switch(i) {
    case 0:return v.w+v.x; case 1:return v.w-v.x;
    case 2:return v.w+v.y; case 3:return v.w-v.y;
    case 4:return v.w+v.z; default:return v.w-v.z;
    }
}
std::vector<ClipVertex> clip(std::vector<ClipVertex> p) {
    for(int i=0;i<6 && !p.empty();++i) {
        std::vector<ClipVertex> out;
        auto a=p.back(); double da=plane(a.p,i);
        for(auto b:p) {
            double db=plane(b.p,i);
            if((da>=0)!=(db>=0)) {
                double t=da/(da-db);
                out.push_back({{a.p.x+t*(b.p.x-a.p.x),a.p.y+t*(b.p.y-a.p.y),
                               a.p.z+t*(b.p.z-a.p.z),a.p.w+t*(b.p.w-a.p.w)},
                               a.u+t*(b.u-a.u),a.v+t*(b.v-a.v),a.brightness+t*(b.brightness-a.brightness)});
            }
            if(db>=0) out.push_back(b);
            a=b; da=db;
        }
        p=std::move(out);
    }
    return p;
}
float edge(Vec3 a,Vec3 b,float x,float y) { return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x); }
acvr_result raster(const std::array<ClipVertex,3> &v,const acvr_eye &e,Image &image,
                   const Triangle &triangle,const MaterialPacket &materials,bool sky) {
    std::array<Vec3,3> s{};
    for(size_t i=0;i<3;++i) {
        if(v[i].p.w<=0) return ACVR_OK;
        s[i]={static_cast<float>(e.rect_x+(v[i].p.x/v[i].p.w+1)*.5*e.rect_width),
              static_cast<float>(e.rect_y+(v[i].p.y/v[i].p.w+1)*.5*e.rect_height),
              static_cast<float>((v[i].p.z/v[i].p.w+1)*.5)};
    }
    float area=edge(s[0],s[1],s[2].x,s[2].y);
    if(!finite(area) || std::abs(area)<1e-8f) return ACVR_OK;
    int xmin=std::max(e.rect_x,static_cast<int>(std::floor(std::min({s[0].x,s[1].x,s[2].x}))));
    int xmax=std::min(e.rect_x+static_cast<int>(e.rect_width)-1,static_cast<int>(std::ceil(std::max({s[0].x,s[1].x,s[2].x}))));
    int ymin=std::max(e.rect_y,static_cast<int>(std::floor(std::min({s[0].y,s[1].y,s[2].y}))));
    int ymax=std::min(e.rect_y+static_cast<int>(e.rect_height)-1,static_cast<int>(std::ceil(std::max({s[0].y,s[1].y,s[2].y}))));
    for(int y=ymin;y<=ymax;++y) for(int x=xmin;x<=xmax;++x) {
        float px=static_cast<float>(x)+.5f,py=static_cast<float>(y)+.5f;
        float a=edge(s[1],s[2],px,py)/area,b=edge(s[2],s[0],px,py)/area,c=1-a-b;
        if(a<0 || b<0 || c<0) continue;
        float z=sky?1.f:a*s[0].z+b*s[1].z+c*s[2].z;
        size_t p=static_cast<size_t>(image.height-1-static_cast<uint32_t>(y))*image.width+static_cast<uint32_t>(x);
        if(z<=image.depth[p]) {
            uint32_t rgb=triangle.rgb;
            if(triangle.material!=NoMaterial) {
                const double w0=a/v[0].p.w,w1=b/v[1].p.w,w2=c/v[2].p.w,denominator=w0+w1+w2;
                if(!std::isfinite(denominator) || denominator<=0) return ACVR_BAD_ARGUMENT;
                const double u=(w0*v[0].u+w1*v[1].u+w2*v[2].u)/denominator;
                const double tv=(w0*v[0].v+w1*v[1].v+w2*v[2].v)/denominator;
                const double brightness=std::clamp((w0*v[0].brightness+w1*v[1].brightness+w2*v[2].brightness)/denominator,0.,255.);
                const auto r=sample_material(materials,triangle.material,u,tv,brightness,rgb);
                if(r!=ACVR_OK) return r;
            }
            image.depth[p]=z;image.rgb[p]=rgb;
        }
    }
    return ACVR_OK;
}
}
Image::Image(uint32_t w,uint32_t h):width(w),height(h) {
    if(!w || !h || w>8192 || h>8192) throw std::invalid_argument("CPU image dimensions out of range");
    rgb.resize(static_cast<size_t>(w)*h); depth.resize(rgb.size(),1.f);
}
acvr_game_camera synthetic_camera() {
    acvr_game_camera c{}; ACVR_INIT(&c); c.flags=ACVR_CAMERA_VIEW_LOCAL;
    c.raster_width=640; c.raster_height=480;
    c.focal_x_px=c.focal_y_px=480; c.centre_x_px=320; c.centre_y_px=240;
    c.viewport_px[2]=640; c.viewport_px[3]=480;
    for(int i=0;i<16;i+=5) c.view_from_scene[i]=1;
    c.near_scene=.1f; c.far_scene=100; return c;
}
Vec3 unproject(const ProjectedVertex &v,const acvr_game_camera &c) {
    return {(v.x16/16-c.centre_x_px)*v.depth/c.focal_x_px,
            (c.centre_y_px-v.y16/16)*v.depth/c.focal_y_px,-v.depth};
}
acvr_result prepare(const SceneInput &in,uint64_t id,Frame &out) {
    if(!finite(in.hud_depth_scene) || in.hud_depth_scene<=0) return ACVR_BAD_ARGUMENT;
    if(auto r=validate_material_packet(in.materials);r!=ACVR_OK) return r;
    Frame f; f.id=id; f.cameras=in.cameras;f.materials=in.materials;
    for(size_t i=0;i<f.cameras.size();++i)
        if(!camera_valid(f.cameras[i]) || f.cameras[i].camera_id!=i) return ACVR_BAD_ARGUMENT;
    for(const auto &p:in.polygons) {
        if(p.camera_id>=f.cameras.size()) return ACVR_BAD_ARGUMENT;
        if(p.layer!=Layer::World && p.layer!=Layer::Hud && p.layer!=Layer::Backdrop && p.layer!=Layer::GunFlash) return ACVR_BAD_ARGUMENT;
        Triangle t{}; t.camera_id=p.camera_id; t.layer=p.layer; t.rgb=p.rgb&0xffffff;
        t.material=p.material;t.attributes=p.attributes;
        if(t.material!=NoMaterial) {
            if(t.material>=f.materials.materials.size()) return ACVR_BAD_ARGUMENT;
            for(const auto &attribute:t.attributes) if(!valid_material_vertex(attribute)) return ACVR_BAD_ARGUMENT;
        }
        for(size_t i=0;i<3;++i) {
            ProjectedVertex v=p.vertices[i];
            if(!finite(v.x16) || !finite(v.y16) || !finite(v.depth) || v.depth<=0) return ACVR_BAD_ARGUMENT;
            if(p.layer==Layer::Hud) v.depth=in.hud_depth_scene;
            t.vertices[i]=unproject(v,f.cameras[p.camera_id]);
            if(!finite(t.vertices[i])) return ACVR_BAD_ARGUMENT;
        }
        if(p.layer!=Layer::GunFlash) f.triangles.push_back(t);
    }
    out=std::move(f); return ACVR_OK;
}
acvr_result project_gun(Vec3 p,const acvr_game_camera &c,float &x,float &y,bool &off) {
    x=y=.5f; off=true;
    if(!camera_valid(c) || !finite(p) || p.z>=0) return ACVR_BAD_ARGUMENT;
    float sx=c.centre_x_px+c.focal_x_px*p.x/-p.z;
    float sy=c.centre_y_px-c.focal_y_px*p.y/-p.z;
    x=sx/static_cast<float>(c.raster_width); y=sy/static_cast<float>(c.raster_height);
    if(!finite(x) || !finite(y)) { x=y=.5f; return ACVR_BAD_ARGUMENT; }
    off=sx<0 || sy<0 || sx>static_cast<float>(c.raster_width) || sy>static_cast<float>(c.raster_height) ||
        sx<c.viewport_px[0] || sy<c.viewport_px[1] ||
        sx>c.viewport_px[0]+c.viewport_px[2] || sy>c.viewport_px[1]+c.viewport_px[3];
    return ACVR_OK;
}
acvr_result raycast(const Frame &f,const acvr_ray &r,acvr_hit &out) {
    if(r.size<sizeof(r) || r.version!=ACVR_STRUCT_VERSION || out.size<ACVR_HIT_V1_SIZE || out.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    Vec3 o{r.origin_scene[0],r.origin_scene[1],r.origin_scene[2]},d{r.direction_scene[0],r.direction_scene[1],r.direction_scene[2]};
    if(!finite(o) || !finite(d) || std::abs(dot(d,d)-1)>1e-4f || !finite(r.max_distance_scene) || r.max_distance_scene<=0) return ACVR_BAD_ARGUMENT;
    out.found=0; out.camera_id=ACVR_NO_CAMERA; out.distance_scene=0;
    std::fill(std::begin(out.position_scene),std::end(out.position_scene),0.f);
    const bool has_tail=out.size>=sizeof(acvr_hit);
    if(has_tail) { out.flags=0; out.screen_x=out.screen_y=.5f; }
    float nearest=r.max_distance_scene;
    for(const auto &t:f.triangles) {
        if(t.layer!=Layer::World) continue;
        Vec3 e1=sub(t.vertices[1],t.vertices[0]),e2=sub(t.vertices[2],t.vertices[0]);
        Vec3 p=cross(d,e2); float det=dot(e1,p);
        if(!finite(det) || std::abs(det)<1e-8f) continue;
        float inv=1/det; Vec3 s=sub(o,t.vertices[0]); float u=dot(s,p)*inv;
        if(!finite(u) || u<0 || u>1) continue;
        Vec3 q=cross(s,e1); float v=dot(d,q)*inv;
        if(!finite(v) || v<0 || u+v>1) continue;
        float distance=dot(e2,q)*inv;
        if(!finite(distance) || distance<0 || distance>nearest) continue;
        Vec3 position{o.x+d.x*distance,o.y+d.y*distance,o.z+d.z*distance};
        if(!finite(position)) continue;
        nearest=distance; out.found=1; out.camera_id=t.camera_id; out.distance_scene=distance;
        out.position_scene[0]=position.x; out.position_scene[1]=position.y; out.position_scene[2]=position.z;
    }
    if(has_tail && out.found && out.camera_id<f.cameras.size()) {
        bool offscreen;
        if(project_gun({out.position_scene[0],out.position_scene[1],out.position_scene[2]},
                       f.cameras[out.camera_id],out.screen_x,out.screen_y,offscreen)==ACVR_OK)
            out.flags=ACVR_HIT_GUN_COORDS;
    }
    return ACVR_OK;
}
acvr_result draw_cpu(const Frame &f,const acvr_eye &e,Image &image,bool hud_only) {
    if(e.size<sizeof(e) || e.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(e.array_layer!=0 || e.rect_x<0 || e.rect_y<0 || !e.rect_width || !e.rect_height ||
        static_cast<uint64_t>(e.rect_x)+e.rect_width>image.width ||
        static_cast<uint64_t>(e.rect_y)+e.rect_height>image.height ||
        image.rgb.size()!=static_cast<size_t>(image.width)*image.height || image.depth.size()!=image.rgb.size()) return ACVR_BAD_ARGUMENT;
    for(float v:e.view_from_scene) if(!finite(v)) return ACVR_BAD_ARGUMENT;
    for(float v:e.projection_from_view) if(!finite(v)) return ACVR_BAD_ARGUMENT;
    if(auto r=validate_material_packet(f.materials);r!=ACVR_OK) return r;
    for(const auto &t:f.triangles) if(t.material!=NoMaterial) {
        if(t.material>=f.materials.materials.size()) return ACVR_BAD_ARGUMENT;
        for(const auto &attribute:t.attributes) if(!valid_material_vertex(attribute)) return ACVR_BAD_ARGUMENT;
    }
    for(uint32_t y=0;y<e.rect_height;++y) for(uint32_t x=0;x<e.rect_width;++x) {
        size_t p=static_cast<size_t>(image.height-1-static_cast<uint32_t>(e.rect_y)-y)*image.width+static_cast<uint32_t>(e.rect_x)+x;
        image.rgb[p]=0; image.depth[p]=1;
    }
    for(const auto &t:f.triangles) {
        if(hud_only!=(t.layer==Layer::Hud)) continue;
        std::vector<ClipVertex> poly;
        for(size_t k=0;k<t.vertices.size();++k) {
            const auto v=t.vertices[k];const auto attr=t.attributes[k];
            V4 p=transform(e.view_from_scene,{v.x,v.y,v.z,t.layer==Layer::Backdrop?0.f:1.f});
            p=transform(e.projection_from_view,p);
            if(t.layer==Layer::Backdrop) p.z=p.w; // infinity, with rotation but no translation
            poly.push_back({p,attr.u,attr.v,attr.brightness});
        }
        poly=clip(std::move(poly));
        for(size_t i=1;i+1<poly.size();++i) {
            const auto r=raster({poly[0],poly[i],poly[i+1]},e,image,t,f.materials,t.layer==Layer::Backdrop);
            if(r!=ACVR_OK) return r;
        }
    }
    return ACVR_OK;
}
acvr_eye desktop_eye(uint32_t index,float eye_x,float convergence,uint32_t w,uint32_t h) {
    if(index>1 || !w || !h || w>8192 || h>8192 || !finite(eye_x) || !finite(convergence) || convergence<=0) throw std::invalid_argument("Invalid desktop eye");
    acvr_eye e{}; ACVR_INIT(&e); e.eye_index=index;
    e.rect_width=w; e.rect_height=h; e.rect_x=static_cast<int32_t>(index*w);
    for(int i=0;i<16;i+=5) e.view_from_scene[i]=1;
    e.view_from_scene[12]=-eye_x;
    constexpr float near=.1f,far=100;
    float t=near*.5f,r=t*static_cast<float>(w)/static_cast<float>(h),shift=-eye_x*near/convergence;
    float l=-r+shift; r+=shift;
    e.projection_from_view[0]=2*near/(r-l); e.projection_from_view[5]=near/t;
    e.projection_from_view[8]=(r+l)/(r-l);
    e.projection_from_view[10]=-(far+near)/(far-near);
    e.projection_from_view[11]=-1; e.projection_from_view[14]=-2*far*near/(far-near);
    return e;
}
std::array<uint32_t,2> time_crisis_adc(float x,float y) {
    if(!finite(x) || !finite(y)) throw std::invalid_argument("Invalid gun coordinates");
    return {static_cast<uint32_t>(std::floor(68.f+std::clamp(x,0.f,1.f)*626.f)),
            static_cast<uint32_t>(std::floor(43.f+std::clamp(y,0.f,1.f)*241.f))};
}
SceneInput synthetic_cube() {
    SceneInput in; in.cameras.push_back(synthetic_camera());
    std::array<Vec3,8> v{{{-.5f,-.5f,-2},{.5f,-.5f,-2},{.5f,.5f,-2},{-.5f,.5f,-2},
                           {-.5f,-.5f,-3},{.5f,-.5f,-3},{.5f,.5f,-3},{-.5f,.5f,-3}}};
    constexpr int faces[12][3]={{0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,4,5},{0,5,1},
        {1,5,6},{1,6,2},{2,6,7},{2,7,3},{3,7,4},{3,4,0}};
    constexpr uint32_t colours[6]={0x44aaff,0x8877cc,0x66bb88,0xffbb44,0xee6688,0x66cccc};
    for(int i=0;i<12;++i) {
        Polygon p; p.rgb=colours[i/2];
        for(size_t j=0;j<3;++j) {
            Vec3 a=v[static_cast<size_t>(faces[i][j])]; float depth=-a.z;
            p.vertices[j]={(320+480*a.x/depth)*16,(240-480*a.y/depth)*16,depth};
        }
        in.polygons.push_back(p);
    }
    return in;
}
SceneInput synthetic_material_cube() {
    auto in=synthetic_cube();auto &packet=in.materials;
    packet.addressing=TileAddressing::Fixed16;packet.palette.resize(0x8000);
    packet.cells.push_back({0,0,0});packet.tiles.emplace_back();
    for(size_t y=0;y<16;++y) for(size_t x=0;x<16;++x)
        packet.tiles[0].pens[y*16+x]=static_cast<uint8_t>(1+((x/4+y/4)&1));
    for(size_t face=0;face<6;++face) {
        Material material;material.colour_word=static_cast<uint32_t>(face<<8);packet.materials.push_back(material);
        const auto colour=in.polygons[face*2].rgb;
        packet.palette[face*256+1]=colour;packet.palette[face*256+2]=colour^0xffffff;
        for(size_t t=0;t<2;++t) {
            auto &polygon=in.polygons[face*2+t];polygon.material=static_cast<uint32_t>(face);
            if(t==0) polygon.attributes={MaterialVertex{.5f,.5f,64},{15.5f,.5f,64},{15.5f,15.5f,64}};
            else polygon.attributes={MaterialVertex{.5f,.5f,64},{15.5f,15.5f,64},{.5f,15.5f,64}};
        }
    }
    return in;
}
}
