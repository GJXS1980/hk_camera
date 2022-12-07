#include "opencv2/opencv.hpp"
#include "MVCamera.h"
#include "ros/ros.h"
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include <image_transport/image_transport.h>
#include <camera_info_manager/camera_info_manager.h>

#include <sstream>
#include <std_srvs/Empty.h>

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

int main(int argc,char** argv) 
{
	mvc::MVCamera mvCamera(0);
	mvCamera.setOutputSize(FRAME_WIDTH, FRAME_HEIGHT);
	mvCamera.openCamera();
	mvCamera.startGrabbing();
	// 初始化ROS节点
	ros::init(argc, argv,"hgcam_image");
	//	创建ROS句柄
	ros::NodeHandle n("~");

	// shared image message
	sensor_msgs::Image img_;
	image_transport::CameraPublisher image_pub_;

	boost::shared_ptr<camera_info_manager::CameraInfoManager> cinfo_;

	// image_transport::Publisher pub; 
    image_transport::ImageTransport it(n);
    image_pub_ = it.advertiseCamera("hgcam/image_raw", 1);

    // pub = it.advertise("camera/image", 10);
	std::string camera_frame, camera_name_, camera_info_url_;
	int image_width_, image_height_;

    n.param("image_width", image_width_, 640);
    n.param("image_height", image_height_, 480);

    // load the camera info
    n.param("camera_frame_id", camera_frame, std::string("hg_camera_link"));

    n.param("camera_name", camera_name_, std::string("head_camera"));
    n.param("camera_info_url", camera_info_url_, std::string(""));
    cinfo_.reset(new camera_info_manager::CameraInfoManager(n, camera_name_, camera_info_url_));

    // check for default camera info
    if (!cinfo_->isCalibrated())
    {
    //   cinfo_->setCameraName(video_device_name_);
      sensor_msgs::CameraInfo camera_info;
      camera_info.header.frame_id = img_.header.frame_id;
      camera_info.width = image_width_;
      camera_info.height = image_height_;
      cinfo_->setCameraInfo(camera_info);
    }

	cv::Mat frame;

	while (ros::ok()) 
	{
		if (mvCamera.readFrame(frame)) 
		{
			//cv::imshow("frame", frame);
            sensor_msgs::ImagePtr msg;
			sensor_msgs::CameraInfoPtr ci(new sensor_msgs::CameraInfo(cinfo_->getCameraInfo()));

            msg=cv_bridge::CvImage(std_msgs::Header(), "bgr8", frame).toImageMsg();
            msg->header.stamp = ros::Time::now();
			// sensor_msgs::ImagePtr转换成sensor_msgs::Image格式
			img_ = *msg;
			img_.header.frame_id = camera_frame;
			ci->header.frame_id = img_.header.frame_id;
			ci->header.stamp = img_.header.stamp;


			image_pub_.publish(img_, *ci);
		} 
		else 
		{
			//cv::imshow("frame", cv::Mat(100, 100, CV_8UC3));
		}

		//int k = cv::waitKey(1);
		/*if (k == 27) 
		{
			cv::destroyAllWindows();
			mvCamera.stopGrabbing();
			mvCamera.closeCamera();
			break;
		}
*/
	}
	return 0;
}
