/*#******************************************************************************
** IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
** 
** By downloading, copying, installing or using the software you agree to this license.
** If you do not agree to this license, do not download, install,
** copy or use the software.
** 
** 
** HVStools : interfaces allowing OpenCV users to integrate Human Vision System models. Presented models originate from Jeanny Herault's original research and have been reused and adapted by the author&collaborators for computed vision applications since his thesis with Alice Caplier at Gipsa-Lab.
** Use: extract still images & image sequences features, from contours details to motion spatio-temporal features, etc. for high level visual scene analysis. Also contribute to image enhancement/compression such as tone mapping.
** 
** Maintainers : Listic lab (code author current affiliation & applications) and Gipsa Lab (original research origins & applications)
** 
**  Creation - enhancement process 2007-2011
**      Author: Alexandre Benoit (benoit.alexandre.vision@gmail.com), LISTIC lab, Annecy le vieux, France
** 
** Theses algorithm have been developped by Alexandre BENOIT since his thesis with Alice Caplier at Gipsa-Lab (www.gipsa-lab.inpg.fr) and the research he pursues at LISTIC Lab (www.listic.univ-savoie.fr).
** Refer to the following research paper for more information:
** Benoit A., Caplier A., Durette B., Herault, J., "USING HUMAN VISUAL SYSTEM MODELING FOR BIO-INSPIRED LOW LEVEL IMAGE PROCESSING", Elsevier, Computer Vision and Image Understanding 114 (2010), pp. 758-773, DOI: http://dx.doi.org/10.1016/j.cviu.2010.01.011
** This work have been carried out thanks to Jeanny Herault who's research and great discussions are the basis of all this work, please take a look at his book:
** Vision: Images, Signals and Neural Networks: Models of Neural Processing in Visual Perception (Progress in Neural Processing),By: Jeanny Herault, ISBN: 9814273686. WAPI (Tower ID): 113266891.
** 
** The retina filter includes the research contributions of phd/research collegues from which code has been redrawn by the author :
** _take a look at the retinacolor.hpp module to discover Brice Chaix de Lavarene color mosaicing/demosaicing and the reference paper:
** ====> B. Chaix de Lavarene, D. Alleysson, B. Durette, J. Herault (2007). "Efficient demosaicing through recursive filtering", IEEE International Conference on Image Processing ICIP 2007
** _take a look at imagelogpolprojection.hpp to discover retina spatial log sampling which originates from Barthelemy Durette phd with Jeanny Herault. A Retina / V1 cortex projection is also proposed and originates from Jeanny's discussions.
** ====> more informations in the above cited Jeanny Heraults's book.
** 
**                          License Agreement
**               For Open Source Computer Vision Library
** 
** Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
** Copyright (C) 2008-2011, Willow Garage Inc., all rights reserved.
** 
**               For Human Visual System tools (hvstools)
** Copyright (C) 2007-2011, LISTIC Lab, Annecy le Vieux and GIPSA Lab, Grenoble, France, all rights reserved.
** 
** Third party copyrights are property of their respective owners.
** 
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
** 
** * Redistributions of source code must retain the above copyright notice,
**    this list of conditions and the following disclaimer.
** 
** * Redistributions in binary form must reproduce the above copyright notice,
**    this list of conditions and the following disclaimer in the documentation
**    and/or other materials provided with the distribution.
** 
** * The name of the copyright holders may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** This software is provided by the copyright holders and contributors "as is" and
** any express or implied warranties, including, but not limited to, the implied
** warranties of merchantability and fitness for a particular purpose are disclaimed.
** In no event shall the Intel Corporation or contributors be liable for any direct,
** indirect, incidental, special, exemplary, or consequential damages
** (including, but not limited to, procurement of substitute goods or services;
** loss of use, data, or profits; or business interruption) however caused
** and on any theory of liability, whether in contract, strict liability,
** or tort (including negligence or otherwise) arising in any way out of
** the use of this software, even if advised of the possibility of such damage.
*******************************************************************************/

#include "precomp.hpp"

#include "retinacolor.hpp"

// @author Alexandre BENOIT, benoit.alexandre.vision@gmail.com, LISTIC : www.listic.univ-savoie.fr, Gipsa-Lab, France: www.gipsa-lab.inpg.fr/

#include <iostream>
#include <ctime>

namespace cv
{

// init static values
static double _LMStoACr1Cr2[]={1.0,  1.0, 0.0,  1.0, -1.0, 0.0,  -0.5, -0.5, 1.0};
//static double _ACr1Cr2toLMS[]={0.5,  0.5, 0.0,   0.5, -0.5, 0.0,  0.5,  0.0, 1.0};
static double _LMStoLab[]={0.5774, 0.5774, 0.5774, 0.4082, 0.4082, -0.8165, 0.7071, -0.7071, 0.0};

// constructor/desctructor
RetinaColor::RetinaColor(const unsigned int NBrows, const unsigned int NBcolumns, const RETINA_COLORSAMPLINGMETHOD samplingMethod)
:BasicRetinaFilter(NBrows, NBcolumns, 3),
 _colorSampling(NBrows*NBcolumns),
 _RGBmosaic(NBrows*NBcolumns*3),
 _tempMultiplexedFrame(NBrows*NBcolumns),
 _demultiplexedTempBuffer(NBrows*NBcolumns*3),
 _demultiplexedColorFrame(NBrows*NBcolumns*3),
 _chrominance(NBrows*NBcolumns*3),
 _colorLocalDensity(NBrows*NBcolumns*3),
 _imageGradient(NBrows*NBcolumns*3)
{
	// link to parent buffers (let's recycle !)
	_luminance=&_filterOutput;
	_multiplexedFrame=&_localBuffer;

	_objectInit=false;
	_samplingMethod=samplingMethod;
	_saturateColors=false;
	_colorSaturationValue=4.0;

	// set default spatio-temporal filter parameters
	setLPfilterParameters(0.0, 0.0, 1.5);
	setLPfilterParameters(0.0, 0.0, 10.5, 1);// for the low pass filter dedicated to contours energy extraction (demultiplexing process)
	setLPfilterParameters(0.0, 0.0, 0.9, 2);

	// init default value on image Gradient
	_imageGradient=0.57;

	// init color sampling map
	_initColorSampling();

	// flush all buffers
	clearAllBuffers();
}

RetinaColor::~RetinaColor()
{

}

/**
* function that clears all buffers of the object
*/
void RetinaColor::clearAllBuffers()
{
	BasicRetinaFilter::clearAllBuffers();
	_tempMultiplexedFrame=0;
	_demultiplexedTempBuffer=0;

	_demultiplexedColorFrame=0;
	_chrominance=0;
	_imageGradient=1;
}

/**
* resize retina color filter object (resize all allocated buffers)
* @param NBrows: the new height size
* @param NBcolumns: the new width size
*/
void RetinaColor::resize(const unsigned int NBrows, const unsigned int NBcolumns)
{
	BasicRetinaFilter::clearAllBuffers();
	_colorSampling.resize(NBrows*NBcolumns);
	_RGBmosaic.resize(NBrows*NBcolumns*3);
	_tempMultiplexedFrame.resize(NBrows*NBcolumns);
	_demultiplexedTempBuffer.resize(NBrows*NBcolumns*3);
	_demultiplexedColorFrame.resize(NBrows*NBcolumns*3);
	_chrominance.resize(NBrows*NBcolumns*3);
	_colorLocalDensity.resize(NBrows*NBcolumns*3);
	_imageGradient.resize(NBrows*NBcolumns*3);

	// link to parent buffers (let's recycle !)
	_luminance=&_filterOutput;
	_multiplexedFrame=&_localBuffer;

	// init color sampling map
	_initColorSampling();

	// clean buffers
	clearAllBuffers();
}


void RetinaColor::_initColorSampling()
{

	// filling the conversion table for multiplexed <=> demultiplexed frame
	srand(time(NULL));

	// preInit cones probabilities
	_pR=_pB=_pG=0;
	switch (_samplingMethod)
	{
	case RETINA_COLOR_RANDOM:
		for (unsigned int index=0 ; index<this->getNBpixels(); ++index)
		{

			// random RGB sampling
			unsigned int colorIndex=rand()%24;

			if (colorIndex<8){
				colorIndex=0;

				++_pR;
			}else
			{
				if (colorIndex<21){
					colorIndex=1;
					++_pG;
				}else{
					colorIndex=2;
					++_pB;
				}
			}
			_colorSampling[index] = colorIndex*this->getNBpixels()+index;
		}
		_pR/=(double)this->getNBpixels();
		_pG/=(double)this->getNBpixels();
		_pB/=(double)this->getNBpixels();
		std::cout<<"Color channels proportions: pR, pG, pB= "<<_pR<<", "<<_pG<<", "<<_pB<<", "<<std::endl;
		break;
	case RETINA_COLOR_DIAGONAL:
		for (unsigned int index=0 ; index<this->getNBpixels(); ++index)
		{
			_colorSampling[index] = index+((index%3+(index%_filterOutput.getNBcolumns()))%3)*_filterOutput.getNBpixels();
		}
		_pR=_pB=_pG=1.0/3.0;
		break;
	case RETINA_COLOR_BAYER: // default sets bayer sampling
		for (unsigned int index=0 ; index<_filterOutput.getNBpixels(); ++index)
		{
			//First line: R G R G
			_colorSampling[index] = index+((index/_filterOutput.getNBcolumns())%2)*_filterOutput.getNBpixels()+((index%_filterOutput.getNBcolumns())%2)*_filterOutput.getNBpixels();
			//First line: G R G R
			//_colorSampling[index] = 3*index+((index/_filterOutput.getNBcolumns())%2)+((index%_filterOutput.getNBcolumns()+1)%2);
		}
		_pR=_pB=0.25;
		_pG=0.5;
		break;
	default:
#ifdef RETINACOLORDEBUG
		std::cerr<<"RetinaColor::No or wrong color sampling method, skeeping"<<std::endl;
#endif
		return;
		break;//.. not useful, yes

	}
	// feeling the mosaic buffer:
	_RGBmosaic=0;
	for (unsigned int index=0 ; index<_filterOutput.getNBpixels(); ++index)
		// the RGB _RGBmosaic buffer contains 1 where the pixel corresponds to a sampled color
		_RGBmosaic[_colorSampling[index]]=1.0;

	// computing photoreceptors local density
	_spatiotemporalLPfilter(&_RGBmosaic[0], &_colorLocalDensity[0]);
	_spatiotemporalLPfilter(&_RGBmosaic[0]+_filterOutput.getNBpixels(), &_colorLocalDensity[0]+_filterOutput.getNBpixels());
	_spatiotemporalLPfilter(&_RGBmosaic[0]+_filterOutput.getDoubleNBpixels(), &_colorLocalDensity[0]+_filterOutput.getDoubleNBpixels());
	unsigned int maxNBpixels=3*_filterOutput.getNBpixels();
	register double *colorLocalDensityPTR=&_colorLocalDensity[0];
	for (unsigned int i=0;i<maxNBpixels;++i, ++colorLocalDensityPTR)
		*colorLocalDensityPTR=1.0/ *colorLocalDensityPTR;

#ifdef RETINACOLORDEBUG
	std::cout<<"INIT 	_colorLocalDensity max, min: "<<_colorLocalDensity.max()<<", "<<_colorLocalDensity.min()<<std::endl;
#endif
	// end of the init step
	_objectInit=true;
}

// public functions

void RetinaColor::runColorDemultiplexing(const std::valarray<double> &multiplexedColorFrame, const bool adaptiveFiltering, const double maxInputValue)
{
	// demultiplex the grey frame to RGB frame
	// -> first set demultiplexed frame to 0
	_demultiplexedTempBuffer=0;
	// -> demultiplex process
	register unsigned int *colorSamplingPRT=&_colorSampling[0];
	register const double *multiplexedColorFramePTR=get_data(multiplexedColorFrame);
	for (unsigned int indexa=0; indexa<_filterOutput.getNBpixels() ; ++indexa)
		_demultiplexedTempBuffer[*(colorSamplingPRT++)]=*(multiplexedColorFramePTR++);

	// interpolate the demultiplexed frame depending on the color sampling method
	if (!adaptiveFiltering)
		_interpolateImageDemultiplexedImage(&_demultiplexedTempBuffer[0]);

	// low pass filtering the demultiplexed frame
	_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0], &_chrominance[0]);
	_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getNBpixels(), &_chrominance[0]+_filterOutput.getNBpixels());
	_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getDoubleNBpixels(), &_chrominance[0]+_filterOutput.getDoubleNBpixels());

	/*if (_samplingMethod=BAYER)
	{
		_applyRIFfilter(_chrominance, _chrominance);
		_applyRIFfilter(_chrominance+_filterOutput.getNBpixels(), _chrominance+_filterOutput.getNBpixels());
		_applyRIFfilter(_chrominance+_filterOutput.getDoubleNBpixels(), _chrominance+_filterOutput.getDoubleNBpixels());
	}*/

	// normalize by the photoreceptors local density and retrieve the local luminance
	register double *chrominancePTR= &_chrominance[0];
	register double *colorLocalDensityPTR= &_colorLocalDensity[0];
	register double *luminance= &(*_luminance)[0];
	if (!adaptiveFiltering)// compute the gradient on the luminance
	{
		if (_samplingMethod==RETINA_COLOR_RANDOM)
			for (unsigned int indexc=0; indexc<_filterOutput.getNBpixels() ; ++indexc, ++chrominancePTR, ++colorLocalDensityPTR, ++luminance)
			{
				// normalize by photoreceptors density
				double Cr=*(chrominancePTR)*_colorLocalDensity[indexc];
				double Cg=*(chrominancePTR+_filterOutput.getNBpixels())*_colorLocalDensity[indexc+_filterOutput.getNBpixels()];
				double Cb=*(chrominancePTR+_filterOutput.getDoubleNBpixels())*_colorLocalDensity[indexc+_filterOutput.getDoubleNBpixels()];
				*luminance=(Cr+Cg+Cb)*_pG;
				*(chrominancePTR)=Cr-*luminance;
				*(chrominancePTR+_filterOutput.getNBpixels())=Cg-*luminance;
				*(chrominancePTR+_filterOutput.getDoubleNBpixels())=Cb-*luminance;
			}
		else
			for (unsigned int indexc=0; indexc<_filterOutput.getNBpixels() ; ++indexc, ++chrominancePTR, ++colorLocalDensityPTR, ++luminance)
			{
				double Cr=*(chrominancePTR);
				double Cg=*(chrominancePTR+_filterOutput.getNBpixels());
				double Cb=*(chrominancePTR+_filterOutput.getDoubleNBpixels());
				*luminance=_pR*Cr+_pG*Cg+_pB*Cb;
				*(chrominancePTR)=Cr-*luminance;
				*(chrominancePTR+_filterOutput.getNBpixels())=Cg-*luminance;
				*(chrominancePTR+_filterOutput.getDoubleNBpixels())=Cb-*luminance;
			}

		// in order to get the color image, each colored map needs to be added the luminance
		// -> to do so, compute:  multiplexedColorFrame - remultiplexed chrominances
		runColorMultiplexing(_chrominance, _tempMultiplexedFrame);
		//lum = 1/3((f*(ImR))/(f*mR) + (f*(ImG))/(f*mG) + (f*(ImB))/(f*mB));
		double *luminancePTR= &(*_luminance)[0];
		chrominancePTR= &_chrominance[0];
		double *demultiplexedColorFramePTR= &_demultiplexedColorFrame[0];
		for (unsigned int indexp=0; indexp<_filterOutput.getNBpixels() ; ++indexp, ++luminancePTR, ++chrominancePTR, ++demultiplexedColorFramePTR)
		{
			*luminancePTR=(multiplexedColorFrame[indexp]-_tempMultiplexedFrame[indexp]);
			*(demultiplexedColorFramePTR)=*(chrominancePTR)+*luminancePTR;
			*(demultiplexedColorFramePTR+_filterOutput.getNBpixels())=*(chrominancePTR+_filterOutput.getNBpixels())+*luminancePTR;
			*(demultiplexedColorFramePTR+_filterOutput.getDoubleNBpixels())=*(chrominancePTR+_filterOutput.getDoubleNBpixels())+*luminancePTR;
		}

	}else
	{
		register const double *multiplexedColorFramePTR= get_data(multiplexedColorFrame);
		for (unsigned int indexc=0; indexc<_filterOutput.getNBpixels() ; ++indexc, ++chrominancePTR, ++colorLocalDensityPTR, ++luminance, ++multiplexedColorFramePTR)
		{
			// normalize by photoreceptors density
			double Cr=*(chrominancePTR)*_colorLocalDensity[indexc];
			double Cg=*(chrominancePTR+_filterOutput.getNBpixels())*_colorLocalDensity[indexc+_filterOutput.getNBpixels()];
			double Cb=*(chrominancePTR+_filterOutput.getDoubleNBpixels())*_colorLocalDensity[indexc+_filterOutput.getDoubleNBpixels()];
			*luminance=(Cr+Cg+Cb)*_pG;
			_demultiplexedTempBuffer[_colorSampling[indexc]] = *multiplexedColorFramePTR - *luminance;

		}

		// compute the gradient of the luminance
		_computeGradient(&(*_luminance)[0]);

		// adaptively filter the submosaics to get the adaptive densities, here the buffer _chrominance is used as a temp buffer
		_adaptiveSpatialLPfilter(&_RGBmosaic[0], &_chrominance[0]);
		_adaptiveSpatialLPfilter(&_RGBmosaic[0]+_filterOutput.getNBpixels(), &_chrominance[0]+_filterOutput.getNBpixels());
		_adaptiveSpatialLPfilter(&_RGBmosaic[0]+_filterOutput.getDoubleNBpixels(), &_chrominance[0]+_filterOutput.getDoubleNBpixels());

		_adaptiveSpatialLPfilter(&_demultiplexedTempBuffer[0], &_demultiplexedColorFrame[0]);
		_adaptiveSpatialLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getNBpixels(), &_demultiplexedColorFrame[0]+_filterOutput.getNBpixels());
		_adaptiveSpatialLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getDoubleNBpixels(), &_demultiplexedColorFrame[0]+_filterOutput.getDoubleNBpixels());

		for (unsigned int index=0; index<_filterOutput.getNBpixels()*3 ; ++index) // cette boucle pourrait �tre supprimee en passant la densit� � la fonction de filtrage
			_demultiplexedColorFrame[index] /= _chrominance[index];

		// compute and substract the residual luminance
		for (unsigned int index=0; index<_filterOutput.getNBpixels() ; ++index)
		{
			double residu = _pR*_demultiplexedColorFrame[index] + _pG*_demultiplexedColorFrame[index+_filterOutput.getNBpixels()] + _pB*_demultiplexedColorFrame[index+_filterOutput.getDoubleNBpixels()];
			_demultiplexedColorFrame[index] = _demultiplexedColorFrame[index] - residu;
			_demultiplexedColorFrame[index+_filterOutput.getNBpixels()] = _demultiplexedColorFrame[index+_filterOutput.getNBpixels()] - residu;
			_demultiplexedColorFrame[index+_filterOutput.getDoubleNBpixels()] = _demultiplexedColorFrame[index+_filterOutput.getDoubleNBpixels()] - residu;
		}

		// multiplex the obtained chrominance
		runColorMultiplexing(_demultiplexedColorFrame, _tempMultiplexedFrame);
		_demultiplexedTempBuffer=0;

		// get the luminance, et and add it to each chrominance
		for (unsigned int index=0; index<_filterOutput.getNBpixels() ; ++index)
		{
			(*_luminance)[index]=multiplexedColorFrame[index]-_tempMultiplexedFrame[index];
			_demultiplexedTempBuffer[_colorSampling[index]] = _demultiplexedColorFrame[_colorSampling[index]];//multiplexedColorFrame[index] - (*_luminance)[index];
		}

		_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0], &_demultiplexedTempBuffer[0]);
		_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getNBpixels(), &_demultiplexedTempBuffer[0]+_filterOutput.getNBpixels());
		_spatiotemporalLPfilter(&_demultiplexedTempBuffer[0]+_filterOutput.getDoubleNBpixels(), &_demultiplexedTempBuffer[0]+_filterOutput.getDoubleNBpixels());

		// get the luminance and add it to each chrominance
		for (unsigned int index=0; index<_filterOutput.getNBpixels() ; ++index)
		{
			_demultiplexedColorFrame[index] = _demultiplexedTempBuffer[index]*_colorLocalDensity[index]+ (*_luminance)[index];
			_demultiplexedColorFrame[index+_filterOutput.getNBpixels()] = _demultiplexedTempBuffer[index+_filterOutput.getNBpixels()]*_colorLocalDensity[index+_filterOutput.getNBpixels()]+ (*_luminance)[index];
			_demultiplexedColorFrame[index+_filterOutput.getDoubleNBpixels()] = _demultiplexedTempBuffer[index+_filterOutput.getDoubleNBpixels()]*_colorLocalDensity[index+_filterOutput.getDoubleNBpixels()]+ (*_luminance)[index];
		}
	}

	// eliminate saturated colors by simple clipping values to the input range
	clipRGBOutput_0_maxInputValue(NULL, maxInputValue);

	/* transfert image gradient in order to check validity
    memcpy((*_luminance), _imageGradient, sizeof(double)*_filterOutput.getNBpixels());
    memcpy(_demultiplexedColorFrame, _imageGradient+_filterOutput.getNBpixels(), sizeof(double)*_filterOutput.getNBpixels());
    memcpy(_demultiplexedColorFrame+_filterOutput.getNBpixels(), _imageGradient+_filterOutput.getNBpixels(), sizeof(double)*_filterOutput.getNBpixels());
    memcpy(_demultiplexedColorFrame+2*_filterOutput.getNBpixels(), _imageGradient+_filterOutput.getNBpixels(), sizeof(double)*_filterOutput.getNBpixels());
	 */

	if (_saturateColors)
	{
		TemplateBuffer<double>::normalizeGrayOutputCentredSigmoide(128, _colorSaturationValue, maxInputValue, &_demultiplexedColorFrame[0], &_demultiplexedColorFrame[0], _filterOutput.getNBpixels());
		TemplateBuffer<double>::normalizeGrayOutputCentredSigmoide(128, _colorSaturationValue, maxInputValue, &_demultiplexedColorFrame[0]+_filterOutput.getNBpixels(), &_demultiplexedColorFrame[0]+_filterOutput.getNBpixels(), _filterOutput.getNBpixels());
		TemplateBuffer<double>::normalizeGrayOutputCentredSigmoide(128, _colorSaturationValue, maxInputValue, &_demultiplexedColorFrame[0]+_filterOutput.getNBpixels()*2, &_demultiplexedColorFrame[0]+_filterOutput.getNBpixels()*2, _filterOutput.getNBpixels());
	}
}

// color multiplexing: input frame size=_NBrows*_filterOutput.getNBcolumns()*3, multiplexedFrame output size=_NBrows*_filterOutput.getNBcolumns()
void RetinaColor::runColorMultiplexing(const std::valarray<double> &demultiplexedInputFrame, std::valarray<double> &multiplexedFrame)
{
	// multiply each color layer by its bayer mask
	register unsigned int *colorSamplingPTR= &_colorSampling[0];
	register double *multiplexedFramePTR= &multiplexedFrame[0];
	for (unsigned int indexp=0; indexp<_filterOutput.getNBpixels(); ++indexp)
		*(multiplexedFramePTR++)=demultiplexedInputFrame[*(colorSamplingPTR++)];
}

void RetinaColor::normalizeRGBOutput_0_maxOutputValue(const double maxOutputValue)
{
	//normalizeGrayOutputCentredSigmoide(0.0, 2, _chrominance);
	TemplateBuffer<double>::normalizeGrayOutput_0_maxOutputValue(&_demultiplexedColorFrame[0], 3*_filterOutput.getNBpixels(), maxOutputValue);
	//normalizeGrayOutputCentredSigmoide(0.0, 2, _chrominance+_filterOutput.getNBpixels());
	//normalizeGrayOutput_0_maxOutputValue(_demultiplexedColorFrame+_filterOutput.getNBpixels(), _filterOutput.getNBpixels(), maxOutputValue);
	//normalizeGrayOutputCentredSigmoide(0.0, 2, _chrominance+2*_filterOutput.getNBpixels());
	//normalizeGrayOutput_0_maxOutputValue(_demultiplexedColorFrame+_filterOutput.getDoubleNBpixels(), _filterOutput.getNBpixels(), maxOutputValue);
	TemplateBuffer<double>::normalizeGrayOutput_0_maxOutputValue(&(*_luminance)[0], _filterOutput.getNBpixels(), maxOutputValue);
}

/// normalize output between 0 and maxOutputValue;
void RetinaColor::clipRGBOutput_0_maxInputValue(double *inputOutputBuffer, const double maxInputValue)
{
	//std::cout<<"RetinaColor::normalizing RGB frame..."<<std::endl;
	// if outputBuffer unsassigned, the rewrite the buffer
	if (inputOutputBuffer==NULL)
		inputOutputBuffer= &_demultiplexedColorFrame[0];

	register double *inputOutputBufferPTR=inputOutputBuffer;
	for (register unsigned int jf = 0; jf < _filterOutput.getNBpixels()*3; ++jf, ++inputOutputBufferPTR)
	{
		if (*inputOutputBufferPTR>maxInputValue)
			*inputOutputBufferPTR=maxInputValue;
		else if (*inputOutputBufferPTR<0)
			*inputOutputBufferPTR=0;
	}
	//std::cout<<"RetinaColor::...normalizing RGB frame OK"<<std::endl;
}

void RetinaColor::_interpolateImageDemultiplexedImage(double *inputOutputBuffer)
{

	switch(_samplingMethod)
	{

	case RETINA_COLOR_RANDOM:
		return; // no need to interpolate
		break;

	case RETINA_COLOR_DIAGONAL:
		_interpolateSingleChannelImage111(inputOutputBuffer);
		break;

	case RETINA_COLOR_BAYER: // default sets bayer sampling
		_interpolateBayerRGBchannels(inputOutputBuffer);
		break;
	default:
		std::cerr<<"RetinaColor::No or wrong color sampling method, skeeping"<<std::endl;
		return;
		break;//.. not useful, yes

	}

}

void RetinaColor::_interpolateSingleChannelImage111(double *inputOutputBuffer)
{
	for (unsigned int indexr=0 ; indexr<_filterOutput.getNBrows(); ++indexr)
	{
		for (unsigned int indexc=1 ; indexc<_filterOutput.getNBcolumns()-1; ++indexc)
		{
			unsigned int index=indexc+indexr*_filterOutput.getNBcolumns();
			inputOutputBuffer[index]=(inputOutputBuffer[index-1]+inputOutputBuffer[index]+inputOutputBuffer[index+1])/3.0;
		}
	}
	for (unsigned int indexc=0 ; indexc<_filterOutput.getNBcolumns(); ++indexc)
	{
		for (unsigned int indexr=1 ; indexr<_filterOutput.getNBrows()-1; ++indexr)
		{
			unsigned int index=indexc+indexr*_filterOutput.getNBcolumns();
			inputOutputBuffer[index]=(inputOutputBuffer[index-_filterOutput.getNBcolumns()]+inputOutputBuffer[index]+inputOutputBuffer[index+_filterOutput.getNBcolumns()])/3.0;
		}
	}
}

void RetinaColor::_interpolateBayerRGBchannels(double *inputOutputBuffer)
{
	for (unsigned int indexr=0 ; indexr<_filterOutput.getNBrows()-1; indexr+=2)
	{
		for (unsigned int indexc=1 ; indexc<_filterOutput.getNBcolumns()-1; indexc+=2)
		{
			unsigned int indexR=indexc+indexr*_filterOutput.getNBcolumns();
			unsigned int indexB=_filterOutput.getDoubleNBpixels()+indexc+1+(indexr+1)*_filterOutput.getNBcolumns();
			inputOutputBuffer[indexR]=(inputOutputBuffer[indexR-1]+inputOutputBuffer[indexR+1])/2.0;
			inputOutputBuffer[indexB]=(inputOutputBuffer[indexB-1]+inputOutputBuffer[indexB+1])/2.0;
		}
	}
	for (unsigned int indexr=1 ; indexr<_filterOutput.getNBrows()-1; indexr+=2)
	{
		for (unsigned int indexc=0 ; indexc<_filterOutput.getNBcolumns(); ++indexc)
		{
			unsigned int indexR=indexc+indexr*_filterOutput.getNBcolumns();
			unsigned int indexB=_filterOutput.getDoubleNBpixels()+indexc+1+(indexr+1)*_filterOutput.getNBcolumns();
			inputOutputBuffer[indexR]=(inputOutputBuffer[indexR-_filterOutput.getNBcolumns()]+inputOutputBuffer[indexR+_filterOutput.getNBcolumns()])/2.0;
			inputOutputBuffer[indexB]=(inputOutputBuffer[indexB-_filterOutput.getNBcolumns()]+inputOutputBuffer[indexB+_filterOutput.getNBcolumns()])/2.0;

		}
	}
	for (unsigned int indexr=1 ; indexr<_filterOutput.getNBrows()-1; ++indexr)
		for (unsigned int indexc=0 ; indexc<_filterOutput.getNBcolumns(); indexc+=2)
		{
			unsigned int indexG=_filterOutput.getNBpixels()+indexc+(indexr)*_filterOutput.getNBcolumns()+indexr%2;
			inputOutputBuffer[indexG]=(inputOutputBuffer[indexG-1]+inputOutputBuffer[indexG+1]+inputOutputBuffer[indexG-_filterOutput.getNBcolumns()]+inputOutputBuffer[indexG+_filterOutput.getNBcolumns()])*0.25;
		}
}

void RetinaColor::_applyRIFfilter(const double *sourceBuffer, double *destinationBuffer)
{
	for (unsigned int indexr=1 ; indexr<_filterOutput.getNBrows()-1; ++indexr)
	{
		for (unsigned int indexc=1 ; indexc<_filterOutput.getNBcolumns()-1; ++indexc)
		{
			unsigned int index=indexc+indexr*_filterOutput.getNBcolumns();
			_tempMultiplexedFrame[index]=(4.0*sourceBuffer[index]+sourceBuffer[index-1-_filterOutput.getNBcolumns()]+sourceBuffer[index-1+_filterOutput.getNBcolumns()]+sourceBuffer[index+1-_filterOutput.getNBcolumns()]+sourceBuffer[index+1+_filterOutput.getNBcolumns()])*0.125;
		}
	}
	memcpy(destinationBuffer, &_tempMultiplexedFrame[0], sizeof(double)*_filterOutput.getNBpixels());
}

void RetinaColor::_getNormalizedContoursImage(const double *inputFrame, double *outputFrame)
{
	double maxValue=0;
	double normalisationFactor=1.0/3.0;
	for (unsigned int indexr=1 ; indexr<_filterOutput.getNBrows()-1; ++indexr)
	{
		for (unsigned int indexc=1 ; indexc<_filterOutput.getNBcolumns()-1; ++indexc)
		{
			unsigned int index=indexc+indexr*_filterOutput.getNBcolumns();
			outputFrame[index]=normalisationFactor*fabs(8.0*inputFrame[index]-inputFrame[index-1]-inputFrame[index+1]-inputFrame[index-_filterOutput.getNBcolumns()]-inputFrame[index+_filterOutput.getNBcolumns()]-inputFrame[index-1-_filterOutput.getNBcolumns()]-inputFrame[index-1+_filterOutput.getNBcolumns()]-inputFrame[index+1-_filterOutput.getNBcolumns()]-inputFrame[index+1+_filterOutput.getNBcolumns()]);
			if (outputFrame[index]>maxValue)
				maxValue=outputFrame[index];
		}
	}
	normalisationFactor=1.0/maxValue;
	// normalisation [0, 1]
	                  for (unsigned int indexp=1 ; indexp<_filterOutput.getNBrows()-1; ++indexp)
	                	  outputFrame[indexp]=outputFrame[indexp]*normalisationFactor;
}

//////////////////////////////////////////////////////////
//        ADAPTIVE BASIC RETINA FILTER
//////////////////////////////////////////////////////////
// run LP filter for a new frame input and save result at a specific output adress
void RetinaColor::_adaptiveSpatialLPfilter(const double *inputFrame, double *outputFrame)
{

	/**********/
	_gain = (1-0.57)*(1-0.57)*(1-0.06)*(1-0.06);

	// launch the serie of 1D directional filters in order to compute the 2D low pass filter
	_adaptiveHorizontalCausalFilter_addInput(inputFrame, outputFrame, 0, _filterOutput.getNBrows());
	_adaptiveHorizontalAnticausalFilter(outputFrame, 0, _filterOutput.getNBrows());
	_adaptiveVerticalCausalFilter(outputFrame, 0, _filterOutput.getNBcolumns());
	_adaptiveVerticalAnticausalFilter_multGain(outputFrame, 0, _filterOutput.getNBcolumns());

}

//  horizontal causal filter which adds the input inside
void RetinaColor::_adaptiveHorizontalCausalFilter_addInput(const double *inputFrame, double *outputFrame, unsigned int IDrowStart, unsigned int IDrowEnd)
{
	register double* outputPTR=outputFrame+IDrowStart*_filterOutput.getNBcolumns();
	register const double* inputPTR=inputFrame+IDrowStart*_filterOutput.getNBcolumns();
	register double *imageGradientPTR= &_imageGradient[0]+IDrowStart*_filterOutput.getNBcolumns();
	for (unsigned int IDrow=IDrowStart; IDrow<IDrowEnd; ++IDrow)
	{
		register double result=0;
		for (unsigned int index=0; index<_filterOutput.getNBcolumns(); ++index)
		{
			//std::cout<<(*imageGradientPTR)<<" ";
			result = *(inputPTR++) + (*imageGradientPTR)* result;
			*(outputPTR++) = result;
			++imageGradientPTR;
		}
		//        std::cout<<" "<<std::endl;
	}
}

//  horizontal anticausal filter  (basic way, no add on)
void RetinaColor::_adaptiveHorizontalAnticausalFilter(double *outputFrame, unsigned int IDrowStart, unsigned int IDrowEnd)
{
	register double* outputPTR=outputFrame+IDrowEnd*(_filterOutput.getNBcolumns())-1;
	register double *imageGradientPTR= &_imageGradient[0]+IDrowEnd*(_filterOutput.getNBcolumns())-1;

	for (unsigned int IDrow=IDrowStart; IDrow<IDrowEnd; ++IDrow)
	{
		register double result=0;
		for (unsigned int index=0; index<_filterOutput.getNBcolumns(); ++index)
		{
			result = *(outputPTR)+  (*imageGradientPTR)* result;
			*(outputPTR--) = result;
			--imageGradientPTR;
		}
	}
}

//  vertical anticausal filter
void RetinaColor::_adaptiveVerticalCausalFilter(double *outputFrame, unsigned int IDcolumnStart, unsigned int IDcolumnEnd)
{
	for (unsigned int IDcolumn=IDcolumnStart; IDcolumn<IDcolumnEnd; ++IDcolumn)
	{
		register double result=0;
		register double *outputPTR=outputFrame+IDcolumn;
		register double *imageGradientPTR= &_imageGradient[0]+IDcolumn;

		for (unsigned int index=0; index<_filterOutput.getNBrows(); ++index)
		{
			result = *(outputPTR) + (*(imageGradientPTR+_filterOutput.getNBpixels())) * result;
			*(outputPTR) = result;
			outputPTR+=_filterOutput.getNBcolumns();
			imageGradientPTR+=_filterOutput.getNBcolumns();

		}
	}
}

//  vertical anticausal filter which multiplies the output by _gain
void RetinaColor::_adaptiveVerticalAnticausalFilter_multGain(double *outputFrame, unsigned int IDcolumnStart, unsigned int IDcolumnEnd)
{
	double* outputOffset=outputFrame+_filterOutput.getNBpixels()-_filterOutput.getNBcolumns();
	double* gradOffset= &_imageGradient[0]+_filterOutput.getNBpixels()-_filterOutput.getNBcolumns();

	for (unsigned int IDcolumn=IDcolumnStart; IDcolumn<IDcolumnEnd; ++IDcolumn)
	{
		register double result=0;
		register double *outputPTR=outputOffset+IDcolumn;
		register double *imageGradientPTR=gradOffset+IDcolumn;
		for (unsigned int index=0; index<_filterOutput.getNBrows(); ++index)
		{
			result = *(outputPTR) + (*(imageGradientPTR+_filterOutput.getNBpixels())) * result;
			*(outputPTR) = _gain*result;
			outputPTR-=_filterOutput.getNBcolumns();
			imageGradientPTR-=_filterOutput.getNBcolumns();
		}
	}
}

///////////////////////////
void RetinaColor::_computeGradient(const double *luminance)
{
	for (unsigned int idLine=2;idLine<_filterOutput.getNBrows()-2;++idLine)
	{
		for (unsigned int idColumn=2;idColumn<_filterOutput.getNBcolumns()-2;++idColumn)
		{
			const unsigned int pixelIndex=idColumn+_filterOutput.getNBcolumns()*idLine;

			// horizontal and vertical local gradients
			const double verticalGrad=fabs(luminance[pixelIndex+_filterOutput.getNBcolumns()]-luminance[pixelIndex-_filterOutput.getNBcolumns()]);
			const double horizontalGrad=fabs(luminance[pixelIndex+1]-luminance[pixelIndex-1]);

			// neighborhood horizontal and vertical gradients
			const double verticalGrad_p=fabs(luminance[pixelIndex]-luminance[pixelIndex-2*_filterOutput.getNBcolumns()]);
			const double horizontalGrad_p=fabs(luminance[pixelIndex]-luminance[pixelIndex-2]);
			const double verticalGrad_n=fabs(luminance[pixelIndex+2*_filterOutput.getNBcolumns()]-luminance[pixelIndex]);
			const double horizontalGrad_n=fabs(luminance[pixelIndex+2]-luminance[pixelIndex]);

			const double horizontalGradient=0.5*horizontalGrad+0.25*(horizontalGrad_p+horizontalGrad_n);
			const double verticalGradient=0.5*verticalGrad+0.25*(verticalGrad_p+verticalGrad_n);

			// compare local gradient means and fill the appropriate filtering coefficient value that will be used in adaptative filters
			if (horizontalGradient<verticalGradient)
			{
				_imageGradient[pixelIndex+_filterOutput.getNBpixels()]=0.06;
				_imageGradient[pixelIndex]=0.57;
			}
			else
			{
				_imageGradient[pixelIndex+_filterOutput.getNBpixels()]=0.57;
				_imageGradient[pixelIndex]=0.06;
			}
		}
	}

}
const bool RetinaColor::applyKrauskopfLMS2Acr1cr2Transform(std::valarray<double> &result)
{
	bool processSuccess=true;
	// basic preliminary error check
	if (result.size()!=_demultiplexedColorFrame.size())
	{
		std::cerr<<"RetinaColor::applyKrauskopfLMS2Acr1cr2Transform: input buffer does not match retina buffer size, conversion aborted"<<std::endl;
		return false;
	}

	// apply transformation
	_applyImageColorSpaceConversion(_demultiplexedColorFrame, result, _LMStoACr1Cr2);

	return processSuccess;
}

const bool RetinaColor::applyLMS2LabTransform(std::valarray<double> &result)
{
	bool processSuccess=true;
	// basic preliminary error check
	if (result.size()!=_demultiplexedColorFrame.size())
	{
		std::cerr<<"RetinaColor::applyKrauskopfLMS2Acr1cr2Transform: input buffer does not match retina buffer size, conversion aborted"<<std::endl;
		return false;
	}

	// apply transformation
	_applyImageColorSpaceConversion(_demultiplexedColorFrame, result, _LMStoLab);

	return processSuccess;
}

// template function able to perform a custom color space transformation
void RetinaColor::_applyImageColorSpaceConversion(const std::valarray<double> &inputFrameBuffer, std::valarray<double> &outputFrameBuffer, const double *transformTable)
{
	// two step methods in order to allow inputFrame and outputFrame to be the same
	unsigned int nbPixels=(unsigned int)(inputFrameBuffer.size()/3), dbpixels=(unsigned int)(2*inputFrameBuffer.size()/3);

	const double *inputFrame=get_data(inputFrameBuffer);
	double *outputFrame= &outputFrameBuffer[0];

	for (unsigned int dataIndex=0; dataIndex<nbPixels;++dataIndex, ++outputFrame, ++inputFrame)
	{
		// first step, compute each new values
		double layer1 = *(inputFrame)**(transformTable+0)  +*(inputFrame+nbPixels)**(transformTable+1)  +*(inputFrame+dbpixels)**(transformTable+2);
		double layer2 = *(inputFrame)**(transformTable+3)  +*(inputFrame+nbPixels)**(transformTable+4)  +*(inputFrame+dbpixels)**(transformTable+5);
		double layer3 = *(inputFrame)**(transformTable+6)  +*(inputFrame+nbPixels)**(transformTable+7)  +*(inputFrame+dbpixels)**(transformTable+8);
		// second, affect the output
		*(outputFrame)          = layer1;
		*(outputFrame+nbPixels) = layer2;
		*(outputFrame+dbpixels) = layer3;
	}
}

}
