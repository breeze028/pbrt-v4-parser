#include "image.h"
#include <iostream>
#include <stdexcept>
using namespace pbrt4::web;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> header(unsigned w,unsigned h,unsigned type=2,unsigned depth=24,unsigned flags=32){std::vector<std::uint8_t> b(18);b[2]=type;b[12]=w;b[13]=w>>8;b[14]=h;b[15]=h>>8;b[16]=depth;b[17]=flags;return b;}
int main(){try{
    auto data=header(2,2);data.insert(data.end(),{0,0,255,0,255,0,255,0,0,255,255,255});
    auto image=decode_tga(data);check(image.rgba[0]==255 && image.rgba[5]==255 && image.rgba[10]==255,"BGR and top origin");
    data[17]=0;image=decode_tga(data);check(image.rgba[2]==255 && image.rgba[8]==255,"bottom origin");
    data[17]=48;image=decode_tga(data);check(image.rgba[1]==255 && image.rgba[4]==255,"right origin");
    auto rle=header(2,1,10);rle.insert(rle.end(),{129,7,19,35});image=decode_tga(rle);check(image.rgba[0]==35 && image.rgba[4]==35,"RLE repeated RGB");
    auto bad=rle;bad[18]=130;bool rejected=false;try{decode_tga(bad);}catch(const std::exception&){rejected=true;}check(rejected,"RLE overflow rejected");
    bad={0,1,2};rejected=false;try{decode_tga(bad);}catch(const std::exception&){rejected=true;}check(rejected,"truncated header rejected");
    auto gray=header(2,1,3,8);gray.insert(gray.end(),{0,255});auto gray_image=decode_tga(gray);
    auto linear=make_mips(gray_image,false,false),srgb=make_mips(gray_image,true,false);
    check(linear.levels.size()==2 && linear.bytes[8]==128,"linear mip averaging");
    check(srgb.bytes[8]==188,"sRGB mip average in linear light");
    auto scalar=make_mips(gray_image,false,true);check(scalar.bytes[0]==0 && scalar.bytes[4]==255 && scalar.bytes[8]==128,"RGB scalar independent of opaque A");
    auto rgba=header(1,1,2,32,40);rgba.insert(rgba.end(),{255,255,255,64});scalar=make_mips(decode_tga(rgba),false,true);check(scalar.bytes[0]==64,"declared alpha scalar");
    auto npot=header(3,1,3,8);npot.insert(npot.end(),{0,0,255});auto mip=make_mips(decode_tga(npot),false,false);check(mip.bytes.back()==255 && mip.bytes[12]==85,"NPOT mip includes final column");
    std::cout<<"PASS TGA origins / BGR / RLE / malformed / alpha and RGB scalar / encoding and NPOT mips\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
