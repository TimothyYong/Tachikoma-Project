// Chilitags
#include <chilitags/chilitags.hpp>
#include "chili_landmarks.h"

// OpenCV
// #include <opencv2/core/core.hpp>
// #include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
// General
// #include <cmath>
// #include <cstring>
// #include <fstream>
// #include <iostream>
// #include <stdio.h>
// #include <string>
// #include <time.h>
#include <iostream>
#include <armadillo>
#include "highgui.h"
#include "imgproc.h"
using namespace std;
using namespace cv;
chili_landmarks::chili_landmarks()
{
}

chili_landmarks::~chili_landmarks()
{
}

void chili_landmarks::update(cv::Mat &cameraFrame, std::mutex &camframelock,
	arma::vec & bearpos, double &bearsize, bool &bearfound
	)
{
	// Paramaters for image aquisition
	int xRes = 640;
	int yRes = 480;
	int cameraIndex = 1;

	// Get camera feed
	cv::VideoCapture capture(cameraIndex);
	if (!capture.isOpened())
	{
		perror("Unable to initialise video capture.");
		exit(0);
	}

	// Set camera feed resolution
	capture.set(CV_CAP_PROP_FRAME_WIDTH, xRes);
	capture.set(CV_CAP_PROP_FRAME_HEIGHT, yRes);

	// Initialize variables to be used in loop
	cv::Mat inputImage;
	chilitags::Chilitags chilitags;
	// char tagInfo[256];

	// Window to display tracked chilitags
	//cv::namedWindow("DisplayChilitags");
	static const cv::Scalar COLOR(255, 0, 255);
	static const int SHIFT = 16;
	static const float PRECISION = 1<<SHIFT;





  const int h1 = 0;
  const int h2 = 100;
  const int s1 = 128;
  const int s2 = 255;
  const int v1 = 128;
  const int v2 = 255;
	arma::vec xbuf(10, arma::fill::zeros);
	arma::vec ybuf(10, arma::fill::zeros);
	arma::vec xcbuf(10, arma::fill::zeros);
	arma::vec ycbuf(10, arma::fill::zeros);
	int xind = 0, yind = 0;
	bool stable = false;







	// Main loop, exiting when 'q is pressed'
	for (int framecount = 0; 'q' != (char) cv::waitKey(1); framecount++)
	{
		// Read input image and parse for chilitags
		capture.read(inputImage);

		cv::Mat img = inputImage.clone();











		Mat hsv;
		cvtColor(img, hsv, COLOR_BGR2HSV);

		// in range masking
		Mat hsv2mask, hsv2;
		inRange(hsv, Scalar(h1, s1, v1), Scalar(h2, s2, v2), hsv2mask);
		//hsv.copyTo(hsv2, hsv2mask);

		arma::mat I;
		cvt_opencv2arma(hsv2mask, I);
		
		//blur
		//I = conv2(I, gauss2(7, 16));
		
		arma::rowvec r = arma::sum(I, 0);
		arma::colvec c = arma::sum(I, 1);
		double midx = arma::sum(r * arma::cumsum(arma::ones<arma::vec>(r.n_elem))) / arma::sum(r);
		double midy = arma::sum(c % arma::cumsum(arma::ones<arma::vec>(c.n_elem))) / arma::sum(c);
		stable = true;
		if (midx < 1 || (midx > (int)I.n_cols - 1) || arma::sum(r) == 0) {
			midx = 0;
		}
		if (midy < 1 || (midy > (int)I.n_rows - 1) || arma::sum(c) == 0) {
			midy = 0;
		}

		// remove noise
		if (midx == 0 || midy == 0) {
			stable = false;
		}
		xbuf[xind] = midx;
		ybuf[yind] = midy;
		midx = arma::mean(xbuf);
		midy = arma::mean(ybuf);
		if (sqrt(arma::var(xbuf) * arma::var(xbuf) + arma::var(ybuf) + arma::var(ybuf)) > 70.0) {
			stable = false;
		}
		
		Scalar color(255, 0, 0);
		//cout << "arma:: " << midx << ", " << midy << endl;
		//cout << "stable: " << stable << endl;
		circle(img, Point((int)midx, (int)midy), 4, color, 0);

		double covx = sqrt(arma::sum(r * arma::square(arma::cumsum(arma::ones<arma::vec>(r.n_elem)) - midx)) / arma::sum(r));
		double covy = sqrt(arma::sum(c % arma::square(arma::cumsum(arma::ones<arma::vec>(c.n_elem)) - midy)) / arma::sum(c));
		xcbuf[xind] = covx;
		ycbuf[yind] = covy;
		covx = arma::accu(xcbuf) / (int)xbuf.n_elem;
		covy = arma::accu(ycbuf) / (int)ybuf.n_elem;
		xind = (xind + 1) % (int)xbuf.n_elem;
		yind = (yind + 1) % (int)ybuf.n_elem;
		//double estw = covx * 3;
		//double esth = covy * 3;
		//blur the image to make it more accurate
		/*I = conv2(I, gauss2(7, 16));
		disp_image("convI", I);
		int minw = (int)midx;
		int maxw = (int)midx;
		for (int minw = (int)midx; minw >= 0; minw--) {
		  if (I(midy, minw) < 0.05) {
			break;
		  }
		}
		for (int maxw = (int)midx; maxw < 640; maxw++) {
		  if (I(midy, maxw) < 0.05) {
			break;
		  }
		}
		int minh = (int)midy;
		int maxh = (int)midy;
		for (int minh = (int)midy; minh >= 0; minh--) {
		  if (I(minh, midx) < 0.05) {
			break;
		  }
		}
		for (int maxh = (int)midy; maxh < 480; maxh++) {
		  if (I(maxh, midx) < 0.05) {
			break;
		  }
		}
		double covy = (double)(maxh - minh);
		double covx = (double)(maxw - minw);*/
		covx = (covx) * 3.3;
		covy = (covy) * 3.3;
		bearpos = arma::vec({ midx - 320, 479 - (midy - 240) });
		bearsize = 533 / (covx) * 12;
		bearfound = stable;

		rectangle(img, Rect(midx-covx/2,midy-covy/2,covx,covy), Scalar(0, 0, 255), 2);

		imshow("oldhsv", hsv);
		imshow("newhsv", hsv2mask);
		imshow("img", img);











    // put the image in ipcdb.h
    //camframelock.lock();
    //cameraFrame = inputImage;
    //camframelock.unlock();

		// Start time for processing
		// int64 startTime = cv::getTickCount();

		// Process chilitags
		chilitags::TagCornerMap detected_tags = chilitags.find(inputImage);

		// Measure the processing time needed for the detection
		// int64 endTime = cv::getTickCount();
		// float processingTime = 1000.0f*((float) endTime - startTime)/cv::getTickFrequency();

		// Reset tagInfo string
	    // std::memset(tagInfo, 0, 256);

	    // Clone input image to label chilitags on & display
		cv::Mat outputImage = inputImage.clone();

		for (int i=0; i<1024; i++)
		{
			this->tags[i][0] = 0;
		}

		for (const std::pair<int, chilitags::Quad> & tag : detected_tags)
		{
			// Get ID and corner points of chilitag
			int id = tag.first;
			const cv::Mat_<cv::Point2f> corners(tag.second);

			// Draw outline around detected chilitag
			// for (size_t i = 0; i < 4; ++i)
			// {
				// cv::line(outputImage, PRECISION*corners(i), PRECISION*corners((i+1)%4), COLOR, 1, CV_AA, SHIFT);
			// }

			// Calculate center based on two corner points
			cv::Point2f center = 0.5f*(corners(0) + corners(2));

			// Calculate x & y in local reference frame
			// Tag size = 9 inches (W)
			// Distance = 24 inches (D)
			// Pixel diagonal width = 200 px (P)
			// Focal length = F = (P x D) / W = 533.3333

			double Z = 60.0; // inches to the ceiling

			double delta_x = center.x - (xRes/2);
			double delta_y = center.y - (yRes/2);

			double u1 = corners(0).x - delta_x;
			double u2 = corners(1).x - delta_x;
			double u3 = corners(2).x - delta_x;
			double u4 = corners(3).x - delta_x;

			double v1 = corners(0).y - delta_y;
			double v2 = corners(1).y - delta_y;
			double v3 = corners(2).y - delta_y;
			double v4 = corners(3).y - delta_y;

			double x1 = u1 * Z / 533.333;
			double x2 = u2 * Z / 533.333;
			double x3 = u3 * Z / 533.333;
			double x4 = u4 * Z / 533.333;

			double y1 = v1 * Z / 533.333;
			double y2 = v2 * Z / 533.333;
			double y3 = v3 * Z / 533.333;
			double y4 = v4 * Z / 533.333;

			///////// AFTER THIS IS GOOD //////////

			double x_squared = pow(abs(corners(0).x - corners(2).x), 2);
			double y_squared = pow(abs(corners(0).y - corners(2).y), 2);
			double diagonal_size = sqrt(x_squared + y_squared);

			double distance_inches = 9 * 533.33 / diagonal_size;
			double x_inches = delta_x * distance_inches / 533.33;
			double y_inches = -delta_y * distance_inches / 533.33;
			//double z_inches = sqrt(pow(distance_inches, 2) - (pow(x_inches, 2) + pow(y_inches, 2)));

			this->tags[id][0] = 1;
			this->tags[id][1] = x_inches;
			this->tags[id][2] = y_inches;
			this->tags[id][3] = distance_inches;

			///////// BEFORE THIS IS GOOD //////////

			// printf("id: %d\t", id);
			// printf("x: %0.3f\t\t", tags[id][1]);
			// printf("y: %0.3f\n", tags[id][2]);

			// Write tagInfo to center of tag in displayed window
			// sprintf(tagInfo, "id: %d x:%2.2f y:%2.2f", id, x_inches, y_inches);
			// cv::putText(outputImage, tagInfo, center, cv::FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(0,255,0), 2.0);
		}

		// Write stats on the current frame (resolution and processing time) to display window
        // cv::putText(outputImage, cv::format("%dx%d %4.0f ms (press q to quit)", outputImage.cols, outputImage.rows, processingTime), cv::Point(32,32), cv::FONT_HERSHEY_SIMPLEX, 0.5f, COLOR);

        // cv::circle(outputImage, cv::Point(320, 240), 15, (0, 0, 0));

        // Display chilitags window
        //cv::imshow("DisplayChilitags", outputImage);
	}

	// Release all used resources
	capture.release();
	//cv::destroyWindow("DisplayChilitags");
}

// int main()
// {
//     // std::thread detect(update);
//     // detect.join();
// }