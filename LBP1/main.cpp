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
    // ת��Ϊfloat����
    phase_img1.convertTo(img1f, CV_32F);
    phase_img2.convertTo(img2f, CV_32F);
    phase_img3.convertTo(img3f, CV_32F);
    Mat dstWrappedPhase;
    Mat dstWrappedPhase8;
    dstWrappedPhase.create(phase_img1.rows, phase_img1.cols, CV_32F);

    // ���������λ
    computeWrappedPhase(&img1f, &img2f, &img3f, &dstWrappedPhase);

    dstWrappedPhase.convertTo(dstWrappedPhase8, CV_8U, 1 / CV_PI * 128, 128);

    return dstWrappedPhase;
}
//��ȡ˫Ŀ��Ƭ��ͶӰ����λ��ȵĵ������
vector<vector<Point3f>> getEqualPhasePoint(Mat& leftPhaseImage, Mat& rightPhaseImage, Mat& projectPhaseImage, int stripe)
{
    //����ͶӰ��������һ������ P �����������λ��ͨ�� ��(x*2*PI*������)/width + ���� ���õ�
    //��������û��˫Ŀ��Ƭ�����ȼٶ���
    /*
    *@input:������������ͼ��ͶӰ��Ͷ�������ͼ������һ�У�����Լ������һ�У�����λ��ȵĵ�
    *@output��vector �洢��λ��ȵ������
    */
    vector<Point3f> leftEqualPhasePoint;
    vector<Point3f> rightEqualPhasePoint;
    vector<vector<Point3f>> EqualPhasePoint;

    float f = 0.035;//���� focal
    float T = 0.36;//ͶӰ����������ľ���

    if (stripe == HORIZONTAL)//ˮƽ����
    {
        for (int i = 0; i < projectPhaseImage.rows; ++i)
        {
            float phaseAtPixel = (i * 2 * CV_PI * STRIPE_NUM) / 480 - (2 * CV_PI) / 3;
            while (phaseAtPixel > CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel - K*CV_PI;
            }
            for (int j = 0; j < projectPhaseImage.cols; j++)
            {
                for (int k = 0; k < leftPhaseImage.cols; k++)
                {
                    if (leftPhaseImage.at<float>(i, k) == phaseAtPixel)
                    {                        
                        float Z = (f * T) / (k - i);
                        leftEqualPhasePoint.push_back(Point3f(i, k, Z));
                    }
                    if (rightPhaseImage.at<float>(i, k) == phaseAtPixel)
                    {
                        float Z = (f * T) / (i - k);
                        rightEqualPhasePoint.push_back(Point3f(i, k, Z));
                    }
                }
            }
        }
    }
    if (stripe == VERTICAL)//��ֱ����
    {
        for (int i = 0; i < projectPhaseImage.rows; ++i)
        {
            float phaseAtPixel = (i * 2 * CV_PI * STRIPE_NUM) / 480 - (2 * CV_PI) / 3;
            while (phaseAtPixel > CV_PI)
            {
                int K = phaseAtPixel / CV_PI;
                phaseAtPixel = phaseAtPixel - K * CV_PI;
            }
            for (int j = 0; j < projectPhaseImage.cols; j++)
            {
                for (int k = 0; k < leftPhaseImage.cols; k++)
                {
                    if (leftPhaseImage.at<float>(i, k) == phaseAtPixel)
                    {
                        float Z = (f * T) / (k - i);
                        leftEqualPhasePoint.push_back(Point3f(i, k, Z));
                    }
                    if (rightPhaseImage.at<float>(i, k) == phaseAtPixel)
                    {
                        float Z = (f * T) / (i - k);
                        rightEqualPhasePoint.push_back(Point3f(i, k, Z));
                    }
                }
            }
        }
    }

    EqualPhasePoint.push_back(leftEqualPhasePoint);
    EqualPhasePoint.push_back(rightEqualPhasePoint);
    return EqualPhasePoint; 
    
}
//����������ϼ����ϵ����ص㣬���б����������������λ��ȵĺ�ѡ���ص�

//�õ���λ��ȵĵ�������ǲ����������С����ĵ��

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
