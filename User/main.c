#include "stm32f10x.h"
#include "OLED.h"
#include "RTC.h"
#include "Key.h"
#include "Encoder.h"
#include "ADC.h"
#include "MPU6050.h"
#include "W25Q64.h"
#include "Buzzer.h"
#include <stdio.h>

/* 页面编号：
   0=主界面  1=Menu  2=Clock菜单  3=秒表  4=姿态
   5=设置菜单  6=日期时间  7=息屏  8=亮度  9=倒计时
   10=时间闹钟  11=计步 */
uint8_t page = 0;
uint8_t main_sel = 0;   // 主界面：0=Menu, 1=Set
uint8_t menu_sel = 0;   // Menu：0=Pose, 1=Clock, 2=Step, 3=Empty
uint8_t sub_sel  = 0;
uint8_t edit_flag = 0;
uint8_t edit_sub  = 0;

/* 设置参数 */
uint8_t sleep_timeout = 10;
uint8_t brightness = 0xCF;

/* 时间闹钟 */
uint8_t alarm_time_h = 7;
uint8_t alarm_time_m = 0;
uint8_t alarm_time_en = 0;
uint8_t alarm_sub = 0;

/* 倒计时 */
uint16_t timer_sec = 30;
uint8_t  timer_running = 0;
uint32_t timer_start = 0;

/* 蜂鸣器 */
uint8_t buzzer_on = 0;

/* 秒表 */
uint32_t stopwatch_start = 0;
uint8_t  stopwatch_running = 0;
uint32_t stopwatch_elapsed = 0;

/* 计步 */
uint32_t step_count = 0;   // 今日步数
uint32_t step_date  = 0;   // 步数所属日期（year*10000+month*100+day）

/* 息屏 */
uint32_t last_active_time = 0;
uint8_t  oled_sleeping = 0;

uint8_t rot_lock = 0;
uint8_t mpu_error = 0;
uint32_t last_save_counter = 0;

int main(void)
{
    OLED_Init();
    RTC_Init();
    W25Q64_Init();
    Buzzer_Init();

    /* 从Flash读回时间 */
    uint32_t saved = W25Q64_ReadTime();
    if (saved != 0xFFFFFFFF && saved != 0)
    {
        PWR_BackupAccessCmd(ENABLE);
        RTC_SetCounter(saved);
        RTC_WaitForLastTask();
        last_save_counter = saved;
    }

    /* 从Flash读回步数和日期 */
    W25Q64_ReadSteps(&step_count, &step_date);
    if (step_count == 0xFFFFFFFF) step_count = 0;
    if (step_date  == 0xFFFFFFFF) step_date  = 0;

    Encoder_Init();
    Key_Init();
    ADC_Init_Config();
    MPU6050_Init();
    OLED_SetBrightness(brightness);

    /* MPU6050 零偏校准 */
    int16_t cal_ax = 0, cal_ay = 0;
    {
        int32_t sum_x = 0, sum_y = 0;
        int16_t t_ax, t_ay, t_az;
        for (int i = 0; i < 50; i++)
        {
            if (MPU6050_ReadAccel(&t_ax, &t_ay, &t_az) == 0)
            { sum_x += t_ax; sum_y += t_ay; }
            for (int j = 0; j < 10000; j++);
        }
        cal_ax = sum_x / 50;
        cal_ay = sum_y / 50;
    }

    last_active_time = RTC_GetCounter();

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

        /* 每60秒保存时间 */
        uint32_t now = RTC_GetCounter();
        if (now - last_save_counter >= 60)
        {
            W25Q64_SaveTime(now);
            last_save_counter = now;
        }

        /* ============ 计步：跨天自动清零 ============ */
        uint32_t today = (uint32_t)year * 10000 + month * 100 + day;
        if (step_date != today)
        {
            step_date = today;
            step_count = 0;
            W25Q64_SaveSteps(step_count, step_date);
        }

        /* ============ 计步：后台检测 ============ */
        if (MPU6050_CheckStep())
        {
            step_count++;
            if (step_count % 100 == 0)   // 每100步存一次
                W25Q64_SaveSteps(step_count, step_date);
        }

        bat = ADC_GetBatteryPercent();

        int8_t rot = Encoder_GetRotate();
        uint8_t key = Key_GetNum();
        uint8_t key2 = Key2_GetNum();

        if (rot == 0) rot_lock = 0;
        if (rot_lock) rot = 0;

        /* ============ 唤醒（息屏期间屏蔽操作） ============ */
        if (key == 1 || key2 == 1 || rot != 0)
        {
            last_active_time = RTC_GetCounter();
            if (oled_sleeping)
            {
                OLED_DisplayOn();
                oled_sleeping = 0;
                key = 0; key2 = 0; rot = 0;
            }
        }

        /* ============ 时间闹钟 ============ */
        if (alarm_time_en && h == alarm_time_h && m == alarm_time_m && s == 0 && !buzzer_on)
        {
            buzzer_on = 1; Buzzer_On(); alarm_time_en = 0;
            OLED_DisplayOn(); oled_sleeping = 0;
            last_active_time = RTC_GetCounter();
        }
        /* 倒计时到点 */
        if (timer_running && RTC_GetCounter() - timer_start >= timer_sec)
        {
            timer_running = 0; buzzer_on = 1; Buzzer_On();
            OLED_DisplayOn(); oled_sleeping = 0;
            last_active_time = RTC_GetCounter();
        }
        /* 蜂鸣器响时任意键停 */
        if (buzzer_on)
        {
            if (key == 1 || key2 == 1 || rot != 0)
            {
                Buzzer_Off(); buzzer_on = 0;
                key = 0; key2 = 0; rot = 0;
            }
        }

        /* ============ 息屏 ============ */
        if (!oled_sleeping && sleep_timeout > 0 &&
            RTC_GetCounter() - last_active_time >= sleep_timeout)
        {
            OLED_DisplayOff();
            oled_sleeping = 1;
        }

        /* ================= 页面0：主界面 ================= */
        if (page == 0)
        {
            if (rot != 0)
            {
                main_sel += rot;
                if (main_sel > 1) main_sel = 0;
                if (main_sel < 0) main_sel = 1;
                rot_lock = 1;
            }
            else if (key == 1)
            {
                if (main_sel == 0)      { page = 1; menu_sel = 0; }
                else if (main_sel == 1) { page = 5; sub_sel = 0; }
                rot_lock = 1; OLED_Clear(); OLED_Update(); continue;
            }

            OLED_Clear();
            sprintf(buf, "%04d-%02d-%02d", year, month, day);
            OLED_ShowString(1, 1, buf);
            sprintf(buf, "%3d%%", bat);
            OLED_ShowString(1, 13, buf);
            sprintf(buf, "%02d:%02d:%02d", h, m, s);
            OLED_ShowString(2, 4, buf);
            OLED_DrawLine(0, 32, 127, 32, 1);
            if (main_sel == 0) OLED_ShowString(4, 1, ">Menu");
            else               OLED_ShowString(4, 1, " Menu");
            if (main_sel == 1) OLED_ShowString(4, 10, ">Set");
            else               OLED_ShowString(4, 10, " Set");
        }
        /* ================= 页面1：Menu ================= */
        else if (page == 1)
        {
            if (key2 == 1) { page = 0; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                menu_sel += rot;
                if (menu_sel > 3) menu_sel = 0;
                if (menu_sel < 0) menu_sel = 3;
                rot_lock = 1;
            }
            else if (key == 1)
            {
                if (menu_sel == 0)      { page = 4; }                    // Pose
                else if (menu_sel == 1) { page = 2; sub_sel = 0; }       // Clock
                else if (menu_sel == 2) { page = 11; }                   // Step
                if (menu_sel != 3) { rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Menu:");
            char *items[] = {"Pose", "Clock", "Step", "Empty"};
            for (int i = 0; i < 4; i++)
            {
                if (menu_sel == i) sprintf(buf, ">%s", items[i]);
                else                sprintf(buf, " %s", items[i]);
                OLED_ShowString(i + 1, 1, buf);
            }
        }
        /* ================= 页面2：Clock菜单 ================= */
        else if (page == 2)
        {
            if (key2 == 1) { page = 1; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                sub_sel += rot;
                if (sub_sel > 3) sub_sel = 0;
                if (sub_sel < 0) sub_sel = 3;
                rot_lock = 1;
            }
            else if (key == 1)
            {
                if (sub_sel == 1)      { page = 10; alarm_sub = 0; rot_lock = 1; }
                else if (sub_sel == 2) { page = 9; rot_lock = 1; }
                else if (sub_sel == 3) { page = 3; rot_lock = 1; }
                if (sub_sel != 0) { OLED_Clear(); OLED_Update(); continue; }
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Clock:");
            char *items[] = {"Time", "Alarm", "Countdown", "Stopwatch"};
            for (int i = 0; i < 4; i++)
            {
                if (sub_sel == i) sprintf(buf, ">%s", items[i]);
                else                sprintf(buf, " %s", items[i]);
                OLED_ShowString(i + 1, 1, buf);
            }
        }
        /* ================= 页面3：秒表 ================= */
        else if (page == 3)
        {
            if (key2 == 1)
            {
                if (stopwatch_running || stopwatch_elapsed != 0)
                {
                    stopwatch_running = 0; stopwatch_elapsed = 0; stopwatch_start = 0;
                }
                else { page = 2; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            }
            else if (key == 1)
            {
                if (stopwatch_running == 0) { stopwatch_start = RTC_GetCounter() - stopwatch_elapsed; stopwatch_running = 1; }
                else { stopwatch_elapsed = RTC_GetCounter() - stopwatch_start; stopwatch_running = 0; }
            }
            else if (rot != 0) { page = 2; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }

            uint32_t elapsed = stopwatch_running ? (RTC_GetCounter() - stopwatch_start) : stopwatch_elapsed;
            uint32_t min = elapsed / 60, sec = elapsed % 60;
            OLED_Clear();
            OLED_ShowString(1, 1, "Stopwatch");
            sprintf(buf, "%02d:%02d", min, sec);
            OLED_ShowString(3, 5, buf);
            if (stopwatch_running) OLED_ShowString(4, 1, "Running");
            else                   OLED_ShowString(4, 1, "Stopped");
        }
        /* ================= 页面4：姿态 ================= */
        else if (page == 4)
        {
            if (key == 1 || key2 == 1 || rot != 0)
            { page = 1; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }

            static int32_t smooth_ax = 0, smooth_ay = 0, smooth_az = 0;
            int16_t raw_ax, raw_ay, raw_az;
            if (MPU6050_ReadAccel(&raw_ax, &raw_ay, &raw_az) != 0) mpu_error = 1;
            else {
                mpu_error = 0;
                smooth_ax = (smooth_ax * 7 + raw_ax * 3) / 10;
                smooth_ay = (smooth_ay * 7 + raw_ay * 3) / 10;
                smooth_az = (smooth_az * 7 + raw_az * 3) / 10;
                ax = (int16_t)smooth_ax - cal_ax;
                ay = (int16_t)smooth_ay - cal_ay;
                az = (int16_t)smooth_az;
            }
            OLED_Clear();
            if (mpu_error) {
                OLED_ShowString(2, 3, "MPU ERROR");
                OLED_ShowString(4, 2, "Check wire!");
            } else {
                sprintf(buf, "AX:%5dAY:%5d", ax, ay);
                OLED_ShowString(1, 1, buf);
                OLED_DrawRectangle(32, 18, 96, 58, 1);
                OLED_DrawLine(60, 38, 68, 38, 1);
                OLED_DrawLine(64, 34, 64, 42, 1);
                int16_t ox = (int32_t)ax * 28 / 4000;
                int16_t oy = (int32_t)ay * 16 / 4000;
                if (ox > 28) ox = 28; if (ox < -28) ox = -28;
                if (oy > 16) oy = 16; if (oy < -16) oy = -16;
                uint8_t px = 64 + ox, py = 38 + oy;
                OLED_DrawPoint(px, py, 1);
                OLED_DrawPoint(px-1, py, 1); OLED_DrawPoint(px+1, py, 1);
                OLED_DrawPoint(px, py-1, 1); OLED_DrawPoint(px, py+1, 1);
                OLED_DrawPoint(px-1, py-1, 1); OLED_DrawPoint(px+1, py+1, 1);
                OLED_DrawPoint(px-1, py+1, 1); OLED_DrawPoint(px+1, py-1, 1);
            }
        }
        /* ================= 页面5：设置菜单 ================= */
        else if (page == 5)
        {
            if (key2 == 1) { page = 0; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                sub_sel += rot;
                if (sub_sel > 2) sub_sel = 0;
                if (sub_sel < 0) sub_sel = 2;
                rot_lock = 1;
            }
            else if (key == 1)
            {
                if (sub_sel == 0)      { page = 6; edit_flag = 0; edit_sub = 0; }
                else if (sub_sel == 1) { page = 7; }
                else if (sub_sel == 2) { page = 8; }
                rot_lock = 1; OLED_Clear(); OLED_Update(); continue;
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Setting:");
            if (sub_sel == 0) OLED_ShowString(2, 1, ">Date Time");
            else              OLED_ShowString(2, 1, " Date Time");
            sprintf(buf, sub_sel == 1 ? ">Sleep  %2ds" : " Sleep  %2ds", sleep_timeout);
            OLED_ShowString(3, 1, buf);
            sprintf(buf, sub_sel == 2 ? ">Bright %3d" : " Bright %3d", brightness);
            OLED_ShowString(4, 1, buf);
        }
        /* ================= 页面6：日期时间 ================= */
        else if (page == 6)
        {
            if (edit_flag == 0)
            {
                if (key2 == 1) { page = 5; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
                if (rot != 0)
                {
                    sub_sel += rot;
                    if (sub_sel > 1) sub_sel = 0;
                    if (sub_sel < 0) sub_sel = 1;
                    rot_lock = 1;
                }
                else if (key == 1) { edit_flag = 1; edit_sub = 0; }

                OLED_Clear();
                OLED_ShowString(1, 1, "Date/Time:");
                if (sub_sel == 0) OLED_ShowString(2, 1, ">Date");
                else              OLED_ShowString(2, 1, " Date");
                if (sub_sel == 1) OLED_ShowString(3, 1, ">Time");
                else              OLED_ShowString(3, 1, " Time");
            }
            else
            {
                if (key2 == 1)
                {
                    edit_flag = 0;
                    RTC_Get_Time(&h, &m, &s);
                    RTC_Get_Date(&year, &month, &day);
                }
                else if (key == 1)
                {
                    if (edit_sub < 2) { edit_sub++; }
                    else { RTC_Set_DateTime(year, month, day, h, m, s); edit_flag = 0; }
                }
                if (rot != 0)
                {
                    if (sub_sel == 0) {
                        if (edit_sub == 0) { year += rot;  if (year > 2100) year = 2000; if (year < 2000) year = 2100; }
                        if (edit_sub == 1) { month += rot; if (month > 12)  month = 1;   if (month < 1)   month = 12; }
                        if (edit_sub == 2) { day += rot;   if (day > 31)    day = 1;     if (day < 1)     day = 31; }
                    } else {
                        if (edit_sub == 0) { h += rot; if (h >= 24) h = 0; if (h < 0) h = 23; }
                        if (edit_sub == 1) { m += rot; if (m >= 60) m = 0; if (m < 0) m = 59; }
                        if (edit_sub == 2) { s += rot; if (s >= 60) s = 0; if (s < 0) s = 59; }
                    }
                }

                OLED_Clear();
                if (sub_sel == 0) {
                    OLED_ShowString(1, 1, "Edit Date");
                    sprintf(buf, "%04d-%02d-%02d", year, month, day);
                    OLED_ShowString(3, 1, buf);
                    char *sub[] = {"Year ", "Month", "Day  "};
                    sprintf(buf, "  %s", sub[edit_sub]);
                    OLED_ShowString(4, 1, buf);
                } else {
                    OLED_ShowString(1, 1, "Edit Time");
                    sprintf(buf, "%02d:%02d:%02d", h, m, s);
                    OLED_ShowString(3, 1, buf);
                    char *sub[] = {"Hour ", "Min  ", "Sec  "};
                    sprintf(buf, "  %s", sub[edit_sub]);
                    OLED_ShowString(4, 1, buf);
                }
            }
        }
        /* ================= 页面7：息屏时间 ================= */
        else if (page == 7)
        {
            if (key2 == 1) { page = 5; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                int16_t v = (int16_t)sleep_timeout + rot;
                if (v > 60) v = 0;
                if (v < 0)  v = 60;
                sleep_timeout = v;
                rot_lock = 1;
            }
            else if (key == 1) { page = 5; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }

            OLED_Clear();
            OLED_ShowString(1, 1, "Sleep Time");
            sprintf(buf, "%2d s", sleep_timeout);
            OLED_ShowString(2, 1, buf);
            OLED_ShowString(3, 1, "Rotate to set");
            OLED_ShowString(4, 1, "PA2 save");
        }
        /* ================= 页面8：亮度 ================= */
        else if (page == 8)
        {
            if (key2 == 1) { page = 5; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                int16_t v = (int16_t)brightness + rot * 16;
                if (v > 255) v = 0;
                if (v < 0)   v = 255;
                brightness = v;
                OLED_SetBrightness(brightness);
                rot_lock = 1;
            }
            else if (key == 1) { page = 5; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }

            OLED_Clear();
            OLED_ShowString(1, 1, "Brightness");
            sprintf(buf, "%3d", brightness);
            OLED_ShowString(2, 1, buf);
            OLED_ShowString(3, 1, "Rotate to set");
            OLED_ShowString(4, 1, "PA2 save");
        }
        /* ================= 页面9：倒计时 ================= */
        else if (page == 9)
        {
            if (key2 == 1) { page = 2; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }
            if (rot != 0)
            {
                int16_t v = (int16_t)timer_sec + rot * 5;
                if (v > 300) v = 5;
                if (v < 5)   v = 300;
                timer_sec = v;
                rot_lock = 1;
            }
            else if (key == 1)
            {
                timer_start = RTC_GetCounter();
                timer_running = 1;
                page = 2; rot_lock = 1; OLED_Clear(); OLED_Update(); continue;
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Countdown");
            sprintf(buf, "%3ds", timer_sec);
            OLED_ShowString(2, 1, buf);
            OLED_ShowString(3, 1, "Rotate to set");
            OLED_ShowString(4, 1, "PA2 start");
        }
        /* ================= 页面10：时间闹钟 ================= */
        else if (page == 10)
        {
            if (key2 == 1) { page = 2; rot_lock = 1; OLED_Clear(); OLED_Update(); continue; }

            if (rot != 0)
            {
                if (alarm_sub == 0) {
                    alarm_time_h += rot;
                    if (alarm_time_h >= 24) alarm_time_h = 0;
                    if (alarm_time_h < 0)   alarm_time_h = 23;
                } else {
                    alarm_time_m += rot;
                    if (alarm_time_m >= 60) alarm_time_m = 0;
                    if (alarm_time_m < 0)   alarm_time_m = 59;
                }
                rot_lock = 1;
            }
            else if (key == 1)
            {
                if (alarm_sub == 0) { alarm_sub = 1; }
                else {
                    alarm_time_en = 1;
                    page = 2; rot_lock = 1;
                    OLED_Clear(); OLED_Update(); continue;
                }
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Set Alarm:");
            sprintf(buf, "%02d:%02d", alarm_time_h, alarm_time_m);
            OLED_ShowString(2, 3, buf);
            if (alarm_sub == 0) OLED_ShowString(3, 1, "Editing Hour");
            else                OLED_ShowString(3, 1, "Editing Min ");
            if (alarm_time_en)  OLED_ShowString(4, 1, "Enabled");
            else                OLED_ShowString(4, 1, "Disabled");
        }
        /* ================= 页面11：计步 ================= */
        else if (page == 11)
        {
            if (key == 1 || key2 == 1 || rot != 0)   // 任意键返回
            {
                page = 1; rot_lock = 1;
                OLED_Clear(); OLED_Update(); continue;
            }

            OLED_Clear();
            OLED_ShowString(1, 1, "Step Counter");
            sprintf(buf, "%5d", step_count);
            OLED_ShowString(2, 5, buf);   // 大号数字居中
            sprintf(buf, "Today:%04d-%02d-%02d", year, month, day);
            OLED_ShowString(3, 1, buf);
            OLED_ShowString(4, 1, "Auto clear daily");
        }

        OLED_Update();
        for (int i = 0; i < 10000; i++);
    }
}