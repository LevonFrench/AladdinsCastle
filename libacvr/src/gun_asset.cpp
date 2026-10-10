// SPDX-License-Identifier: MIT
#include "gun_asset.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>

namespace acvr {
namespace {
using Json=nlohmann::json;
void require(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
uint32_t word(const std::vector<uint8_t> &b,size_t at) {
    require(at<=b.size() && b.size()-at>=4,"truncated GLB word");
    return uint32_t(b[at]) | (uint32_t(b[at+1])<<8) | (uint32_t(b[at+2])<<16) | (uint32_t(b[at+3])<<24);
}
uint32_t number(const Json &j) {
    require(j.is_number_integer(),"expected nonnegative integer");
    const auto v=j.get<int64_t>(); require(v>=0 && uint64_t(v)<=UINT32_MAX,"integer out of range"); return uint32_t(v);
}
uint32_t value(const Json &j,const char *key,uint32_t fallback=0) { return j.contains(key)?number(j.at(key)):fallback; }
float scalar(const Json &j) { require(j.is_number(),"expected scalar"); const float f=j.get<float>(); require(std::isfinite(f),"nonfinite scalar"); return f; }
template<size_t N> std::array<float,N> vector(const Json &j) {
    require(j.is_array() && j.size()==N,"wrong vector length"); std::array<float,N> out;
    for(size_t i=0;i<N;++i) out[i]=scalar(j[i]);
    return out;
}
Matrix identity() { Matrix m{}; for(unsigned i=0;i<4;++i) m[5*i]=1; return m; }
Matrix multiply(const Matrix &a,const Matrix &b) {
    Matrix m{};
    for(unsigned col=0;col<4;++col) for(unsigned row=0;row<4;++row)
        for(unsigned k=0;k<4;++k) m[col*4+row]+=a[k*4+row]*b[col*4+k];
    for(float f:m) require(std::isfinite(f),"nonfinite composed transform");
    return m;
}
Matrix transform(const Json &j) {
    if(j.contains("matrix")) {
        require(!j.contains("translation")&&!j.contains("rotation")&&!j.contains("scale"),"mixed matrix and TRS");
        auto m=vector<16>(j.at("matrix"));
        require(m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1,"non-affine node transform");
        const float determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
        require(std::isfinite(determinant) && std::abs(determinant)>1e-12f,"singular node matrix");
        return m;
    }
    const auto t=j.contains("translation")?vector<3>(j.at("translation")):std::array<float,3>{0,0,0};
    const auto s=j.contains("scale")?vector<3>(j.at("scale")):std::array<float,3>{1,1,1};
    const auto q=j.contains("rotation")?vector<4>(j.at("rotation")):std::array<float,4>{0,0,0,1};
    require(s[0]!=0 && s[1]!=0 && s[2]!=0,"singular node scale");
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    require(std::abs(x*x+y*y+z*z+w*w-1)<.001f,"rotation is not a unit quaternion");
    Matrix m{1-2*y*y-2*z*z,2*x*y+2*z*w,2*x*z-2*y*w,0,
        2*x*y-2*z*w,1-2*x*x-2*z*z,2*y*z+2*x*w,0,
        2*x*z+2*y*w,2*y*z-2*x*w,1-2*x*x-2*y*y,0,t[0],t[1],t[2],1};
    for(unsigned col=0;col<3;++col) for(unsigned row=0;row<3;++row) m[col*4+row]*=s[col];
    return m;
}
struct Accessor {
    const std::vector<uint8_t> &bytes;
    size_t offset=0,stride=0,count=0,width=0;
    uint32_t component=0,components=0;
    bool normalized=false;
    Accessor(const Json &doc,const std::vector<uint8_t> &binary,uint32_t index):bytes(binary) {
        const auto &all=doc.at("accessors"); require(index<all.size(),"accessor index out of range");
        const auto &a=all.at(index); require(!a.contains("sparse"),"sparse accessor unsupported");
        const auto &views=doc.at("bufferViews"); const auto vi=number(a.at("bufferView")); require(vi<views.size(),"view index out of range");
        const auto &v=views.at(vi); require(value(v,"buffer")==0,"external buffer reference");
        const auto type=a.at("type").get<std::string>();
        components=type=="SCALAR"?1u:type=="VEC3"?3u:type=="VEC4"?4u:0u;
        require(components!=0,"unsupported accessor shape"); component=number(a.at("componentType"));
        width=component==5121?1u:component==5123?2u:(component==5125||component==5126)?4u:0u;
        require(width!=0,"unsupported accessor component"); count=number(a.at("count"));
        require(count>0 && count<=100000,"accessor count out of bounds");
        normalized=a.value("normalized",false);
        const size_t start=value(v,"byteOffset"),length=number(v.at("byteLength")),local=value(a,"byteOffset");
        require(start<=binary.size() && length<=binary.size()-start && local<=length,"buffer view exceeds payload");
        stride=value(v,"byteStride",uint32_t(width*components));
        require(stride>=width*components && stride%width==0 && (start+local)%width==0,"invalid accessor alignment/stride");
        require(count-1<=(length-local)/stride && width*components<=length-local-(count-1)*stride,"accessor exceeds view");
        offset=start+local;
    }
    uint32_t integer(size_t i,unsigned c=0) const {
        const size_t at=offset+i*stride+c*width; uint32_t v=0;
        for(size_t n=0;n<width;++n) v|=uint32_t(bytes[at+n])<<(8*n);
        return v;
    }
    float real(size_t i,unsigned c) const {
        const auto v=integer(i,c); float f=0;
        if(component==5126) std::memcpy(&f,&v,sizeof(f));
        else { require(normalized && (component==5121||component==5123),"colour must be normalized"); f=float(v)/(component==5121?255.f:65535.f); }
        require(std::isfinite(f),"nonfinite vertex"); return f;
    }
};
}

bool decode_gun_glb(const std::vector<uint8_t> &bytes,GunAsset &out,std::string &error) noexcept {
    try {
        require(bytes.size()>=28 && bytes.size()<=32u*1024u*1024u,"GLB size out of bounds");
        require(word(bytes,0)==0x46546c67 && word(bytes,4)==2 && word(bytes,8)==bytes.size(),"invalid GLB header");
        const size_t json_size=word(bytes,12);
        require(word(bytes,16)==0x4e4f534a && json_size<=4u*1024u*1024u && json_size%4==0 && json_size<=bytes.size()-20,"invalid JSON chunk");
        const auto doc=Json::parse(bytes.begin()+20,bytes.begin()+20+json_size,
            [](int depth,Json::parse_event_t,Json &) { require(depth<=64,"JSON nesting too deep"); return true; });
        require(doc.at("asset").at("version")=="2.0","unsupported glTF version");
        require(!doc.contains("extensionsRequired") || doc.at("extensionsRequired").empty(),"required extensions unsupported");
        require(!doc.contains("images") || doc.at("images").empty(),"gun images unsupported");
        require(!doc.contains("skins") || doc.at("skins").empty(),"skinning unsupported");
        const size_t bin_header=20+json_size, bin_size=word(bytes,bin_header);
        require(word(bytes,bin_header+4)==0x004e4942 && bin_size%4==0 && bin_header+8<=bytes.size() && bin_size==bytes.size()-bin_header-8,"invalid BIN chunk");
        const auto &buffers=doc.at("buffers"); require(buffers.is_array() && buffers.size()==1 && !buffers[0].contains("uri"),"GLB must be self-contained");
        const size_t payload=number(buffers[0].at("byteLength")); require(payload<=bin_size && bin_size-payload<=3,"invalid BIN padding");
        std::vector<uint8_t> binary(bytes.begin()+bin_header+8,bytes.begin()+bin_header+8+payload);
        GunAsset asset; std::map<std::string,uint32_t> names;
        const auto &nodes=doc.at("nodes"); require(nodes.is_array() && !nodes.empty() && nodes.size()<=1024,"node count out of bounds");
        asset.nodes.resize(nodes.size());
        for(uint32_t i=0;i<nodes.size();++i) {
            const auto &j=nodes[i]; auto &n=asset.nodes[i]; n.name=j.at("name").get<std::string>();
            require(!n.name.empty() && n.name.size()<=128 && names.emplace(n.name,i).second,"missing/duplicate node name");
            require(!j.contains("skin") && !j.contains("weights"),"deforming gun node unsupported"); n.local=transform(j);
        }
        require(names.count("grip") && names.count("muzzle"),"grip/muzzle required"); asset.grip=names.at("grip"); asset.muzzle=names.at("muzzle");
        const auto &scenes=doc.at("scenes"); const auto scene=value(doc,"scene"); require(scene<scenes.size(),"scene index out of range");
        const auto &roots=scenes.at(scene).at("nodes"); require(roots.size()==1 && number(roots[0])==asset.grip,"grip must be the sole root");
        const auto id=identity(); for(unsigned i=0;i<16;++i) require(std::abs(asset.nodes[asset.grip].local[i]-id[i])<.00001f,"grip root must preserve metre units");
        for(uint32_t i=0;i<nodes.size();++i) if(nodes[i].contains("children")) {
            require(nodes[i].at("children").is_array(),"children must be an array");
            for(const auto &c:nodes[i].at("children")) {
                const auto child=number(c); require(child<nodes.size() && child!=asset.grip && asset.nodes[child].parent==-1,"invalid node parent");
                asset.nodes[child].parent=int32_t(i);
            }
        }
        std::vector<uint32_t> order{asset.grip}; std::vector<bool> visited(nodes.size());
        for(size_t at=0;at<order.size();++at) {
            const auto i=order[at]; require(!visited[i],"cyclic hierarchy"); visited[i]=true;
            auto &node=asset.nodes[i]; node.bind_world=node.parent<0?node.local:multiply(asset.nodes[uint32_t(node.parent)].bind_world,node.local);
            if(nodes[i].contains("children")) for(const auto &c:nodes[i].at("children")) order.push_back(number(c));
        }
        require(order.size()==nodes.size(),"unreachable nodes");
        require(asset.nodes[asset.muzzle].parent==int32_t(asset.grip),"muzzle must be independent of animated geometry");
        const auto &muzzle=asset.nodes[asset.muzzle].bind_world;
        require(muzzle[14]<0 && std::abs(muzzle[8])<.001f && std::abs(muzzle[9])<.001f && std::abs(muzzle[10]-1)<.001f,"muzzle bore must point -Z");
        const auto &materials=doc.at("materials"); require(materials.is_array() && !materials.empty() && materials.size()<=64,"material count out of bounds");
        for(const auto &j:materials) {
            GunMaterial m; m.name=j.at("name").get<std::string>();
            require(m.name=="body"||m.name=="accent"||m.name=="dark"||m.name=="glass","unknown gun material role");
            if(j.contains("pbrMetallicRoughness")) {
                const auto &p=j.at("pbrMetallicRoughness"); require(!p.contains("baseColorTexture")&&!p.contains("metallicRoughnessTexture"),"textures unsupported");
                if(p.contains("baseColorFactor")) m.base_colour=vector<4>(p.at("baseColorFactor"));
            }
            for(float f:m.base_colour) require(f>=0 && f<=1,"invalid material colour");
            asset.materials.push_back(m);
        }
        const auto &meshes=doc.at("meshes"); require(meshes.is_array() && meshes.size()<=1024,"mesh count out of bounds");
        const bool explicit_lod=names.count("LOD0")!=0; require(!names.count("LOD1")||explicit_lod,"LOD1 requires LOD0");
        asset.lod_count=names.count("LOD1")?2u:1u;
        size_t decoded_vertices=0;
        for(uint32_t i=0;i<nodes.size();++i) if(nodes[i].contains("mesh")) {
            uint32_t lod=0; bool in_lod=!explicit_lod;
            for(int32_t a=int32_t(i);a>=0;a=asset.nodes[uint32_t(a)].parent) {
                if(asset.nodes[uint32_t(a)].name=="LOD0") { require(!in_lod,"nested LODs"); in_lod=true; lod=0; }
                if(asset.nodes[uint32_t(a)].name=="LOD1") { require(!in_lod,"nested LODs"); in_lod=true; lod=1; }
            }
            require(in_lod,"mesh outside LOD tree"); const auto mi=number(nodes[i].at("mesh")); require(mi<meshes.size(),"mesh index out of range");
            for(const auto &p:meshes[mi].at("primitives")) {
                require(asset.primitives.size()<2048,"primitive count out of bounds");
                require(value(p,"mode",4)==4 && !p.contains("targets"),"only rigid triangles supported");
                GunPrimitive primitive; primitive.node=i; primitive.lod=lod; primitive.material=number(p.at("material"));
                require(primitive.material<asset.materials.size(),"material index out of range");
                const auto &attributes=p.at("attributes"); Accessor position(doc,binary,number(attributes.at("POSITION")));
                require(position.component==5126 && position.components==3 && !position.normalized,"positions must be float vec3");
                decoded_vertices+=position.count;
                require(decoded_vertices<=100000,"decoded vertex budget exceeded");
                primitive.vertices.resize(position.count);
                for(size_t v=0;v<position.count;++v) for(unsigned c=0;c<3;++c) primitive.vertices[v].position[c]=position.real(v,c);
                if(attributes.contains("NORMAL")) {
                    Accessor normal(doc,binary,number(attributes.at("NORMAL")));
                    require(normal.count==position.count && normal.component==5126 && normal.components==3 && !normal.normalized,"invalid normals");
                    primitive.has_normals=true;
                    for(size_t v=0;v<position.count;++v) for(unsigned c=0;c<3;++c) primitive.vertices[v].normal[c]=normal.real(v,c);
                }
                if(attributes.contains("COLOR_0")) {
                    Accessor colour(doc,binary,number(attributes.at("COLOR_0")));
                    require(colour.count==position.count && (colour.components==3 || colour.components==4),"invalid vertex colours");
                    for(size_t v=0;v<position.count;++v) for(unsigned c=0;c<colour.components;++c) {
                        const auto f=colour.real(v,c); require(f>=0 && f<=1,"vertex colour outside 0..1"); primitive.vertices[v].colour[c]=f;
                    }
                }
                if(p.contains("indices")) {
                    Accessor indices(doc,binary,number(p.at("indices")));
                    require(indices.components==1 && indices.component!=5126 && !indices.normalized,"invalid index type");
                    for(size_t v=0;v<indices.count;++v) { const auto index=indices.integer(v); require(index<position.count,"index exceeds vertices"); primitive.indices.push_back(index); }
                } else for(uint32_t v=0;v<position.count;++v) primitive.indices.push_back(v);
                require(primitive.indices.size()%3==0,"incomplete triangle");
                asset.triangles[lod]+=uint32_t(primitive.indices.size()/3);
                require(asset.triangles[lod]<=(lod==0?8000u:2000u),"gun triangle budget exceeded");
                asset.primitives.push_back(std::move(primitive));
            }
        }
        require(asset.triangles[0]>0 && (asset.lod_count==1 || asset.triangles[1]>0),"empty gun LOD");
        out=std::move(asset); error.clear(); return true;
    } catch(const std::exception &e) { try { error=e.what(); } catch(...) {} return false; }
      catch(...) { try { error="gun decode failed"; } catch(...) {} return false; }
}
}
