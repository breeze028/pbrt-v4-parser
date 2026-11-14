// SPDX-License-Identifier: Apache-2.0
#include <pbrt4/pbrt4.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace pbrt4 {
Vec3 operator+(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator-(Vec3 a) { return {-a.x,-a.y,-a.z}; }
Vec3 operator*(Vec3 a,double b) { return {a.x*b,a.y*b,a.z*b}; }
Vec3 operator*(Vec3 a,Vec3 b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
Vec3 operator/(Vec3 a,double b) { return a*(1/b); }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 normalize(Vec3 a) { auto n = std::sqrt(dot(a,a)); return n > 0 ? a/n : Vec3{}; }
Matrix4 operator*(const Matrix4& a,const Matrix4& b) {
    Matrix4 c; c.m.fill(0);
    for (int r=0;r<4;++r) for (int j=0;j<4;++j) for (int k=0;k<4;++k) c.m[r*4+j] += a.m[r*4+k]*b.m[k*4+j];
    return c;
}
std::optional<Matrix4> inverse(const Matrix4& a) {
    double w[4][8]{};
    for (int r=0;r<4;++r) { for (int c=0;c<4;++c) w[r][c]=a.m[r*4+c]; w[r][r+4]=1; }
    for (int c=0;c<4;++c) {
        int pivot=c; for (int r=c+1;r<4;++r) if (std::abs(w[r][c])>std::abs(w[pivot][c])) pivot=r;
        if (w[pivot][c] == 0 || !std::isfinite(w[pivot][c])) return {};
        for (int k=0;k<8;++k) std::swap(w[pivot][k],w[c][k]);
        double d=w[c][c]; for (double& v:w[c]) v/=d;
        for (int r=0;r<4;++r) if (r!=c) { d=w[r][c]; for (int k=0;k<8;++k) w[r][k]-=d*w[c][k]; }
    }
    Matrix4 b; for (int r=0;r<4;++r) for (int c=0;c<4;++c) { b.m[r*4+c]=w[r][c+4]; if (!std::isfinite(b.m[r*4+c])) return {}; }
    return b;
}
Vec3 transform_point(const Matrix4& a,Vec3 p) {
    auto& m=a.m; double w=m[12]*p.x+m[13]*p.y+m[14]*p.z+m[15];
    return Vec3{m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]}/w;
}
Vec3 transform_vector(const Matrix4& a,Vec3 p) {
    auto& m=a.m; return {m[0]*p.x+m[1]*p.y+m[2]*p.z,m[4]*p.x+m[5]*p.y+m[6]*p.z,m[8]*p.x+m[9]*p.y+m[10]*p.z};
}
Vec3 transform_normal(const Matrix4& a,Vec3 n) {
    auto inv=inverse(a); if (!inv) return {};
    auto& m=inv->m; return normalize({m[0]*n.x+m[4]*n.y+m[8]*n.z,m[1]*n.x+m[5]*n.y+m[9]*n.z,m[2]*n.x+m[6]*n.y+m[10]*n.z});
}
double determinant3(const Matrix4& a) {
    auto& m=a.m; return m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
}
const Parameter* find_parameter(const std::vector<Parameter>& p,std::string_view n) {
    for (auto i=p.rbegin();i!=p.rend();++i) if (i->name==n) return &*i;
    return nullptr;
}
const Parameter* find_parameter(const Entity& e,std::string_view n) {
    auto p=find_parameter(e.parameters,n); return p ? p : find_parameter(e.inherited_parameters,n);
}
std::optional<double> scalar(const Entity& e,std::string_view n) {
    auto p=find_parameter(e,n); if (!p || p->values.size()!=1) return {};
    if (auto d=std::get_if<double>(&p->values[0])) return *d;
    if (auto i=std::get_if<std::int64_t>(&p->values[0])) return static_cast<double>(*i);
    return {};
}
std::optional<std::string> string_value(const Entity& e,std::string_view n) {
    auto p=find_parameter(e,n); if (!p || p->values.size()!=1) return {};
    if (auto s=std::get_if<std::string>(&p->values[0])) return *s;
    return {};
}
}
