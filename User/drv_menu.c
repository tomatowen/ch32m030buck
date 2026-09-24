#include "drv_menu.h"


// 假设你的显存是 OLED_GRAM[8][128]，共 1024 字节
static uint8_t OLED_BgBuffer[8][128];
uint8_t menu_activated=0,menu1_activated=0;
int8_t item_selected=0,menu1_item_selected=0,menu1_item_max=0;
/* 菜单项文本 */
static const char *s_menu_items[MENU_COUNT] = {
    "LoadConfig",
    "SaveTo",
    "AutoRecovery",
    "BrakeRetry",
    "Vol_Offset",
    "Cur_Offset",
    "PD_Info",
    "Exit"
};


void menu1_show_config_list(uint8_t focused){
    uint8_t i;
    // 1. 计算当前聚焦项所在的页码 (从 1 开始)
    uint8_t page = (focused / ITEMS_PER_PAGE) + 1;
    
    // 2. 计算当前项在当前页内的相对索引 (0 ~ ITEMS_PER_PAGE-1)
    uint8_t itemnum = focused % ITEMS_PER_PAGE;
    
    // 3. 计算当前页的起始菜单项索引
    uint8_t start_index = (page - 1) * ITEMS_PER_PAGE;
    
    // 4. 计算当前页的结束菜单项索引（防止越界）
    uint8_t end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > menu1_item_max) {
        end_index = menu1_item_max;
    }
    
    OLED_Clear();
    
    // 5. 循环绘制当前页的项，Y 坐标应该根据“页内行号 (i - start_index)”来计算！
    for(i = start_index; i < end_index; i++){
        uint8_t row_line = i - start_index; // 页内第几行 (0, 1, 2...)

        if (i == menu1_item_max-1) {
             OLED_ShowString_fast(0, row_line * MENU_FONT_HEIGHT, "<---- Return", MENU_FONT_HEIGHT); // 或者显示 "<----"
        } else {
        
            OLED_ShowNum(0, row_line * MENU_FONT_HEIGHT, i + 1, 1, MENU_FONT_HEIGHT);
            OLED_ShowFloatNum(2 * MENU_FONT_WIDTH, row_line * MENU_FONT_HEIGHT, (float)config_buffer.VoltTarget[i] / 1000.0, 2, 3, MENU_FONT_HEIGHT);
            OLED_ShowFloatNum(9 * MENU_FONT_WIDTH, row_line * MENU_FONT_HEIGHT, (float)config_buffer.CurrentTarget[i] / 1000.0, 1, 3, MENU_FONT_HEIGHT);
        }
    }
    
    // 6. 高亮当前选中的行（直接用 itemnum 作为页内行号）
    OLED_ReverseArea(0, itemnum * MENU_FONT_HEIGHT, MENU_PANEL_WIDTH, MENU_FONT_HEIGHT);
}

void menu1_show_enable_list(uint8_t focused){
    OLED_Clear();
    OLED_ShowString_fast(0, 0 * MENU_FONT_HEIGHT, "disable", MENU_FONT_HEIGHT); 
    OLED_ShowString_fast(0, 1 * MENU_FONT_HEIGHT, "enable", MENU_FONT_HEIGHT); 
    OLED_ShowString_fast(0, 2* MENU_FONT_HEIGHT, "<---- Return", MENU_FONT_HEIGHT); // 或者显示 "<----"

    // 6. 高亮当前选中的行（直接用 itemnum 作为页内行号）
    OLED_ReverseArea(0, focused * MENU_FONT_HEIGHT, MENU_PANEL_WIDTH, MENU_FONT_HEIGHT);

}

void menu1_show_num(uint8_t focused, uint8_t rate){
     uint8_t i;
    // 1. 计算当前聚焦项所在的页码 (从 1 开始)
    uint8_t page = (focused / ITEMS_PER_PAGE) + 1;
    
    // 2. 计算当前项在当前页内的相对索引 (0 ~ ITEMS_PER_PAGE-1)
    uint8_t itemnum = focused % ITEMS_PER_PAGE;
    
    // 3. 计算当前页的起始菜单项索引
    uint8_t start_index = (page - 1) * ITEMS_PER_PAGE;
    
    // 4. 计算当前页的结束菜单项索引（防止越界）
    uint8_t end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > menu1_item_max) {
        end_index = menu1_item_max;
    }
    
    OLED_Clear();
    
    // 5. 循环绘制当前页的项，Y 坐标应该根据“页内行号 (i - start_index)”来计算！
    for(i = start_index; i < end_index; i++){
        uint8_t row_line = i - start_index; // 页内第几行 (0, 1, 2...)

        if (i == menu1_item_max-1) {
             OLED_ShowString_fast(0, row_line * MENU_FONT_HEIGHT, "<---- Return", MENU_FONT_HEIGHT); // 或者显示 "<----"
        } else {
        
            OLED_ShowNum(0, row_line * MENU_FONT_HEIGHT, (i)*rate, 3, MENU_FONT_HEIGHT);
        }
    }
    
    // 6. 高亮当前选中的行（直接用 itemnum 作为页内行号）
    OLED_ReverseArea(0, itemnum * MENU_FONT_HEIGHT, MENU_PANEL_WIDTH, MENU_FONT_HEIGHT);  
}


void menu1_show_pdcaps(uint8_t focused){
    uint8_t i;
    uint16_t voltage, current;

    // 1. 计算当前聚焦项所在的页码 (从 1 开始)
    uint8_t page = (focused / ITEMS_PER_PAGE) + 1;
    
    // 2. 计算当前项在当前页内的相对索引 (0 ~ ITEMS_PER_PAGE-1)
    uint8_t itemnum = focused % ITEMS_PER_PAGE;
    
    // 3. 计算当前页的起始菜单项索引
    uint8_t start_index = (page - 1) * ITEMS_PER_PAGE;
    
    // 4. 计算当前页的结束菜单项索引（防止越界）
    uint8_t end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > menu1_item_max) {
        end_index = menu1_item_max;
    }
    
    OLED_Clear();
    
    // 5. 循环绘制当前页的项，Y 坐标应该根据“页内行号 (i - start_index)”来计算！
    for(i = start_index; i < end_index; i++){
        uint8_t row_line = i - start_index; // 页内第几行 (0, 1, 2...)
        PD_PDO_Analyse(i+1,&Adapter_SrcCap[1],&current,&voltage);

        if (i == menu1_item_max-1) {
             OLED_ShowString_fast(0, row_line * MENU_FONT_HEIGHT, "<---- Return", MENU_FONT_HEIGHT); // 或者显示 "<----"
        } else {
        
            OLED_ShowNum(0, row_line * MENU_FONT_HEIGHT, i + 1, 1, MENU_FONT_HEIGHT);
            OLED_ShowFloatNum(2 * MENU_FONT_WIDTH, row_line * MENU_FONT_HEIGHT, (float)voltage / 1000.0, 2, 1, MENU_FONT_HEIGHT);
            OLED_ShowFloatNum(9 * MENU_FONT_WIDTH, row_line * MENU_FONT_HEIGHT, (float)current / 1000.0, 1, 1, MENU_FONT_HEIGHT);
        }
    }
    
    // 6. 高亮当前选中的行（直接用 itemnum 作为页内行号）
    OLED_ReverseArea(0, itemnum * MENU_FONT_HEIGHT, MENU_PANEL_WIDTH, MENU_FONT_HEIGHT);
}

void Show_Menu(uint8_t focused) {
    uint8_t i = 0;
    // 1. 计算当前聚焦项所在的页码 (从 1 开始)
    uint8_t page = (focused / ITEMS_PER_PAGE) + 1;
    
    // 2. 计算当前项在当前页内的相对索引 (0 ~ ITEMS_PER_PAGE-1)
    uint8_t itemnum = focused % ITEMS_PER_PAGE;
    
    // 3. 计算当前页的起始菜单项索引
    uint8_t start_index = (page - 1) * ITEMS_PER_PAGE;
    
    // 4. 计算当前页的结束菜单项索引（防止越界）
    uint8_t end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > MENU_COUNT) {
        end_index = MENU_COUNT;
    }

    OLED_Clear();
   
    // 5. 刷新当前页的菜单项
    for (i = start_index; i < end_index; i++) {
        // 计算在屏幕上的显示 Y 坐标
        uint8_t y_pos = (i - start_index) * MENU_FONT_HEIGHT;
        OLED_ShowString_fast(0, y_pos, (char *)s_menu_items[i], MENU_FONT_HEIGHT);
    }
    
    // 6. 高亮当前选中的行
    OLED_ReverseArea(0, itemnum * MENU_FONT_HEIGHT, MENU_PANEL_WIDTH, MENU_FONT_HEIGHT);
}

void Show_Menu1(uint8_t focused){
    switch(item_selected){
        case 0:
        case 1:
            menu1_show_config_list(focused);
            break;
        case 2:
            menu1_show_enable_list(focused);
            break;
        case 3:
            menu1_show_num(focused,1);
            break;
        case 4:
        case 5:
            menu1_show_num(focused,10);
            break;
            
        case 6:
            menu1_show_pdcaps(focused);
        default:
    }
    
}

void Exit_Menu1(void){
    menu1_activated = 0;
    menu1_item_selected = 0;
    menu1_item_max = 0;
    Show_Menu(item_selected);
}

void Enter_Menu(void) {
    // 将当前屏幕显存完整备份到后台
    Backup_GRAM(&OLED_BgBuffer[0][0]);
    Show_Menu(0);
        
    // 切换到你的菜单状态
    menu_activated = 1;
    menu1_activated = 0;
    item_selected = 0;
}

void Exit_Menu(void) {
    // 将备份恢复到显存
    Restore_GRAM(&OLED_BgBuffer[0][0]);;
    
    // 立即刷新到屏幕硬件
    //OLED_Refresh(); // 或者 OLED_Refresh();
    
    // 切换回主界面状态
    menu_activated = 0;
    menu1_activated = 0;
    item_selected = 0;
}

/*
event: Button signle click: 1, double click: 2, long click 3, encoder: 4
diff: encoder diffence with previous value
*/
void Process_Menu(uint8_t event,int16_t diff){
    if(menu_activated){
        switch(event)
        {
            case 1 :
                if(menu1_activated){
                    switch(item_selected){
                        case 0:
                        //最后一行是返回，其他是加载存储的电压电流
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.selected = menu1_item_selected;
                                Load_Buffer_V_I();
                            }
                            Exit_Menu1();
 
                            break;
                        case 1:
                            //最后一行是返回，其他则是存储当前设置的电压和电流
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.VoltTarget[menu1_item_selected] = ADC_GetTargetOutputVoltage_mv();
                                config_buffer.CurrentTarget[menu1_item_selected] = ADC_GetTargetCurrent_mA();
                                CfgUpdateToFlashFunc();
                            }
                            Exit_Menu1();

                            break;
                        case 2:
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.autorecovery = menu1_item_selected;                            
                            }
                            Exit_Menu1();
                        case 3:
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.brakecount = (menu1_item_selected);                            
                            }
                            Exit_Menu1();
                        case 4:
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.voltage_offset = (menu1_item_selected)*10;                            
                            }
                            Exit_Menu1();
                        case 5:
                            if(menu1_item_selected != menu1_item_max-1)
                            {
                                config_buffer.current_offset = (menu1_item_selected)*10;                            
                            }
                            Exit_Menu1();

                        break; 
                        case 6:
                            Exit_Menu1();                           
                    }

                }else{
                    switch(item_selected)
                    {
                        case 0:
                        case 1:
                            menu1_item_max = ConfigVoltageCount+1;
                            menu1_item_selected = 0;
                            menu1_activated = 1; 
                            Show_Menu1(menu1_item_selected);
                            break;
                        case 2:
                            menu1_item_max = 3;
                            menu1_item_selected =config_buffer.autorecovery;
                            menu1_activated = 1;
                            Show_Menu1(menu1_item_selected);
                            break;
                        case 3:
                            menu1_item_max = 12;
                            menu1_item_selected = config_buffer.brakecount;
                            menu1_activated = 1;
                            Show_Menu1(menu1_item_selected);
                            break;
                        case 4:
                            menu1_item_max = 12;
                            menu1_item_selected = config_buffer.voltage_offset/10;
                            menu1_activated = 1;
                            Show_Menu1(menu1_item_selected);
                            break;
                        case 5:
                            menu1_item_max = 12;
                            menu1_item_selected = config_buffer.current_offset/10;
                            menu1_activated = 1;
                            Show_Menu1(menu1_item_selected);
                            break;
                        case 6:
                            menu1_item_max = Adapter_SrcCap[0]+1;
                            menu1_item_selected = 0;
                            menu1_activated = 1;
                            Show_Menu1(menu1_item_selected);
                            break;                           
                        case 7:
                            Exit_Menu();
                            break;
                    }
                    
                }
                break;
            case 2 :
                break;
            case 3 :
                break;
            case 4:

                if (diff != 0) {
                    if(menu1_activated){
                        if (diff > 0) {
                            menu1_item_selected++;
                            if (menu1_item_selected >= menu1_item_max) {
                                menu1_item_selected = 0; // 循环到开头
                            }
                        } else {
                            menu1_item_selected--;
                            if (menu1_item_selected < 0) {
                                menu1_item_selected = menu1_item_max - 1; // 循环到结尾
                            }
                        }

                        // 2. 检查当前 item_selected 是否超出了当前显示的“页”，如果跨页了就重绘整页
                        // 你也可以根据需求，选择每次移动都直接调用 Show_Menu(item_selected) 刷新整页，
                        // 这样最省心且不会出现光标和背景不同步的问题：
                        Show_Menu1(menu1_item_selected);

                    }else{
                        //printf("diff: %d\r\n",diff);
                        // 1. 根据 diff 调整选中的项
                        if (diff > 0) {
                            item_selected++;
                            if (item_selected >= MENU_COUNT) {
                                item_selected = 0; // 循环到开头
                            }
                        } else {
                            item_selected--;
                            if (item_selected < 0) {
                                item_selected = MENU_COUNT - 1; // 循环到结尾
                            }
                        }

                        // 2. 检查当前 item_selected 是否超出了当前显示的“页”，如果跨页了就重绘整页
                        // 你也可以根据需求，选择每次移动都直接调用 Show_Menu(item_selected) 刷新整页，
                        // 这样最省心且不会出现光标和背景不同步的问题：
                        Show_Menu(item_selected);
                    }
                }

                
                break;
            default:
                break;
        }
    }

}
