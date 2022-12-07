//
// Created by hoyin on 2022/2/10.
//

#include <opencv2/imgproc.hpp>
#include "MVCamera.h"
#include "pthread.h"

using namespace mvc;

MVCamera* MVCamera::curCamera;

void printOptionFail(const char* option, int nRet) {
	printf("%s fail! nRet [%x]\n", option, nRet);
}

void PressEnterToExit(void)
{
	int c;
	while ( (c = getchar()) != '\n' && c != EOF );
	fprintf( stderr, "\nPress enter to exit.\n");
	while( getchar() != '\n');
}

bool MVCamera::PrintDeviceInfo(MV_CC_DEVICE_INFO *pstMVDevInfo) {
	if (nullptr == pstMVDevInfo)
	{
		printf("The Pointer of pstMVDevInfo is NULL!\n");
		return false;
	}
	if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
	{
		int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
		int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
		int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
		int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

		// ch:打印当前相机ip和用户自定义名字 | en:print current ip and user defined name
		printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
		printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
		printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
	}
	else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
	{
		printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
		printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
	}
	else
	{
		printf("Not support.\n");
	}

	return true;
}

MVCamera::MVCamera() {

}


MVCamera::MVCamera(unsigned int cameraDeviceIndex) : deviceIndex(cameraDeviceIndex) {
	memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

//	枚举设备
	nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
	if (MV_OK != nRet) {
		printOptionFail("MV_CC_EnumDevices", nRet);
		return;
	}

	if (stDeviceList.nDeviceNum > 0) {
		for (int i = 0; i < stDeviceList.nDeviceNum; ++i) {
			printf("[device %d]:\n", i);
			MV_CC_DEVICE_INFO *pDeviceInfo = stDeviceList.pDeviceInfo[i];
			if (nullptr == pDeviceInfo) {
				return;
			}
			PrintDeviceInfo(pDeviceInfo);
		}
	} else {
		printf("Find No Devices!\n");
	}

//	选择设备并创建句柄
	nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[deviceIndex]);
	if (MV_OK != nRet) {
		printOptionFail("MV_CC_CreateHandle", nRet);
		return;
	}
}

int MVCamera::openCamera() {
	if (MV_OK != nRet) {
		printf("MVCamera device detect failed\n");
		return nRet;
	}

//	打开设备
	nRet = MV_CC_OpenDevice(handle);
	if (MV_OK != nRet) {
		printOptionFail("MV_CC_OpenDevice", nRet);
		return nRet;
	}

	// ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
	if (stDeviceList.pDeviceInfo[deviceIndex]->nTLayerType == MV_GIGE_DEVICE)
	{
		int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
		if (nPacketSize > 0)
		{
			nRet = set(GEV_SCPS_PACKET_SIZE, nPacketSize);
			if(nRet != MV_OK)
			{
				printf("Warning: Set Packet Size fail nRet [0x%x]!\n", nRet);
			}
		}
		else
		{
			printf("Warning: Get Packet Size fail nRet [0x%x]!\n", nPacketSize);
		}
	}

	// ch:获取数据包大小 | en:Get payload size
	memset(&stParam, 0, sizeof(MVCC_INTVALUE));
	nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
	if (MV_OK != nRet)
	{
		printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
		return nRet;
	}

	return nRet;
}

int MVCamera::startGrabbing() {
//	触发模式为off
	nRet = set(CAP_PROP_TRIGGER_MODE, 0);
	if (MV_OK != nRet) return nRet;
//	注册抓图回调
	setCurCamera();
//	nRet = MV_CC_RegisterImageCallBackEx(handle, ImageCallBackEx, handle);
	nRet = MV_CC_RegisterImageCallBackForBGR(handle, ImageCallBackExForBGR8, handle);
	if (MV_OK != nRet) {
		printf("MV_CC_RegisterImageCallBackEx fail! nRet [%x]\n", nRet);
	}

//	开始取流
	nRet = MV_CC_StartGrabbing(handle);
	if (MV_OK != nRet)
	{
		printf("MV_CC_StartGrabbing fail! nRet [%x]\n", nRet);
	}

	MV_FRAME_OUT_INFO_EX stImageInfo = {0};
	memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
	pData = (unsigned char *)malloc(sizeof(unsigned char) * stParam.nCurValue);
	if (nullptr == pData)
	{
		printf("initialize pData fail!");
	}
	return nRet;
}

int MVCamera::stopGrabbing() {
	//	停止取流
	nRet = MV_CC_StopGrabbing(handle);
	if (MV_OK != nRet) {
		printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
	}
	return nRet;
}

int MVCamera::closeCamera() {
	// 关闭设备
	nRet = MV_CC_CloseDevice(handle);
	if (MV_OK != nRet)
	{
		printf("MV_CC_CloseDevice fail! nRet [%x]\n", nRet);
		return nRet;
	}

// 销毁句柄
	nRet = MV_CC_DestroyHandle(handle);
	if (MV_OK != nRet)
	{
		printf("MV_CC_DestroyHandle fail! nRet [%x]\n", nRet);
	}

	return nRet;
}

int MVCamera::set(CameraProperties type, float value) throw(std::invalid_argument)
{
	switch (type)
	{
//		floatValue
		case CAP_PROP_FRAMERATE:
		{
			//********** frame **********/

			nRet = MV_CC_SetFloatValue(handle, "AcquisitionFrameRate", value);

			if (MV_OK == nRet)
			{
				printf("set AcquisitionFrameRate OK! value=%f\n",value);
			}
			else
			{
				printf("Set AcquisitionFrameRate Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_EXPOSURE_TIME:
		{
			//********** frame **********/

			nRet = MV_CC_SetFloatValue(handle, "ExposureTime", value); //曝光时间

			if (MV_OK == nRet)
			{
				printf("set ExposureTime OK! value=%f\n",value);
			}
			else
			{
				printf("Set ExposureTime Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_GAMMA:
		{
			//********** frame **********/

			nRet = MV_CC_SetFloatValue(handle, "Gamma", value); //伽马越小 亮度越大

			if (MV_OK == nRet)
			{
				printf("set Gamma OK! value=%f\n",value);
			}
			else
			{
				printf("Set Gamma Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}

		default:
			printf("Set property fail. Unexpected property or incorrect value type(float).\n");
			throw std::invalid_argument("Unexpected property or incorrect value type.");
	}
	return nRet;
}

int MVCamera::set(CameraProperties type, int value) throw(std::invalid_argument)
{
	switch (type)
	{
//		integerValue
		case CAP_PROP_BURSTFRAMECOUNT:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "AcquisitionBurstFrameCount", value);

			if (MV_OK == nRet)
			{
				printf("set AcquisitionBurstFrameCount OK!\n");
			}
			else
			{
				printf("Set AcquisitionBurstFrameCount Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_HEIGHT:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "Height", value); //图像高度

			if (MV_OK == nRet)
			{
				printf("set Height OK!\n");
			}
			else
			{
				printf("Set Height Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_WIDTH:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "Width", value); //图像宽度

			if (MV_OK == nRet)
			{
				printf("set Width OK!\n");
			}
			else
			{
				printf("Set Width Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_OFFSETX:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "OffsetX", value); //图像宽度

			if (MV_OK == nRet)
			{
				printf("set Offset X OK!\n");
			}
			else
			{
				printf("Set Offset X Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_OFFSETY:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "OffsetY", value); //图像宽度

			if (MV_OK == nRet)
			{
				printf("set Offset Y OK!\n");
			}
			else
			{
				printf("Set Offset Y Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_GAINAUTO:
		{
			//********** frame **********/

			nRet = MV_CC_SetEnumValue(handle, "GainAuto", value); //亮度 越大越亮

			if (MV_OK == nRet)
			{
				printf("set GainAuto OK! value=%d\n",value);
			}
			else
			{
				printf("Set GainAuto Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_SATURATION:
		{
			//********** frame **********/

			nRet = MV_CC_SetIntValue(handle, "Saturation", value); //饱和度 默认128 最大255

			if (MV_OK == nRet)
			{
				printf("set Saturation OK! value=%d\n",value);
			}
			else
			{
				printf("Set Saturation Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_TRIGGER_MODE:
		{

			nRet = MV_CC_SetEnumValue(handle, "TriggerMode", value); // 触发模式

			if (MV_OK == nRet)
			{
				printf("set TriggerMode OK!\n");
			}
			else
			{
				printf("Set TriggerMode Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_TRIGGER_SOURCE:
		{

			nRet = MV_CC_SetEnumValue(handle, "TriggerSource", value);

			if (MV_OK == nRet)
			{
				printf("set TriggerSource OK!\n");
			}
			else
			{
				printf("Set TriggerSource Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_LINE_SELECTOR:
		{

			nRet = MV_CC_SetEnumValue(handle, "LineSelector", value);

			if (MV_OK == nRet)
			{
				printf("set LineSelector OK!\n");
			}
			else
			{
				printf("Set LineSelector Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case GEV_SCPS_PACKET_SIZE:
			nRet = MV_CC_SetIntValue(handle, "GevSCPSPacketSize", value);
			if (MV_OK == nRet) {
				printf("set GevSCPSPacketSize OK!\n");
			} else {
				printf("Set GevSCPSPacketSize Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		default:
			printf("Set property fail. Unexpected property or incorrect value type(int).\n");
			throw std::invalid_argument("Unexpected property or incorrect value type.");
			return 0;
	}
	return nRet;
}

int MVCamera::set(CameraProperties type, bool value) throw(std::invalid_argument)
{
	switch (type)
	{
//		booleanValue
		case CAP_PROP_FRAMERATE_ENABLE:
		{
			//********** frame **********/

			nRet = MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", value);

			if (MV_OK == nRet)
			{
				printf("set AcquisitionFrameRateEnable OK! value=%d\n",value);
			}
			else
			{
				printf("Set AcquisitionFrameRateEnable Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_GAMMA_ENABLE:
		{
			//********** frame **********/

			nRet = MV_CC_SetBoolValue(handle, "GammaEnable", value); //伽马因子是否可调  默认不可调（false）

			if (MV_OK == nRet)
			{
				printf("set GammaEnable OK! value=%d\n",value);
			}
			else
			{
				printf("Set GammaEnable Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}
		case CAP_PROP_SATURATION_ENABLE:
		{
			//********** frame **********/

			nRet = MV_CC_SetBoolValue(handle, "SaturationEnable", value); //饱和度是否可调 默认不可调(false)

			if (MV_OK == nRet)
			{
				printf("set SaturationEnable OK! value=%d\n",value);
			}
			else
			{
				printf("Set SaturationEnable Failed! nRet = [%x]\n\n", nRet);
			}
			break;
		}

		default:
			printf("Set property fail. Unexpected property or incorrect value type(bool).\n");
			throw std::invalid_argument("Unexpected property or incorrect value type.");
			return 0;
	}
	return nRet;
}

void MVCamera::ImageCallBackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser) {
	unsigned char *pDataForRGB = (unsigned char *) malloc(pFrameInfo->nWidth * pFrameInfo->nHeight * 4 + 2048);
	if (nullptr == pDataForRGB) {
		printf("initialize pDataForRGB fail!");
	}
	int nRet = MV_OK;
	MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
	stConvertParam.nWidth = pFrameInfo->nWidth;
	stConvertParam.nHeight = pFrameInfo->nHeight;
	stConvertParam.pSrcData = pData;
	stConvertParam.nSrcDataLen = pFrameInfo->nFrameLen;
	stConvertParam.enSrcPixelType = pFrameInfo->enPixelType;
	stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
	stConvertParam.pDstBuffer = pDataForRGB;
	stConvertParam.nDstBufferSize = pFrameInfo->nWidth * pFrameInfo->nHeight * 4 + 2048;
	nRet = MV_CC_ConvertPixelType(pUser, &stConvertParam);
	MV_CC_ConvertPixelType(pUser, &stConvertParam);
	if (MV_OK != nRet)
	{
		printf("MV_CC_ConvertPixelType fail! nRet [%x]\n", nRet);
	}
	curCamera->curFrame = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pDataForRGB).clone();
}

void MVCamera::ImageCallBackExForBGR8(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser) {
	curCamera->curFrame = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pData);
}

bool MVCamera::readFrame(cv::Mat& frame) {
	if (curFrame.empty()) {
		return false;
	}
	try {
		cv::resize(curFrame, frame, cv::Size(outputWidth, outputHeight));
	} catch (cv::Exception) {
		std::cout << "in MVCamera -> readFrame() -> resize()" << std::endl;
		return false;
	}
	return true;
}

void MVCamera::setCurCamera() {
	curCamera = this;
}

void MVCamera::setOutputSize(int width, int height) {
	this->outputWidth = width;
	this->outputHeight = height;
}

bool MVCamera::isOK() {
	return nRet == MV_OK;
}


