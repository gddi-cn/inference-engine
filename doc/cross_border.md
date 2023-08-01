### 越界计数模块
> src/modules/postprocess/cross_border.h
```C++
// 用于统计人流量和车流量, 具体实现需要根据 update_position 返回结果记录从左侧越过或者右侧越过的目标数量
// 需要用到跟踪模块的跟踪结果

/**
 * @brief 构造函数
 * 
 * @param border_type 边界类型, 目前只支持单一射线
 */
CrossBorder(const BorderType border_type);


/**
 * @brief 初始化边界
 * 
 * @param border 边界 [(x1, y1), (x2, y2)]
 * @param margin 边界宽度, 排除在边界徘徊目标
 * @return true 
 * @return false 
 */
bool init_border(const std::vector<Point2i> &border, const int margin = 0);

/**
 * @brief 更新位置信息
 * 
 * @param rects 目标集 {track_id, {{x, y, w, h}, ...}}
 * @return 若 .size() > 0, 则代表有目标越界, {{track_id, 0|1}, ...}, 0 代表从射线左侧越过, 1 代表从右侧越过
 */
std::map<int, int> update_position(const std::map<int, Rect2f> &rects);
```