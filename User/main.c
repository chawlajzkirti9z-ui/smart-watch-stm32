#include "stm32f10x.h"
#include "OLED.h"
#include "RTC.h"
#include "Key.h"
#include "Encoder.h"
#include "ADC.h"
#include "MPU6050.h"
#include "W25Q64.h"
#include <stdio.h>

/* ==================== 全局变量 ==================== */
uint8_t page = 0;       // 页面：0=主界面, 1=设置, 2=秒表, 3=姿态
uint8_t set_mode = 0;   // 设置项：0=退出, 1=年, 2=月, 3=日, 4=时, 5=分, 6=秒
uint8_t edit_flag = 0;  // 编辑标志：0=选择, 1=编辑

uint32_t stopwatch_start = 0;
uint8_t  stopwatch_running = 0;
uint32_t stopwatch_elapsed = 0;

uint32_t last_save_counter = 0;  // 上次保存的RTC计数器
uint8_t rot_lock = 0;            // 编码器防抖锁

/* ==================== 主函数 ==================== */
int main(void)
{
    OLED_Init();
    RTC_Init();
    W25Q64_Init();

    // 从Flash读回上次保存的时间
    uint32_t saved = W25Q64_ReadTime();
    if (saved != 0xFFFFFFFF && saved != 0)
    {
        PWR_BackupAccessCmd(ENABLE);
        RTC_SetCounter(saved);
        RTC_WaitForLastTask();
        last_save_counter = saved;
    }

    Encoder_Init();
    Key_Init();
    ADC_Init_Config();
    MPU6050_Init();

    /* ============ MPU6050 零偏校准 ============ */
    /* 上电时保持板子水平静止，程序读50次求平均作为零点 */
    int16_t cal_ax = 0, cal_ay = 0;
    {
        int32_t sum_x = 0, sum_y = 0;
        int16_t t_ax, t_ay, t_az;
        for (int i = 0; i < 50; i++)
        {
            MPU6050_ReadAccel(&t_ax, &t_ay, &t_az);
            sum_x += t_ax;
            sum_y += t_ay;
            for (int j = 0; j < 10000; j++);
        }
        cal_ax = sum_x / 50;
        cal_ay = sum_y / 50;
    }

    uint8_t h, m, s;
    uint16_t year;
    uint8_t month, day;
    uint8_t bat = 0;
    int16_t ax, ay, az;
    char buf[24];

    while (1)
    {
        RTC_Get_Time(&h, &m, &s);
        RTC_Get_Date(&year, &month, &day);

        // 每60秒保存一次时间
        uint32_t now = RTC_GetCounter();
        if (now - last_save_counter >= 60)
        {
            W25Q64_SaveTime(now);
            last_save_counter = now;
        }

        bat = ADC_GetBatteryPercent();

        int8_t rot = Encoder_GetRotate();
        uint8_t key = Key_GetNum();

        if (rot == 0) rot_lock = 0;
        if (rot_lock) rot = 0;

        /* ================= 页面0：主界面 ================= */
        if (page == 0)
        {
            if (key == 1)   // 按确认进设置
            {
                page = 1; set_mode = 0; edit_flag = 0;
                OLED_Clear(); OLED_Update();
                continue;
            }
            else if (rot != 0)   // 旋转进秒表
            {
                page = 2; stopwatch_running = 0; stopwatch_elapsed = 0;
                stopwatch_start = 0; rot_lock = 1;
                OLED_Clear(); OLED_Update();
                continue;
            }

            OLED_Clear();

            // 第1行：日期 左对齐
            sprintf(buf, "%04d-%02d-%02d", year, month, day);
            OLED_ShowString(1, 1, buf);

            // 第1行：电量 右对齐（第13列）
            sprintf(buf, "%3d%%", bat);
            OLED_ShowString(1, 13, buf);

            // 第3行：时间居中
            sprintf(buf, "%02d:%02d:%02d", h, m, s);
            OLED_ShowString(3, 5, buf);

            // 第4行：Menu 左，Set 右（与电量同列）
            OLED_ShowString(4, 1, "Menu");
            OLED_ShowString(4, 14, "Set");
        }
        /* ================= 页面1：设置界面 ================= */
        else if (page == 1)
        {
            if (edit_flag == 0)
            {
                if (rot != 0) { set_mode += rot; if (set_mode > 6) set_mode = 0; if (set_mode < 0) set_mode = 6; }
                else if (key == 1)
                {
                    if (set_mode == 0)   // 退出设置
                    {
                        page = 0; edit_flag = 0;
                        OLED_Clear(); OLED_Update();
                        continue;
                    }
                    else { edit_flag = 1; }   // 进入编辑
                }
            }
            else if (edit_flag == 1)
            {
                if (rot != 0)
                {
                    // 年份范围 2000~2100，与RTC基准年一致
                    if (set_mode == 1) { year += rot;  if (year > 2100) year = 2000; if (year < 2000) year = 2100; }
                    if (set_mode == 2) { month += rot; if (month > 12)  month = 1;   if (month < 1)   month = 12; }
                    if (set_mode == 3) { day += rot;   if (day > 31)    day = 1;     if (day < 1)     day = 31; }
                    if (set_mode == 4) { h += rot;     if (h >= 24)    h = 0;       if (h < 0)       h = 23; }
                    if (set_mode == 5) { m += rot;     if (m >= 60)    m = 0;       if (m < 0)       m = 59; }
                    if (set_mode == 6) { s += rot;     if (s >= 60)    s = 0;       if (s < 0)       s = 59; }
                    RTC_Set_DateTime(year, month, day, h, m, s);
                    stopwatch_running = 0; stopwatch_elapsed = 0; stopwatch_start = 0;
                }
                else if (key == 1) { edit_flag = 0; set_mode++; if (set_mode > 6) set_mode = 0; }
            }

            OLED_Clear();
            if (edit_flag == 0) OLED_ShowString(1, 1, "Select");
            else                OLED_ShowString(1, 1, "Editing");
            sprintf(buf, "%04d-%02d-%02d", year, month, day); OLED_ShowString(2, 1, buf);
            sprintf(buf, "%02d:%02d:%02d", h, m, s); OLED_ShowString(3, 1, buf);
            char *mode_str[] = {"Exit ", "Year ", "Month", "Day  ", "Hour ", "Min  ", "Sec  "};
            if (edit_flag == 0) sprintf(buf, ">%s", mode_str[set_mode]);
            else                sprintf(buf, " %s:", mode_str[set_mode]);
            OLED_ShowString(4, 1, buf);
        }
        /* ================= 页面2：秒表界面 ================= */
        else if (page == 2)
        {
            if (key == 1)   // 回主界面
            {
                page = 0;
                OLED_Clear(); OLED_Update();
                continue;
            }
            else if (rot != 0)   // 去姿态界面
            {
                stopwatch_running = 0; stopwatch_elapsed = 0; stopwatch_start = 0;
                page = 3; rot_lock = 1;
                OLED_Clear(); OLED_Update();
                continue;
            }

            uint32_t elapsed = stopwatch_running ? (RTC_GetCounter() - stopwatch_start) : stopwatch_elapsed;
            uint32_t min = elapsed / 60;
            uint32_t sec = elapsed % 60;

            OLED_Clear();
            OLED_ShowString(1, 1, "Stopwatch");
            sprintf(buf, "%02d:%02d", min, sec);
            OLED_ShowString(3, 5, buf);
            if (stopwatch_running) OLED_ShowString(4, 1, "Running");
            else                   OLED_ShowString(4, 1, "Stopped");
        }
        /* ================= 页面3：姿态界面 ================= */
        else if (page == 3)
        {
            if (key == 1 || rot != 0)   // 回主界面
            {
                page = 0; rot_lock = 1;
                OLED_Clear(); OLED_Update();
                continue;
            }

            // 读取MPU6050并一阶低通滤波
            static int32_t smooth_ax = 0, smooth_ay = 0, smooth_az = 0;
            int16_t raw_ax, raw_ay, raw_az;
            MPU6050_ReadAccel(&raw_ax, &raw_ay, &raw_az);
            smooth_ax = (smooth_ax * 7 + raw_ax * 3) / 10;
            smooth_ay = (smooth_ay * 7 + raw_ay * 3) / 10;
            smooth_az = (smooth_az * 7 + raw_az * 3) / 10;
            ax = (int16_t)smooth_ax - cal_ax;
            ay = (int16_t)smooth_ay - cal_ay;
            az = (int16_t)smooth_az;

            OLED_Clear();

            // 顶部数值
            sprintf(buf, "AX:%5dAY:%5d", ax, ay);
            OLED_ShowString(1, 1, buf);

            // 外框 + 中心十字
            OLED_DrawRectangle(32, 18, 96, 58, 1);
            OLED_DrawLine(60, 38, 68, 38, 1);
            OLED_DrawLine(64, 34, 64, 42, 1);

            // 偏移量计算
            int16_t offset_x = (int32_t)ax * 28 / 4000;
            int16_t offset_y = (int32_t)ay * 16 / 4000;
            if (offset_x > 28)  offset_x = 28;
            if (offset_x < -28) offset_x = -28;
            if (offset_y > 16)  offset_y = 16;
            if (offset_y < -16) offset_y = -16;

            uint8_t px = 64 + offset_x;
            uint8_t py = 38 + offset_y;

            // 画实心点 5×5
            OLED_DrawPoint(px, py, 1);
            OLED_DrawPoint(px-1, py, 1); OLED_DrawPoint(px+1, py, 1);
            OLED_DrawPoint(px, py-1, 1); OLED_DrawPoint(px, py+1, 1);
            OLED_DrawPoint(px-1, py-1, 1); OLED_DrawPoint(px+1, py+1, 1);
            OLED_DrawPoint(px-1, py+1, 1); OLED_DrawPoint(px+1, py-1, 1);
        }

        OLED_Update();  // 把显存刷到屏幕
        for (int i = 0; i < 10000; i++);
    }
}