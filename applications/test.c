/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-14     Jasmine       the first version
 */
//static void svc_lcd_spi_hw_init(void)
//{
//    HPM_IOC->PAD[IOC_PAD_PA30].FUNC_CTL = IOC_PA30_FUNC_CTL_GPIO_A_30;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOA,LCD_SCK_PIN,gpiom_soc_gpio0);
//    gpio_set_pin_output(LCD_SCK_GPIO_CTRL,LCD_SCK_GPIO_OE,LCD_SCK_PIN);
//
//    HPM_IOC->PAD[IOC_PAD_PA31].FUNC_CTL=IOC_PA31_FUNC_CTL_GPIO_A_31;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOA,LCD_SDA_PIN,gpiom_soc_gpio0);
//    gpio_set_pin_output(LCD_SDA_GPIO_CTRL,LCD_SDA_GPIO_OE,LCD_SDA_PIN);
//
//    gpio_write_pin(LCD_CLK_GPIO_CTRL,LCD_CLK_GPIO_INDEX,LCD_CLK_PIN,1);
//    gpio_wirte_pin(LCD_DAT_GPIO_CTEL,LCD_DAT_GPIO_INDEX,LCD_DAT_PIN,0);
//
//}
//
//
//static void svc_lcd_ctrl_pins_init(void)
//{
//    //PA03
//    HPM_IOC->PAD[IOC_PAD_PA03].FUNC_CTL = IOC_PA03_FUNC_CTL_GPIO_A_03;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOA,LCD_RSTB_PIN,gpiom_soc_gpio0);
//    gpio_set_pin_output(LCD_RSTB_GPIO_CTRL,LCD_RSTB_GPIO_OE,LCD_RSTB_PIN);
//
//    //PC16
//    HPM_IOC->PAD[IOC_PAD_PC16].FUNC_CTL = IOC_PC16_FUNC_CTL_GPIO_C_16;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOC,LCD_BACKLIGHT_PIN,gpiom_soc_gpio0);
//    gpio_set_pin_output(LCD_BACKLIGHT_GPIO_CTRL,LCD_BACKLIGHT_GPIO_OE,LCD_BACKLIGHT_PIN);
//
//    //PA28
//    HPM_IOC->PAD[IOC_PAD_PA28].FUNC_CTL = IOC_PA28_FUNC_CTL_GPIO_A_28;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOA,LCD_CSN_PIN,gpiom_soc_gpio0);
//    gpiom_set_pin_output(LCD_CSN_GPIO_CTRL,LCD_CSN_GPIO_OE,LCD_CSN_PIN);
//
//    //PA29
//    HPM_IOC->PAD[IOC_PAD_PA29].FUNC_CTL = IOC_PA29_FUNC_CTL_GPIO_A_29;
//    gpiom_set_pin_controller(HPM_GPIOM,GPIOM_ASSIGN_GPIOA,LCD_A0_PIN,gpiom_soc_gpio0);
//    gpio_set_pin_output(LCD_A0_GPIO_CTRL,LCD_A0_GPIO_OE,LCD_A0_PIN);
//
//}
//
//static void lcd_spi_delay_us(void)
//{
//    board_delay_us(1);
//}
//
//static void lcd_sck_set(rt_bool high)
//{
//    gpio_write_pin(LCD_CLK_GPIO_CTRL,LCD_CLK_GPIO_INDEX,LCD_CLK_PIN,high ? 1:0);
//}
//
//
//
//
//
