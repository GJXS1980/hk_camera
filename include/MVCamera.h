//
// Created by hoyin on 2022/2/10.
//

#ifndef MVCAMERA_H
#define MVCAMERA_H


#include "iostream"
#include "cstring"
#include "unistd.h"
#include "MvCameraControl.h"
#include "opencv2/core.hpp"


namespace mvc {
	const int MAX_WIDTH = 3072;
	const int MAX_HEIGHT = 2048;

	enum CameraProperties
	{
		CAP_PROP_FRAMERATE_ENABLE,  //帧数可调
		CAP_PROP_FRAMERATE,         //帧数
		CAP_PROP_BURSTFRAMECOUNT,   //外部一次触发帧数
		CAP_PROP_HEIGHT,            //图像高度
		CAP_PROP_WIDTH,             //图像宽度
		CAP_PROP_EXPOSURE_TIME,     //曝光时间
		CAP_PROP_GAMMA_ENABLE,      //伽马因子可调
		CAP_PROP_GAMMA,             //伽马因子
		CAP_PROP_GAINAUTO,          //亮度
		CAP_PROP_SATURATION_ENABLE, //饱和度可调
		CAP_PROP_SATURATION,        //饱和度
		CAP_PROP_OFFSETX,           //X偏置
		CAP_PROP_OFFSETY,           //Y偏置
		CAP_PROP_TRIGGER_MODE,      //外部触发
		CAP_PROP_TRIGGER_SOURCE,    //触发源
		CAP_PROP_LINE_SELECTOR,     //触发线

		GEV_SCPS_PACKET_SIZE,		//网络包大小
	};

	class MVCamera {
	private:
		int nRet = MV_OK;
		unsigned int deviceIndex;
		void *handle = nullptr;
		MV_CC_DEVICE_INFO_LIST stDeviceList;
		MVCC_INTVALUE stParam;				// 数据包大小
		unsigned char *pData;
		cv::Mat curFrame;
		int outputWidth, outputHeight;

//	************ private functions *************
		static bool PrintDeviceInfo(MV_CC_DEVICE_INFO *pstMVDevInfo);

//	************ 抓图回调 ***********************
		static MVCamera *curCamera;
		void setCurCamera();
		static void __stdcall ImageCallBackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser);
		static void __stdcall ImageCallBackExForBGR8(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser);
	public:
		explicit MVCamera(unsigned int cameraDeviceIndex);

		MVCamera();

		int openCamera();
		int startGrabbing();
		int stopGrabbing();
		int closeCamera();
		bool readFrame(cv::Mat& frame);
		int set(CameraProperties type, float value) throw(std::invalid_argument);
		int set(CameraProperties type, int value) throw(std::invalid_argument);
		int set(CameraProperties type, bool value) throw(std::invalid_argument);
		void setOutputSize(int width, int height);
		bool isOK();
	};
}

#endif //MVCAMERA_H
