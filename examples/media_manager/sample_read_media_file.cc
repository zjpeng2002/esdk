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
#include <unistd.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "logger.h"
#include "media_manager.h"

using namespace edge_sdk;

ErrorCode ESDKInit();

namespace {

std::queue<MediaFile> media_file_queue;
std::mutex media_file_queue_mutex;
std::condition_variable media_file_queue_cv;
std::shared_ptr<MediaFilesReader> media_files_reader;

}  // namespace

static ErrorCode ReadMediaFile(const MediaFile& file, std::vector<uint8_t>& image) {
    // 定义1MB的缓冲区用于分块读取文件
    char buf[1024 * 1024];
    
    // 打开指定路径的媒体文件
    auto fd = media_files_reader->Open(file.file_path);
    
    // 如果打开失败，返回系统错误
    if (fd < 0) {
        return kErrorSystemError;
    }
    
    // 循环读取文件内容
    do {
        // 从文件中读取数据到缓冲区，最多读取1MB
        auto nread = media_files_reader->Read(fd, buf, sizeof(buf));
        
        // 如果成功读取到数据
        if (nread > 0) {
            // 将读取到的数据追加到image vector中
            image.insert(image.end(), (uint8_t*)buf, (uint8_t*)(buf + nread));
        } else {
            // 读取失败或到达文件末尾，关闭文件并退出循环
            media_files_reader->Close(fd);
            break;
        }
    } while (1);  // 无限循环，直到读取完整个文件

    // 记录日志：输出文件大小和实际读取到的数据大小
    INFO("filesize: %d, image.size: %d", file.file_size, image.size());

    // 返回成功状态
    return kOk;
}

static void DumpMediaFile(const std::string& filename, const std::vector<uint8_t>& data) {
    FILE* f = fopen(filename.c_str(), "wb");
    if (f) {
        fwrite(data.data(), data.size(), 1, f);
        fclose(f);
    }
}

static ErrorCode MediaFilesUpdateCallback(const MediaFile& file) {
    INFO("File update: %s", file.file_name.c_str());
    std::lock_guard<std::mutex> l(media_file_queue_mutex); // 使用互斥锁保护共享资源meida_file_queue，对象生命周期结束时自动释放锁
    media_file_queue.push(file); // 将文件添加到队列中
    media_file_queue_cv.notify_one(); // 通知等待的线程有新的文件可用
    return kOk;
}

static void ProcessMediaFileThread() {
    std::vector<uint8_t> image;
    while (1) {
        std::unique_lock<std::mutex> l(media_file_queue_mutex);  // 使用互斥锁保护共享资源meida_file_queue

        media_file_queue_cv.wait(l,[] { return media_file_queue.size() != 0; }); // 等待条件满足

        MediaFile file = media_file_queue.front(); // 从前端队列中取出第一个文件
        media_file_queue.pop(); // 从队列中移除已取出的文件
        l.unlock(); // 释放互斥锁

        INFO("process media file:%s", file.file_name.c_str());
        auto rc = ReadMediaFile(file, image);
        if (rc == kOk) {
            DumpMediaFile(file.file_name, image);
        } else {
            ERROR("dump media file failed: %s", file.file_name.c_str());
        }

        image.clear();
    }
}

ErrorCode StartReadMediaFileSample() {
    media_files_reader = MediaManager::Instance()->CreateMediaFilesReader();
    auto rc = media_files_reader->Init();
    if (rc != kOk) {
        ERROR("init media files reader error");
        return rc;
    }
 
    // 注册媒体文件更新回调函数
    rc = MediaManager::Instance()->RegisterMediaFilesObserver(
std::bind(MediaFilesUpdateCallback, std::placeholders::_1));
    if (rc != kOk) {
        ERROR("registe media files observer");
        return rc;
    }

    auto t = std::thread(&ProcessMediaFileThread);
    t.detach();

    return kOk;
}

int main(int argc, char** argv) {
    // 初始化ESDK，获取返回码
    auto rc = ESDKInit();
    if (rc != kOk) {
        ERROR("pre init failed");
        return -1;
    }

    // 设置媒体文件不自动删除，获取返回码
    rc = MediaManager::Instance()->SetDroneNestAutoDelete(false);
    
    // 如果设置成功
    if (rc == kOk) {
        // 启动读取媒体文件示例程序
        if (StartReadMediaFileSample() != kOk) {
            ERROR("start media file sample failed");
        }
    } else {
        // 如果设置失败，记录错误日志
        ERROR("set media file auto delete failed");
    }

    // 主循环，每3秒休眠一次，保持程序运行
    while (1) sleep(3);

    // 程序正常结束，返回0
    return 0;
}
