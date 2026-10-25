// SPDX-License-Identifier: GPL-3.0-or-later
// Theatre scene for one eye: the game picture on a (curved) screen, the held
// gun model and the aim dot, all depth-tested in one image, so the gun is in
// front of the picture wherever it is nearer. Needs a current OpenGL 3.3
// context; container objects (VAOs, FBOs) belong to the context that made them.
#pragma once
#include "WinGL.h"
#include "MiniJson.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tgl {
#define TGL_FUNCS(X) \
  X(PFNGLCREATESHADERPROC, CreateShader) X(PFNGLSHADERSOURCEPROC, ShaderSource) X(PFNGLCOMPILESHADERPROC, CompileShader) \
  X(PFNGLGETSHADERIVPROC, GetShaderiv) X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) X(PFNGLDELETESHADERPROC, DeleteShader) \
  X(PFNGLCREATEPROGRAMPROC, CreateProgram) X(PFNGLATTACHSHADERPROC, AttachShader) X(PFNGLLINKPROGRAMPROC, LinkProgram) \
  X(PFNGLGETPROGRAMIVPROC, GetProgramiv) X(PFNGLDELETEPROGRAMPROC, DeleteProgram) X(PFNGLUSEPROGRAMPROC, UseProgram) \
  X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) X(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv) \
  X(PFNGLUNIFORM3FVPROC, Uniform3fv) X(PFNGLUNIFORM4FVPROC, Uniform4fv) X(PFNGLUNIFORM2FPROC, Uniform2f) X(PFNGLUNIFORM1IPROC, Uniform1i) X(PFNGLUNIFORM1FPROC, Uniform1f) \
  X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays) \
  X(PFNGLGENBUFFERSPROC, GenBuffers) X(PFNGLBINDBUFFERPROC, BindBuffer) X(PFNGLBUFFERDATAPROC, BufferData) X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) \
  X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) \
  X(PFNGLACTIVETEXTUREPROC, ActiveTexture) X(PFNGLGENERATEMIPMAPPROC, GenerateMipmap) \
  X(PFNGLGENRENDERBUFFERSPROC, GenRenderbuffers) X(PFNGLBINDRENDERBUFFERPROC, BindRenderbuffer) \
  X(PFNGLRENDERBUFFERSTORAGEPROC, RenderbufferStorage) X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, FramebufferRenderbuffer) \
  X(PFNGLDELETERENDERBUFFERSPROC, DeleteRenderbuffers) X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus)
#define X(type, name) inline type name = nullptr;
TGL_FUNCS(X)
#undef X
inline bool Load()
{
  if (!Host_LoadGL()) return false;
#define X(type, name) name = reinterpret_cast<type>(wglGetProcAddress("gl" #name)); if (!name) return false;
  TGL_FUNCS(X)
#undef X
  return true;
}
}

namespace theatre {
struct Mat4 {
  std::array<float, 16> m{};   // column-major
  static Mat4 Identity() { Mat4 r; r.m = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }; return r; }
  static Mat4 Translation(float x, float y, float z) { auto r = Identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
  static Mat4 Rotation(float x, float y, float z, float a)
  {
    const float n = std::sqrt(x * x + y * y + z * z); x /= n; y /= n; z /= n;
    const float c = std::cos(a), s = std::sin(a), t = 1 - c; auto r = Identity();
    r.m[0] = t * x * x + c; r.m[4] = t * x * y - s * z; r.m[8] = t * x * z + s * y;
    r.m[1] = t * x * y + s * z; r.m[5] = t * y * y + c; r.m[9] = t * y * z - s * x;
    r.m[2] = t * x * z - s * y; r.m[6] = t * y * z + s * x; r.m[10] = t * z * z + c; return r;
  }
  static Mat4 FromPose(const float p[3], const float q[4])
  {
    const float x = q[0], y = q[1], z = q[2], w = q[3]; auto r = Identity();
    r.m[0] = 1 - 2 * (y * y + z * z); r.m[4] = 2 * (x * y - z * w); r.m[8] = 2 * (x * z + y * w);
    r.m[1] = 2 * (x * y + z * w); r.m[5] = 1 - 2 * (x * x + z * z); r.m[9] = 2 * (y * z - x * w);
    r.m[2] = 2 * (x * z - y * w); r.m[6] = 2 * (y * z + x * w); r.m[10] = 1 - 2 * (x * x + y * y);
    r.m[12] = p[0]; r.m[13] = p[1]; r.m[14] = p[2]; return r;
  }
  Mat4 RigidInverse() const
  {
    auto r = Identity();
    for (int c = 0; c < 3; ++c) for (int row = 0; row < 3; ++row) r.m[c * 4 + row] = m[row * 4 + c];
    for (int row = 0; row < 3; ++row) r.m[12 + row] = -(r.m[row] * m[12] + r.m[4 + row] * m[13] + r.m[8 + row] * m[14]);
    return r;
  }
  // Arguments are the tangents of the OpenXR field-of-view angles.
  static Mat4 Projection(float l, float r, float d, float u, float n = .03f, float f = 50.f)
  {
    Mat4 p; p.m[0] = 2 / (r - l); p.m[5] = 2 / (u - d); p.m[8] = (r + l) / (r - l); p.m[9] = (u + d) / (u - d);
    p.m[10] = -(f + n) / (f - n); p.m[11] = -1; p.m[14] = -2 * f * n / (f - n); return p;
  }
};
inline Mat4 operator*(const Mat4 &a, const Mat4 &b)
{
  Mat4 r; for (int c = 0; c < 4; ++c) for (int row = 0; row < 4; ++row) for (int k = 0; k < 4; ++k) r.m[c * 4 + row] += a.m[k * 4 + row] * b.m[c * 4 + k];
  return r;
}

// Where the screen is: its frame (origin at the viewer's head when it was placed,
// facing -Z), radius/distance, size and curvature. curve=0 is flat.
struct ScreenGeometry {
  Mat4 frame = Mat4::Identity();
  float distance = 2.5f, width = 2.4f, height = 1.8f, curve = 1.f, lift = 0.f;
  float arc() const { return width / distance; }
  // Point on the screen for picture coordinates (0,0 top left .. 1,1 bottom right), in the frame.
  std::array<float, 3> local(float u, float v) const
  {
    const float y = lift + (.5f - v) * height;
    if (curve <= 0.f) return { (u - .5f) * width, y, -distance };
    const float r = distance / curve, a = (u - .5f) * width / r;   // radius grows as curvature falls
    return { r * std::sin(a), y, -distance + r * (1 - std::cos(a)) };
  }
  // Intersects a ray (world space) with the screen. Returns false when it misses.
  bool hit(const float origin[3], const float dir[3], float &u, float &v) const
  {
    const Mat4 inv = frame.RigidInverse();
    float o[3], d[3];
    for (int i = 0; i < 3; ++i) {
      o[i] = inv.m[i] * origin[0] + inv.m[4 + i] * origin[1] + inv.m[8 + i] * origin[2] + inv.m[12 + i];
      d[i] = inv.m[i] * dir[0] + inv.m[4 + i] * dir[1] + inv.m[8 + i] * dir[2];
    }
    float x, y;
    if (curve <= 0.f) {
      if (!(d[2] < -1e-6f)) return false;
      const float t = (-distance - o[2]) / d[2]; if (t <= 0) return false;
      x = o[0] + t * d[0]; y = o[1] + t * d[1]; u = x / width + .5f;
    } else {
      // Cylinder of radius r about a vertical axis at (0, -distance + r).
      const float r = distance / curve, cz = -distance + r;
      const float px = o[0], pz = o[2] - cz;
      const float A = d[0] * d[0] + d[2] * d[2], B = 2 * (px * d[0] + pz * d[2]), C = px * px + pz * pz - r * r;
      const float disc = B * B - 4 * A * C; if (A < 1e-9f || disc < 0) return false;
      const float s = std::sqrt(disc); float t = (-B - s) / (2 * A); if (t <= 0) t = (-B + s) / (2 * A); if (t <= 0) return false;
      const float hx = px + t * d[0], hz = pz + t * d[2];
      if (hz > 0) return false;   // the back half of the cylinder
      y = o[1] + t * d[1]; u = std::atan2(hx, -hz) * r / width + .5f;
    }
    v = .5f - (y - lift) / height;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
  }
};

inline GLuint Shader(GLenum type, const char *source)
{
  GLuint s = tgl::CreateShader(type); tgl::ShaderSource(s, 1, &source, nullptr); tgl::CompileShader(s);
  GLint ok = 0; tgl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[2048]{}; tgl::GetShaderInfoLog(s, sizeof log, nullptr, log); tgl::DeleteShader(s); throw std::runtime_error(std::string("shader: ") + log); }
  return s;
}
inline GLuint Program(const char *vs, const char *fs)
{
  GLuint v = Shader(GL_VERTEX_SHADER, vs), f = Shader(GL_FRAGMENT_SHADER, fs), p = tgl::CreateProgram();
  tgl::AttachShader(p, v); tgl::AttachShader(p, f); tgl::LinkProgram(p); tgl::DeleteShader(v); tgl::DeleteShader(f);
  GLint ok = 0; tgl::GetProgramiv(p, GL_LINK_STATUS, &ok); if (!ok) throw std::runtime_error("program link failed");
  return p;
}

// The gun: one of the project's GLB models (metres, +Y up, muzzle along -Z) with
// its TOML for the trigger motion and colours.
class Gun {
public:
  bool loaded() const { return lod0 >= 0; }
  void load(const std::string &glb, const std::string &toml)
  {
    std::ifstream f(glb, std::ios::binary); if (!f) throw std::runtime_error("cannot read " + glb);
    std::string b((std::istreambuf_iterator<char>(f)), {});
    auto u32 = [&](std::size_t at) { std::uint32_t v; std::memcpy(&v, b.data() + at, 4); return v; };
    if (b.size() < 28 || u32(0) != 0x46546c67 || u32(4) != 2 || u32(16) != 0x4e4f534a) throw std::runtime_error("not a GLB file");
    const std::size_t jn = u32(12); if (jn > b.size() - 28) throw std::runtime_error("bad GLB JSON chunk");
    doc = mini_json::parse(b.substr(20, jn));
    const std::size_t bo = 20 + jn; if (bo + 8 > b.size() || u32(bo + 4) != 0x004e4942 || u32(bo) > b.size() - bo - 8) throw std::runtime_error("bad GLB binary chunk");
    bin.assign(b.begin() + bo + 8, b.begin() + bo + 8 + u32(bo));
    for (const auto &j : doc["nodes"].a) {
      Node n; if (j.has("name")) n.name = j["name"].str(); if (j.has("mesh")) n.mesh = (int)j["mesh"].num();
      if (j.has("children")) for (const auto &c : j["children"].a) n.children.push_back((int)c.num());
      if (j.has("matrix")) for (int k = 0; k < 16; ++k) n.local.m[k] = (float)j["matrix"][k].num();
      else {
        float p[3]{}, q[4]{ 0,0,0,1 };
        if (j.has("translation")) for (int k = 0; k < 3; ++k) p[k] = (float)j["translation"][k].num();
        if (j.has("rotation")) for (int k = 0; k < 4; ++k) q[k] = (float)j["rotation"][k].num();
        n.local = Mat4::FromPose(p, q);
        if (j.has("scale")) for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) n.local.m[c * 4 + r] *= (float)j["scale"][c].num();
      }
      nodes.push_back(n);
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) for (int c : nodes[i].children) {
      if (c < 0 || (std::size_t)c >= nodes.size() || nodes[c].parent >= 0) throw std::runtime_error("bad gun hierarchy");
      nodes[c].parent = (int)i;
    }
    grip = find("grip"); muzzle = find("muzzle"); lod0 = find("LOD0");
    metadata(toml);
  }
  // Must run in the context that draws.
  void upload()
  {
    if (!loaded()) return;
    // Matte parts are lit like libacvr's gun renderer (assets/guns/runtime-lighting.json, gun_lighting_r2): 0.35
    // ambient + 0.65 of a light fixed in the camera's space, no specular, no rim; `lightW` is that light turned
    // into world space for the eye being drawn. A part whose tint gives metal/rough values (look.z = 1) is
    // polished: Schlick Fresnel reflecting a small made-up studio (dark floor, lighter ceiling, a bright horizon
    // band and one key light), with roughness widened where normals change fast so highlights do not sparkle.
    program = Program(
      "#version 330 core\nlayout(location=0)in vec3 p;layout(location=1)in vec3 n;uniform mat4 mvp,model;out vec3 N,W;"
      "void main(){gl_Position=mvp*vec4(p,1);N=mat3(model)*n;W=(model*vec4(p,1)).xyz;}",
      "#version 330 core\nin vec3 N,W;uniform vec4 base,look,eye;uniform vec3 lightW;uniform int encode;out vec4 o;"
      "const vec3 L=normalize(vec3(-.4,.8,.6));"
      "vec3 env(vec3 r,float g){float y=r.y;"
      "vec3 e=mix(vec3(.12,.11,.10),mix(vec3(.38,.37,.36),vec3(.85,.85,.88),smoothstep(0.,.9,y)),smoothstep(-.45,.05,y));"
      "e+=vec3(1.3,1.2,1.05)*exp(-abs(y-.05)*mix(9.,3.,g))*(1.-.4*g);"
      "e+=vec3(6.,5.6,5.)*pow(max(dot(r,L),0.),mix(300.,12.,g))*(1.-.7*g);return e;}"
      "void main(){vec3 n=normalize(N);float l=.35+.65*max(dot(n,normalize(lightW)),0.);vec3 c;"
      "if(look.z<.5)c=base.rgb*l;"
      "else{vec3 v=normalize(eye.xyz-W);if(dot(n,v)<0.)n=-n;vec3 r=reflect(-v,n);"
      "float m=clamp(look.x,0.,1.),g=clamp(look.y+.6*length(fwidth(n)),.04,1.);"
      "vec3 F0=mix(vec3(.04),base.rgb,m);vec3 F=F0+(max(vec3(1.-g),F0)-F0)*pow(1.-max(dot(n,v),0.),5.);"
      "c=base.rgb*(1.-m)*l+F*env(r,g)+base.rgb*m*.10*l;c=c/(1.+.25*c);}"
      "o=vec4(encode!=0?pow(c,vec3(1./2.2)):c,1);}");
    meshes.assign(doc["meshes"].size(), {});
    for (std::size_t mi = 0; mi < meshes.size(); ++mi) for (const auto &p : doc["meshes"][mi]["primitives"].a) {
      if (p.has("mode") && (int)p["mode"].num() != 4) continue;
      auto pos = accessor((unsigned)p["attributes"]["POSITION"].num(), 12), nor = accessor((unsigned)p["attributes"]["NORMAL"].num(), 12);
      if (pos.type != 5126 || nor.type != 5126 || pos.count != nor.count) throw std::runtime_error("unsupported gun vertices");
      const int ct = (int)doc["accessors"][(std::size_t)p["indices"].num()]["componentType"].num();
      const std::size_t ib = ct == 5121 ? 1 : ct == 5123 ? 2 : ct == 5125 ? 4 : 0; if (!ib) throw std::runtime_error("unsupported gun indices");
      auto ix = accessor((unsigned)p["indices"].num(), ib);
      std::vector<float> v(pos.count * 6); std::vector<std::uint32_t> idx(ix.count);
      for (std::size_t k = 0; k < pos.count; ++k) { std::memcpy(&v[k * 6], pos.ptr + k * pos.stride, 12); std::memcpy(&v[k * 6 + 3], nor.ptr + k * nor.stride, 12); }
      for (std::size_t k = 0; k < ix.count; ++k) { std::uint32_t x = 0; std::memcpy(&x, ix.ptr + k * ix.stride, ib); if (x >= pos.count) throw std::runtime_error("gun index out of range"); idx[k] = x; }
      Prim out; out.count = (GLsizei)ix.count;
      if (p.has("material")) {
        const auto &mat = doc["materials"][(std::size_t)p["material"].num()];
        const std::string name = mat.has("name") ? mat["name"].str() : "";
        // body and accent: the role's tint times the material's own base factor, as libacvr does. The factor is
        // read from the GLB (the model builder owns it), not assumed; a material without one counts as 1.
        const bool hasFactor = mat.has("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].has("baseColorFactor");
        auto factor = [&](int c) { return hasFactor ? (float)mat["pbrMetallicRoughness"]["baseColorFactor"][c].num() : 1.f; };
        if (name == "body" || name == "accent") {
          const bool isBody = name == "body"; out.color = isBody ? body : accent; out.look = isBody ? bodyLook : accentLook;
          for (int c = 0; c < 3; ++c) out.color[c] *= factor(c);
        }
        else if (hasFactor)
          for (int c = 0; c < 4; ++c) out.color[c] = factor(c);
      }
      tgl::GenVertexArrays(1, &out.vao); tgl::BindVertexArray(out.vao);
      tgl::GenBuffers(1, &out.vbo); tgl::BindBuffer(GL_ARRAY_BUFFER, out.vbo); tgl::BufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
      tgl::GenBuffers(1, &out.ibo); tgl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ibo); tgl::BufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
      tgl::EnableVertexAttribArray(0); tgl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, nullptr);
      tgl::EnableVertexAttribArray(1); tgl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void *>(12));
      meshes[mi].push_back(out);
    }
    tgl::BindVertexArray(0);
  }
  void release()
  {
    for (auto &mesh : meshes) for (auto &p : mesh) { tgl::DeleteVertexArrays(1, &p.vao); tgl::DeleteBuffers(1, &p.vbo); tgl::DeleteBuffers(1, &p.ibo); }
    meshes.clear(); if (program) tgl::DeleteProgram(program); program = 0;
  }
  // The model's root: the bore along the tracked aim ray, the grip at the hand.
  Mat4 alignedRoot(const Mat4 &gripWorld, const Mat4 &aimWorld, float kick) const
  {
    using V = std::array<float, 3>;
    auto dot = [](const V &a, const V &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    auto unit = [&](V v) { const float n = std::sqrt(dot(v, v)); if (n < 1e-7f) return V{ 0, 1, 0 }; for (auto &x : v) x /= n; return v; };
    auto cross = [](const V &a, const V &b) -> V { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; };
    const V back = unit({ aimWorld.m[8], aimWorld.m[9], aimWorld.m[10] });
    V up{ gripWorld.m[4], gripWorld.m[5], gripWorld.m[6] };
    float along = dot(up, back); for (int i = 0; i < 3; ++i) up[i] -= along * back[i];
    if (dot(up, up) < 1e-6f) { up = { aimWorld.m[4], aimWorld.m[5], aimWorld.m[6] }; along = dot(up, back); for (int i = 0; i < 3; ++i) up[i] -= along * back[i]; }
    const V right = unit(cross(up, back)); up = unit(cross(back, right));
    auto basis = Mat4::Identity(); for (int i = 0; i < 3; ++i) { basis.m[i] = right[i]; basis.m[4 + i] = up[i]; basis.m[8 + i] = back[i]; }
    auto bore = world(muzzle, 0); bore.m[12] = bore.m[13] = bore.m[14] = 0;
    auto root = basis * bore.RigidInverse(); const auto anchor = world(grip, 0);
    for (int i = 0; i < 3; ++i)
      root.m[12 + i] = gripWorld.m[12 + i] - root.m[i] * anchor.m[12] - root.m[4 + i] * anchor.m[13] - root.m[8 + i] * anchor.m[14] + .01f * kick * up[i] + .02f * kick * back[i];
    return root;
  }
  // recoil: 0..1 of the slide's travel this frame (an arcade-style blowback slide, if the model has one).
  float recoilMs() const { return slideMs; }
  // eye: the viewing eye's position in world space (for the polished parts' reflections).
  void draw(const Mat4 &root, const Mat4 &viewProj, float trigger, bool encode, float recoil = 0.f, const float *eye = nullptr, const float *lightWorld = nullptr)
  {
    if (!program) return;
    m_recoil = std::fmin(std::fmax(recoil, 0.f), 1.f);
    tgl::UseProgram(program); tgl::Uniform1i(tgl::GetUniformLocation(program, "encode"), encode ? 1 : 0);
    const float at[4] = { eye ? eye[0] : 0.f, eye ? eye[1] : 0.f, eye ? eye[2] : 0.f, 1 };
    tgl::Uniform4fv(tgl::GetUniformLocation(program, "eye"), 1, at);
    const float up[3] = { 0.f, 1.f, 0.f };
    tgl::Uniform3fv(tgl::GetUniformLocation(program, "lightW"), 1, lightWorld ? lightWorld : up);
    drawNode(lod0, root, viewProj, trigger);
  }
private:
  struct Node { std::string name; Mat4 local = Mat4::Identity(); int mesh = -1, parent = -1; std::vector<int> children; };
  struct Prim { GLuint vao = 0, vbo = 0, ibo = 0; GLsizei count = 0; std::array<float, 4> color{ .5f, .5f, .5f, 1 }, look{ 0, 1, 0, 0 }; };
  struct Access { const unsigned char *ptr; std::size_t stride, count; int type; };
  mini_json::Value doc; std::vector<unsigned char> bin; std::vector<Node> nodes; std::vector<std::vector<Prim>> meshes;
  int grip = -1, muzzle = -1, lod0 = -1; std::array<int, 2> triggers{ -1, -1 };
  std::array<float, 3> axis{ -1, 0, 0 }; std::array<float, 2> range{ 0, 0 };
  std::array<int, 2> slides{ -1, -1 }; std::array<float, 3> slideAxis{ 0, 0, 1 }; std::array<float, 2> slideRange{ 0, 0 };   // metres, or radians when slideRotates
  bool slideRotates = false; float slideMs = 45, m_recoil = 0;
  std::array<float, 4> body{ .1f, .1f, .12f, 1 }, accent{ .3f, .3f, .32f, 1 };
  std::array<float, 4> bodyLook{ 0, 1, 0, 0 }, accentLook{ 0, 1, 0, 0 };   // metal, roughness, polished (1) or matte (0)
  GLuint program = 0;
  int find(const std::string &s) const { for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i].name == s) return (int)i; throw std::runtime_error("gun model lacks node " + s); }
  Access accessor(unsigned index, std::size_t element)
  {
    const auto &a = doc["accessors"][index]; const auto &v = doc["bufferViews"][(std::size_t)a["bufferView"].num()];
    const std::size_t off = (std::size_t)((v.has("byteOffset") ? v["byteOffset"].num() : 0) + (a.has("byteOffset") ? a["byteOffset"].num() : 0));
    const std::size_t stride = v.has("byteStride") ? (std::size_t)v["byteStride"].num() : element, count = (std::size_t)a["count"].num();
    if (!count || stride < element || off > bin.size() || element > bin.size() - off || (count - 1) > (bin.size() - off - element) / stride) throw std::runtime_error("gun accessor out of bounds");
    return { bin.data() + off, stride, count, (int)a["componentType"].num() };
  }
  Mat4 world(int i, float trigger) const
  {
    auto local = nodes[i].local;
    if ((i == triggers[0] || i == triggers[1]) && range[1] != range[0]) local = local * Mat4::Rotation(axis[0], axis[1], axis[2], range[0] + std::fmin(std::fmax(trigger, 0.f), 1.f) * (range[1] - range[0]));
    if ((i == slides[0] || i == slides[1]) && m_recoil > 0) {
      const float d = slideRange[0] + m_recoil * (slideRange[1] - slideRange[0]);
      // kind = "slide" moves the node along the axis; kind = "rotate" turns it about the axis through its own origin.
      local = local * (slideRotates ? Mat4::Rotation(slideAxis[0], slideAxis[1], slideAxis[2], d) : Mat4::Translation(slideAxis[0] * d, slideAxis[1] * d, slideAxis[2] * d));
    }
    return nodes[i].parent < 0 ? local : world(nodes[i].parent, trigger) * local;
  }
  void drawNode(int index, const Mat4 &root, const Mat4 &vp, float trigger)
  {
    const auto &n = nodes[index];
    if (n.mesh >= 0 && (std::size_t)n.mesh < meshes.size()) {
      const auto model = root * world(index, trigger), mvp = vp * model;
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(program, "mvp"), 1, GL_FALSE, mvp.m.data());
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(program, "model"), 1, GL_FALSE, model.m.data());
      for (const auto &p : meshes[n.mesh]) {
        tgl::Uniform4fv(tgl::GetUniformLocation(program, "base"), 1, p.color.data());
        tgl::Uniform4fv(tgl::GetUniformLocation(program, "look"), 1, p.look.data());
        tgl::BindVertexArray(p.vao); glDrawElements(GL_TRIANGLES, p.count, GL_UNSIGNED_INT, nullptr);
      }
    }
    for (int c : n.children) drawNode(c, root, vp, trigger);
  }
  void metadata(const std::string &path)
  {
    std::ifstream f(path); if (!f) return;   // colours and trigger motion are optional
    std::string t((std::istreambuf_iterator<char>(f)), {});
    auto section = [&](const std::string &name) { auto s = t.find("[" + name + "]"); if (s == std::string::npos) return std::string(); s += name.size() + 2; return t.substr(s, t.find("\n[", s) - s); };
    auto value = [](const std::string &s, const std::string &key) { std::smatch m; std::regex re("(?:^|\\n)\\s*" + key + "\\s*=\\s*([^\\r\\n]+)"); return std::regex_search(s, m, re) ? m[1].str() : std::string(); };
    auto numbers = [](std::string s) { std::vector<float> v; const auto l = s.find('['), r = s.find(']'); if (l == std::string::npos || r == std::string::npos) return v; s = s.substr(l + 1, r - l - 1); for (auto &c : s) if (c == ',') c = ' '; std::istringstream in(s); for (float x; in >> x;) v.push_back(x); return v; };
    const auto trig = section("motion.trigger");
    if (!trig.empty()) {
      const auto a = numbers(value(trig, "axis")), r = numbers(value(trig, "range"));
      if (a.size() == 3 && r.size() == 2) { axis = { a[0], a[1], a[2] }; range = { r[0], r[1] }; }
      std::regex q("\"([^\"]+)\""); const auto names = value(trig, "lod_nodes"); int i = 0;
      for (auto it = std::sregex_iterator(names.begin(), names.end(), q); it != std::sregex_iterator() && i < 2; ++it)
        for (std::size_t k = 0; k < nodes.size(); ++k) if (nodes[k].name == (*it)[1].str()) triggers[i++] = (int)k;
    }
    const auto rec = section("motion.recoil");   // [motion.recoil] drive = "recoil": the slide kicks back per shot (kind: slide or rotate)
    if (!rec.empty()) {
      const auto a = numbers(value(rec, "axis")), r = numbers(value(rec, "range")); const auto ms = value(rec, "duration_ms");
      if (a.size() == 3 && r.size() == 2) { slideAxis = { a[0], a[1], a[2] }; slideRange = { r[0], r[1] }; }
      slideRotates = value(rec, "kind").find("rotate") != std::string::npos;
      if (!ms.empty()) slideMs = std::stof(ms);
      std::regex q("\"([^\"]+)\""); const auto names = value(rec, "lod_nodes"); int i = 0;
      for (auto it = std::sregex_iterator(names.begin(), names.end(), q); it != std::sregex_iterator() && i < 2; ++it)
        for (std::size_t k = 0; k < nodes.size(); ++k) if (nodes[k].name == (*it)[1].str()) slides[i++] = (int)k;
    }
    const auto tints = section("tints");
    if (!tints.empty()) {
      std::smatch m; const auto def = value(tints, "default"); std::regex q("\"([^\"]+)\"");
      if (std::regex_search(def, m, q)) {
        const auto line = value(tints, m[1].str());
        for (auto pair : { std::make_pair("body", &body), std::make_pair("accent", &accent) }) {
          std::regex hex(std::string(pair.first) + "\\s*=\\s*\"#([0-9a-fA-F]{6})\"");
          if (std::regex_search(line, m, hex)) { const unsigned rgb = (unsigned)std::stoul(m[1].str(), nullptr, 16); for (int c = 0; c < 3; ++c) (*pair.second)[c] = std::pow(((rgb >> (16 - 8 * c)) & 255) / 255.f, 2.2f); }
        }
        // Optional, on the same one-line table: body_metal / body_rough and accent_metal / accent_rough (0..1).
        for (auto pair : { std::make_pair("body", &bodyLook), std::make_pair("accent", &accentLook) }) {
          std::regex metal(std::string(pair.first) + "_metal\\s*=\\s*([0-9.]+)"), rough(std::string(pair.first) + "_rough\\s*=\\s*([0-9.]+)");
          std::smatch mm, mr;
          const bool hasMetal = std::regex_search(line, mm, metal), hasRough = std::regex_search(line, mr, rough);
          if (hasMetal || hasRough) *pair.second = { hasMetal ? std::stof(mm[1].str()) : 0.f, hasRough ? std::stof(mr[1].str()) : .5f, 1, 0 };
        }
      }
    }
  }
};

// What one eye sees. The picture texture is sRGB (decoded on sampling).
struct EyeScene {
  GLuint picture = 0; int pictureW = 0, pictureH = 0;
  ScreenGeometry screen;
  bool showGun = false; Mat4 gunRoot = Mat4::Identity(); float trigger = 0; float recoil = 0;   // slide travel 0..1
  bool showDot = false; float dotU = .5f, dotV = .5f;
  bool crt = false;
  float u0 = 0, uw = 1;   // the part of the picture this eye sees (side-by-side stereo)
  float v0 = 0, vh = 1;   // and its rows (a letterboxed picture)
  bool frame = true; float fadeU = 0, fadeV = 0;   // dark frame; edges fading into the dark
  // The HUD panel: either one picture with alpha (sRGB, premultiplied), or the layer over black and over white
  // (linear 8-bit values as the game drew them), whose difference gives the alpha.
  GLuint hud = 0, hudOverWhite = 0; ScreenGeometry hudPanel; float hudOpacity = 1; bool hudKeyBlack = false;
};

class Renderer {
public:
  void init(Gun *gun)
  {
    m_gun = gun;
    m_screenProgram = Program(
      "#version 330 core\nlayout(location=0)in vec3 p;layout(location=1)in vec2 t;uniform mat4 mvp;out vec2 uv0;"
      "void main(){gl_Position=mvp*vec4(p,1);uv0=t;}",
      // Sharp bilinear: whole texels stay crisp when magnified, edges blend over one screen
      // pixel; minified pictures use the mipmaps. Optional CRT scanlines.
      "#version 330 core\nin vec2 uv0;uniform sampler2D picture;uniform vec2 size,range,rows,fade;uniform int crt,encode;out vec4 o;"
      "void main(){vec2 uv=vec2(range.x+uv0.x*range.y,rows.x+uv0.y*rows.y);"
      // Half a texel inside the crop: bilinear filtering must not blend in the black rows above and below a letterboxed eye or the other eye's edge column.
      "vec2 hp=.5/size;uv=clamp(uv,vec2(range.x,rows.x)+hp,vec2(range.x+range.y,rows.x+rows.y)-hp);vec2 texel=uv*size;vec2 ppt=1./max(fwidth(texel),vec2(1e-5));vec3 c;"
      "if(min(ppt.x,ppt.y)<1.)c=texture(picture,uv).rgb;else{vec2 k=floor(ppt);vec2 f=fract(texel)-.5;"
      "vec2 r=.5-.5/k;vec2 g=(f-clamp(f,-r,r))*k+.5;c=textureGrad(picture,(floor(texel)+g)/size,dFdx(uv),dFdy(uv)).rgb;}"
      "if(crt!=0){float s=abs(fract(texel.y)-.5)*2.;c*=mix(1.,.62,s*s);}"
      "if(fade.x>0.)c*=smoothstep(0.,fade.x,uv0.x)*smoothstep(0.,fade.x,1.-uv0.x);"
      "if(fade.y>0.)c*=smoothstep(0.,fade.y,uv0.y)*smoothstep(0.,fade.y,1.-uv0.y);"
      "o=vec4(encode!=0?pow(c,vec3(1./2.2)):c,1);}");
    m_flatProgram = Program(
      "#version 330 core\nlayout(location=0)in vec3 p;uniform mat4 mvp;void main(){gl_Position=mvp*vec4(p,1);}",
      "#version 330 core\nuniform vec4 base;uniform int encode;out vec4 o;void main(){o=vec4(encode!=0?pow(base.rgb,vec3(1./2.2)):base.rgb,1);}");
    m_hudProgram = Program(
      "#version 330 core\nlayout(location=0)in vec3 p;layout(location=1)in vec2 t;uniform mat4 mvp;out vec2 uv0;"
      "void main(){gl_Position=mvp*vec4(p,1);uv0=t;}",
      "#version 330 core\nin vec2 uv0;uniform sampler2D hud,white;uniform float opacity;uniform int encode,matte,keyBlack;out vec4 o;"
      "void main(){vec4 c=texture(hud,uv0);"
      "if(matte!=0){vec3 w=texture(white,uv0).rgb;c.a=clamp(1.-max(max(w.r-c.r,w.g-c.g),w.b-c.b),0.,1.);if(encode==0)c.rgb=pow(c.rgb,vec3(2.2));}"
      "else if(encode!=0)c.rgb=pow(c.rgb,vec3(1./2.2));"
      "if(keyBlack!=0&&max(max(c.r,c.g),c.b)<.012)c=vec4(0);"
      "o=c*opacity;}");
    tgl::GenVertexArrays(1, &m_screenVao); tgl::GenBuffers(1, &m_screenVbo);
    tgl::GenVertexArrays(1, &m_dotVao); tgl::GenBuffers(1, &m_dotVbo);
    if (m_gun) m_gun->upload();
  }
  void release()
  {
    if (m_gun) m_gun->release();
    if (m_screenProgram) tgl::DeleteProgram(m_screenProgram); if (m_flatProgram) tgl::DeleteProgram(m_flatProgram);
    if (m_hudProgram) tgl::DeleteProgram(m_hudProgram); m_hudProgram = 0;
    tgl::DeleteVertexArrays(1, &m_screenVao); tgl::DeleteBuffers(1, &m_screenVbo);
    tgl::DeleteVertexArrays(1, &m_dotVao); tgl::DeleteBuffers(1, &m_dotVbo);
    m_screenProgram = m_flatProgram = m_screenVao = m_screenVbo = m_dotVao = m_dotVbo = 0;
  }
  // Draws into the bound framebuffer (colour + depth). encode: write sRGB values by hand
  // (for a target that is not an sRGB format).
  void draw(const EyeScene &s, const Mat4 &projection, const Mat4 &view, int w, int h, bool encode)
  {
    glViewport(0, 0, w, h); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    const float bg = encode ? .03f : .002f;   // a dark theatre
    glClearColor(bg, bg, bg * 1.2f, 1); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const Mat4 vp = projection * view;
    buildScreen(s.screen);
    // A thin dark frame behind the picture edges.
    if (m_flatProgram && s.frame) {
      ScreenGeometry frame = s.screen; frame.width += .06f; frame.height += .06f; frame.distance += .01f;
      buildScreenInto(frame, m_frameVerts);
      tgl::UseProgram(m_flatProgram);
      const Mat4 mvp = vp * s.screen.frame;
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(m_flatProgram, "mvp"), 1, GL_FALSE, mvp.m.data());
      const float c[4] = { .012f, .012f, .014f, 1 };
      tgl::Uniform4fv(tgl::GetUniformLocation(m_flatProgram, "base"), 1, c);
      tgl::Uniform1i(tgl::GetUniformLocation(m_flatProgram, "encode"), encode ? 1 : 0);
      drawStrip(m_frameVerts);
    }
    if (s.picture) {
      tgl::UseProgram(m_screenProgram);
      const Mat4 mvp = vp * s.screen.frame;
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(m_screenProgram, "mvp"), 1, GL_FALSE, mvp.m.data());
      tgl::Uniform2f(tgl::GetUniformLocation(m_screenProgram, "size"), (float)s.pictureW, (float)s.pictureH);
      tgl::Uniform2f(tgl::GetUniformLocation(m_screenProgram, "range"), s.u0, s.uw);
      tgl::Uniform2f(tgl::GetUniformLocation(m_screenProgram, "rows"), s.v0, s.vh);
      tgl::Uniform2f(tgl::GetUniformLocation(m_screenProgram, "fade"), s.fadeU, s.fadeV);
      tgl::Uniform1i(tgl::GetUniformLocation(m_screenProgram, "crt"), s.crt ? 1 : 0);
      tgl::Uniform1i(tgl::GetUniformLocation(m_screenProgram, "encode"), encode ? 1 : 0);
      tgl::Uniform1i(tgl::GetUniformLocation(m_screenProgram, "picture"), 0);
      tgl::ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, s.picture);
      drawStrip(m_screenVerts);
    }
    if (s.hud && m_hudProgram) {
      // Premultiplied over the world, no depth written (the gun still goes in front where it is nearer).
      buildScreenInto(s.hudPanel, m_hudVerts);
      glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
      tgl::UseProgram(m_hudProgram);
      const Mat4 mvp = vp * s.hudPanel.frame;
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(m_hudProgram, "mvp"), 1, GL_FALSE, mvp.m.data());
      tgl::Uniform1f(tgl::GetUniformLocation(m_hudProgram, "opacity"), s.hudOpacity);
      tgl::Uniform1i(tgl::GetUniformLocation(m_hudProgram, "encode"), encode ? 1 : 0);
      tgl::Uniform1i(tgl::GetUniformLocation(m_hudProgram, "hud"), 0);
      tgl::Uniform1i(tgl::GetUniformLocation(m_hudProgram, "white"), 1);
      tgl::Uniform1i(tgl::GetUniformLocation(m_hudProgram, "matte"), s.hudOverWhite ? 1 : 0);
      tgl::Uniform1i(tgl::GetUniformLocation(m_hudProgram, "keyBlack"), s.hudKeyBlack ? 1 : 0);
      tgl::ActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, s.hudOverWhite);
      tgl::ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, s.hud);
      drawStrip(m_hudVerts);
      glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }
    if (s.showDot) drawDot(s, vp, encode);
    if (s.showGun && m_gun && m_gun->loaded()) {
      const Mat4 eyeWorld = view.RigidInverse(); const float eye[3] = { eyeWorld.m[12], eyeWorld.m[13], eyeWorld.m[14] };
      // The light sits at (0.36, 0.70, 0.62) in the viewer's space (runtime-lighting.json), so it is turned into world space for this eye.
      const float lv[3] = { .36f, .70f, .62f }; const float ln = std::sqrt(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]);
      float lightWorld[3]; for (int c = 0; c < 3; ++c) lightWorld[c] = (view.m[c * 4] * lv[0] + view.m[c * 4 + 1] * lv[1] + view.m[c * 4 + 2] * lv[2]) / ln;
      m_gun->draw(s.gunRoot, vp, s.trigger, encode, s.recoil, eye, lightWorld);
    }
    tgl::BindVertexArray(0); tgl::UseProgram(0);
  }
private:
  Gun *m_gun = nullptr;
  GLuint m_screenProgram = 0, m_flatProgram = 0, m_hudProgram = 0, m_screenVao = 0, m_screenVbo = 0, m_dotVao = 0, m_dotVbo = 0;
  std::vector<float> m_screenVerts, m_frameVerts, m_hudVerts;
  static void buildScreenInto(const ScreenGeometry &g, std::vector<float> &out)
  {
    const int columns = g.curve > 0 ? 64 : 1; out.clear();
    for (int i = 0; i <= columns; ++i) {
      const float u = (float)i / columns;
      for (int row = 0; row < 2; ++row) { const float v = (float)row; const auto p = g.local(u, v); out.insert(out.end(), { p[0], p[1], p[2], u, v }); }
    }
  }
  void buildScreen(const ScreenGeometry &g) { buildScreenInto(g, m_screenVerts); }
  void drawStrip(const std::vector<float> &verts)
  {
    tgl::BindVertexArray(m_screenVao); tgl::BindBuffer(GL_ARRAY_BUFFER, m_screenVbo);
    tgl::BufferData(GL_ARRAY_BUFFER, verts.size() * 4, verts.data(), GL_STREAM_DRAW);
    tgl::EnableVertexAttribArray(0); tgl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, nullptr);
    tgl::EnableVertexAttribArray(1); tgl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, reinterpret_cast<void *>(12));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)(verts.size() / 5));
  }
  // An orange disc with a dark rim, lying on the screen just in front of the picture.
  void drawDot(const EyeScene &s, const Mat4 &vp, bool encode)
  {
    const float r = s.screen.height / 110.f;
    auto ring = [&](float radius, float lift, const float colour[4]) {
      std::vector<float> v; const auto c = s.screen.local(s.dotU, s.dotV);
      const float du = radius / s.screen.width, dv = radius / s.screen.height;
      // Toward the viewer: the screen normal at this point, in the frame.
      float nx = -c[0], nz = -(c[2] + s.screen.distance - (s.screen.curve > 0 ? s.screen.distance / s.screen.curve : 0)); if (s.screen.curve <= 0) { nx = 0; nz = 1; }
      const float nl = std::sqrt(nx * nx + nz * nz); nx /= nl; nz /= nl;
      v.insert(v.end(), { c[0] + nx * lift, c[1], c[2] + nz * lift });
      for (int i = 0; i <= 24; ++i) {
        const float a = i * 6.2831853f / 24, p = s.dotU + du * std::cos(a), q = s.dotV + dv * std::sin(a);
        const auto e = s.screen.local(p, q); v.insert(v.end(), { e[0] + nx * lift, e[1], e[2] + nz * lift });
      }
      tgl::UseProgram(m_flatProgram);
      const Mat4 mvp = vp * s.screen.frame;
      tgl::UniformMatrix4fv(tgl::GetUniformLocation(m_flatProgram, "mvp"), 1, GL_FALSE, mvp.m.data());
      tgl::Uniform4fv(tgl::GetUniformLocation(m_flatProgram, "base"), 1, colour);
      tgl::Uniform1i(tgl::GetUniformLocation(m_flatProgram, "encode"), encode ? 1 : 0);
      tgl::BindVertexArray(m_dotVao); tgl::BindBuffer(GL_ARRAY_BUFFER, m_dotVbo);
      tgl::BufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STREAM_DRAW);
      tgl::EnableVertexAttribArray(0); tgl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
      glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(v.size() / 3));
    };
    const float rim[4] = { 0, 0, 0, 1 }, fill[4] = { 1.f, .2f, .02f, 1 };
    ring(r * 1.35f, .004f, rim); ring(r, .006f, fill);
  }
};
}
