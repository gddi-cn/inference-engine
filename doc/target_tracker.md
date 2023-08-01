### 目标跟踪模块
> src/modules/bytetrack/target_tracker.h
```C++
// 用于目标跟踪，根据 update_object 返回的 target_id 标识同一目标 

// 构造参数
TrackOption {
    float track_thresh{0.5};// 跟踪阈值
    float high_thresh{0.6}; // 大于该阈值的框进入高队列
    float match_thresh{0.8};// 大于该阈值的框直接匹配
    int max_frame_lost{30}; // 最大丢失帧数
}

// 目标信息
struct TrackObject {
    int class_id;   // 目标类型 ID
    float prob;     // 目标置信度
    Rect2f rect;    // 目标位置信息(x, y, w, h)
};

/**
 * @brief 构造函数
 * 
 * @param option 跟踪参数
 */
TargetTracker(const TrackOption &option);


/**
 * @brief 更新目标信息
 * 
 * @param objects 检测输出的目标信息
 * @return std::map<int, TrackObject> 跟踪结果信息 {{key, value}, ...}
 *         key 即为 track_id, value 为跟踪后的位置信息, 可用于可视化，防止预览框抖动问题
 */
std::map<int, TrackObject> update_object(const std::vector<TrackObject> &objects);
```