// SPDX-License-Identifier: Apache-2.0
#include "image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace pbrt4::web {
ImageData decode_tga(std::span<const std::uint8_t> input){
    if(input.size()<18)throw std::runtime_error("Truncated TGA header");
    auto word=[&](int p){return std::uint32_t(input[p])|(std::uint32_t(input[p+1])<<8);};
    const auto type=input[2],depth=input[16],descriptor=input[17];
    const bool gray=type==3 || type==11,rle=type==10 || type==11;
    if(input[1] || (type!=2 && type!=3 && type!=10 && type!=11) || (gray?depth!=8:(depth!=24 && depth!=32)))throw std::runtime_error("Unsupported TGA: expected uncolormapped RGB24/RGBA32/gray8");
    ImageData image;image.width=word(12);image.height=word(14);image.alpha=depth==32 && (descriptor&15)!=0;
    const auto count=std::uint64_t(image.width)*image.height;
    if(!count || image.width>16384 || image.height>16384 || count>67108864)throw std::runtime_error("Invalid or oversized TGA dimensions");
    image.rgba.resize(static_cast<std::size_t>(count)*4);std::size_t cursor=18+input[0],pixel=0;
    auto read=[&](){
        const auto bytes=depth/8;if(cursor+bytes>input.size())throw std::runtime_error("Truncated TGA pixels");
        std::array<std::uint8_t,4> color;
        if(gray)color={input[cursor],input[cursor],input[cursor],255};
        else color={input[cursor+2],input[cursor+1],input[cursor],image.alpha?input[cursor+3]:std::uint8_t(255)};
        cursor+=bytes;return color;
    };
    auto put=[&](const auto& color){
        const auto x=pixel%image.width,y=pixel/image.width;
        const auto dx=(descriptor&16)?image.width-1-x:x,dy=(descriptor&32)?y:image.height-1-y;
        std::copy(color.begin(),color.end(),image.rgba.begin()+(dy*image.width+dx)*4);++pixel;
    };
    while(pixel<count){
        unsigned run=1;bool repeated=false;
        if(rle){if(cursor>=input.size())throw std::runtime_error("Truncated TGA RLE packet");auto packet=input[cursor++];run=(packet&127)+1;repeated=(packet&128)!=0;}
        if(pixel+run>count)throw std::runtime_error("TGA RLE packet exceeds image");
        if(repeated){auto color=read();for(unsigned i=0;i<run;++i)put(color);}else for(unsigned i=0;i<run;++i)put(read());
    }return image;
}
namespace {
double linear(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
double encoded(double x){return x<=.0031308?12.92*x:1.055*std::pow(x,1/2.4)-.055;}
std::uint8_t byte(double x){return static_cast<std::uint8_t>(std::clamp(std::lround(x*255),0l,255l));}
}
ImageMips make_mips(ImageData image,bool srgb,bool scalar){
    if(image.rgba.size()!=std::size_t(image.width)*image.height*4 || !image.width || !image.height)throw std::runtime_error("Invalid mip image storage");
    if(scalar)for(std::size_t i=0;i<image.rgba.size();i+=4){
        double v=image.alpha?image.rgba[i+3]/255.:0.;
        if(!image.alpha)for(int c=0;c<3;++c){double x=image.rgba[i+c]/255.;v+=(srgb?linear(x):x)/3;}
        auto b=byte(v);image.rgba[i]=image.rgba[i+1]=image.rgba[i+2]=b;image.rgba[i+3]=255;
    }
    // Scalar images already contain linear data, even with an explicit encoding.
    const bool encoded_color=srgb&&!scalar;
    ImageMips result;auto w=image.width,h=image.height;
    for(;;){
        result.levels.push_back({w,h,static_cast<std::uint32_t>(result.bytes.size()),static_cast<std::uint32_t>(image.rgba.size())});
        result.bytes.insert(result.bytes.end(),image.rgba.begin(),image.rgba.end());if(w==1 && h==1)break;
        auto nw=std::max(1u,w/2),nh=std::max(1u,h/2);std::vector<std::uint8_t> next(std::size_t(nw)*nh*4);
        // Area box resampling also accounts for the last row/column of NPOT images.
        for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){
            const double x0=double(x)*w/nw,x1=double(x+1)*w/nw,y0=double(y)*h/nh,y1=double(y+1)*h/nh;
            double sums[4]{},total=0;
            for(unsigned sy=unsigned(y0);sy<std::min(h,unsigned(std::ceil(y1)));++sy)for(unsigned sx=unsigned(x0);sx<std::min(w,unsigned(std::ceil(x1)));++sx){
                auto weight=(std::min(x1,double(sx+1))-std::max(x0,double(sx)))*(std::min(y1,double(sy+1))-std::max(y0,double(sy)));total+=weight;
                for(int c=0;c<4;++c){double v=image.rgba[(std::size_t(sy)*w+sx)*4+c]/255.;if(encoded_color && c<3)v=linear(v);sums[c]+=weight*v;}
            }
            for(int c=0;c<4;++c){auto v=sums[c]/total;if(encoded_color && c<3)v=encoded(v);next[(std::size_t(y)*nw+x)*4+c]=byte(v);}
        }image.rgba=std::move(next);w=nw;h=nh;
    }return result;
}
}
