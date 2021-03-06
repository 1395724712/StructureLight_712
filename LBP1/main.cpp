#include <opencv.hpp>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>

using namespace std;
using namespace cv;
//水平条纹宏定义为 0，竖直条纹宏定义为 1， STRIPE_NUM 为条纹数量
#define HORIZONTAL 0
#define VERTICAL 1
#define STRIPE_NUM 26
//一个二维点坐标的结构体，本来是考虑使用opencv 自带的 Point，但是有一个 map_EqualPhasePoint.insert({ Point(col, row), EqualPhasePoint }); 老是报错，索性自己定义了一个结构体
struct Point2t
{
    Point2t(int x_, int y_)
    {
        x = x_;
        y = y_;
    }
    Point2t(const Point2t& p2t)
    {
        this->x = p2t.x;
        this->y = p2t.y;
    }
    bool operator <(const Point2t& p2t) const
    {
        if (this->x == p2t.x)
        {
            return this->y < p2t.y;
        }
        return this->x < p2t.x;
    }
    int x;
    int y;
};
 //这块是参考@卢平悦 的计算包裹相位代码，返回值是一个[-PI, PI]范围的相位值
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
//这块是对左右相机求包裹相位的一个包装代码，省去了每次需要分别考虑左右相机的烦恼，直接传左右相机相片路径就可以了
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
/*获取双目照片与投影仪相位相等的点的坐标
*@input:左右两张条纹图，投影仪投射的条纹图，遍历一行（极线约束的这一行）找相位相等的点
*@output：//map<投影仪二维像素点（u, v）,极线约束上的左右照片像素3维点（x, y, z）>
*/

map<Point2t, vector<vector<Point3f>>> getEqualPhasePoint(Mat& leftPhaseImage, Mat& rightPhaseImage, Mat& projectPhaseImage, int stripe)
{
    //对于投影仪上任意一个像素 P ，求出它的相位，通过 【(x*2*PI*条纹数)/width + 初相 】得到
    //由于现在没有双目照片，就先假定吧
    vector<Point3f> leftEqualPhasePoint;
    vector<Point3f> rightEqualPhasePoint;
    vector<vector<Point3f>> EqualPhasePoint;

    float f = 0.035;//焦距 focal
    float T = 0.36;//投影仪至相机光心距离
    
    map<Point2t, vector<vector<Point3f>>> map_EqualPhasePoint;
    //水平条纹 每一行的相位都一样 待完善
    if (stripe == HORIZONTAL)
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
                for (int col_cameraPhaseImage = 0; col_cameraPhaseImage < leftPhaseImage.cols; ++col_cameraPhaseImage)
                {
                    if (leftPhaseImage.at<float>(row, col_cameraPhaseImage) == phaseAtPixel)
                    {                        
                        float Z = (f * T) / (col_cameraPhaseImage - row);
                        leftEqualPhasePoint.push_back(Point3f(col_cameraPhaseImage, row, Z));
                    }
                    if (rightPhaseImage.at<float>(row, col_cameraPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (row - col_cameraPhaseImage);
                        rightEqualPhasePoint.push_back(Point3f(col_cameraPhaseImage, row, Z));
                    }
                }
            }
        }
    }
    //竖直条纹 其每一列的相位都一致
    if (stripe == VERTICAL)
    {
#pragma omp parallel for
        for (int col = 0; col < projectPhaseImage.cols; ++col)
        {
            float phaseAtPixel = (col * 2 * CV_PI * STRIPE_NUM) / projectPhaseImage.cols - (2 * CV_PI) / 3;
            while (phaseAtPixel > CV_PI)//主要是为了防止出现 phaseAtPixel 不在[-PI, PI] 这段区间时进行 K* PI 的一个截取，注意正负
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel - K * CV_PI;
            }
            while (phaseAtPixel < -CV_PI)//主要是为了防止出现 phaseAtPixel 不在[-PI, PI] 这段区间时进行 K* PI 的一个截取，注意正负
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel + K * CV_PI;
            }
            for (int row = 0; row < projectPhaseImage.rows; ++row)//外层循环是每一列，内层循环是投影仪的每一行 找左右图中跟其同行（极线约束）中相位相等的像素
            {

                for (int col_cameraPhaseImage = 0; col_cameraPhaseImage < leftPhaseImage.cols; ++col_cameraPhaseImage)
                {
                    if (leftPhaseImage.at<float>(row, col_cameraPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (col_cameraPhaseImage - col);
                        leftEqualPhasePoint.push_back(Point3f(col_cameraPhaseImage, row, Z));
                    }
                    if (rightPhaseImage.at<float>(row, col_cameraPhaseImage) == phaseAtPixel)
                    {
                        float Z = (f * T) / (col - col_cameraPhaseImage);//
                        if (Z < 0)
                            break;
                        float X = (col_cameraPhaseImage * T) / (col - col_cameraPhaseImage);
                        float Y = (row * T) / (col - col_cameraPhaseImage);
                        rightEqualPhasePoint.push_back(Point3f(X, Y, Z));
                    }//right 为负？(为负的给去掉)数量太多

                }
                //在左或者右中找到了匹配点
                if (!leftEqualPhasePoint.empty() || !rightEqualPhasePoint.empty())
                {
                    EqualPhasePoint.push_back(leftEqualPhasePoint);
                    EqualPhasePoint.push_back(rightEqualPhasePoint);
                    map_EqualPhasePoint.insert({ Point2t(col, row), EqualPhasePoint });
                    leftEqualPhasePoint.clear();
                    rightEqualPhasePoint.clear();
                    EqualPhasePoint.clear();
                }

            }
        }
    }


    return map_EqualPhasePoint;
    
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

    map<Point2t, vector<vector<Point3f>>> EqualPhasePoint = getEqualPhasePoint(leftWrappedimage, rightWrappedimage, projectPhaseImage, VERTICAL);
   

    return 0;

}
