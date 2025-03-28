#include <string>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <sys/stat.h>
#include <ctime>
#include "common/image_processor.h"
#include "logger.h"
#include "util_misc.h"
#include "sample_liveview.h"
#include "stream_decoder.h"

using namespace cv;


namespace  edge_app{
class StreamDecodeRecorder : public StreamDecoder {
    public:
     StreamDecodeRecorder(const std::string& name, std::shared_ptr<StreamDecoder>& decoder) : StreamDecoder(name),
        name_(name), decoder_(decoder) {
        char current_dir[256];
        if (GetCurrentFileDirPath(__FILE__, sizeof(current_dir), current_dir) != 0) {
            WARN("get current directory failed");
            return;
        }
         snprintf(file_path_, sizeof(file_path_), "%s/../../build/%s", current_dir, "h264save");
        char cmd[532];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", file_path_);
        auto ret = system(cmd);
        if (ret != 0) {
            WARN("mkdir %s failed", file_path_);
        }
        INFO("h264 recorder init successfully");
     }

     ~StreamDecodeRecorder() override {
         if (file_) {
             fclose(file_);
         }
     }

     int32_t Init() override {
         return decoder_->Init();
     }

     int32_t DeInit() override {
         return decoder_->DeInit();
     }

     int32_t Decode(const uint8_t* data, size_t length,
                    DecodeResultCallback result_callback) {
         auto get_current_boot_time = [] {
             struct timespec ts;
             clock_gettime(CLOCK_BOOTTIME, &ts);
             return ts.tv_sec;
         };

         auto now = get_current_boot_time();
         if (now - create_new_file_time_ >= 60) {
             create_new_file_time_ = now;
             if (file_) {
                 fclose(file_);
                 file_ = NULL;
             }
             char buf[32];
             auto now = time(NULL);
             strftime(buf, sizeof(buf), "_%Y-%m-%d-%H-%M-%S", localtime(&now));
             std::string file = file_path_ + std::string("/") + name_ +
                 std::string(buf) + ".h264";
             file_ = fopen(file.c_str(), "w+b");
             INFO("create h264 file: %s", file.c_str());
             if (!file_) {
                 ERROR("create file failed: %s", file.c_str());
             }
         }
         if (now - create_new_file_time_ < 5) {
             if (file_) {
                 fwrite(data, length, 1, file_);
             }
         }

         return decoder_->Decode(data, length, result_callback);
     }

    private:
        FILE* file_ = NULL;
        uint32_t create_new_file_time_ = 0;
        std::string name_;
        char file_path_[256];
        std::shared_ptr<StreamDecoder> decoder_;
    };
}// namespace edge_app
