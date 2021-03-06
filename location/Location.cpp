//
// Created by yangcheng on 2019/1/14.
//

#include <cmath>
#include "../sensor/GPS.h"
#include "../sensor/Accelerometer.h"
#include "../models/AHRS.h"
#include "../models/StrapdownAHRS.h"
#include "Location.h"
#include "../math/LPF.h"
#include "iostream"

using namespace Eigen;
using namespace routing;
//using namespace std;

/**
 * Location 初始化。
 */
Location::Location() {
    // 初始化参数
    this->status.Init();
    LPF lpf;
    lpf.LowPassFilter2ndFactorCal(&status);
}

Location::~Location() {}

/**
 * 定位,计算当前位置
 *
 * @param gyro_data, 陀螺仪原始数据, w(x,y,z)
 * @param acc_data, 加速计原始数据, a(x,y,z)
 * @param mag_data, 地磁计原始数据, m(x,y,z)
 * @param gps_data, GPS原始数据, gps(lng,lat,alt,accuracy,speed,bearing,t)
 * @param g_data, 重力感应数据, g(x,y,z)
 * @param ornt_data, 方向传感器数据, o(roll,pitch,yaw)
 * @param status, 状态容器, 包含位置,姿态,速度,参数等信息
 */
void Location::PredictCurrentPosition(Vector3d &gyro_data, Vector3d &acc_data, Vector3d &mag_data, VectorXd &gps_data,
                                      Vector3d &g_data, Vector3d &ornt_data) {

    // 传感器参数
    Parameters parameters = status.parameters;
//    // 利用标定参数较正当前传感器数据
//    Vector3d gyro_data_cali;
//    Vector3d acc_data_cali;
//    Vector3d mag_data_cali;
//    Vector3d g_data_format;
//    // 较正陀螺仪, 单位rad
//    gyro_data_cali(0) = gyro_data(0) - parameters.gyro_coef(0);
//    gyro_data_cali(1) = gyro_data(1) - parameters.gyro_coef(1);
//    gyro_data_cali(2) = gyro_data(2) - parameters.gyro_coef(2);
//    // 较正加速计, 单位为g
//    acc_data_cali(0) = (acc_data(0) / status.parameters.g - parameters.acc_coef(0)) * parameters.acc_coef(3);
//    acc_data_cali(1) = (acc_data(1) / status.parameters.g - parameters.acc_coef(1)) * parameters.acc_coef(4);
//    acc_data_cali(2) = (acc_data(2) / status.parameters.g - parameters.acc_coef(2)) * parameters.acc_coef(5);
//    // 重力数据归一
//    g_data_format = g_data / status.parameters.g;
//    // 较正地磁计, 单位μt
//    mag_data_cali(0) = (mag_data(0) / 1000.0 - parameters.mag_coef(0)) * parameters.mag_coef(3);
//    mag_data_cali(1) = (mag_data(1) / 1000.0 - parameters.mag_coef(1)) * parameters.mag_coef(4);
//    mag_data_cali(2) = (mag_data(2) / 1000.0 - parameters.mag_coef(2)) * parameters.mag_coef(5);
////
////    Vector3d g(0, 0, 1.0);
//
//    // 获取姿态
//    AHRS ahrs;
//    Vector4d q_attitude;
//    if(this->status.parameters.ins_count == 0) {
//        Quaternions quaternions;
//        q_attitude = quaternions.GetQFromEuler(ornt_data);
//    } else {
//        q_attitude = status.attitude.q_attitude;
//    }
//    Vector3d *err = &status.parameters.err;
//    double ki = status.parameters.ki;
//    double kp = status.parameters.kp;
//    double halfT = status.parameters.halfT;
////    StrapdownAHRS strapdownAHRS;
////    Vector4d attitude = strapdownAHRS.StrapdownUpdateAttitude(q_attitude, gyro_data, &status);
//    Vector4d attitude = ahrs.UpdateAttitude(err, q_attitude, gyro_data_cali, g_data, mag_data_cali, ki, kp, halfT);
//    // 更新姿态
//    status.attitude.q_attitude = attitude;

    // 加速计从b系转到n系
//    Quaternions quaternions;
//    attitude = quaternions.GetQFromEuler(ornt_data);
//    Matrix3d newRotated_b2n = quaternions.GetDCMFromQ(attitude);
//    Vector3d acc_b = acc_data_cali - g_data_format;
//    Vector3d final_acc = newRotated_b2n * acc_b * status.parameters.g;
//    Vector3d final_acc_lpf = lpf.LowPassFilter2nd(&status,final_acc);
//    std::cout << "final acc " << final_acc.transpose() << std::endl;
//    std::cout << "final acclpf " << final_acc_lpf.transpose() << std::endl;

    // 记录起始位置和当前位置
    double start_x = status.position.x;
    double start_y = status.position.y;
    double start_lng = status.position.lng;
    double start_lat = status.position.lat;

    // 更新惯性位置,速度
    Accelerometer accelerometer;
//    accelerometer.PositionIntegral(&status, final_acc_lpf, status.parameters.t);
    Quaternions quaternions;
    Vector4d attitude = quaternions.GetQFromEuler(ornt_data);
    accelerometer.StrapdownUpdateVelocityPosition(&status, acc_data, attitude, g_data);

    // 获取GPS精度
    GPS gps;
    // 计算传感器运动距离
    double end_x = status.position.x;
    double end_y = status.position.y;
    double distance = sqrt((end_x - start_x) * (end_x - start_x) + (end_y - start_y) * (end_y - start_y));

    // 判断是否采用GPS数据
    bool is_gps_valid = gps.IsGPSValid(&status, &gps_data);
    if (!is_gps_valid) {
        // 采用惯导更新经纬度
        // 获取航向角
        double heading = ornt_data(2) + status.parameters.diff_gps_ornt;
        status.attitude.yaw = heading;
        // 计算航向角
        Vector2d gps_new = gps.CalDestination(start_lng, start_lat, distance, heading);
        // 更新经纬度
        status.position.lng = gps_new(0);
        status.position.lat = gps_new(1);
        // 更新相关参数
        status.parameters.ins_count += 1;
        status.parameters.ins_dist += distance;
    } else {
        // 采用GPS数据更新经纬度和方位角
        double gps_speed = gps_data(4);
        double gps_bearing = gps_data(5);
        status.position.lng = gps_data(0);
        status.position.lat = gps_data(1);
        status.position.altitude = gps_data(2);
        status.attitude.yaw = gps_bearing;
        gps.UpdateVelocity(&status, gps_speed, gps_bearing);
        // 更新相关参数值,gps(lng,lat,alt,accuracy,speed,bearing,t)
        status.parameters.gps_pre_lng = gps_data(0);
        status.parameters.gps_pre_lat = gps_data(1);
        status.parameters.gps_pre_altitude = gps_data(2);
        status.parameters.gps_pre_accuracy = gps_data(3);
        status.parameters.gps_pre_speed = gps_speed;
        status.parameters.gps_pre_bearing = gps_bearing;
        status.parameters.gps_pre_t = gps_data(6);
        // 时间t影响因子自调整
        AutoAdjustTFactor(&status, gps_data, status.parameters.ins_dist);
        // 更新GPS方向和方向传感器Z轴方向
        UpdateZaxisWithGPS(&status, gps_data, ornt_data);
        std::cout << status.parameters.diff_gps_ornt << std::endl;
        // 更新其他INS变量
        status.parameters.gps_count += 1;
        status.parameters.ins_count = 0;
        status.parameters.ins_dist = 0;
        // 更新融合定位结果输出
        status.gnssins.accuracy = gps_data(3);
        status.gnssins.speed = gps_speed;
        // 每个x,y,z都是相对与上一个准确的GPS数据。
        status.position.x = 0.0;
        status.position.y = 0.0;
        status.position.z = 0.0;
    }

    // 更新融合定位的结果，精度沿用GPS信号好时的精度,速度由于加速计计算的是三个方位的速度，故速度还是沿用GPS的速度
    status.gnssins.lng = status.position.lng;
    status.gnssins.lat = status.position.lat;
    status.gnssins.altitude = status.position.altitude;
    status.gnssins.bearing = status.attitude.yaw;

//    cout << distance << " " << status.parameters.t << " " << status.velocity.v_x << " " << status.velocity.v_y << endl;
}

void Location::SetHz(double f) {
    this->status.parameters.Hz = f;
    this->status.parameters.acc_hz = f / 2.0;
    this->status.parameters.halfT = 1.0 / (f * 2.0);
    this->status.parameters.t = 1.0 / (f * (this->status.parameters.static_t_factor));
}


/**
 * 获取当前融合定位结果作为输出
 * @return
 */
GNSSINS Location::GetGNSSINS() {
    return this->status.gnssins;
}


/**
 * 获取当前位置
 * @return
 */
Position Location::GetCurrentPosition() {
    return this->status.position;
}

/**
 * 获取当前方位角
 * @return
 */
double Location::GetCurrentBearing() {
    return this->status.attitude.yaw;
}

/**
 * 自调节机制, 利用GPS运动距离调整t的放大因子
 *
 * @param status
 * @param gps_data, gps(lng,lat,alt,accuracy,speed,bearing,t)
 * @param ins_distance
 */
void Location::AutoAdjustTFactor(routing::Status *status, Eigen::VectorXd &gps_data, double ins_distance) {

    static int cnt = 0;
    static MatrixXd gps_queue(3, 7);
    static Vector3d ins_move_dist(3);

    if (cnt < 3) {
        gps_queue.row(cnt) = gps_data;
        ins_move_dist(cnt) = ins_distance;
        cnt += 1;
    } else {
        GPS gps;
        double lng1 = gps_queue(0, 0);
        double lat1 = gps_queue(0, 1);
        double lng2 = gps_queue(1, 0);
        double lat2 = gps_queue(1, 1);
        double lng3 = gps_queue(2, 0);
        double lat3 = gps_queue(2, 1);
        double gps_dist = gps.CalDistance(lng1, lat1, lng2, lat2) + gps.CalDistance(lng2, lat2, lng3, lat3);
        double ins_dist = ins_move_dist(1) + ins_move_dist(2);
        double deltaT = (gps_queue(2, 6) - gps_queue(0, 6)) / 1000.0;
        if (gps_dist != 0.0 && ins_dist != 0.0 && deltaT != 0.0) {
            (*status).parameters.move_t_factor *= sqrt(gps_dist / ins_dist);
        }
//                    cout << gps_dist << " ins_dist= " << ins_dist << " t= " << (*status).parameters.t << " deltaT= " << deltaT << " " << (*status).velocity.v_x << " " << (*status).velocity.v_y  << " speed= " << gps_data(4) << " newt= " << (*status).parameters.move_t_factor << endl;
        // 先进先出
        gps_queue.row(0) = gps_queue.row(1);
        gps_queue.row(1) = gps_queue.row(2);
        gps_queue.row(2) = gps_data;
        ins_move_dist(0) = ins_move_dist(1);
        ins_move_dist(1) = ins_move_dist(2);
        ins_move_dist(2) = ins_distance;
//        cnt -= 1;
    }
}

/**
 * 方向传感器和GPS方向差值修正
 *
 * @param status
 * @param gps_data, gps(lng,lat,alt,accuracy,speed,bearing,t)
 * @param ornt_data
 */
void Location::UpdateZaxisWithGPS(routing::Status *status, Eigen::VectorXd &gps_data, Eigen::Vector3d &ornt_data) {
    static MatrixXd gps_queue((*status).parameters.queue_gps_ornt, 7);
    static MatrixXd ornt_queue((*status).parameters.queue_gps_ornt, 3);
    static int cnt = 0;

    if (cnt < (*status).parameters.queue_gps_ornt) {
        if(gps_data(4) > (*status).parameters.gps_static_speed_threshold){
            gps_queue.row(cnt) = gps_data;
            ornt_queue.row(cnt) = ornt_data;
            cnt += 1;
        }
    } else {
        VectorXd gps_bearing = gps_queue.col(5);
        VectorXd ornt_bearing = ornt_queue.col(2);
        double diff_gps_ornt = (gps_bearing - ornt_bearing).mean();
        (*status).parameters.diff_gps_ornt = diff_gps_ornt;
//        std::cout << "gps_data " << gps_data.transpose() << std::endl;
//        std::cout << "gps_bearing " << gps_bearing.transpose() << std::endl;
//        std::cout << "ornt_data " << ornt_data.transpose() << std::endl;
//        std::cout << "ornt_bearing " << ornt_bearing.transpose() << std::endl;
//        std::cout << "(gps_bearing - ornt_bearing) " << (gps_bearing - ornt_bearing).transpose() << std::endl;
//        std::cout << "---------" << std::endl;
        if (gps_data(4) > (*status).parameters.gps_static_speed_threshold) {
            for (int i = 0; i < (*status).parameters.queue_gps_ornt - 1; i++) {
                gps_queue.row(i) = gps_queue.row(i + 1);
                ornt_queue.row(i) = ornt_queue.row(i + 1);
            }
            gps_queue.row((*status).parameters.queue_gps_ornt - 1) = gps_data;
            ornt_queue.row((*status).parameters.queue_gps_ornt - 1) = ornt_data;
        }
    }
}