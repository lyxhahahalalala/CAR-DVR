#include <rtthread.h>

#include "board.h"
#include "hpm_gpio_drv.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_vehicle_io.h"

/*
 * 閺夆晜鐟ら崬銈夊棘閸ワ附顐介柡鍕靛灠缂嶅宕?MCU 濞撴皜鍛暠闁汇垹鐏氱花顔剧不閿涘嫭鍊為柡宥囶焾缁洪箖濡? *
 * 閻庣懓鍟紞瀣礈瀹ュ熆鎺楀礃瀹曞洦鐣卞☉鎾存椤╋箓姊婚鈧。浠嬪嫉?5 濞戞搩浜风槐?
 * 1. 闁告帇鍊栭弻鍥ㄧ▔閼姐倖鏆?BAT24 闁哄嫷鍨伴幆渚€鎯囬悢鐑樼暠閻庢稒锚濠€顏堟晬瀹€鍐ｅ亾鐏炶偐鐟濋柡鍕靛灥椤箓鎯夐浣诡槯闁硅埖鐗曟慨鈺冩嫚椤栨艾鐏查柕? * 2. 闁告帇鍊栭弻鍥╂惥閸涱垶鐛撻柣銏ゆ涧椤旀劙寮伴姘剨鐎规瓕灏欑划锟犲礂閸涱厼鐓傞悺鎺戝暱椤у嫰寮ㄩ娑欏闁瑰搫顦遍弫鎼佸绩鐠鸿櫣鍟查柕? * 3. 濞戞捁宕甸弫鎼佸箳婢跺本鏆╅柛姘嚱缁辨繄鎷嬮埡鍐厙缂備胶鍠愮€垫粓鍨惧鈧换姘跺箰?-> 闁稿繗娅曞┃鈧柛鎴濇椤?-> 闁哄牃鍋撶紓浣哥墛閺屽洭鎮界喊妯峰亾濠靛鈧孩鎯旇箛鏂胯吂閺夆晜绋忛埀? * 4. 闁革负鍔忕粔瀵糕偓鍦攰缁绘洖鈻介埄鍐╂澒缂佸瞼鍎ゅ鍌涗繆閸屾稓浜☉鎾瑰吹閺佹悂宕ｉ崼鐔跺垝濠㈣泛绋勭槐婵囩▔鐠囨彃袟濠㈣泛绉崇紞鍛存晬瀹€鍕級闁稿繐绉村畷閬嶅捶閵娿儱纾归柟鍝勵槺閺佹悂鎮╅懜纰樺亾娴ｇ鍋? * 5. 闁革负鍔嶅〒鍓佺磼閸繂褰犻柡鍫熸そ濡礁鈻撻崹顐㈢樆濡炪倕鎼花顓㈠箯婢跺绉甸柣銏犵仛缁噣骞掕閸╂鎳樺鍓х闁兼澘濂旂粭澶愬及椤栨瑧顏遍柛娆欑稻閻ㄧ敻宕楅妸鈺佸姤闁瑰嘲顦扮敮鈧柕? *
 * 鐟滅増鎸告晶鐘虫交濞嗘垵顣奸弶鈺偵戦惀鍛村嫉婢跺澶?SoC 闁告绻愰幃鎾诲礂閾忣偅绨氶悹鍥敱閻即濡? * 濞戞梻鍠庡銊╁及椤栨繍鍤涢柨娑樿嫰閻ｇ娀鎮抽弶鎸庤含濞戞挻妲掗々锔锯偓鐟版湰閸ㄦ岸鎯冮崟顒佇﹂柨? * 闁炽儲绮庨弫绋库攦閹邦喖笑闁诡兛绀侀崹浠嬪棘?+ 闁瑰搫顦遍弫鍛婄┍濠靛洤鐦?+ 闁哄牃鍋撶紓浣哥墢閳ユ牗绂掗懜鍨劷闁汇垽娼ф慨鈺傛媴濠ч敮鍋撳┑鍕ㄥ亾? */

/* 濞戞挸顑夊鐗堟交?4 濞戞搩浜滈悾顖溾偓瑙勭煯缁犵喖鎯冮崟顒佇﹂柡鍫墯閺嬪啯绂掗幆閭︽矗闁瑰灝绉崇紞鏃堟儍閸曨厽鏆╂繝褎鍔栫敮鍫曞礆閹澘澹栭柕?*/

/* 闁汇垹鐏氱花顕€骞掕閸╂绱掗悢鍓侇伇闂侇喛濮ょ€垫洟宕?GPIO0闁?*/
#define SVC_POWER_CTRL_GPIO              HPM_GPIO0

/* 閺夆晜鐟ょ花娲箳瑜嶉崺妤呮嚇濮樿泛鍘撮柛?GPIOC 閺夊牊鎸搁崵顓㈠极閻楀牆绁﹂悗闈涘閻°劑宕抽妸銈囩憪闁?*/
#define SVC_POWER_CTRL_GPIO_INDEX        GPIO_DO_GPIOC

/* PC11 -> PWR_24V_EN闁?*/
#define SVC_POWER_CTRL_PWREN_24V_PIN     11

/* PC12 -> PWR_SOC_EN闁?*/
#define SVC_POWER_CTRL_PWREN_SOC_PIN     12

/* PC13 -> MCU_SUPER_C_CHRG闁?*/
#define SVC_POWER_CTRL_SUPERCAP_CHRG_PIN 13

/* PC14 -> MCU_PWR_HOLD闁?*/
#define SVC_POWER_CTRL_PWR_HOLD_PIN      14

/*
 * 闁汇垹鐏氱花顕€鎮╅懜纰樺亾娴ｈ绨氶梻鍐煐椤斿瞼鈧鐭粻鐔煎Υ? *
 * 閺夆晜鐟╅崳鐑藉极閸涱喖澹堥柟璺猴躬濡礁鈻撻崹顐㈩€曠€电増顨嗛惁顔芥綇閸愨晝顏告俊銈嗙啲缁辨繈寮伴娆掔濞存粌妫滈鈧☉鎾村絻瑜版盯骞嶉幘鍐茬オ闁告粌鑻幃妤冪磼椤擄紕娈堕悹鍥ㄦ礃濞插潡鎯勭壕瀣垫綆闁? * 1. 婵繐绲介悥鑸垫交閹邦垼鏀介柟? * 2. 閻℃帒鎳庨鎰┍濠靛洤鐦柟? * 3. 闁稿繗娅曞┃鈧柛鎴濇椤︻剟骞€? * 4. 闁哄牃鍋撶紓浣哥墛閺屽洭鎮介崹顐熷亾? *
 * 閺夆晜鐟﹂悧閬嶅触鎼淬劍妗ㄩ柛妤€鍘栨繛鍥礃瀹ュ棗澶?SoC 闁稿繗娅曞┃鈧悹鍥敱閻即鏁嶇仦鑲╃槏闁哄牆顦板Σ鎴犳兜椤旇棄鐦弶鐐村灊缂嶅懐绱旈琛″亾? */
typedef enum
{
    /* 闁告帗绻傞～鎰板嫉椤忓棛鍙€闁诡兛闄嶉埀?*/
    SVC_POWER_STAGE_UNKNOWN = 0,

    /* 鐟滅増鎸告晶鐘诲籍閵忥紕姊鹃柡鍫濐槷鐎靛矂鎮界喊澶岀濞戞梻鍠愰惀鍛村嫉婢跺鍔€鐎殿喖绻楃换姗€宕楅妷锕€绔撮柣銏犵仛缁侊妇绮欑€ｃ劉鍋?*/
    SVC_POWER_STAGE_MAIN_OFF,

    /* 濞戞捁宕甸弫鎼佸捶閵娧冩疇闁挎稑濂旂徊?ACC/ON 闂侇喛濮ゅ﹢顓炩攽閳ь剙煤濮瑰洠鍋?*/
    SVC_POWER_STAGE_STANDBY,

    /* 濞戞捁宕甸弫鎼佸捶閵娧冩疇闁挎稑鐡慍C 闁哄牆顦伴弲銉╁Υ?*/
    SVC_POWER_STAGE_ACC_ACTIVE,

    /* 濞戞捁宕甸弫鎼佸捶閵娧冩疇闁挎稑顒 闁哄牆顦伴弲銉╁Υ?*/
    SVC_POWER_STAGE_ON_ACTIVE,

    /* 濞戞捁宕甸弫鎼佸箳婢跺本鏆╅柛姘嚱缁辨繄鎼鹃崨顖炵崜闁汇垽娼ч鎰潰閿濆懏韬柟鐐灣缂嶅洨鍖栭懡銈囧煚闁?*/
    SVC_POWER_STAGE_SUPERCAP_HOLD,

    /* 鐎规瓕灏欑划锟犲礃閸愯尙鏆伴悷鏇氱閸櫻囧嫉閻氬绀夋慨婵撶到濠€顏堝磻濮橆剙褰犻柡鍫濇惈閸ｎ垱寰勯崶銉㈠亾?*/
    SVC_POWER_STAGE_SHUTDOWN_PENDING,

    /* 鐎规瓕灏欑划鈩冩交濞戞ê寮抽柡鍫氬亾缂備礁鐗婇弻鍥偨闂堟稑袟濞达絾绮撳Ο浣糕枔閻愬厜鍋?*/
    SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS
} svc_power_stage_t;

/* 鐟滅増鎸告晶鐘绘偐閼哥鍋撴担瑙勭皻闁圭鍋撳璺哄濡礁鈻撻悙鍏夊亾?*/
static svc_power_stage_t g_power_stage = SVC_POWER_STAGE_UNKNOWN;

/* 閻犱焦婢樼紞宥嗘交濞戞ê寮抽悺鎺戞噽妤犲洭鎮介棃娑卞晣濞ｅ洦绻冪€垫棃姊奸懜娈垮斀闁?tick闁挎稑鐬奸弫銈嗙鎼淬垹搴婄紒?HOLD 鐎圭寮剁€垫梻绱掗鐑嗘▼濞戞柨鎳岄埀?*/
static rt_tick_t g_supercap_hold_enter_tick = 0;

/* 閻犱焦婢樼紞宥嗘交濞戞ê寮?SHUTDOWN_PENDING 闁?tick闁挎稑鐬奸弫銈嗙鎼淬垹鑵归弶鈺傜⊕濞撳墎绱掗崼鐔哥劷闁汇垹鐏氬鍌涙償韫囧鍋?*/
static rt_tick_t g_shutdown_pending_enter_tick = 0;

/*
 * 闁瑰搫顦遍弫绋棵规担琛℃煠闂佸じ绀侀悺銊╁冀閸パ呯闁? *
 * 濞戞挴鍋撻柡鍐跨細鐎靛矂鎮介崹顐㈢闁汇垹鐏氱粊锔剧矙鐎ｎ偒鍔€鐎殿喖绻愮槐鎴炴叏鐎ｅ墎绀夐悘蹇氫含閻ゅ棙鎷呭鍐ｅ亾? * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锛勬喆閿濆懎鏋€闁? * 濞戞捁宕甸弫鎼佸箳婢跺鍟婂ù鐘劚閹鏁嶅畝鈧弫鎼佸储鐎ｎ剚鍩涚紓渚囧幖瑜板宕犻弽鐢电闁绘鍩栭埀顑跨劍濠р偓闁告瑥鐗撻弫濠勬嫚椤栫偐鍋撻埀顒勫炊閻愬瓨鐝梺顐ｄ亢缁诲秶鎮扮仦閿亾娴ｇ儤鐣遍梻鍌ゅ櫍椤ｄ粙濡? */
static rt_bool_t g_power_loss_latched = RT_FALSE;

/* 閺夆晜鐟ら柌婊堝冀閸パ呯濞ｅ洦绻嗛惁澶嬫交濞戞ê寮?SHUTDOWN_PENDING 闁哄啳娉涜ぐ褔宕戝顐ゎ伇婵炲棌鈧啿寮抽柛娆欑到婵晜鎷呭┃搴撳亾?*/
static rt_bool_t g_shutdown_prepare_done = RT_FALSE;

/*
 * 閻℃帒鎳愭鍥偨闂堟侗鍟?ready 闁哄秴娲ょ换鏃堝Υ? *
 * 閻庣懓鍟抽妴鍐矆鐞涒檧鍋撳鍡欑婵炲枴銈囩閻炴稑鐭佺换鍐矙鐎ｂ晞鍘柨娑樼焷缁夊鈧湱鎳撻崙锛勭磼韫囨挸甯犻柛鎺撳閸愮粯寰勯悢鍛婃殰闁圭偓鍨剁敮鈧柣銏犵仛閺佸湱浜搁崣銉у晩闁炽儲绺块埀? * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锛勬喆閿濆懎鏋€闁? * 闁告帗鐭粭鍌炴偨閸偅顦ч悺鎺戞噹椤旀劖娼诲Ο鑽ゆ⒕闁稿繐鎳忓褔鏁嶇仦钘夌ケ閻炴凹鍋夐銈堛亹閹炬潙鐏囬柛娆樺灡閺侇噣骞橀幋鐐茬闁汇垹鐏氶弫鍦焊閸撗勭暠闂傚偆鍣ｉ。浠嬪Υ? */
static rt_bool_t g_supercap_ready = RT_FALSE;

/*
 * 濞戞挸锕ｇ粩鎾箯瀹ュ嫬鐦滈柣銏犵仛濡叉悂宕ラ敃鈧悺銊╁捶閵婏絺鍋? *
 * 閺夆晜鐟ら柌婊堟煂韫囧海鐟╅梻鍌樺妿閺併倖绂嶆惔鈥茬驳闁瑰搫顦遍弫绋库柦閸喗瀚查柟顓滃灩椤︽彃鈻介幐搴⒕婵炴潙顑冮埀? * 闁搞儳濮崇拹鐔煎箣閹存粍绮﹂柣婊勬緲濠€顏呯▔瀹ュ棙笑闁活亜顑傞埀顒佺矊缂嶅宕滃鍡樼畳婵炲备鍓濆﹢浣圭▔閼姐倖鏆╅柍銉︾箚缁绘牗绋婇崼銏㈡殕闁告娲╃槐?
 * 闁兼澘鏈Σ鍝ユ啺娴ｇ闅橀柛鎺戞閳ь剚绮堢€靛矂鎮介棃娑樼仩闁瑰搫顦花锟犲灳濠靛﹦绠烽柡鍕靛灙閳ь剚绮堢€靛矂鎮介棃娑樼仩闁诡厹鍨归ˇ鍙夌閸″繆鍋撳┑鍕ㄥ亾? */
static rt_bool_t g_prev_main_present = RT_FALSE;

/*
 * 濞戞挸顑夊?4 濞戞搩浜ｉ鎼佸极閺夋寧鐝ら柛蹇嬪姂閸庢挳鎮介妸銈囪壘闁告绮慨鍫ュΥ? *
 * 闁告鍠庡ú婊堝及椤栨繃绨犲☉鎾筹功閺佹悂宕㈢€ｎ亝瀚查弶鍫熸尭閸欏棙绋夊鍕獥闁稿秴绻愰悿鍕殽鐏炵瓔鍚囬柣銏犵仛缁噣鏌囬敐鍡欏妤犵偞褰冮崳锝夋晬? * 濠碘€冲€归悘澶嬬▔閳ь剟骞忓澶婃珰闁哄秴鍢插銊╁礆閸モ晛笑闁诡兛绶ょ槐婵嬫偐閼哥鍋撴担瑙勭皻濞村吋宀稿顏嗘暜缁嬫鍟囬柡鍕尰婵牓濡? */

/* 濞戞捁宕甸弫鍛婃交閻愮數鏁鹃悗娑櫭﹢顏嗘兜椤旀鍚囬悹浣插墲閺嗙喖宕抽妸锝傚亾?*/
static rt_uint8_t g_main_present_confirm_count = 0;

/* 濞戞捁宕甸弫鍛婃交閻愮數鏁惧☉鎾卞灩閵囨垹娑甸娆惧悋閻犱讲鍓濋弳鐔煎闯閵婏絺鍋?*/
static rt_uint8_t g_main_loss_confirm_count = 0;

/* 閻℃帒鎳愭鍥偨闂堟侗鍟囬弶鈺冨仧閻㈢粯娼忛幆褍鐓?ready 闂傚啫鐗嗛埀顒冨亹閳ユ鎷嬮妶鍫悁闁轰焦婢樺▍鎺楀Υ?*/
static rt_uint8_t g_supercap_ready_confirm_count = 0;

/* 閻℃帒鎳愭鍥偨闂堟侗鍟囬弶鈺冨仧閻㈢粯鎷呮惔婵堣壘濞达絽楠哥敮鍥⒓閸績鍋撻懖鈹锯偓妯兼媼閵堝牜鍚€闁轰焦婢樺▍鎺楀Υ?*/
static rt_uint8_t g_supercap_low_confirm_count = 0;

/*
 * 濞戞挸顑夊鐗堢▔閵堝嫰鍤嬮柡宥呮搐缁绘棃鎮介妸銈囪壘濞ｅ洦绻嗛惁澶愬嫉閳ь剛绱掗崼鐔哥劷闁汇垽娼ф慨鈺傛媴濠婂啫娑ч柟绗涘棭鏀藉☉鎾亾婵炲枴鎵冲亾? *
 * 闁哄牃鍋撶紓浣哥墛閺屽洭鎮芥担鍐炬蕉闁瑰嘲妫欓崹姘▔閵堝棭鍔勯柨? * 1. 闁稿繐鐗嗛崹?SoC/24V/閻℃帒鎳庨鎰板礂閸涱垱鏆╅柣鈺冾焾閸櫻勬綇閹惧啿姣?
 * 2. 闁告劕绉归崳鎾绩?MCU 闁煎浜ｉ棅鈺傜┍濠靛洤鐦?
 */

/* 缂佹鍏涚粩鏉戭潰閵夛附浠樼紓浣哥墛閺屽洭鎮介棃娑樞楀ù锝嗙矋濡叉悂宕ラ敃鈧崙锛勭磼韫囨柨鈷旈悶娑樿閳?*/
static rt_bool_t g_final_soc_cut_done = RT_FALSE;

/* 缂佹鍏涚花鈺侇潰閵夛附浠樼紓浣哥墛閺屽洭鎮介棃娑樞楀ù锝嗙矋濡叉悂宕ラ敃鈧崙锛勭磼韫囨柨鈷旈悶娑樿閳?*/
static rt_bool_t g_final_hold_cut_done = RT_FALSE;

/* 閺夆晜鐟╅崳鐑藉及閹呯濠㈠湱澧楀Σ鎴炵▔閳ь剚绋夌€ｎ亶妲诲ù锝呯Т閸ら亶寮敮顔剧闁告艾閰ｅ鐗堢▔閼姐倖鏆╅柟顓滃灩椤︽煡寮張鐢电獥濞戞捁顕ф慨鈺冩嫬閸愵亝鏆忛柕?*/
void rt_hw_cpu_reset(void);

/* 闁硅泛锕︽慨鎼佸箑娴ｅ湱浜ｅ☉鎾村礃濞村棝骞嬮幇顏呯溄闁煎磭鏅ú鍧楀箳閵壯勭畽闁瑰啿鍊诲▓鎴犫偓娑欘殘椤戜焦绋夌拠褏绀夊〒姘仒缁剚绋夐幓鎺戠稉闁瑰灚鎸稿畵鍐喆閸屾氨妾柕?*/
static const char *svc_power_stage_to_str(svc_power_stage_t stage)
{
    switch (stage)
    {
    case SVC_POWER_STAGE_MAIN_OFF:
        return "MAIN_OFF";

    case SVC_POWER_STAGE_STANDBY:
        return "STANDBY";

    case SVC_POWER_STAGE_ACC_ACTIVE:
        return "ACC_ACTIVE";

    case SVC_POWER_STAGE_ON_ACTIVE:
        return "ON_ACTIVE";

    case SVC_POWER_STAGE_SUPERCAP_HOLD:
        return "SUPERCAP_HOLD";

    case SVC_POWER_STAGE_SHUTDOWN_PENDING:
        return "SHUTDOWN_PENDING";

    case SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS:
        return "SHUTDOWN_IN_PROGRESS";

    case SVC_POWER_STAGE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

/*
 * 闁硅泛艌閳ь剚绮嶉鐘电矓閹烘挻顦ч梻鍌滆ˉ閳ь剚绻冨畷鑼不濡や礁鐏囬柍銉︾矌閸ゅ海绮欑€ｎ亝鍣柡鍫㈠枙椤撴悂寮悧鍫仹闁轰焦澹嬮埀顒佺缚閳? *
 * 濞撴艾顑呴々褔鏁? * 濠碘€冲€归悘澶岀棯鐠恒劉鏌ら柛娑栧妽濠€锟犲及?1000ms闁挎稑鐬奸垾妯兼媼閵堝棙顦ч梻鍌氱摠濡?3000ms闁? * 闂侇叏绲藉銊╂閳ь剛鎲版担鐣岀缂?3 婵炲棴绻濋崗妯侯煥闄囬崘濠氬级閳ュ弶顐介柨娑樻湰婢х姷绮诲Δ鍐ｂ偓妯兼媼閵堝棗鐏囩紒鏂款儍閳? *
 * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锟犲箮婵犲倸绠甸柟鑸电墱缁儤绋夐埀顒勫箣閹般劉鍋撳鍡╁悁闁轰焦婢樺▍鎺懳熼垾宕団偓鐑藉灳濠垫挾绀夐柛姘叄濞间即鏌呴弰蹇曞竼闁哄洤顕彊閻庤鍝庨埀? */
static rt_uint8_t svc_power_ms_to_confirm_count(rt_uint32_t time_ms)
{
    rt_uint32_t count;

    /* 闁告碍鍨崇粭鍌炲矗閺嶃劍娈婚柨娑樼焸娴尖晠宕楀鍛瘜缂佺姵銇炵粩鏉戔枎鎺抽埀?*/
    count = (time_ms + APP_POWER_TASK_PERIOD_MS - 1U) / APP_POWER_TASK_PERIOD_MS;

    /* 闁哄牃鍋撻悘蹇斿灩閳ユ鎷?1 婵炲棌妲勭槐婵嬫焼閸喖甯冲ù鑲╁Т閸?0ms 闁哄啫鐖奸埀顒佹缁额偅寰勬潏銊︽珡闁?*/
    if (count == 0U)
    {
        count = 1U;
    }

    /* 鐟滅増鎸告晶鐘垫媼閳╁啯娈堕柛锝冨妽濡?8bit闁挎稑鏈〒鑸靛緞瑜嶈ぐ褔鎳楅挊澶婄厒 255闁?*/
    if (count > 255U)
    {
        count = 255U;
    }

    return (rt_uint8_t)count;
}

/*
 * 闂侇偅姘ㄩ弫銈囨兜椤旀鍚囬悹浣插墲閺嗙喖宕抽妸锔界函闁哄倹婢橀崵閬嶅极閼割兘鍋? *
 * condition 濞戞捁娅ｅ﹢锟犳晬? * 閻犱讲鍓濋弳鐔煎闯閵娧呮焾闁告梻濯寸槐婵堟偘閵娧佷粵闁哄鈧弶顐介柛锔哄姀缁绘稓绱掗鐔峰К閻℃帞鍋ㄩ埀? *
 * condition 濞戞挸鎼禍锝夋晬? * 閻犱讲鍓濋弳鐔煎闯閵婏妇顏搁梻鍡氼啇缁辨繄鎮伴妸褋浠涢柛妯肩帛婵牊娼婚崶鈹炬煠閻炴凹鍋呮晶锕傚棘椤撱劎绀夐梻鍥ｅ亾閻熸洑绶氶崳鎼佸棘閹殿喒鈧鎷嬮妶鍐ｅ亾? */
static void svc_power_update_confirm_counter(rt_bool_t condition, rt_uint8_t *counter)
{
    if (condition)
    {
        if (*counter < 255U)
        {
            (*counter)++;
        }
    }
    else
    {
        *counter = 0U;
    }
}

/*
 * 闁告帗绻傞～鎰板礌閺嶎偅鏆╂繝褎鍔栫敮鍫曞礆閹澘澹栭柕? *
 * 閺夆晜鐟╅崳鐑芥儍閸曨偄鏂ч柛鎺撶懄濡叉悂鏁? * 闁稿繐鐗婃俊鎼佸箥閳ь剟寮垫径濠傚綘闂佹鍠楃敮鍫曞礆閹澘澹栭梺顔挎閸ㄥ灚鎱ㄧ€ｎ亜顕ч柛鎺楊暒缁斿瓨绋夐浣插亾濠婂嫭顫栫痪顓у枔閳ь兛绀佽ぐ鍙夛紣閸曨剛銈撮柍銉︾箘濞堟垿鎮╅懜纰樺亾娓氬﹦绀?
 * 濞戞挸绉崇欢椋庢導閺嶎剙袩闁绘娲ｇ粭鍌炴偨閻㈠摜甯涢悹浣靛€曢埀顒傤儠閳? *
 * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锟犳焼閸喖甯抽柨? * 闁哄鐏濋悺娆撳礃瀹勭増鍎欓柛鏂诲妸閳ь兛鑳堕崕褰掑触椤栨艾袟闁靛棔娴囩粔瀵糕偓纭咁潐閻ｎ偊宕㈢€ｎ亝鍎欓柛鏂诲妽濡炲倿鏁? * 濞戞挸绉撮幃鎾寸▔婵犲嫭鏆╅悹渚灠缁剁偟鏁敂鑺ラ檷濞戞挸绉撮幃鎾搭渶濡鍚囬柣銏ゆ涧闁解晠鏁嶇仦绛嬪殼闁奸顥愰、鎴炵▔鏉炴壆鐟濆☉鎾亾闁奸攱鐣埀? */
static void svc_power_init_ctrl_pins(void)
{
    /* PC11 -> PWR_24V_EN闁挎稑鐭傜划顖滄媼閵堝懎甯ラ柟宄邦樀閻濐噣濡?*/
    HPM_IOC->PAD[IOC_PAD_PC11].FUNC_CTL = IOC_PC11_FUNC_CTL_GPIO_C_11;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 11, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_24V_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 1);

    /* PC12 -> PWR_SOC_EN闁挎稑鐭傜划顖滄媼閵堝懎甯ラ柟宄邦樀閻濐噣濡?*/
    HPM_IOC->PAD[IOC_PAD_PC12].FUNC_CTL = IOC_PC12_FUNC_CTL_GPIO_C_12;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 12, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_SOC_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 1);

    /* PC13 -> MCU_SUPER_C_CHRG闁挎稑鐭傜划顖滄媼閵堝懎甯掗悹浣圭摃缁夊鐥閺佸摜鈧湱鎳撻崢鏍偨閻愬厜鍋?*/
    HPM_IOC->PAD[IOC_PAD_PC13].FUNC_CTL = IOC_PC13_FUNC_CTL_GPIO_C_13;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 13, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 1);
#else
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
#endif

    /* PC14 -> MCU_PWR_HOLD闁挎稑鐭傜划顖滄媼閵堝懎甯ュǎ鍥ㄧ箖鐎?MCU闁?*/
    HPM_IOC->PAD[IOC_PAD_PC14].FUNC_CTL = IOC_PC14_FUNC_CTL_GPIO_C_14;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 14, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWR_HOLD_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 1);
}

/*
 * 缂佹鍏涚粩鏉戭潰閵夛附浠樼紓浣哥墛閺屽洭鎮介棃娑樞楀ù锝嗗殠閳? *
 * 闁稿繐鐗嗛崹蹇涘箳婢舵稓绐?
 * 1. PWR_SOC_EN
 * 2. PWR_24V_EN
 * 3. MCU_SUPER_C_CHRG
 *
 * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锟犲礂閸喐鏆柟鍝勵槸椤﹀鏌堥妸銉﹀濡ゅ倹锚婵盯鎳撳Δ鍐╃ゲ闁稿繑濞婇幗鑲╂崉椤栥倗绀?
 * 闁告劕绉堕弫?MCU 闁煎浜滅换浣衡偓鐟版湰閸ㄦ岸寮甸埀顒勫触鎼达絾鐣卞ǎ鍥ㄧ箖鐎垫棃鏌屾繝鍐╂澒闁? */
static void svc_power_cut_soc_outputs(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 0);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 0);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
#endif
}

/*
 * 缂佹鍏涚花鈺侇潰閵夛附浠樼紓浣哥墛閺屽洭鎮介棃娑樞楀ù锝嗗殠閳? *
 * 闁哄牃鍋撻柛姘閸熲偓闂佹彃锕ラ弬?MCU 闁煎浜ｉ棅鈺傜┍濠靛洤鐦柕? * 閺夆晜鐟ょ粩鎾嚇濮橆厼顎欏ù锝呴閹鏁嶇€涱湁U 閻忓繐妫滅换姗€宕楅妷褎鍩傛慨婵撶稻鐢偓闁汇垻鍋ｉ埀? */
static void svc_power_release_mcu_hold(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 0);
}

/* 濞戞挸顑夊鐗堟交濞嗗繐娈ゅ☉?raw 闁告帇鍊栭弻鍥礄閼恒儲娈堕柨娑樿嫰閸欏繘鏌堥妸銉ユ锭闁稿鍩冮埀顒佺矊鐢偅鎱ㄧ€ｎ喗顫岄柛濠勫帶閸ㄤ粙寮娑掑亾濠垫挾绀夊☉鎾崇Т閻㈩偊宕㈢紒妯侯潔闁?*/

static rt_bool_t svc_power_is_main_present_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_bat24_mv >= APP_PWR_MAIN_PRESENT_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_ready_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_READY_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_available_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_low_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv < APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV);
}

/* 闁告帇鍊栭弻鍥亹閹惧啿顤呴柡鍕靛灠閹礁顔忛懠顒傜梾濠㈣泛瀚花顒勫箳婢跺本鏆╅柛姘瀹曟劗绮欑€ｃ劉鍋?*/
static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage)
{
    return ((stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS));
}

/*
 * 闁?tick 鐎瑰壊鍠栭埀顒€鍚嬪畷鑼不濡や礁鐏囨慨锝庡亞椤濡? * 閺夆晜鐟╅崳椋庣磼閻斿墎顏遍悘蹇庢祰椤ュ﹪鏁嶇仦鑺ュ€甸梻?hold/pending 闁哄啫鐖煎Λ璺ㄧ磼閻旀椿鍚€闂侇喚鏅弫銈夊触鐏炶偐顏卞┑鍌涱殘閻ｈ鈻旈弴妯峰亾? */
static rt_uint32_t svc_power_ticks_to_ms(rt_tick_t start_tick)
{
    rt_tick_t now_tick;
    rt_tick_t delta_tick;

    if (start_tick == 0)
    {
        return 0;
    }

    now_tick = rt_tick_get();
    delta_tick = now_tick - start_tick;

    return (rt_uint32_t)((rt_uint64_t)delta_tick * 1000UL / RT_TICK_PER_SECOND);
}

/* 鐟滅増鎸告晶鐘诲箳婢跺本鏆╁ǎ鍥ㄧ箖鐎垫柨顔忛懠顒傜梾闁归晲鑳堕悽缁樺緞濮橆偆鐣介柕?*/
static rt_uint32_t svc_power_get_hold_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_supercap_hold_enter_tick);
}

/* 鐟滅増鎸告晶鐘诲礂閾忣偅绨氶柛鎴濇椤︻剟姊奸懜娈垮斀鐎规瓕灏欑划锟犲箰娴ｈ櫣鏁惧鑸电煯缁犳瑩濡?*/
static rt_uint32_t svc_power_get_shutdown_pending_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_shutdown_pending_enter_tick);
}

/*
 * 濞戞捁宕甸弫鎼佸捶閵娧冩疇闁哄啫澧庡▓鎴濐潰閿濆懐鍩楅梻鍐煐椤斿矂宕氶妶鍡樼劷闁? *
 * 閺夆晜鐟╅崳閿嬬▔瀹ュ懎鏅欓柟铏瑰劋濞煎懐鎼鹃崨顓у晣闁告帇鍊栭弻鍥晬? * 闁搞儳濮崇拹鐔烘惥閸涱収鍟囬柛锔哄妺鐎靛矂鎮介棃娑欒含缂佹儳娼″Ο浣糕枔闂堟稑娑ч悹鎰枙閻宕ユ惔鈥抽叡闁稿繐鎳愰弫鎼佸椽瀹€鈧慨鎼佸箑娴ｇ瓔娼庨悗鐢靛櫐缁?
 * 濞戞挸绉烽姘跺矗瀹ュ牏绠栭柡澶堝劦濡棗顫㈤姀銏ゅ厙缂備胶鍠曠换姗€宕楅妷锔诲妧閻㈩垱鐡曠换宥囨偘鐏炵儵鍋? */
static svc_power_stage_t svc_power_eval_normal_stage(const app_vehicle_io_state_t *vehicle_state)
{
    if (vehicle_state->wk_on != 0U)
    {
        return SVC_POWER_STAGE_ON_ACTIVE;
    }

    if (vehicle_state->wk_acc != 0U)
    {
        return SVC_POWER_STAGE_ACC_ACTIVE;
    }

    return SVC_POWER_STAGE_STANDBY;
}

/*
 * 闁汇垹鐏氱花顕€鎮╅懜纰樺亾娴ｈ绨氶柡宥囶焾缁洪箖宕氶妶鍡樼劷闁告垼濮ら弳鐔煎Υ? *
 * 閺夆晜鐟╅崳鐑藉磻濮樿鲸鐣卞ù婊冾儐閸庡繘寮?4 婵縿鍎荤槐?
 * 1. 闁稿繐鐗婂ú鍧楀棘閻楀牆顣查柡鍫濐槺閳ユ鎷嬮妶鍫悁闁轰焦婢樺▍?
 * 2. 闁告劕绉垫俊鎼佸储閻斿娼楅梺鎻掓处閻楅亶鏌岃箛鎾崇秮闁瑰瓨鍔忛埀顒佺矊閸戯紕娑甸娆惧悋闁炽儲绻勫▓鎴︽焻閺勫繒甯嗛柣妯垮煐閳? * 3. 婵☆偀鍋撴繛鏉戭儎鐎靛矂鎮介崹顐㈢闁汇垹鐏氶柈銊╁椽鐏炲彞鍒掑璺虹У闁?
 * 4. 闁告劕鍟块悾楣冨嫉椤掑啰鏋傞柣鈺婂枟閻栵綁鎮╅懜纰樺亾娴ｈ绨氶梻鍐煐椤?
 */
static svc_power_stage_t svc_power_eval_stage(const app_adc_snapshot_t *adc_snapshot,
                                              const app_vehicle_io_state_t *vehicle_state)
{
    rt_bool_t main_present_raw;
    rt_bool_t main_present;
    rt_bool_t main_falling_edge;
    rt_bool_t main_rising_edge;
    rt_bool_t supercap_available;
    rt_uint8_t main_present_confirm_target;
    rt_uint8_t main_loss_confirm_target;
    rt_uint8_t supercap_ready_confirm_target;
    rt_uint8_t supercap_low_confirm_target;

    /* 闁稿繐鐗婃俊绋啃掗銈庢健闂佹澘绉堕悿鍡涘箲閵忋垻鏆柟瀛樺姉閳ユ鎷嬮妶鍡╁仹闁轰浇鍩囬埀?*/
    main_present_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_PRESENT_CONFIRM_MS);
    main_loss_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_LOSS_CONFIRM_MS);
    supercap_ready_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_READY_CONFIRM_MS);
    supercap_low_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_LOW_CONFIRM_MS);

    /* 闁哄洤鐡ㄩ弻濠囧箥閳ь剟寮垫径濠傜闁硅埖鐗為鎼佸极閺夋寧鐝ら柕?*/
    main_present_raw = svc_power_is_main_present_raw(adc_snapshot);
    svc_power_update_confirm_counter(main_present_raw, &g_main_present_confirm_count);
    svc_power_update_confirm_counter(!main_present_raw, &g_main_loss_confirm_count);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    svc_power_update_confirm_counter(svc_power_is_supercap_ready_raw(adc_snapshot), &g_supercap_ready_confirm_count);
    svc_power_update_confirm_counter(svc_power_is_supercap_low_raw(adc_snapshot), &g_supercap_low_confirm_count);
#else
    g_supercap_ready_confirm_count = 0U;
    g_supercap_low_confirm_count = 0U;
    g_supercap_ready = RT_FALSE;
#endif

    /* 闁告劕绉垫俊鎼佸储閻斿娼楅柛鎺嬪€栭弻鍥ㄦ姜椤掍礁搴婇柟瀛樺姀閳ь剚绮庨垾妯兼媼閵堝懏鍊甸柍銉︾箘濞堟垿鏌呴弰蹇曞竼闁绘鍩栭埀顑块檷閳?*/
    main_present = (g_main_present_confirm_count >= main_present_confirm_target);
    main_falling_edge = (g_prev_main_present == RT_TRUE) && (g_main_loss_confirm_count >= main_loss_confirm_target);
    main_rising_edge = (g_prev_main_present == RT_FALSE) && (main_present == RT_TRUE);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    supercap_available = svc_power_is_supercap_available_raw(adc_snapshot);
#else
    supercap_available = RT_FALSE;
#endif

    /*
     * 濞戞捁宕甸弫鎼佸捶閵娧冩疇闁哄啳顔愮槐?
     * 1. 濠碘€冲€归悘澶庛亹閹惧啿顤呴柡鍕靛灡鐢偓闁汇垹鐏氱粊锔剧矙鐎ｂ晞鍘柡澶堝劤閺佹悂骞侀姀鐙€妲婚柨娑樿嫰濮樸劍绋夌拠鎻捫楀璺虹С缂?
     * 2. 濠碘€冲€归悘澶屾惥閸涱収鍟囩€规瓕灏欑划锟犲礂閸涱厼鐓?ready闁挎稑鑻銊ф媼妫颁胶绉?
     * 3. 闁绘帟娉涢幃妤呭炊閻愭彃鐓傛慨婵撶到閻栬埖娼婚幇顖ｆ斀闁诡兛绀侀崹浠嬪棘?     */
    if (main_present)
    {
        if (main_rising_edge && svc_power_is_shutdown_flow_stage(g_power_stage))
        {
            APP_NON_CAN_LOG("PWR event: main power restored during %s, reset now\r\n",
                            svc_power_stage_to_str(g_power_stage));
            rt_thread_mdelay(APP_PWR_RECOVERY_RESET_DELAY_MS);
            rt_hw_cpu_reset();
        }

        g_power_loss_latched = RT_FALSE;

#if APP_PWR_SUPERCAP_MGMT_ENABLE
        if (g_supercap_ready_confirm_count >= supercap_ready_confirm_target)
        {
            g_supercap_ready = RT_TRUE;
        }
#else
        g_supercap_ready = RT_FALSE;
#endif

        g_prev_main_present = RT_TRUE;
        return svc_power_eval_normal_stage(vehicle_state);
    }

    /* 閺夆晝鍋熼悽鑽ゆ兜椤旀鍚囬柛鎺楊暒鐎靛矂鎮介崗鍛婁涪濠㈣泛宕幃妤呮晬鐏炴儳顤呴柟璺猴梗缁楀倹绋夐埀顒勫箯瀹ュ嫬鐦滈柣銏ゆ涧閻°劑宕烽妸锔惧灱闊洦顨嗙粩濠氬箳婢跺牃鍋?*/
    if (g_main_loss_confirm_count >= main_loss_confirm_target)
    {
        g_prev_main_present = RT_FALSE;
    }

    /*
     * 闁告瑯浜濆﹢渚€鎯囬悢鍓插妧闁告瑦鍨归弫鎾诲灳濠娾偓鐎靛矂鎮介崹顐㈢闁汇垹鐏氶柈銊╁灳濠靛洦顦ч柨娑樻湰婢х娀宕ラ姘楅柟鍝勵槺閺佺霉娴ｈ　鏌ら柕?     * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锟犳焼閸喖甯抽柛鎺撶煯缁楀倿鎮介崹顐ｎ槯濞戞捁宕甸弫鍛婃交濡崵姊剧紒瀣暱閻ｉ箖濡存担鐣屝㈤悗褰掆偓娑氱槏婵炲备鈧啿甯犲┑鍌涙灮缁?
     * 闁绘鍩栭埀顑跨劍濠р偓闁告顥愰銈嗘交濞戞瑥绔撮柣銏犵仛缁侊妇绮欑€ｃ劉鍋?     */
    if ((main_falling_edge == RT_TRUE) && (g_power_loss_latched == RT_FALSE))
    {
        g_power_loss_latched = RT_TRUE;

        /* 濠碘€冲€归悘澶屾惥閸涱収鍟囩€规瓕灏欑划?ready 濞戞挻鏌ㄧ紞瀣礈瀹ュ牏绠峰鍓佸櫐缁辨繄浜告潏顐ょ濞ｅ洦绻冪€垫棃姊奸懜娈垮斀闁?*/
#if APP_PWR_SUPERCAP_MGMT_ENABLE
        if (g_supercap_ready && supercap_available)
        {
            return SVC_POWER_STAGE_SUPERCAP_HOLD;
        }
#endif

        /* 闁告熬绠戦崹顖炴儎鐎涙ê澶嶉弶鈺傜☉閸櫻囧嫉閸濆嫬娅欏璺烘储閳?*/
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
    }

    /*
     * 闁瑰搫顦遍弫绋棵规担琛℃煠濞戞挴鍋撻柡鍐跨畵閺€锝団偓娑欙公缁辨繄浜告潏鈺傚煕缂備緡鍘介柈銊╁箳婢跺本鏆╂繛缈犺兌閳诲ジ骞掗妸銊х闁?     * 濞戞挸绉撮崢鎴犳媼缁嬪灝鏅欓柛銉у仱閳ь兘鍋撻柛鎺斿濞呮﹢鏌呭鍗炐﹂柟顑块檷閳?     */
    if (g_power_loss_latched)
    {
        /* 濠碘€冲€归悘澶婎啅閼碱剛鐥呴弶鈺傜☉閸欏棝宕楅搹顐ｇ皻闁告艾楠稿畷鎰矙鐎ｅ墎绀夐悘蹇撳船瑜把囧捶?pending 闁?in_progress 濞戞柨顑夊Λ鍧楀箳閵娿劎绠婚柕?*/
        if ((g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
            || (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
        {
            if (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS)
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }

            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

        /* 濠碘€冲€归悘澶屾惥閸涱収鍟囬弶鈺冨仧閻㈢粯鎷呮惔鈥崇闁挎稑鏈崹銊╂嚀閸涱剛绠介柟闀愮劍濡炲倿姊婚弶鎴濆殥缂備礁绻楃粔鎾籍鐠佸湱绀夐悘蹇氶哺鐢娼诲☉妯虹厒闁稿繗娅曞┃鈧柛鎴濇椤︻剟濡?*/
        if ((g_supercap_low_confirm_count >= supercap_low_confirm_target)
            || (svc_power_get_hold_elapsed_ms() >= APP_PWR_HOLD_PREPARE_TIMEOUT_MS))
        {
            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

#if APP_PWR_SUPERCAP_MGMT_ENABLE
        return SVC_POWER_STAGE_SUPERCAP_HOLD;
#else
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
#endif
    }

    /* 闁哄啨鍨洪惀鍛村嫉婢跺鐦滈柣銏㈩暜缁辨繃绋婇悢鍝ユ⒕闁哄牆顦伴婊冾嚕韫囨氨绠婚柛蹇嬪劜鐢偓闁汇垹鐏氱粊锔剧矙鐎ｎ偅顦ч柨娑樺缁绘岸骞愭担鍛婅含 MAIN_OFF闁?*/
    return SVC_POWER_STAGE_MAIN_OFF;
}

/*
 * 闁哄牃鍋撶紓浣哥墛閺屽洭鎮介棃娑樞楀ù锝嗙矋婢х晫鎮扮仦钘夋瘣闁轰浇鍩囬埀? *
 * 鐟滅増鎸告晶鐘诲矗椤忓嫭韬?SHUTDOWN_IN_PROGRESS 闂傚啳鍩栭宀勫箥瑜戦、鎴︽儑閻斿壊鍔€闁瑰嘲顦抽崜濂稿礉閵娿倗绋婇柕? * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锟犲箮婵為敮鍋撳鍛﹂柟顑跨劍鐢娼诲☉鎺嗗亾濠靛棙瀚查柍銉︾矊婵晜鎷呭鍕挃閻炴稑澶囬埀顒佺箖婵泛顕ｉ埀顒勬晬鐏炶姤鍊电紓渚囧幗濞茶法鈧顫夊Σ妤冩嫬閸愨晜顦ч幖鏉戠箞閳? */
static void svc_power_handle_final_power_cut(void)
{
    rt_uint32_t pending_ms;

    if (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
    {
        return;
    }

    /* 闁哄牃鍋撶紓浣哥墕婵晜鎷呭鍛暠闁烩晝顭堥顕€寮崼鏇燂紵濞寸姰鍎寸换姗€宕?pending 闁汇劌瀚鍌炲礆鐠佸疇绀嬮柛鈺佹惈閸ｎ垶濡?*/
    pending_ms = svc_power_get_shutdown_pending_elapsed_ms();

    /* 闁稿繐鐗嗛崹?SoC/24V/閻℃帒鎳庨鎰板礂閸涱垱鏆╅柣鈺冾焾閸櫻勬綇閹惧啿姣夐柕?*/
    if ((g_final_soc_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_SOC_CUT_DELAY_MS))
    {
        g_final_soc_cut_done = RT_TRUE;
        svc_power_cut_soc_outputs();
#if APP_PWR_SUPERCAP_MGMT_ENABLE
        APP_NON_CAN_LOG("PWR action: cut PWR_SOC_EN/PWR_24V_EN/SUPER_C_CHRG\r\n");
#else
        APP_NON_CAN_LOG("PWR action: cut PWR_SOC_EN/PWR_24V_EN\r\n");
#endif
    }

    /* 闁告劕绉归崳鎾绩?MCU 濞ｅ洦绻冪€垫棃濡?*/
    if ((g_final_hold_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_HOLD_CUT_DELAY_MS))
    {
        g_final_hold_cut_done = RT_TRUE;
        APP_NON_CAN_LOG("PWR action: release MCU_PWR_HOLD\r\n");
        svc_power_release_mcu_hold();
    }
}

/*
 * 闁绘鍩栭埀顑跨閸ㄥ繘骞戦姀鐘插汲闁告瑱绲芥慨鈺傛媴濠婂喚妲遍柣鐐叉閳? *
 * 婵炲鍔嶉崜浼存晬? * 閺夆晜鐟╅崳閿嬪緞閸曨厽鍊為柣銊ュ濡叉悂鍨惧鍡欑闁稿繈鍎查悡鍥ㄧ▔椤忓牊鈻夋繛鍫濈仛濡炲倻鎲版担闀愮驳濞寸姭鍋撳☉鏂跨墐閳ь剚绻愮槐?
 * 闁兼澘濂旂粭澶愬及椤栨せ鍋撳鍕Ж濞戞挴鍋撻柟宄扮Ч閸忔﹢宕戝顐ょ焼濞戞柨鐗冮埀顒佺缚閳? * 閺夆晜鐟﹂悧閬嶅矗椤栨瑤绨伴梺顒€鐏濋崢銈夋煂瀹ュ拋妲婚柟绗涘棭鏀介柛蹇嬪劚瑜版盯宕濋妸銈囩▕闁? */
static void svc_power_handle_stage_transition(svc_power_stage_t new_stage)
{
    if (new_stage == g_power_stage)
    {
        return;
    }

    if (new_stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
    {
        /* 閺夆晜绋戦崣鍡欐惥閸涱垶鐛撻柣銏ゆ涧椤旀劖绌卞┑鍥х槷闁哄啳顔愮槐婵堟媼閺夎法绉块悹褔顥撻崑锝夋晬鐏炲€熷珯婵炴挸鎳忕敮鈧柛姘捣閻㈠姊奸懜娈垮斀闁哄秴娲ょ换鏃堝Υ?*/
        g_supercap_hold_enter_tick = rt_tick_get();
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 缂佹鍏涚粩鎾籍閸洘锛熼柛蹇撶枃閸庢宕楁径娑氱闂傚嫬绉崇紞鍡涘礉閻旂儵鍋撳灏栧亾?*/
        lcd_backlight_off();
        APP_NON_CAN_LOG("PWR event: enter SUPERCAP_HOLD, backlight off\r\n");
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING) && (g_shutdown_prepare_done == RT_FALSE))
    {
        /*
         * 濠碘€冲€归悘澶愬及椤栨せ鍋撳鈧€靛矂鎮介崹顐㈢闁汇垽娼ч幃妤呮儎鐎涙ê澶嶉弶?SHUTDOWN_PENDING闁炽儲绻愮槐?
         * 婵縿鍊栧鍌涙交濡崵姊鹃柡鍫濐槹椤掓粌顕ｈ箛姘辩闁稿繈鍎寸换?hold闁挎稑鐭佺换鏍煂瀹€鍏鎷嬫０浣侯伇濞戞搩浜ｉ幑锝夋倷閻у摜绀夊〒姘仒缁剟寮妷銉х闁告粌鐭侀鎼佸籍閸撲胶鍩犲☉鎾亾闁?         */
        if (g_supercap_hold_enter_tick == 0)
        {
            g_supercap_hold_enter_tick = rt_tick_get();
        }

        g_shutdown_pending_enter_tick = rt_tick_get();
        g_shutdown_prepare_done = RT_TRUE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 閺夆晜绋戦崣鍡涘礂閾忣偅绨氶柛鎴濇椤︻剟姊奸懜娈垮斀闁哄啯婀圭弧鍐兜椤旇崵绠介柤鍐茶嫰閸樻粓宕楅幎鑺ワ紨闁?*/
        lcd_backlight_off();
        APP_NON_CAN_LOG("PWR event: enter SHUTDOWN_PENDING, hold=%lums ready=%d\r\n",
                        svc_power_get_hold_elapsed_ms(),
                        g_supercap_ready);
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
          && (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
    {
        /* 缂佹鍏涚粩鏉戔枎闄囩换姗€宕楅妷锔戒粯缂備礁鐗婇弻鍥偨閻㈠憡鈻夋繛鍫濈仛濡炲倿鏁嶇仦鎯р叺濞戞挴鍋撻柡澶嗗墲濡晞绠涘Δ瀣濞撴艾銇樼花顒勫级鐠恒劑鐛撳Δ鐘茬焷閻﹀寮捄铏圭闁?*/
        APP_NON_CAN_LOG("PWR event: enter SHUTDOWN_IN_PROGRESS, pending=%lums\r\n",
                        svc_power_get_shutdown_pending_elapsed_ms());
    }
    else if ((new_stage == SVC_POWER_STAGE_STANDBY)
          || (new_stage == SVC_POWER_STAGE_ACC_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_ON_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_MAIN_OFF))
    {
        /* 闁搞儳鍋涢崺宀勫疾椤曗偓閳ь剚姘ㄦ慨鎼佸箑娴ｈ顦ч柨娑樻湰婵℃悂骞掓径灞炬毄婵炵繝鑳堕埢濂告儎缁嬪灝褰犻柛娆愶耿閸ｅ搫銆掗崨顔肩闁?*/
        g_supercap_hold_enter_tick = 0;
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;
    }

    g_power_stage = new_stage;
}

/*
 * 闁汇垹鐏氱花顔剧棯鐠恒劉鏌ら柛蹇嬪劚瑜版盯濡? *
 * 缂佹崘娉曢埢濂稿礃閸涱喚妲ㄥ☉鎾亾闁瑰嘲绉存禒娑㈡儍閸曨亞鐨戦柟顖氭噹缁躲垽宕堕崫鍕毎闁? * 1. 閻犲洨绮〒鍫曞棘閺夋寧褰ラ柣? * 2. 閻犲洤瀚崣濠囨偐閼哥鍋撴担瑙勭皻闁烩晩鍠楅悥锝夋⒓閼告鍞?
 * 3. 濠㈣泛瀚幃濠囨偐閼哥鍋撴担绋跨€奸柟骞垮灩閸欏棝宕ｉ敐鍛楀ù? * 4. 濠㈣泛瀚幃濠囧嫉閳ь剛绱掗崼鐔哥劷闁汇垽娼ф慨鈺傛媴? * 5. 闁瑰灚鎸稿畵鍐亹閹惧啿顤呴柣妯垮煐閳? */
static void svc_power_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        const app_adc_snapshot_t *adc_snapshot;
        const app_vehicle_io_state_t *vehicle_state;
        svc_power_stage_t power_stage;
        rt_uint32_t hold_ms;

        /* 閻犲洩顕цぐ鍥嫉閳ь剟寮?ADC 闁汇垹鐏氱花顔炬喆閸屾稓銈撮煫鍥跺亞閸欏酣濡?*/
        adc_snapshot = svc_adc_get_snapshot();

        /* 閻犲洩顕цぐ鍥嫉閳ь剟寮幏灞剧盃閺夊牆妫滅欢顓㈠礂閵夈倗鐟㈤梺鍨€婚弫鎼佹偐閼哥鍋撴担鍛婂渐闁绘挆浣插亾?*/
        vehicle_state = svc_vehicle_io_get_state();

        /* 闁活潿鍔嶅〒鍫曞棘閹峰瞼缈婚柛蹇嬪劥閻﹀孩瀵奸悧鍫熸嫳閺夌儐鍠氭慨鎼佸箑娴ｈ绨氶柣鈺婂枟閻栵綁姊奸懜娈垮斀闁?*/
        power_stage = svc_power_eval_stage(adc_snapshot, vehicle_state);

        /* 濠碘€冲€归悘澶愭⒓閼告鍞介柛娆惷€靛弶绂嶉崱顓犵闁圭瑳鍡╂斀閻庣數鎳撶花鏌ユ儍閸曨偄寮抽柛娆欑到婵晜鎷呭┃搴撳亾?*/
        svc_power_handle_stage_transition(power_stage);

        /* 濠碘€冲€归悘澶婎啅閼碱剛鐥呴弶鈺傜☉閸欏棝寮甸埀顒傜磼閸喐鐒介柣銏㈡暬濡礁鈻撶喊澶岀闁告帗鐟︽晶鐣屾偘瀹€鈧﹢鈥愁潰閿濆洦鐣遍柟宄邦槼閸撳ジ宕濋妸銈囩▕闁?*/
        svc_power_handle_final_power_cut();

        /* 閻犱緤绱曢悾鏄忋亹閹惧啿顤呴柟鍝勵槺閺佺霉娴ｈ　鏌ょ€规瓕灏欑划锟犲箰娴ｈ櫣鏁惧ù婊冩椤︽寧绋婇崨顐熷亾?*/
        hold_ms = svc_power_get_hold_elapsed_ms();

        /* 缂備胶鍠嶇粩鎾箥閹惧啿绁憸鐗堟尭婢х娀鎮介崹顐ょ埍闁绘鍩栭埀顑跨筏缁辨繃绗熸径鍝ヨ壘闁哄娉曟鍥嚂閺冨洨娈堕柕?*/
//        APP_NON_CAN_LOG("PWR: BAT24=%lumV SUPER=%lumV READY=%d ACC=%d ON=%d LI=%lumV CHRG=%d STDBY=%d HOLD=%lums STAGE=%s\r\n",
//                        adc_snapshot->est_bat24_mv,
//                        adc_snapshot->est_super_c_mv,
//                        g_supercap_ready,
//                        vehicle_state->wk_acc,
//                        vehicle_state->wk_on,
//                        vehicle_state->li_bat_est_mv,
//                        vehicle_state->li_bat_chrg,
//                        vehicle_state->li_bat_stdby,
//                        hold_ms,
//                        svc_power_stage_to_str(power_stage));
        rt_thread_mdelay(APP_POWER_TASK_PERIOD_MS);
    }
}

int svc_power_init(void)
{
    /*
     * 闁稿繐鐗嗛崹鍨叏鐎ｎ亜顕ч柟纰樺亾闁哄牆顦遍弫绋库攦閹邦厼浠橀柛鎺撳劶閸撳ジ鏁?     * 濞ｅ洦绻嗛惁澶娦掕箛鏃戝仹濞戞挸锕﹂弫鎼佹焾閹存帞鐭ょ紓浣哄枍缁旀挳鎯冮崟顔煎濞达絽绉舵慨鎼佸箑娴ｅ摜纾诲┑顔碱儍閳?     */
    svc_power_init_ctrl_pins();

    /*
     * 闁告劕绉垫俊鎼佹偐閼哥鍋撴担瑙勭皻閺夆晜鍔橀、鎴﹀籍鐠哄搫缍侀梺鎻掔箲缁斿姊块煬娴嬪亾?     * 閺夆晜鐟﹂悧閬嶅磻濮橆厽笑濞戞捁妗ㄧ花锛勬兜椤旇崵绠芥慨锝呯箲椤愬ジ宕ラ姘楅梺顔藉灊缁娀宕ョ仦鍓у闁汇劌瀚崹鍨叏鐎ｂ晝鐟愬☉鎾愁儐閺嬪啫顕ｉ埀顒佹叏鐎ｅ墎绀?
     * 濞戞挸绉磋ぐ鍫熺▔婵犱胶顏辨繛鍠°倗绠ラ悶娑樻湰閻ｎ偊鎮惧▎鎴澬﹂柟顑跨婵傛牠宕鍐ｅ亾?     */
    g_power_stage = SVC_POWER_STAGE_UNKNOWN;
    g_supercap_hold_enter_tick = 0;
    g_shutdown_pending_enter_tick = 0;
    g_power_loss_latched = RT_FALSE;
    g_shutdown_prepare_done = RT_FALSE;
    g_supercap_ready = RT_FALSE;
    g_prev_main_present = RT_FALSE;
    g_main_present_confirm_count = 0;
    g_main_loss_confirm_count = 0;
    g_supercap_ready_confirm_count = 0;
    g_supercap_low_confirm_count = 0;
    g_final_soc_cut_done = RT_FALSE;
    g_final_hold_cut_done = RT_FALSE;

    return RT_EOK;
}

int svc_power_task_start(void)
{
    rt_thread_t thread;

    /* 闁告帗绋戠紓鎾绘偨閸偆鐖辩紒鐙呯磿閹﹦鐥捄銊㈡煠闁?*/
    thread = rt_thread_create(APP_POWER_TASK_NAME,
                              svc_power_thread_entry,
                              RT_NULL,
                              APP_POWER_TASK_STACK_SIZE,
                              APP_POWER_TASK_PRIORITY,
                              APP_POWER_TASK_TICK);
    if (thread == RT_NULL)
    {
        APP_NON_CAN_LOG("power thread create failed\r\n");
        return -RT_ERROR;
    }

    /* 闁告凹鍨版慨鈺呮偨閸偆鐖辩紒鐙呯磿閹﹦鐥捄銊㈡煠闁?*/
    rt_thread_startup(thread);
    return RT_EOK;
}
