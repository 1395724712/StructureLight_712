#include <opencv.hpp>
#include <vector>
using namespace std;
using namespace cv;

#define HORIZONTAL 0
#define VERTICAL 1
#define STRIPE_NUM 26
 
void computeWrappedPhase(Mat* img1, Mat* img2, Mat* img3, Mat* dstWrappedPhase)
{
    typedef float phaseType;
    Mat temp1, temp2;
    temp1.create(img1->rows, img1->cols, CV_32F);
    temp2.create(img2->rows, img2->cols, CV_32F);
    temp1 = sqrt(3) * (*img1 - *img3);
    temp2 = *img2 * 2 - *img1 - *img3;
    int height = img1->rows;
    int width = img1->cols;
#pragma omp parallel for
    for(int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
        {
            auto phase = atan2f(temp1.at<phaseType>(i, j), temp2.at<phaseType>(i, j));
            dstWrappedPhase->at<phaseType>(i, j) = phase;
        }
}
Mat getWrappedPhase(vector<string> &path)
{
    if (path.size() != 3)
    {
        cerr << "path value must have 3 !" << endl;
        exit(-1);
    }        
    Mat img1f, img2f, img3f;
    Mat phase_img1 = imread(path[0], IMREAD_GRAYSCALE);
    Mat phase_img2 = imread(path[1], IMREAD_GRAYSCALE);
    Mat phase_img3 = imread(path[2], IMREAD_GRAYSCALE);
    // 转换为float类型
    phase_img1.convertTo(img1f, CV_32F);
    phase_img2.convertTo(img2f, CV_32F);
    phase_img3.convertTo(img3f, CV_32F);
    Mat dstWrappedPhase;
    Mat dstWrappedPhase8;
    dstWrappedPhase.create(phase_img1.rows, phase_img1.cols, CV_32F);

    // 计算包裹相位
    computeWrappedPhase(&img1f, &img2f, &img3f, &dstWrappedPhase);

    dstWrappedPhase.convertTo(dstWrappedPhase8, CV_8U, 1 / CV_PI * 128, 128);

    return dstWrappedPhase;
}
//获取双目照片与投影仪相位相等的点的坐标
vector<vector<Point3f>> getEqualPhasePoint(Mat& leftPhaseImage, Mat& rightPhaseImage, Mat& projectPhaseImage, int stripe)
{
    //对于投影仪上任意一个像素 P ，求出它的相位，通过 【(x*2*PI*条纹数)/width + 初相 】得到
    //由于现在没有双目照片，就先假定吧
    /*
    *@input:左右两张条纹图，投影仪投射的条纹图，遍历一行（极线约束的这一行）找相位相等的点
    *@output：vector 存储相位相等点的坐标
    */
    vector<Point3f> leftEqualPhasePoint;
    vector<Point3f> rightEqualPhasePoint;
    vector<vector<Point3f>> EqualPhasePoint;

    float f = 0.035;//焦距 focal
    float T = 0.36;//投影仪至相机光心距离

    if (stripe == HORIZONTAL)//水平条纹
    {
        for (int row = 0; row < projectPhaseImage.rows; ++row)
        {
            float phaseAtPixel = (row * 2 * CV_PI * STRIPE_NUM) / projectPhaseImage.cols - (2 * CV_PI) / 3;
            while (phaseAtPixel > CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel - K*CV_PI;
            }
            while (phaseAtPixel < -CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel + K * CV_PI;
            }
            for (int col = 0; col < projectPhaseImage.cols; ++col)
            {
                for (int col_leftPhaseImage = 0; col_leftPhaseImage < leftPhaseImage.cols; ++col_leftPhaseImage)
                {
                    if (leftPhaseImage.at<float>(row, col_leftPhaseImage) == phaseAtPixel)
                    {                        
                        float Z = (f * T) / (col_leftPhaseImage - row);
                        leftEqualPhasePoint.push_back(Point3f(col_leftPhaseImage, row, Z));
                    }
                    if (rightPhaseImage.at<float>(row, col_leftPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (row - col_leftPhaseImage);
                        rightEqualPhasePoint.push_back(Point3f(col_leftPhaseImage, row, Z));
                    }
                }
            }
        }
    }
    if (stripe == VERTICAL)//竖直条纹
    {
#pragma omp parallel for
        for (int col = 0; col < projectPhaseImage.cols; ++col)
        {
            float phaseAtPixel = (col * 2 * CV_PI * STRIPE_NUM) / projectPhaseImage.cols - (2 * CV_PI) / 3;
            while (phaseAtPixel > CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel - K * CV_PI;
            }
            while (phaseAtPixel < -CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel + K * CV_PI;
            }
            for (int row = 0; row < projectPhaseImage.rows; ++row)
            {
                for (int col_leftPhaseImage = 0; col_leftPhaseImage < leftPhaseImage.cols; ++col_leftPhaseImage)
                {
                    if (leftPhaseImage.at<float>(row, col_leftPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (col_leftPhaseImage - row);
                        leftEqualPhasePoint.push_back(Point3f(col_leftPhaseImage, row, Z));
                    }
                    if (rightPhaseImage.at<float>(row, col_leftPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (row - col_leftPhaseImage);
                        rightEqualPhasePoint.push_back(Point3f(col_leftPhaseImage, row, Z));
                    }
                }
            }
        }
    }

    EqualPhasePoint.push_back(leftEqualPhasePoint);
    EqualPhasePoint.push_back(rightEqualPhasePoint);
    return EqualPhasePoint; 
    
}
//对于照相机上极线上的像素点，进行遍历，求出与上面相位相等的候选像素点

//拿到相位相等的点进行三角测量，求出最小距离的点对

int main(int argc, const char *argv[])
{
    Mat projectPhaseImage = imread("./Fringe_vert1.bmp");

    vector<string> leftCameraPath;
    leftCameraPath.push_back("./PhaseShiftleft02.bmp");
    leftCameraPath.push_back("./PhaseShiftleft03.bmp");
    leftCameraPath.push_back("./PhaseShiftleft04.bmp");
    Mat leftWrappedimage = getWrappedPhase(leftCameraPath);

    vector<string> rightCameraPath;
    rightCameraPath.push_back("./PhaseShiftright02.bmp");
    rightCameraPath.push_back("./PhaseShiftright03.bmp");
    rightCameraPath.push_back("./PhaseShiftright04.bmp");
    Mat rightWrappedimage = getWrappedPhase(rightCameraPath);

    vector<vector<Point3f>> EqualPhasePoint = getEqualPhasePoint(leftWrappedimage, rightWrappedimage, projectPhaseImage, VERTICAL);
   

    return 0;

}
