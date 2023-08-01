#ifndef NODE_SPECIFIC_MSG_H_
#define NODE_SPECIFIC_MSG_H_

#include <map>
#include <queue>
#include <algorithm>
#include <cmath>

#include "node_msg_def.h"
#define PI 3.14159265

#include <opencv2/core.hpp>
#include "node_any_basic.hpp"
#include "message_templates.hpp"
#include "modules/tracking/tracking.h"

#ifdef WITH_TENSORRT
#include "modules/pose/mmpose.h"
#endif

namespace gddi
{
    namespace nodes
    {

        struct FrameInfo;
        struct TrackingBox_;

        class BaseSpecificMsg
        {
        public:
            BaseSpecificMsg(const std::string &node_name)
                : node_name_(node_name){};
            const std::string &name() { return node_name_; }
            virtual ~BaseSpecificMsg() = default;

            // common method
            float _IoF(const std::vector<int> &region, const cv::Rect &box);

            // post-process method
            virtual void set_data(std::shared_ptr<FrameInfo> &image_info) {}
            virtual void reset_data() {}
            virtual std::map<std::string, int> count(){ return {}; };

            virtual std::map<std::string, std::vector<int>>
            roi(const std::vector<int> &region,
                const std::vector<std::string> &labels,
                float iof_thres){ return {}; };

            virtual std::map<std::pair<std::string, std::string>,
                             std::vector<std::pair<int, int>>>
            distance(const std::vector<std::string> &label_pairs,
                     float dis_thres,
                     bool reserve){ return {}; };

            virtual std::map<int, std::vector<std::pair<int, std::string>>>
            trajectory(int interval,
                       float dis_thres,
                       float radian_thres){ return {}; };

        protected:
            const std::string node_name_;
        };

        class SpecificBoxInfo : public BaseSpecificMsg
        {
        public:
            SpecificBoxInfo(const std::string &node_name,
                            std::shared_ptr<FrameInfo> &image_info);
            SpecificBoxInfo(const std::string &node_name);
            virtual void set_data(std::shared_ptr<FrameInfo> &image_info)
            {
                image_info_ = image_info;
            }
            virtual void reset_data() { image_info_ = nullptr; }
            virtual std::map<std::string, int> count();

            virtual std::map<std::string, std::vector<int>>
            roi(const std::vector<int> &region,
                const std::vector<std::string> &labels,
                float iof_thres);

            virtual std::map<std::pair<std::string, std::string>,
                             std::vector<std::pair<int, int>>>
            distance(const std::vector<std::string> &label_pairs,
                     float dis_thres,
                     bool reserve);

            bool _box_distance(const cv::Rect &box1, const cv::Rect &box2,
                               float dis_thres, bool reserve);
            void _pair_distance(std::pair<std::string, std::string> &pair_label,
                                float dis_thres, bool reserve);

        private:
            std::shared_ptr<FrameInfo> image_info_;
            std::map<std::string, int> map_label_count_;
            std::map<std::string, std::vector<int>> map_roi_measure_;
            std::map<std::pair<std::string, std::string>,
                     std::vector<std::pair<int, int>>>
                map_distance_;
        };

        class SpecificTrackingInfo : public BaseSpecificMsg
        {
        public:
            SpecificTrackingInfo(const std::string &node_name,
                                 const int64_t frame_idx,
                                 std::shared_ptr<FrameInfo> &image_info);
            SpecificTrackingInfo(const std::string &node_name);

            virtual void set_data(std::shared_ptr<FrameInfo> &image_info)
            {
                image_info_ = image_info;
            }
            virtual void reset_data() { image_info_ = nullptr; }
            virtual std::map<std::string, int> count();

            virtual std::map<std::string, std::vector<int>>
            roi(const std::vector<int> &region,
                const std::vector<std::string> &labels,
                float iof_thres);

            virtual std::map<int, std::vector<std::pair<int, std::string>>>
            trajectory(int interval,
                       float dis_thres,
                       float radian_thres);

            int calculate_direction(cv::Rect_<int> &rect1, cv::Rect_<int> &rect2,
                                    float dis_thres, float radian_thres);

        private:
            static std::vector<int> direction;
            int64_t frame_idx_;
            std::shared_ptr<FrameInfo> image_info_;
            std::map<std::string, int> map_label_count_;
            std::map<std::string, std::vector<int>> map_roi_measure_;
            std::map<int, std::vector<std::pair<int, std::string>>> map_trajectory_;

            std::map<int, std::queue<TrackingBox>> trace_queue_;
        };

#ifdef WITH_TENSORRT
        class SpecificPoseInfo : public BaseSpecificMsg
        {
        public:
            SpecificPoseInfo(const std::string &node_name,
                             std::shared_ptr<FrameInfo> &image_info);
            //目前只针对人，默认算人手距离，即暂不考虑label_pairs
            virtual std::map<std::pair<std::string, std::string>,
                             std::vector<std::pair<int, int>>> distance(const std::vector<std::string> &label_pairs,
                          float dis_thres,
                          bool reserve);

            // virtual void count();
            // virtual void roi(const std::vector<int> &region,
            //                  const std::vector<std::string> &labels,
            //                  float iof_thres);
            // virtual void trajectory(int interval,
            //                         float dis_thres,
            //                         float radian_thres);

        private:
            std::shared_ptr<FrameInfo> image_info_;
            //for each bbox in image_info, correspond to a vector<PoseKeyPoint>
            std::vector<std::vector<PoseKeyPoint>> keypoints_;
            //return result from distance()
            std::vector<std::pair< int, int>> keypoints_pair;
        };
#endif

    } // namespace nodes
    
    namespace nodes
    {
        // ===== BaseSpecificMsg =====
        float BaseSpecificMsg::_IoF(const std::vector<int> &region,
                                    const cv::Rect &box)
        {
            float l_x = std::max(region[0], box.tl().x);
            float l_y = std::max(region[1], box.tl().y);
            float r_x = std::min(region[2], box.br().x);
            float r_y = std::min(region[3], box.br().y);
            if (r_x <= l_x || r_y <= l_y)
                return 0;
            float over_area = (r_x - l_x) * (r_y - l_y);
            float box_area = (box.br().x - box.tl().x) * (box.br().y - box.tl().y);
            return static_cast<float>(over_area) / box_area;
        }

        // ===== SpecificBoxInfo =====
        SpecificBoxInfo::SpecificBoxInfo(const std::string &node_name,
                                         std::shared_ptr<FrameInfo> &image_info)
            : BaseSpecificMsg(node_name),
              image_info_(image_info){};

        SpecificBoxInfo::SpecificBoxInfo(const std::string &node_name)
            : BaseSpecificMsg(node_name) {}

        std::map<std::string, int> SpecificBoxInfo::count()
        {
            map_label_count_.clear();
            for (auto &item : image_info_->ext_info.back().vec_target_box)
            {
                if (map_label_count_.count(item.label) == 0)
                {
                    map_label_count_[item.label] = 1;
                }
                else
                {
                    map_label_count_[item.label]++;
                }
            }
            return map_label_count_;
        }

        std::map<std::string, std::vector<int>>
        SpecificBoxInfo::roi(const std::vector<int> &region,
                             const std::vector<std::string> &labels,
                             float iof_thres)
        {
            map_roi_measure_.clear();
            int box_id = -1;
            for (auto &item : image_info_->ext_info.back().vec_target_box)
            {
                box_id++;
                if (labels.size() == 0 ||
                    (labels.size() == 1 && labels[0].size() == 0) ||
                    (std::find(labels.begin(), labels.end(), item.label) != labels.end()))
                {
                    if (_IoF(region, cv::Rect{item.box.x, item.box.y, item.box.width, item.box.height}) > iof_thres)
                    {
                        map_roi_measure_[item.label].push_back(box_id);
                    }
                }
            }
            return map_roi_measure_;
        }

        bool SpecificBoxInfo::_box_distance(
            const cv::Rect &box1, const cv::Rect &box2,
            float dis_thres,
            bool reserve)
        {
            float box1_x = box1.tl().x + box1.width / 2.0;
            float box1_y = box1.tl().y + box1.height / 2.0;
            float box2_x = box2.tl().x + box2.width / 2.0;
            float box2_y = box2.tl().y + box2.height / 2.0;
            float dis = sqrt(pow((box2_x - box1_x), 2) + pow((box2_y - box1_y), 2));
            return reserve ? dis > dis_thres : dis <= dis_thres;
        }

        void SpecificBoxInfo::_pair_distance(
            std::pair<std::string, std::string> &pair_label,
            float dis_thres,
            bool reverse)
        {
            auto box_info = image_info_->ext_info.back().vec_target_box;
            int box_len = box_info.size();
            for (int i = 0; i < box_len; i++)
            {
                if (pair_label.first != box_info[i].label)
                {
                    continue;
                }
                int j = 0;
                if (pair_label.first == pair_label.second)
                {
                    j = i + 1;
                }
                for (; j < box_len && j != i; j++)
                {
                    if (pair_label.second == box_info[j].label)
                    {
                        if (_box_distance(cv::Rect{box_info[i].box.x, box_info[i].box.y, box_info[i].box.width, box_info[i].box.height},
                                          cv::Rect{box_info[j].box.x, box_info[j].box.y, box_info[j].box.width, box_info[j].box.height},
                                          dis_thres, reverse))
                        {
                            map_distance_[pair_label].push_back(
                                std::pair<int, int>(i, j));
                        }
                    }
                }
            }
        }

        std::map<std::pair<std::string, std::string>, std::vector<std::pair<int, int>>>
        SpecificBoxInfo::distance(const std::vector<std::string> &label_pairs,
                                  float dis_thres,
                                  bool reserve)
        {
            map_distance_.clear();
            int pair_size = label_pairs.size() / 2;
            for (int i = 0; i < pair_size; i++)
            {
                std::pair<std::string, std::string> pair(label_pairs[2 * i],
                                                         label_pairs[2 * i + 1]);
                _pair_distance(pair, dis_thres, reserve);
            }
            return map_distance_;
        }

        // ===== SpecificTrackingInfo =====
        std::vector<int> SpecificTrackingInfo::direction{
            0, 45, 90, 135, 180, 225, 270, 315};

        SpecificTrackingInfo::SpecificTrackingInfo(
            const std::string &node_name, const int64_t frame_idx,
            std::shared_ptr<FrameInfo> &image_info)
            : BaseSpecificMsg(node_name),
              frame_idx_(frame_idx),
              image_info_(image_info){};

        SpecificTrackingInfo::SpecificTrackingInfo(const std::string &node_name)
            : BaseSpecificMsg(node_name){};

        std::map<std::string, int> SpecificTrackingInfo::count()
        {
            map_label_count_.clear();
            auto tracked_info = image_info_->tracked_box_info;
            if (tracked_info.status)
            {
                for (auto &item : tracked_info.tracked_box)
                {
                    if (map_label_count_.count(item.label) == 0)
                    {
                        map_label_count_[item.label] = 1;
                    }
                    else
                    {
                        map_label_count_[item.label]++;
                    }
                }
                return map_label_count_;
            }
            return {};
        }

        std::map<std::string, std::vector<int>>
        SpecificTrackingInfo::roi(const std::vector<int> &region,
                                  const std::vector<std::string> &labels,
                                  float iof_thres)
        {
            map_roi_measure_.clear();
            for (auto &item : image_info_->tracked_box_info.tracked_box)
            {
                if (labels.size() == 0 ||
                    (labels.size() == 1 && labels[0].size() == 0) ||
                    (std::find(labels.begin(), labels.end(), item.label) != labels.end()))
                {
                    if (_IoF(region, item.box) > iof_thres)
                    {
                        map_roi_measure_[item.label].push_back(item.id);
                    }
                }
            }
            return map_roi_measure_;
        }

        int SpecificTrackingInfo::calculate_direction(
            cv::Rect_<int> &rect1, cv::Rect_<int> &rect2,
            float dis_thres, float radian_thres)
        {
            int x1 = rect1.x + rect1.width / 2;
            int y1 = rect1.y + rect1.height / 2;
            int x2 = rect2.x + rect2.width / 2;
            int y2 = rect2.y + rect2.height / 2;
            float x = x2 - x1;
            float y = y2 - y1;
            float dis = sqrt(pow(x, 2) + pow(y, 2));
            if (dis < dis_thres)
            {
                return -1;
            }
            // calc direction by atan function
            if (x == 0)
            {
                return y > 0 ? direction[2] : direction[6];
            }
            if (y == 0)
            {
                return x > 0 ? direction[0] : direction[4];
            }
            float rad = atan(y / x) * 180 / PI;
            if (rad < 0)
            {
                rad = y > 0 ? 180 + rad : 360 + rad;
            }
            else if (rad > 0)
            {
                rad = y > 0 ? rad : 180 + rad;
            }

            for (auto &r : direction)
            {
                if (r - radian_thres <= rad && rad <= r + radian_thres)
                {
                    return r;
                }
            }

            return -1;
        }

        std::map<int, std::vector<std::pair<int, std::string>>>
        SpecificTrackingInfo::trajectory(int interval,
                                         float dis_thres,
                                         float radian_thres)
        {
            map_trajectory_.clear();
            std::vector<TrackingBox> &
                tracked_box = image_info_->tracked_box_info.tracked_box;

            if ((frame_idx_ - 1) % interval == 0)
            {
                for (auto box : tracked_box)
                {
                    if (box.id != 0)
                    {
                        trace_queue_[box.id].push(box);
                    }
                }

                for (auto &t : trace_queue_)
                {
                    if (!t.second.empty())
                    {
                        auto front = t.second.front();
                        std::cout << front.id << " " << front.frame << std::endl;
                        if (front.frame < frame_idx_ - interval)
                        {
                            t.second.pop();
                        }
                    }
                    std::cout << "\t" << t.second.size() << std::endl;

                    if (t.second.size() == 2)
                    {
                        int direction_res = calculate_direction(t.second.front().box,
                                                                t.second.back().box,
                                                                dis_thres,
                                                                radian_thres);
                        if (direction_res != -1)
                        {
                            map_trajectory_[direction_res].push_back(
                                {t.second.back().id, t.second.back().label});
                        }
                    }
                }
            }
            return map_trajectory_;
        }

#ifdef WITH_TENSORRT
        // ===== SpecificPoseInfo =====
        SpecificPoseInfo::SpecificPoseInfo(
            const std::string &node_name,
            std::shared_ptr<FrameInfo> &image_info)
            : BaseSpecificMsg(node_name),
              image_info_(image_info){};

        std::map<std::pair<std::string, std::string>,
                 std::vector<std::pair<int, int>>>
        SpecificPoseInfo::distance(const std::vector<std::string> &label_pairs,
                                   float dis_thres,
                                   bool reserve)
        {
            //返回距离值合适的关键点对
            //left/right hand keypoint index: [9/10] (index: 0,1, ..., 16)

            //use L1 distance dis_thres
            float L2_Threshold = dis_thres * dis_thres;

            std::vector<std::pair< int, int>> &result = keypoints_pair;
            int N = keypoints_.size();
            for (int i = 0; i < N; i++)
            {
                if (keypoints_[i].size() == 0)
                    continue;
                for (int j = 0; j < N; j++)
                {
                    if (keypoints_[j].size() == 0)
                        continue;
                    for (int handi = 9; handi <= 10; handi++)
                    {
                        float xi = keypoints_[i][handi].x;
                        float yi = keypoints_[i][handi].y;
                        for (int handj = 9; handj <= 10; handj++)
                        {
                            float xj = keypoints_[j][handj].x;
                            float yj = keypoints_[j][handj].y;

                            float distance = (xi - xj) * (xi - xj) + (yi - yj) * (yi - yj);

                            if ((distance > L2_Threshold) ^ reserve)
                            {
                                result.push_back(std::pair< int, int>(i, j));
                            }
                        }
                    }
                }
            }

            std::map<std::pair<std::string, std::string>,
                 std::vector<std::pair<int, int>>> map_results;
            
            map_results[std::make_pair<std::string, std::string>("person","person")]=result;
            return map_results;
        }
#endif

    } // namespace nodes
} // namespace gddi


#endif // end NODE_SPECIFIC_MSG_H_
