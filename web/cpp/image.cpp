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
constexpr std::uint64_t linear_max=4294967295ull;
struct Transfer {
    std::array<std::uint64_t,256> decode;
    std::array<std::uint64_t,255> boundary;
    Transfer(){
        for(unsigned i=0;i<256;++i)decode[i]=std::llround(linear(i/255.)*linear_max);
        for(unsigned i=0;i<255;++i)boundary[i]=std::llround(linear((i+.5)/255.)*linear_max);
    }
};
const Transfer transfer;
std::uint8_t color_byte(std::uint64_t sum,std::uint64_t total){
    unsigned lo=0,hi=255;
    while(lo<hi){auto mid=(lo+hi)/2;if(sum>=transfer.boundary[mid]*total)lo=mid+1;else hi=mid;}
    return static_cast<std::uint8_t>(lo);
}
}
ImageMips make_mips(ImageData image,bool srgb,bool scalar){
    if(image.rgba.size()!=std::size_t(image.width)*image.height*4 || !image.width || !image.height)throw std::runtime_error("Invalid mip image storage");
    if(scalar)for(std::size_t i=0;i<image.rgba.size();i+=4){
        std::uint64_t sum=0;for(int c=0;c<3;++c)sum+=srgb?transfer.decode[image.rgba[i+c]]:image.rgba[i+c];
        auto b=image.alpha?image.rgba[i+3]:static_cast<std::uint8_t>(srgb?(sum*255+3*linear_max/2)/(3*linear_max):(sum+1)/3);
        image.rgba[i]=image.rgba[i+1]=image.rgba[i+2]=b;image.rgba[i+3]=255;
    }
    // Scalar images already contain linear data, even with an explicit encoding.
    const bool encoded_color=srgb&&!scalar;
    ImageMips result;auto w=image.width,h=image.height;
    for(;;){
        result.levels.push_back({w,h,static_cast<std::uint32_t>(result.bytes.size()),static_cast<std::uint32_t>(image.rgba.size())});
        result.bytes.insert(result.bytes.end(),image.rgba.begin(),image.rgba.end());if(w==1 && h==1)break;
        auto nw=std::max(1u,w/2),nh=std::max(1u,h/2);std::vector<std::uint8_t> next(std::size_t(nw)*nh*4);
        // Exact rational area weights include NPOT edges without half-byte drift.
        // Q32 linear-light values keep color sums deterministic across native/WASM.
        for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){
            const std::uint64_t x0=std::uint64_t(x)*w,x1=std::uint64_t(x+1)*w,y0=std::uint64_t(y)*h,y1=std::uint64_t(y+1)*h;
            std::uint64_t sums[4]{},total=std::uint64_t(w)*h;
            for(unsigned sy=unsigned(y0/nh);sy<(y1+nh-1)/nh;++sy)for(unsigned sx=unsigned(x0/nw);sx<(x1+nw-1)/nw;++sx){
                auto weight=(std::min(x1,std::uint64_t(sx+1)*nw)-std::max(x0,std::uint64_t(sx)*nw))*(std::min(y1,std::uint64_t(sy+1)*nh)-std::max(y0,std::uint64_t(sy)*nh));
                for(int c=0;c<4;++c){auto v=image.rgba[(std::size_t(sy)*w+sx)*4+c];sums[c]+=weight*(encoded_color&&c<3?transfer.decode[v]:v);}
            }
            for(int c=0;c<4;++c)next[(std::size_t(y)*nw+x)*4+c]=encoded_color&&c<3?color_byte(sums[c],total):static_cast<std::uint8_t>((sums[c]+total/2)/total);
        }image.rgba=std::move(next);w=nw;h=nh;
    }return result;
}
}
