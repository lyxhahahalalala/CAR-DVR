#ifndef APPLICATIONS_APP_CONFIG_H_
#define APPLICATIONS_APP_CONFIG_H_

/* 绾跨▼閰嶇疆 */
#define APP_LED_TASK_NAME            "led_th"
#define APP_LED_TASK_STACK_SIZE      2048
#define APP_LED_TASK_PRIORITY        10
#define APP_LED_TASK_TICK            10

#define APP_ADC_TASK_NAME            "adc_th"
#define APP_ADC_TASK_STACK_SIZE      4096
#define APP_ADC_TASK_PRIORITY        11
#define APP_ADC_TASK_TICK            10

#define APP_POWER_TASK_NAME          "power_th"
#define APP_POWER_TASK_STACK_SIZE    2048
#define APP_POWER_TASK_PRIORITY      8
#define APP_POWER_TASK_TICK          10

#define APP_CAN_TASK_NAME            "can_th"
#define APP_CAN_TASK_STACK_SIZE      2048
#define APP_CAN_TASK_PRIORITY        9
#define APP_CAN_TASK_TICK            10
#define APP_CAN_DEV_NAME             BOARD_CAN_NAME
#define APP_CAN_BAUDRATE             CAN250kBaud
#define APP_CAN_RX_THREAD_NAME       "can_rx"
#define APP_CAN_RX_THREAD_STACK      3072
#define APP_CAN_RX_THREAD_PRIORITY   18
#define APP_CAN_RX_THREAD_TICK       20
#define APP_CAN_TX_THREAD_NAME       "can_tx"
#define APP_CAN_TX_THREAD_STACK      2048
#define APP_CAN_TX_THREAD_PRIORITY   19
#define APP_CAN_TX_THREAD_TICK       20
#define APP_CAN_ERR_THREAD_NAME      "can_err"
#define APP_CAN_ERR_THREAD_STACK     2048
#define APP_CAN_ERR_THREAD_PRIORITY  20
#define APP_CAN_ERR_THREAD_TICK      20
#define APP_CAN_TX_MQ_NAME           "can_txq"
#define APP_CAN_TX_MQ_DEPTH          16
#define APP_CAN_TEST_TX_ENABLE        1
#define APP_CAN_TEST_TX_THREAD_NAME   "can_ttx"
#define APP_CAN_TEST_TX_THREAD_STACK  2048
#define APP_CAN_TEST_TX_THREAD_PRIORITY 21
#define APP_CAN_TEST_TX_THREAD_TICK   20
#define APP_CAN_TEST_TX_PERIOD_MS     1000
#define APP_CAN_TEST_TX_ID            0x123UL

#define APP_DEBUG_CAN_ONLY           0

#if APP_DEBUG_CAN_ONLY
#define APP_NON_CAN_LOG(...)         ((void)0)
#else
#define APP_NON_CAN_LOG(...)         rt_kprintf(__VA_ARGS__)
#endif

#define APP_IO_TASK_NAME             "vehio_th"
#define APP_IO_TASK_STACK_SIZE       2048
#define APP_IO_TASK_PRIORITY         12
#define APP_IO_TASK_TICK             10

#define APP_STORAGE_TASK_NAME        "store_th"
#define APP_STORAGE_TASK_STACK_SIZE  2048
#define APP_STORAGE_TASK_PRIORITY    13
#define APP_STORAGE_TASK_TICK        10

#define APP_LCD_TASK_NAME            "lcd_th"
#define APP_LCD_TASK_STACK_SIZE      2048
#define APP_LCD_TASK_PRIORITY        14
#define APP_LCD_TASK_TICK            10

/* LED 娴嬭瘯鍙傛暟 */
#define APP_LED_TOGGLE_PERIOD_MS     500

/* ADC 璁惧涓庨€氶亾閰嶇疆 */
#define APP_ADC_DEV_NAME             "adc0"
#define APP_ADC_CH_BAT_24V           2
#define APP_ADC_CH_LI_BAT_4V2        3
#define APP_ADC_CH_SUPER_C_5V        4
#define APP_ADC_CH_KEY               11
#define APP_ADC_SAMPLE_PERIOD_MS     1000
#define APP_ADC_STARTUP_DELAY_MS     1000

/* ADC 浼扮畻鐢靛帇鍙傛暟
 * 褰撳墠鎸?16bit 婊￠噺绋嬪拰 3.3V 鍙傝€冪數鍘嬩及绠楋紝鍚庣画鍙牴鎹疄娴嬪啀鏍″噯銆?
 */
#define APP_ADC_FULL_SCALE           65535UL
#define APP_ADC_VREF_MV              3300UL

/* BAT24 鍒嗗帇锛?M / 91K锛岃緭鍏ョ數鍘?= ADC鑴氱數鍘?* (1000K + 91K) / 91K */
#define APP_ADC_BAT24_DIV_UP_KOHM    1000UL
#define APP_ADC_BAT24_DIV_DOWN_KOHM  91UL

/* LI_BAT 鍒嗗帇锛?1K / 91K锛岃緭鍏ョ數鍘?= ADC鑴氱數鍘?* 2 */
#define APP_ADC_LI_BAT_DIV_UP_KOHM   91UL
#define APP_ADC_LI_BAT_DIV_DOWN_KOHM 91UL

/* 鐢垫簮绠＄悊鍙傛暟銆?
 * 杩欎竴缁勫弬鏁板叏閮ㄦ湇鍔′簬 svc_power 鐘舵€佹満锛?
 * 鐩殑鏄妸鈥滀富鐢垫槸鍚﹀瓨鍦ㄣ€佽秴绾х數瀹规槸鍚﹀彲鐢ㄣ€佷粈涔堟椂鍊欒繘鍏ュ叧鏈哄噯澶囥€佷粈涔堟椂鍊欐墽琛屾渶缁堟柇鐢碘€?
 * 杩欎簺绛栫暐缁熶竴鏀跺彛鍒伴厤缃枃浠朵腑锛岄伩鍏嶅悗缁埌涓氬姟浠ｇ爜閲屽埌澶勬壘榄旀硶鏁板瓧銆?
 *
 * 杩欓噷鏈変竴涓幇瀹炲墠鎻愶細
 * 褰撳墠 SUPER 鐩稿叧鎵撳嵃鍊艰繕鏄寜鐜版湁 ADC 鏄剧ず鍙ｅ緞鍦ㄧ敤锛?
 * 涔熷氨鏄畠杩樻病鏈夊畬鍏ㄦ崲绠楁垚鏈€缁堢湡瀹炴瘝绾跨數鍘嬨€?
 * 鎵€浠ュ悗闈㈠鏋滀綘淇浜?SUPER 鐨勫垎鍘嬫崲绠楋紝
 * 涓嬮潰杩欎簺 SUPER 闃堝€艰璺熺潃涓€璧疯皟鏁淬€?
 */

/* 鍒ゆ柇鈥滀富鐢靛凡缁忕ǔ瀹氬瓨鍦ㄢ€濈殑闂ㄩ檺銆?
 * 浣庝簬杩欎釜鍊兼椂锛屼笉璁や负 BAT24 鏄湁鏁堜富鐢点€?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅鍒氫笂鐢点€佹帀鐢佃竟缂樸€佺數鍘嬫姈鍔ㄦ椂璇垽涓荤數瀛樺湪鐨勯棶棰樸€?
 */
#define APP_PWR_MAIN_PRESENT_THRESHOLD_MV          18000UL

/* 鍒ゆ柇鈥滆秴绾х數瀹瑰凡缁忓厖鍒板彲鍙備笌鎺夌數淇濇寔鈥濈殑闂ㄩ檺銆?
 * 鍙湁鍏呭埌杩欎釜鑼冨洿锛屾墠鎶?supercap_ready 缃负 1銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅鈥滃垰涓婄數鏃惰秴瀹硅繕娌″厖婊★紝鍗磋璇綋鎴愬彲鏀拺鎺夌數鏀跺熬鈥濈殑闂銆?
 */
#define APP_PWR_SUPERCAP_READY_THRESHOLD_MV        2100UL

/* 鍒ゆ柇鈥滀富鐢垫帀鐢垫椂锛岃秴绾х數瀹硅繕澶熻繘鍏ヤ繚鎸侀樁娈碘€濈殑闂ㄩ檺銆?
 * 褰撳墠鍜?READY 闃堝€间繚鎸佷竴鑷达紝鍏堜繚璇侀€昏緫绠€鍗曞彲楠岃瘉銆?
 * 鍚庨潰濡傛灉闇€瑕侊紝涔熷彲浠ユ妸 READY 鍜?HOLD 闃堝€兼媶鎴愪笉鍚岀瓑绾с€?
 */
#define APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV         2100UL

/* 鍒ゆ柇鈥滆秴绾х數瀹瑰凡缁忎綆鍒颁笉閫傚悎缁х画鎷栧欢锛屽繀椤绘帹杩涘叧鏈衡€濈殑闂ㄩ檺銆?
 * 浣庝簬杩欎釜鍊煎悗锛屼笉鍐嶇户缁暱鏃堕棿淇濇寔锛岃€屾槸杩涘叆 SHUTDOWN_PENDING銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅瓒呭鐢甸噺缁х画琚嫋绌恒€佸鑷存渶缁堟柇鐢佃繃鏅氱殑闂銆?
 */
#define APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV     1800UL

/* 浠庤繘鍏ヨ秴绾х數瀹逛繚鎸佸紑濮嬶紝鍒拌繘鍏ュ叧鏈哄噯澶囩殑鏈€闀跨瓑寰呮椂闂淬€?
 * 鍗充娇瓒呭杩樻病鏄庢樉鎺夊埌浣庡帇闃堝€硷紝瓒呰繃杩欎釜鏃堕棿涔熻鎺ㄨ繘鍏虫満銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅绯荤粺涓€鐩存寕鍦ㄤ繚鎸侀樁娈典笉鏀跺熬鐨勯棶棰樸€?
 */
#define APP_PWR_HOLD_PREPARE_TIMEOUT_MS            10000UL

/* 浠?SHUTDOWN_PENDING 鎺ㄨ繘鍒?SHUTDOWN_IN_PROGRESS 鐨勬椂闂淬€?
 * 杩欐鏃堕棿鐩稿綋浜庣暀缁欎笂灞傚仛鈥滄渶鍚庡噯澶囧姩浣溾€濈殑绐楀彛銆?
 * 鐩墠 SoC 鍏虫満璇锋眰杩樻病鎺ワ紝杩欓噷鍏堜綔涓烘椂搴忛鏋朵繚鐣欍€?
 */
#define APP_PWR_SHUTDOWN_IN_PROGRESS_MS            3000UL

/* 鎺夌數杩囩▼涓鏋滀富鐢电獊鐒舵仮澶嶏紝鍏堢煭寤舵椂鍐嶈蒋浠跺浣嶃€?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅鈥滆秴瀹规病鏀剧┖鍙堥噸鏂颁笂鐢碉紝绯荤粺鍗″湪鍗婃帀鐢电姸鎬佲€濈殑闂銆?
 */
#define APP_PWR_RECOVERY_RESET_DELAY_MS            50UL

/* 涓荤數鎭㈠纭鏃堕棿銆?
 * 瑕佽繛缁弧瓒宠繖涔堜箙锛屾墠璁ゅ畾涓荤數鐪熺殑鎭㈠绋冲畾銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅杞︿笂鐢垫簮鎶栧姩銆佹彃鎷旂灛鎬侀€犳垚鐨勮鎭㈠闂銆?
 */
#define APP_PWR_MAIN_PRESENT_CONFIRM_MS            1000UL

/* 涓荤數涓㈠け纭鏃堕棿銆?
 * 瑕佽繛缁帀鍒伴槇鍊间互涓嬭繖涔堜箙锛屾墠璁ゅ畾涓荤數鐪熺殑娌′簡銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅鐬椂璺岃惤銆侀噰鏍锋姈鍔ㄥ鑷磋瑙﹀彂鎺夌數娴佺▼鐨勯棶棰樸€?
 */
#define APP_PWR_MAIN_LOSS_CONFIRM_MS               1000UL

/* 瓒呯骇鐢靛 ready 纭鏃堕棿銆?
 * 瑕佽繛缁弧瓒宠繖涔堜箙锛屾墠鎶?supercap_ready 缃负 1銆?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅瓒呭鍏呯數杩囩▼涓數鍘嬪垰纰板埌闃堝€煎氨琚繃鏃╁垽涓?ready 鐨勯棶棰樸€?
 */
#define APP_PWR_SUPERCAP_READY_CONFIRM_MS          3000UL

/* 瓒呯骇鐢靛浣庡帇纭鏃堕棿銆?
 * 瑕佽繛缁綆浜庝綆鍘嬮槇鍊艰繖涔堜箙锛屾墠璁ゅ畾瀹冪湡鐨勪綆浜嗐€?
 * 杩欐牱鍋氭槸涓轰簡瑙ｅ喅鎺夌數鏈湡閲囨牱娉㈠姩瀵艰嚧鍏虫満鐘舵€佹潵鍥炴姈鍔ㄧ殑闂銆?
 */
#define APP_PWR_SUPERCAP_LOW_CONFIRM_MS            2000UL

/* 0: disable supercap hold state machine, 1: enable */
#define APP_PWR_SUPERCAP_MGMT_ENABLE               0

/* 杩涘叆 SHUTDOWN_IN_PROGRESS 鍚庯紝鍏堢瓑寰呰繖涔堜箙鍐嶅垏 SoC/24V/瓒呭鍏呯數杈撳嚭銆?
 * 杩欐牱鍋氭槸涓轰簡璁╁叧鏈哄悗鍗婄▼鏈夋竻鏅扮殑涓ゆ鍔ㄤ綔锛岃€屼笉鏄竴杩涙渶缁堥樁娈靛氨鍏ㄩ儴鍚屾椂鎷夋帀銆?
 */
#define APP_PWR_FINAL_SOC_CUT_DELAY_MS             1000UL

/* 鍒囧畬 SoC/24V 鐩稿叧杈撳嚭鍚庯紝鍐嶇瓑杩欎箞涔呴噴鏀?MCU 鑷韩淇濇寔銆?
 * 杩欐牱鍋氭槸涓轰簡淇濊瘉 MCU 鏈夋渶鍚庝竴鐐规椂闂村畬鎴愬熬閮ㄧ姸鎬佹帹杩涳紝鍐嶇湡姝ｆ妸鑷繁涔熸柇鎺夈€?
 */
#define APP_PWR_FINAL_HOLD_CUT_DELAY_MS            1500UL

/* 绌洪鏋剁嚎绋嬫墦鍗板懆鏈?*/
#define APP_POWER_TASK_PERIOD_MS     1000
#define APP_CAN_TASK_PERIOD_MS       1200
#define APP_IO_TASK_PERIOD_MS        1400
#define APP_STORAGE_TASK_PERIOD_MS   1600
#define APP_LCD_TASK_PERIOD_MS       1800

#endif /* APPLICATIONS_APP_CONFIG_H_ */
