/*
    Copyright (c) 2011, Philipp Krähenbühl
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the Stanford University nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY Philipp Krähenbühl ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Philipp Krähenbühl BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "densecrf.h"
#include <cstdio>
#include <cmath>
#include "util.h"
#include <iostream>
#include "../../approximateES.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include "probimage.h"
//#include <opencv2/opencv.hpp>

#define NO_NORMALIZATION 0
#define MEAN_NORMALIZATION 1
#define PIXEL_NORMALIZATION 2
using namespace std;
//using namespace cv;

#define SSTR( x ) dynamic_cast< std::ostringstream & >( \
                        ( std::ostringstream() << std::dec << x ) ).str()


// Store the colors we read, so that we can write them again.
int nColors = 0;
int colors[255];
unsigned int getColor( const unsigned char * c ){
	return c[0] + 256*c[1] + 256*256*c[2];
}
void putColor( unsigned char * c, unsigned int cc ){
	c[0] = cc&0xff; c[1] = (cc>>8)&0xff; c[2] = (cc>>16)&0xff;
}
// Produce a color image from a bunch of labels
unsigned char * colorize( short* map, int W, int H ){
	unsigned char * res = new unsigned char[ W*H*3 ];
	for( int k=0; k<W*H; k++ ){
        int r,g,b;
        get_color( map[k], r, g, b);
        res[3*k] = (unsigned char)r;
        res[3*k+1] = (unsigned char)g;
        res[3*k+2] = (unsigned char)b;
		//int c = colors[ map[k] ];
		//putColor( r+3*k, c );
	}
	return res;
}



float * classifyCompressed( const ProbImage& prob_im, int W, int H, int M , short* map)
{
	float * res = new float[W*H*M];
    float epsilon = 0; //1e-10;
	for( int k=0; k<W*H; k++ )
    {
        float * r = res + k*M;
        float mx = 0.0f;
        int imx = 0;
        for( int j=0; j<M; j++ )
        {
            float prob = prob_im(k%W, k/W, j);
            r[j] = -log( prob + epsilon);
            if( mx < prob )
            {
                mx = prob;
                imx = j;
            }
        }
        map[k] = (short)imx;
    }
	return res;
}

float * classifyCompressedNoLOG( const ProbImage& prob_im, int W, int H, int M , short* map)
{
	float * res = new float[W*H*M];
    float epsilon = 0; //1e-10;
	for( int k=0; k<W*H; k++ )
    {
        float * r = res + k*M;
        float mx = prob_im(k%W, k/W,0);
        int imx = 0;
        for( int j=0; j<M; j++ )
        {
            /*float prob = prob_im(k%W, k/W, j);
            r[j] = -log( prob + epsilon);
            if( mx < prob )
            {
                mx = prob;
                imx = j;
            }*/
            float boost_energy = prob_im(k%W, k/W, j);
            r[j] = -boost_energy;
            if( mx < boost_energy)
            {
                mx = boost_energy;
                imx = j;
            }
        }
        map[k] = (short)imx;
    }
	return res;
}


float * classifyGT_PROB( const ProbImage& prob_im, int W, int H, int M , short* map)
{
    float GT_PROB = 0.5;
	float * res = new float[W*H*M];
    float epsilon = 0; //1e-10;
	for( int k=0; k<W*H; k++ )
    {
        float * r = res + k*M;
        float mx = prob_im(k%W, k/W,0);
        int imx = 0;
        for( int j=0; j<M; j++ )
        {
            /*float prob = prob_im(k%W, k/W, j);
            r[j] = -log( prob + epsilon);
            if( mx < prob )
            {
                mx = prob;
                imx = j;
            }*/
            float boost_energy = prob_im(k%W, k/W, j);
            r[j] = -boost_energy;
            if( mx < boost_energy)
            {
                mx = boost_energy;
                imx = j;
            }
        }
        for(int j=0; j<M;j++)
            r[j] = -log( (1.0-GT_PROB)/(M-1) );
        r[imx] = -log(GT_PROB);
        map[k] = (short)imx;
    }
	return res;
}

float * classify( const ProbImage& prob_im, int W, int H, int M , short* map)
{
    return classifyCompressedNoLOG(prob_im, W, H, M, map);
}



int main( int argc, char* argv[]){
	if (argc<4){
		printf("Usage: %s image uncompressed_unary outputimg.ppm\n", argv[0] );
		return 1;
	}
    const int M = 21;
    int W, H, GW, GH;
    ProbImage prob_im;
    prob_im.decompress(argv[2]);
    GW = prob_im.width();
    GH = prob_im.height();
    unsigned char *im = readPPM( argv[1], W, H);
    if(!im){
        printf("Failed to load image!\n");
        return 1;
    }
    
    if(GW != W || GH != H )
    {
        printf("Error reading files!\n");
        return 1;
    }
    //classify( const ProbImage& prob_im, int W, int H, int M , short* map)
    short *map = new short[W*H];
    float *unary = classify( prob_im, W, H, M, map);
#pragma omp parallel for //log: 0_1_2_6_2_2
 for(int logl=-1; logl<=2;logl++) //0; 21-> 0
 {
     float l = powf(2,logl);
//#pragma omp parallel for
    for(int gsx = -1; gsx <= 3; gsx+=1) //3; 21-> 1,2
    {
        float expgsx = powf(2, gsx);
//#pragma omp parallel for
        for(int w1=0; w1<=3;w1+=1) //5; 21-> 2
        {
            float expw1 = powf(2, w1);
//#pragma omp parallel for
            for(int bsx = 6;  bsx <= 6; bsx+=1) //78; 21->
            {
                float expbsx = powf(2, bsx);
//#pragma omp parallel for
                for(int bsr = 2; bsr <= 2; bsr+= 1) //3 <- 7; 21->
                {
                    float expbsr = powf(2, bsr);
//#pragma omp parallel for
                    for(int w2=-1; w2 <=3; w2+= 1) //5;  21-> 5
                    {
                        float expw2 = powf(2, w2);
                        cout<<"[logl, gsx, w1, bsx, bsr, w] = "<<"["<<logl<<", "<<gsx<<", "<<w1<<", "<<bsx<<", "<<bsr<<", "<<w2<<"]"<<endl;
                        DenseCRF2D crf(W,H,M);
                        float* l_unary = new float[M*H*W];
                        for(int idx=0; idx < M*H*W; idx++)
                            l_unary[idx] = l*unary[idx];
                        crf.setUnaryEnergy( l_unary);
                        crf.setInitX(l_unary);
                        crf.addPairwiseGaussian( expgsx, expgsx, expw1);
                        string out(argv[3]);
                        string s = string("CROSSVAL-UNCOMPRESSED-COORD/")+SSTR(logl)+string("_")+SSTR(gsx)+string("_")+SSTR(w1)+string("_")+SSTR(bsx)+string("_")+SSTR(bsr)+string("_")+SSTR(w2)+string("/")+out;
                        string mkdir_s = string("mkdir -p ")+s;
                        system(mkdir_s.c_str());
                        string wrt_s = s+string("/")+out+string(".ppm");
                        crf.addPairwiseBilateral(expbsx, expbsx, expbsr, expbsr, expbsr, im, expw2);
                        crf.map(15, map);
                        unsigned char *res = colorize( map, W, H);
                        writePPM( wrt_s.c_str(), W, H, res);
                        delete[] res;
                        delete[] l_unary;
                    }
                }
            }
        }
    }
 }
    
    delete[] map;
    delete[] unary;
    delete[] im;
}
