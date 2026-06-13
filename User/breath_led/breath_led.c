/* 硬件配置 */
#include "gd32f30x.h"

/***
 * 基本原理: 人眼视觉暂留. 光信号消失, 视觉形象会保持1/24秒. 亮度保持时间需要<=45ms左右, 否则就不够平滑
 * 调试方法:
 * 		首先设置PWM频率, 调解占空比, 找到合适的亮度范围
 * 		然后在某个时间内完成亮度从灭到全亮的变化
 */

/* 硬件配置 */
#define BREATH_LED_TIMERx           TIMER7
#define BREATH_LED_TIMERx_CLK       RCU_TIMER7
#define BREATH_LED_TIMERx_CHANNEL   TIMER_CH_1
#define BREATH_LED_GPIO_PORT        GPIOC
#define BREATH_LED_GPIO_PIN         GPIO_PIN_7
#define BREATH_LED_GPIO_CLK         RCU_GPIOC

/* 函数声明 */
void rcu_config(void);
void gpio_config(void);
void timer_config(void);
void nvic_config(void);

int timer_pwm_led_init(void)
{
    /* 系统时钟配置 */
    rcu_config();
    /* GPIO配置 */
    gpio_config();
    /* 定时器配置 */
    timer_config();
    /* NVIC配置 */
    nvic_config();

    /* 启用定时器 */
    timer_enable(BREATH_LED_TIMERx);
}

void timer_pwm_breath_led_on()
{
    /*配置GPIO复用*/
    gpio_init(BREATH_LED_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, BREATH_LED_GPIO_PIN);
    /* 打开中断, 启用定时器 */
    nvic_config();
    /* 启用定时器 */
    timer_enable(BREATH_LED_TIMERx);
}

void timer_pwm_breath_led_off()
{
    /*关闭定时器*/
    timer_disable(BREATH_LED_TIMERx);
}

void timer_pwm_black_led_on()
{
    /*关闭定时器*/
    timer_disable(BREATH_LED_TIMERx);

    /*配置GPIO为推挽输出, 非复用*/
    gpio_init(BREATH_LED_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, BREATH_LED_GPIO_PIN);
    gpio_bit_set(BREATH_LED_GPIO_PORT, BREATH_LED_GPIO_PIN);
}

void timer_pwm_black_led_off()
{
    /*关闭定时器*/
    timer_disable(BREATH_LED_TIMERx);

    /*配置GPIO为推挽输出, 非复用*/
    gpio_init(BREATH_LED_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, BREATH_LED_GPIO_PIN);
    gpio_bit_reset(BREATH_LED_GPIO_PORT, BREATH_LED_GPIO_PIN);
}


/* 系统时钟配置 */
void rcu_config(void)
{
    rcu_periph_clock_enable(BREATH_LED_TIMERx_CLK);
    rcu_periph_clock_enable(BREATH_LED_GPIO_CLK);
}

/* GPIO配置（复用功能） */
void gpio_config(void)
{
    /* 配置PC7为复用推挽输出 */
    gpio_init(BREATH_LED_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, BREATH_LED_GPIO_PIN);
    /* 配置GPIO复用功能为TIMER7 */
    //gpio_pin_remap_config(GPIO_TIMER7_CH1_PARTIAL_REMAP, ENABLE);
}

/* 定时器PWM配置 */
void timer_config(void)
{
    timer_parameter_struct timer_initpara;
    timer_oc_parameter_struct timer_ocinitpara;

    /* 初始化定时器参数结构体 */
    timer_struct_para_init(&timer_initpara);
    
    /* 配置定时器基础参数 */
    timer_initpara.prescaler         = 11;	//120M = 10 * 1000000
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 99999;	//100 * 1000. 10ms - 500
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(BREATH_LED_TIMERx, &timer_initpara);

    /* 初始化输出比较参数结构体 */
    timer_channel_output_struct_para_init(&timer_ocinitpara);
    
    /* 配置通道输出参数 */
    timer_ocinitpara.outputstate    = TIMER_CCX_ENABLE;
    timer_ocinitpara.outputnstate   = TIMER_CCXN_DISABLE;
	//timer_ocinitpara.ocpolarity     = TIMER_OC_POLARITY_HIGH;
    //timer_ocinitpara.ocnpolarity    = TIMER_OCN_POLARITY_HIGH;
    //timer_ocinitpara.ocidlestate    = TIMER_OC_IDLE_STATE_LOW;
    //timer_ocinitpara.ocnidlestate   = TIMER_OCN_IDLE_STATE_LOW;
    timer_ocinitpara.ocpolarity     = TIMER_OC_POLARITY_LOW;//TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocnpolarity    = TIMER_OCN_POLARITY_LOW;//TIMER_OCN_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate    = TIMER_OC_IDLE_STATE_HIGH;//TIMER_OC_IDLE_STATE_LOW;
    timer_ocinitpara.ocnidlestate   = TIMER_OCN_IDLE_STATE_HIGH;//TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(BREATH_LED_TIMERx, BREATH_LED_TIMERx_CHANNEL, &timer_ocinitpara);

    /* 配置PWM模式 */
    timer_channel_output_pulse_value_config(BREATH_LED_TIMERx, BREATH_LED_TIMERx_CHANNEL, 0);
    timer_channel_output_mode_config(BREATH_LED_TIMERx, BREATH_LED_TIMERx_CHANNEL, TIMER_OC_MODE_PWM1);
    timer_channel_output_shadow_config(BREATH_LED_TIMERx, BREATH_LED_TIMERx_CHANNEL, TIMER_OC_SHADOW_DISABLE);

	/* 这一部分是人为加的 */
	timer_auto_reload_shadow_enable(TIMER7);
	timer_automatic_output_enable(TIMER7);

    /* 配置更新中断 */
    timer_interrupt_enable(BREATH_LED_TIMERx, TIMER_INT_UP);
    timer_update_event_enable(BREATH_LED_TIMERx);
}

/* NVIC中断配置 */
void nvic_config(void)
{
    /* 根据知识库中的中断向量表，TIMER7使用TIMER7_BRK_TIMER11_IRQn等中断 */
    nvic_irq_enable(TIMER7_UP_IRQn, 0, 0);
}


/* 定时器中断服务函数 */
void TIMER7_UP_IRQHandler(void)
{
    if (timer_interrupt_flag_get(BREATH_LED_TIMERx, TIMER_INT_FLAG_UP) != RESET) {
		static uint32_t counter = 0;
		static uint32_t duty = 0;
		static int8_t dir = 1;
		
		if (counter++ % 4 == 0)	//中断10ms执行一次, 占空比(亮度)切换, 40ms执行一次
		{
			/*1s会有100次运算*/
			if (duty >= 300) { dir = -1; }
			if (duty <= 150) { dir = 1; }
			
			
			if (duty <= 250) {
				duty += dir * 1;	//100, 慢
			} else{
				duty += dir * 2;	//25, 块
			}
		}
		
		timer_channel_output_pulse_value_config(BREATH_LED_TIMERx, BREATH_LED_TIMERx_CHANNEL, duty);
        timer_interrupt_flag_clear(BREATH_LED_TIMERx, TIMER_INT_FLAG_UP);
    }
}
