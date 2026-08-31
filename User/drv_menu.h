#ifndef __DRV_MENU_H
#define __DRV_MENU_H

#include "stdio.h"
#include "oled.h"
#include "drv_config.h"
#include "PD_Process.h"

#define MENU_COUNT          8

/*配置参数*/
#define MENU_PANEL_WIDTH (128)      // 菜单面板宽度
#define MENU_PANEL_HEIGHT (64)      // 菜单面板高度
#define MENU_ITEM_XOFFSET (4)       // 菜单项X轴偏移
#define MENU_ITEM_HEIGHT (8)       // 菜单项高度
#define MENU_FONT_WIDTH (8)         // 字体宽度
#define MENU_FONT_HEIGHT (16)       // 字体高度
#define MENU_ANIMATION_SPEED (0.2f) // 动画速度[0,1], 为1时无动画效果
#define MENU_TITLE (1)              // 是否显示标题
#define ITEMS_PER_PAGE (MENU_PANEL_HEIGHT / MENU_FONT_HEIGHT)

/*移植结束*/

#define MENU_EVENT_ENTER 1
#define MENU_EVENT_BACK 2
#define MENU_EVENT_WHEEL_UP 3
#define MENU_EVENT_WHEEL_DOWN 4

#define MENU_ITEM_CMD_NAME 0  // 读名
#define MENU_ITEM_CMD_ENTER 1 // 执行
#define MENU_ITEM_CMD_BACK 2  // 退出
#define MENU_ITEM_CMD_COUNT 3 // 获取项数量

#define MENU_CURSOR_TYPE_NORMAL 0 // 正常
#define MENU_CURSOR_TYPE_EDIT 1   // 编辑光标
#define MENU_CURSOR_TYPE_NONE 2   // 无光标


typedef struct _MenuItemTypeStru
{
    char *name;
    void (*func)(void);
} MenuItemTypeStru;

typedef struct _MenuItemTypeList
{
    struct _MenuItemTypeList *next;
    char *name;
    void (*func)(void);
} MenuItemTypeList;

typedef struct _Menu
{
    struct _Menu *next;
    void *items;
    
    int16_t item_count;
    int16_t item_index;

    int16_t cursor_width;
    int8_t cursor_limit;
    int8_t cursor_index;
} Menu;

extern uint8_t menu_activated,menu1_activated;
extern int8_t item_selected,menu1_item_selected;

void Show_Menu(uint8_t page);
void Enter_Menu(void);
void Exit_Menu(void);
void Process_Menu(uint8_t event,int16_t diff);

#endif