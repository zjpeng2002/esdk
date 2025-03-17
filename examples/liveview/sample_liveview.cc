/**
 ********************************************************************
 *
 * @copyright (c) 2023 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */
#include "sample_liveview.h"
#include <unistd.h>
#include "liveview/liveview.h"

#define BITRATE_CALCULATE_BITS_PER_BYTE 8
#define BITRATE_CALCULATE_INTERVAL_TIME_MS 2000

using namespace edge_sdk;

namespace edge_app {

LiveviewSample::LiveviewSample(const std::string& name) {
    liveview_ = edge_sdk::CreateLiveview();
    liveview_status_ = 0;
}

ErrorCode LiveviewSample::StreamCallback(const uint8_t* data, size_t len)
 {
    auto now =  std::chrono::system_clock::now();  // 时间点记录
    // 线程状态校验，若存在则调用InputStream方法传递数据
    if (stream_processor_thread_) {
        stream_processor_thread_->InputStream(data, len); // 数据投递
    }

    // 比特率计算
    receive_stream_data_total_size_ += len; // 累加接受的数据总量
    auto duration  = std::chrono::duration_cast<std::chrono::milliseconds>(now - receive_stream_data_time_).count(); // 计算距离上次统计的时间间隔（毫秒）
    // 达到统计间隔时更新比特率
    if (duration >= BITRATE_CALCULATE_INTERVAL_TIME_MS) {
        auto kbps = receive_stream_data_total_size_ * BITRATE_CALCULATE_BITS_PER_BYTE / (BITRATE_CALCULATE_INTERVAL_TIME_MS / 1000) / 1024;     // 计算比特率（Kbps = (总字节数 * 8) / 时间间隔秒数 / 1024）
         // 更新状态变量
        stream_bitrate_kbps_ = kbps;
        receive_stream_data_total_size_  = 0;
        receive_stream_data_time_  = now;
    }
    return kOk;
}

ErrorCode LiveviewSample::Init(Liveview::CameraType type, Liveview::StreamQuality quality,std::shared_ptr<StreamProcessorThread> processor)  
 {
    stream_processor_thread_ = processor; // 将外部线程对象存入成员变量
    auto stream_callback =std::bind
    (&LiveviewSample::StreamCallback,  // 目标函数：流数据到达时的回调
    this,                                                                   // 绑定当前对象实例
    std::placeholders::_1,                               // 占位符：const uint8_t* data
    std::placeholders::_2);                             // 占位符：const uint8_t* data

    // 配置初始化参数
    Liveview::Options option = {type, quality, stream_callback};

    // 调用SDK初始化接口
    auto rc = liveview_->Init(option); 

    // 订阅状态变更
    liveview_->SubscribeLiveviewStatus(std::bind(
&LiveviewSample::LiveviewStatusCallback,  // 状态回调函数
this,                                                                                    // 绑定当前对象实例
std::placeholders::_1));                                            // 参数：LiveviewStatus status

    return rc;
}

ErrorCode LiveviewSample::Start()
 {
    // 视频流处理线程启动
    if (stream_processor_thread_->Start() != 0) {
        ERROR("stream processor start failed");
        return kErrorInvalidOperation;
    }

    std::thread([&] {
        // Waiting for the liveview to be available before starting,
        // otherwise the StartH264Stream() will fail.
        while (liveview_status_ == 0) sleep(1);    // 等待 liveview_status_ 变为非零值（表示摄像头初始化完成或硬件就绪）
        auto rc = liveview_->StartH264Stream(); // 调用 liveview_ 对象的 StartH264Stream 方法启动视频流
        if (rc != kOk) {
            ERROR("Failed to start liveview: %d", rc);
        }
    }).detach();
    return kOk;
}

void LiveviewSample::LiveviewStatusCallback(const Liveview::LiveviewStatus& status)
 {
    liveview_status_ = status;
    DEBUG("status: %d", status);
}

int32_t InitLiveviewSample(std::shared_ptr<LiveviewSample>& liveview_sample, edge_sdk::Liveview::CameraType type,edge_sdk::Liveview::StreamQuality quality,std::shared_ptr<StreamDecoder> stream_decoder,std::shared_ptr<ImageProcessor> image_processor)
{
    // 原始流数据 → StreamDecoder 解码 → 图像帧 → ImageProcessor 处理 → 输出结果
    // 创建ImageProcessorThread 的图像处理线程，绑定解码器，注入图像处理逻辑
    auto image_processor_thread = std::make_shared<ImageProcessorThread>(stream_decoder->Name()); 
    image_processor_thread->SetImageProcessor(image_processor);

    
    auto stream_processor_thread =std::make_shared<StreamProcessorThread>(stream_decoder->Name()); // 创建流处理线程，绑定解码器和图像处理线程
    stream_processor_thread->SetStreamDecoder(stream_decoder); // 设置视频流解码器，用于将原始字节流解码为图像帧
    stream_processor_thread->SetImageProcessorThread(image_processor_thread); //建立流水线，解码后的数据直接传递给图像处理线程

    // 调用 LiveviewSample 对象的初始化方法，传递摄像头类型、质量参数和流处理线程。
    auto rc = liveview_sample->Init(type, quality, stream_processor_thread);
    if (rc != kOk) {
        ERROR("liveview sample init failed");
        return -1;
    }

    return 0;
}

ErrorCode LiveviewSample::SetCameraSource(edge_sdk::Liveview::CameraSource source) 
{
    return liveview_->SetCameraSource(source);
}

}  // namespace edge_app

