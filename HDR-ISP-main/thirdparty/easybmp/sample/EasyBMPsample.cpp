/*************************************************
*                                                *
*  EasyBMP Cross-Platform Windows Bitmap Library * 
*                                                *
*  Author: Paul Macklin                          *
*   email: macklin01@users.sourceforge.net       *
* support: http://easybmp.sourceforge.net        *
*                                                *
*          file: EasyBMPsample.cpp               * 
*    date added: 03-31-2006                      *
* date modified: 12-01-2006                      *
*       version: 1.06                            *
*                                                *
*   License: BSD (revised/modified)              *
* Copyright: 2005-6 by the EasyBMP Project       * 
*                                                *
* description: Sample application to demonstrate *
*              some functions and capabilities   *
*                                                *
*************************************************/

#include "EasyBMP.h"
using namespace std;

// 主函数：演示EasyBMP库的多种功能
int main( int argc, char* argv[] )
{
 // 输出版本信息和版权声明
 cout << endl
      << "Using EasyBMP Version " << _EasyBMP_Version_ << endl << endl
      << "Copyright (c) by the EasyBMP Project 2005-6" << endl
      << "WWW: http://easybmp.sourceforge.net" << endl << endl;

 // 创建BMP对象：用于存储文本图像
 BMP Text;
 // 从文件读取文本图像（包含透明背景的文字）
 Text.ReadFromFile("EasyBMPtext.bmp");

 // 创建BMP对象：用于存储背景图像
 BMP Background;
 // 从文件读取背景图像
 Background.ReadFromFile("EasyBMPbackground.bmp");

 // 创建BMP对象：用于存储最终输出图像
 BMP Output;
 // 设置输出图像的大小与背景图像相同
 Output.SetSize( Background.TellWidth() , Background.TellHeight() );
 // 设置输出图像的位深度为24位（真彩色）
 Output.SetBitDepth( 24 );

 // 将背景图像完全复制到输出图像中
 // 参数说明：源图像，源图像x范围(0到最大宽度-1)，源图像y范围(最大高度-1到0)，目标图像，目标位置(0,0)
 RangedPixelToPixelCopy( Background, 0, Output.TellWidth()-1,
                         Output.TellHeight()-1 , 0,
                         Output, 0,0 );	

 // 将文本图像的一部分透明地复制到输出图像中
 // 参数说明：源图像，源x范围(0-380)，源y范围(43-0)，目标图像，目标位置(110,5)，透明色(使用左上角像素作为透明色)
 RangedPixelToPixelCopyTransparent( Text, 0, 380,
                                    43, 0,
									Output, 110,5,
									*Text(0,0) );

 // 将文本图像的另一部分透明地复制到输出图像中
 // 参数说明：源图像，源x范围(0-最大宽度-1)，源y范围(最大宽度-1-50)，目标图像，目标位置(100,442)，透明色(使用(0,49)位置的像素作为透明色)
 RangedPixelToPixelCopyTransparent( Text, 0, Text.TellWidth()-1,
                                    Text.TellWidth()-1, 50,
									Output, 100,442,
									*Text(0,49) );

 // 设置输出图像的位深度为32位（带Alpha通道的RGBA）
 Output.SetBitDepth( 32 );
 cout << "writing 32bpp ... " << endl;
 // 将32位图像写入文件，演示高色深输出
 Output.WriteToFile( "EasyBMPoutput32bpp.bmp" );

 // 设置输出图像的位深度为24位（RGB真彩色）
 Output.SetBitDepth( 24 );
 cout << "writing 24bpp ... " << endl;
 // 将24位图像写入文件，这是最常见的位图格式
 Output.WriteToFile( "EasyBMPoutput24bpp.bmp" );

 // 设置输出图像的位深度为8位（256色）
 Output.SetBitDepth( 8 );
 cout << "writing 8bpp ... " << endl;
 // 将8位图像写入文件，演示调色板模式
 Output.WriteToFile( "EasyBMPoutput8bpp.bmp" );

 // 设置输出图像的位深度为4位（16色）
 Output.SetBitDepth( 4 );
 cout << "writing 4bpp ... " << endl;
 // 将4位图像写入文件，演示低色深模式
 Output.WriteToFile( "EasyBMPoutput4bpp.bmp" );

 // 设置输出图像的位深度为1位（黑白）
 Output.SetBitDepth( 1 );
 cout << "writing 1bpp ... " << endl;
 // 将1位图像写入文件，演示单色模式
 Output.WriteToFile( "EasyBMPoutput1bpp.bmp" );

 // 重新设置为24位，准备进行缩放操作
 Output.SetBitDepth( 24 );
 // 按百分比缩放图像：'p'表示按百分比，50表示缩小到50%
 Rescale( Output, 'p' , 50 );
 cout << "writing 24bpp scaled image ..." << endl;
 // 将缩放后的图像写入文件，演示图像缩放功能
 Output.WriteToFile( "EasyBMPoutput24bpp_rescaled.bmp" );

 return 0;
}
