#if !defined(ROOMACTION)
#define ROOMACTION

// 輔助函式：將房間狀態轉換為字串
const char* get_status_str(room_status_t status);
/** @brief 取得所有房間的當前狀態。
 * @return 包含所有房間狀態的格式化字串。*/
char* get_all_status();
/**
 * @brief 預約房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已被預約或次數超限)
 */
int reserve_room(int room_id);
/**
 * @brief 報到/Check-in
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int check_in(int room_id);

/**
 * @brief 釋放房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int release_room(int room_id);
/**
 * @brief 延長使用
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已延長過或狀態不對)
 */
int extend_room(int room_id);
#endif // ROOMACTION
