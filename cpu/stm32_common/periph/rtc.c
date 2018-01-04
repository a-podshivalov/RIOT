/*
 * Copyright (C) 2015 Lari Lehtomäki
 *               2016 Laksh Bhatia
 *               2016-2017 OTA keys S.A.
 *               2017 Freie Universität Berlin
 *               2017 Unwired Devices LLC [info@unwds.com]
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_stm32_common
 * @{
 * @file
 * @brief       Low-level RTC driver implementation
 *
 * @author      Lari Lehtomäki <lari@lehtomaki.fi>
 * @author      Laksh Bhatia <bhatialaksh3@gmail.com>
 * @author      Vincent Dupont <vincent@otakeys.com>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Oleg Artamonov <oleg@unwds.com>
 * @}
 */

#include <time.h>
#include "cpu.h"
#include "stmclk.h"
#include "periph/rtc.h"

/* guard file in case no RTC device was specified */
#if defined(RTC) && !defined(CPU_FAM_STM32F1)

/* map some CPU specific register names */
#if defined (CPU_FAM_STM32L0) || defined(CPU_FAM_STM32L1)
#define EN_REG              (RCC->CSR)
#define EN_BIT              (RCC_CSR_RTCEN)
#define RST_BIT             (RCC_CSR_RTCRST)
#define CLKSEL_MASK         (RCC_CSR_RTCSEL)
#define CLKSEL_LSE          (RCC_CSR_RTCSEL_LSE)
#define CLKSEL_LSI          (RCC_CSR_RTCSEL_LSI)
#else
#define EN_REG              (RCC->BDCR)
#define EN_BIT              (RCC_BDCR_RTCEN)
#define CLKSEL_MASK         (RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCSEL_1)
#define CLKSEL_LSE          (RCC_BDCR_RTCSEL_0)
#define CLKSEL_LSI          (RCC_BDCR_RTCSEL_1)
#endif

/* interrupt line name mapping */
#if defined(CPU_FAM_STM32F0) || defined(CPU_FAM_STM32L0)
#define IRQN                (RTC_IRQn)
#define ISR_NAME            isr_rtc
#else
#define IRQN                (RTC_Alarm_IRQn)
#define ISR_NAME            isr_rtc_alarm
#endif

/* EXTI bitfield mapping */
#if defined(CPU_FAM_STM32L4)
#define EXTI_IMR_BIT        (EXTI_IMR1_IM18)
#define EXTI_FTSR_BIT       (EXTI_FTSR1_FT18)
#define EXTI_RTSR_BIT       (EXTI_RTSR1_RT18)
#define EXTI_PR_BIT         (EXTI_PR1_PIF18)
#else
#if defined(CPU_FAM_STM32L0)
#define EXTI_IMR_BIT        (EXTI_IMR_IM17)
#else
#define EXTI_IMR_BIT        (EXTI_IMR_MR17)
#endif
#define EXTI_FTSR_BIT       (EXTI_FTSR_TR17)
#define EXTI_RTSR_BIT       (EXTI_RTSR_TR17)
#define EXTI_PR_BIT         (EXTI_PR_PR17)
#endif

/* write protection values */
#define WPK1                (0xCA)
#define WPK2                (0x53)

/* define TR, DR, and ALRMAR position and masks */
#define TR_H_MASK           (RTC_TR_HU | RTC_TR_HT)
#define TR_M_MASK           (RTC_TR_MNU | RTC_TR_MNT)
#define TR_S_MASK           (RTC_TR_SU | RTC_TR_ST)
#define DR_Y_MASK           (RTC_DR_YU | RTC_DR_YT)
#define DR_M_MASK           (RTC_DR_MU | RTC_DR_MT)
#define DR_D_MASK           (RTC_DR_DU | RTC_DR_DT)
#define DR_WDU_MASK         (RTC_DR_WDU)
#define ALRM_D_MASK         (RTC_ALRMAR_DU | RTC_ALRMAR_DT)
#define ALRM_H_MASK         (RTC_ALRMAR_HU | RTC_ALRMAR_HT)
#define ALRM_M_MASK         (RTC_ALRMAR_MNU | RTC_ALRMAR_MNT)
#define ALRM_S_MASK         (RTC_ALRMAR_SU | RTC_ALRMAR_ST)
#ifndef RTC_DR_YU_Pos
#define RTC_DR_YU_Pos       (16U)
#endif
#ifndef RTC_DR_MU_Pos
#define RTC_DR_MU_Pos       (8U)
#endif
#ifndef RTC_DR_DU_Pos
#define RTC_DR_DU_Pos       (0U)
#endif
#ifndef RTC_DR_WDU_Pos
#define RTC_DR_WDU_Pos      (13U)
#endif
#ifndef RTC_TR_HU_Pos
#define RTC_TR_HU_Pos       (16U)
#endif
#ifndef RTC_TR_MNU_Pos
#define RTC_TR_MNU_Pos      (8U)
#endif
#ifndef RTC_TR_SU_Pos
#define RTC_TR_SU_Pos       (0U)
#endif
#ifndef RTC_ALRMAR_DU_Pos
#define RTC_ALRMAR_DU_Pos   (24U)
#endif
#ifndef RTC_ALRMAR_HU_Pos
#define RTC_ALRMAR_HU_Pos   (16U)
#endif
#ifndef RTC_ALRMAR_MNU_Pos
#define RTC_ALRMAR_MNU_Pos  (8U)
#endif
#ifndef RTC_ALRMAR_SU_Pos
#define RTC_ALRMAR_SU_Pos   (0U)
#endif

/* figure out sync and async prescalers
 * NB: lower PRE_ASYNC values increase rtctimers_millis accuracy,
 * but also increase power consumption
 */
#if CLOCK_LSE
#define PRE_ASYNC           (7)
#define PRE_SYNC            ((CLOCK_LSE / (PRE_ASYNC + 1)) - 1)
#elif (CLOCK_LSI == 40000)
#define PRE_ASYNC           (124)
#define PRE_SYNC            ((CLOCK_LSI / (PRE_ASYNC + 1)) - 1)
#elif (CLOCK_LSI == 37000)
#define PRE_ASYNC           (124)
#define PRE_SYNC            ((CLOCK_LSI / (PRE_ASYNC + 1)) - 1)
#elif (CLOCK_LSI == 32000)
#define PRE_ASYNC           (127)
#define PRE_SYNC            ((CLOCK_LSI / (PRE_ASYNC + 1)) - 1)
#else
#error "rtc: unable to determine RTC SYNC and ASYNC prescalers from LSI value"
#endif

#define RTC_SSR_TO_US                   (((10000000 / PRE_SYNC) + 5)/10) /**< conversion from RTC_SSR to microseconds */

#define RTCTIMERS_MILLIS_OVERHEAD       (0)
#define RTCTIMERS_MILLIS_BACKOFF        (((10000/RTC_SSR_TO_US) + 5)/10)
#define RTCTIMERS_MILLIS_ISR_BACKOFF    (((10000/RTC_SSR_TO_US) + 5)/10)

/* struct tm counts years since 1900 but RTC has only two-digit year hence the
 * offset of 100 years. */
#define YEAR_OFFSET         (100)

static struct {
    rtc_alarm_cb_t cb_a;        /**< callback called from RTC interrupt */
    rtc_alarm_cb_t cb_b;        /**< Subseconds alarm callback */
    rtc_wkup_cb_t  cb_wkup;     /**< Wake up timer callback */
    
    void *arg_a;                /**< argument passed to the callback */
    void *arg_b;                /**< argument passed to subseconds alarm callback */
    void *arg_wkup;             /**< argument passed to wakeup callback */
} isr_ctx;

static uint32_t val2bcd(int val, int shift, uint32_t mask)
{
    uint32_t bcdhigh = 0;

    while (val >= 10) {
        bcdhigh++;
        val -= 10;
    }

    return ((((bcdhigh << 4) | val) << shift) & mask);
}

static int bcd2val(uint32_t val, int shift, uint32_t mask)
{
    int tmp = (int)((val & mask) >> shift);
    return (((tmp >> 4) * 10) + (tmp & 0x0f));
}

static inline void rtc_unlock(void)
{
    /* unlock RTC */
    RTC->WPR = WPK1;
    RTC->WPR = WPK2;
}

static inline void rtc_lock(void)
{
    /* lock RTC device */
    RTC->WPR = 0xff;
}

void rtc_init(void)
{
    /* enable low frequency clock */
    stmclk_enable_lfclk();

    /* select input clock and enable the RTC */
    stmclk_dbp_unlock();
    
    EN_REG &= ~(CLKSEL_MASK);
#if CLOCK_LSE
    EN_REG |= (CLKSEL_LSE | EN_BIT);
#else
    EN_REG |= (CLKSEL_LSI | EN_BIT);
#endif

    rtc_unlock();
    /* enter RTC init mode */
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF)) {}
    /* reset configuration */
    RTC->CR = 0;
    RTC->ISR = RTC_ISR_INIT;
    /* configure prescaler (RTC PRER) */
    RTC->PRER = (PRE_SYNC | (PRE_ASYNC << 16));
    /* Set 24-h clock */
    RTC->CR &= ~RTC_CR_FMT;
    /* Timestamps enabled */
    RTC->CR |= RTC_CR_TSE;
    
    /* exit RTC init mode */
    RTC->ISR &= ~RTC_ISR_INIT;
    while (RTC->ISR & RTC_ISR_INITF) {}
    
    rtc_lock();

    /* configure the EXTI channel, as RTC interrupts are routed through it.
     * Needs to be configured to trigger on rising edges. */
    EXTI->FTSR &= ~(EXTI_FTSR_BIT);
    EXTI->RTSR |= EXTI_RTSR_BIT;
    EXTI->IMR  |= EXTI_IMR_BIT;
    EXTI->PR   |= EXTI_PR_BIT;
    /* enable global RTC interrupt */
    NVIC_EnableIRQ(IRQN);
}

int rtc_set_time(struct tm *time)
{
    rtc_unlock();
    /* enter RTC init mode */
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF)) {}
    
    RTC->DR = (val2bcd((time->tm_year % 100), RTC_DR_YU_Pos, DR_Y_MASK) |
               val2bcd(time->tm_mon,  RTC_DR_MU_Pos, DR_M_MASK) |
               val2bcd(time->tm_wday, RTC_DR_WDU_Pos, DR_WDU_MASK) |
               val2bcd(time->tm_mday, RTC_DR_DU_Pos, DR_D_MASK));
    RTC->TR = (val2bcd(time->tm_hour, RTC_TR_HU_Pos, TR_H_MASK) |
               val2bcd(time->tm_min,  RTC_TR_MNU_Pos, TR_M_MASK) |
               val2bcd(time->tm_sec,  RTC_TR_SU_Pos, TR_S_MASK));
               
    /* exit RTC init mode */
    RTC->ISR &= ~RTC_ISR_INIT;
    while (RTC->ISR & RTC_ISR_INITF) {}
    rtc_lock();
    
    while (!(RTC->ISR & RTC_ISR_RSF)) {}

    return 0;
}

int rtc_get_time(struct tm *time)
{
    /* save current time */
    uint32_t tr = RTC->TR;
    uint32_t dr = RTC->DR;
    time->tm_year = bcd2val(dr, RTC_DR_YU_Pos, DR_Y_MASK) + YEAR_OFFSET;
    time->tm_mon  = bcd2val(dr, RTC_DR_MU_Pos, DR_M_MASK);
    time->tm_mday = bcd2val(dr, RTC_DR_DU_Pos, DR_D_MASK);
    
    time->tm_wday = bcd2val(dr, RTC_DR_WDU_Pos, DR_WDU_MASK);
    /* tm_wday should be days since Sunday, so it's 0 if today is Sunday */
    /* STM32 returns day of week instead, so Monday is 1 and Sunday is 7 */
    if (time->tm_wday == 7) {
        time->tm_wday = 0;
    }
    
    time->tm_hour = bcd2val(tr, RTC_TR_HU_Pos, TR_H_MASK);
    if ((tr & RTC_TR_PM) && (RTC->CR & RTC_CR_FMT)) {
        time->tm_hour += 12;
    }
    
    time->tm_min  = bcd2val(tr, RTC_TR_MNU_Pos, TR_M_MASK);
    time->tm_sec  = bcd2val(tr, RTC_TR_SU_Pos, TR_S_MASK);

    return 0;
}

int rtc_set_alarm(struct tm *time, rtc_alarm_cb_t cb, void *arg)
{
    rtc_unlock();

    /* disable existing alarm (if enabled) */
    rtc_clear_alarm();

    /* save callback and argument */
    isr_ctx.cb_a = cb;
    isr_ctx.arg_a = arg;

    /* set wakeup time */
    RTC->ALRMAR = (val2bcd(time->tm_wday, RTC_ALRMAR_DU_Pos, ALRM_D_MASK) |
                   val2bcd(time->tm_hour, RTC_ALRMAR_HU_Pos, ALRM_H_MASK) |
                   val2bcd(time->tm_min, RTC_ALRMAR_MNU_Pos, ALRM_M_MASK) |
                   val2bcd(time->tm_sec,  RTC_ALRMAR_SU_Pos, ALRM_S_MASK));

    /* Enable Alarm A */
    RTC->ISR &= ~(RTC_ISR_ALRAF);
    RTC->CR |= (RTC_CR_ALRAE | RTC_CR_ALRAIE);

    rtc_lock();

    return 0;
}

int rtc_get_alarm(struct tm *time)
{
    uint32_t dr = RTC->DR;
    uint32_t alrm = RTC->ALRMAR;

    time->tm_year = bcd2val(dr, RTC_DR_YU_Pos, DR_Y_MASK) + YEAR_OFFSET;
    time->tm_mon  = bcd2val(dr, RTC_DR_MU_Pos, DR_M_MASK);
    time->tm_mday = bcd2val(alrm, RTC_ALRMAR_DU_Pos, ALRM_D_MASK);
    time->tm_hour = bcd2val(alrm, RTC_ALRMAR_HU_Pos, ALRM_H_MASK);
    time->tm_min  = bcd2val(alrm, RTC_ALRMAR_MNU_Pos, ALRM_M_MASK);
    time->tm_sec  = bcd2val(alrm, RTC_ALRMAR_SU_Pos, ALRM_S_MASK);

    return 0;
}

void rtc_clear_alarm(void)
{
    RTC->CR &= ~(RTC_CR_ALRAE | RTC_CR_ALRAIE);
    while (!(RTC->ISR & RTC_ISR_ALRAWF)) {}

    isr_ctx.cb_a = NULL;
    isr_ctx.arg_a = NULL;
}

int rtc_millis_set_alarm(int milliseconds, rtc_alarm_cb_t cb, void *arg)
{   
    rtc_unlock();
    
    RTC->CR &= ~(RTC_CR_ALRBE | RTC_CR_ALRBIE);
    while (!(RTC->ISR & RTC_ISR_ALRBWF)) {}
    
    /* setting seconds */
    int seconds = milliseconds/1000;
    RTC->ALRMBR = val2bcd(seconds, RTC_ALRMAR_SU_Pos, ALRM_S_MASK);
    
    /* minutes, hours and date doesn't matter */
    RTC->ALRMBR |= (RTC_ALRMBR_MSK2 | RTC_ALRMBR_MSK3 | RTC_ALRMBR_MSK4);

    uint32_t msec = milliseconds % 1000;
    uint32_t alarm_millis_time = PRE_SYNC - (msec*1000)/RTC_SSR_TO_US;
       
    /* set up subseconds alarm */
    uint32_t regalarm = RTC->ALRMBSSR;
    regalarm |= (0x8 << 24); // compare 8 bits only
    regalarm &= ~(RTC_ALRMBSSR_SS);
    regalarm |= (alarm_millis_time & 0xFF);
    RTC->ALRMBSSR = regalarm;
    
    /* Enable Alarm B */
    RTC->CR |= RTC_CR_ALRBE;
    RTC->CR |= RTC_CR_ALRBIE;
    RTC->ISR &= ~(RTC_ISR_ALRBF);

    isr_ctx.cb_b = cb;
    isr_ctx.arg_b = arg;
    
    rtc_lock();

    return 0;
}

void rtc_millis_clear_alarm(void)
{
    rtc_unlock();
    /* Disable Alarm B */
    RTC->CR &= ~(RTC_CR_ALRBE | RTC_CR_ALRBIE);
    while (!(RTC->ISR & RTC_ISR_ALRBWF)) {}
    
    isr_ctx.cb_b = NULL;
    isr_ctx.arg_b = NULL;
    rtc_lock();
}

int rtc_millis_get_time(uint32_t *millis)
{
    /* clear RSF bit */
    RTC->ISR &= ~RTC_ISR_RSF;
    
    /* wait for RSF to be set by hardware */
    while (!(RTC->ISR & RTC_ISR_RSF)) {}
    
    /* RTC registers need to be read at least twice when running at f < 32768*7 = 229376 Hz APB1 clock */
    uint32_t rtc_ssr_counter = RTC->SSR;

    /* second read */
    if (RTC->SSR != rtc_ssr_counter) {
        /* 3rd read if 1st and 2nd don't match */
        rtc_ssr_counter = RTC->SSR;
    }

    uint32_t milliseconds = ((PRE_SYNC - rtc_ssr_counter)*RTC_SSR_TO_US)/1000;
    
    /* clear RSF bit */
    RTC->ISR &= ~RTC_ISR_RSF;
    
    /* wait for RSF to be set by hardware */
    while (!(RTC->ISR & RTC_ISR_RSF)) {}
    
    /* RTC registers need to be read at least twice when running at f < 32768*7 = 229376 Hz APB1 clock */
    /* reading TR locks registers so it must be read first, DR must be read last */
    uint32_t rtc_time_reg = RTC->TR;

    /* second read */
    if (RTC->TR != rtc_time_reg) {
        /* 3rd read if 1st and 2nd don't match */
        rtc_time_reg = RTC->TR;
    }
    
    uint32_t seconds  = (((rtc_time_reg & RTC_TR_ST)  >>  4) * 10) + ((rtc_time_reg & RTC_TR_SU)  >>  0);
    
    *millis = milliseconds + 1000*seconds;

    /* unlock RTC registers by reading DR */
    rtc_ssr_counter = RTC->DR;
    
    return 0;
}

int rtc_set_wakeup(uint32_t period_us, rtc_wkup_cb_t cb, void *arg)
{
    /* Enable write access to RTC registers */
    rtc_unlock();
    
    /* Disable periodic wakeup */
    RTC->CR &= ~(RTC_CR_WUTE);
    while ((RTC->ISR & RTC_ISR_WUTWF) == 0) ;
    
    /* Set wakeup timer value */
    period_us = ((period_us * 100)/12207) - 1;   
    RTC->WUTR = (period_us & 0xFFFF);
    
    /* Set wakeup timer clock source to RTCCLK/4 */
    /* Min period 244 us, maximum 8 s */
    RTC->CR &= ~(RTC_CR_WUCKSEL);
    RTC->CR |= (RTC_CR_WUCKSEL_1);
    
    /* Enable periodic wakeup */
    RTC->CR |= RTC_CR_WUTE;
    RTC->CR |= RTC_CR_WUTIE;
    RTC->ISR &= ~(RTC_ISR_WUTF);

    /* Enable RTC write protection */
    rtc_lock();

    EXTI->IMR  |= EXTI_IMR_MR20;
    EXTI->RTSR |= EXTI_RTSR_TR20;
    NVIC_SetPriority(RTC_WKUP_IRQn, RTC_IRQ_PRIO);
    NVIC_EnableIRQ(RTC_WKUP_IRQn);

    isr_ctx.cb_wkup = cb;
    isr_ctx.arg_wkup = arg;

    return 0;
}

void rtc_enable_wakeup(void) {
    rtc_unlock();
    /* Enable wakeup */
    RTC->CR |= RTC_CR_WUTE;
    rtc_lock();
}

void rtc_disable_wakeup(void)
{
    rtc_unlock();
    /* Disable wakeup */
    RTC->CR &= ~(RTC_CR_WUTE);
    rtc_lock();
}

int rtc_save_backup(uint32_t data, uint8_t reg_num) {    
    __IO uint32_t tmp = 0;
    
    tmp = RTC_BASE + 0x50;
    tmp += (reg_num * 4);

    /* Write the specified register */
    *(__IO uint32_t *)tmp = (uint32_t)data;
    
    return 0;
}

uint32_t rtc_restore_backup(uint8_t reg_num) {
    __IO uint32_t tmp = 0;
    
    tmp = RTC_BASE + 0x50;
    tmp += (reg_num * 4);

    /* Read the specified register */
    return (*(__IO uint32_t *)tmp);
}

void rtc_poweron(void)
{
    stmclk_dbp_unlock();
    EN_REG |= EN_BIT;
    /* stmclk_dbp_lock(); */
}

void rtc_poweroff(void)
{
    stmclk_dbp_unlock();
    EN_REG &= ~EN_BIT;
    stmclk_dbp_lock();
}

void ISR_NAME(void)
{
    if (RTC->ISR & RTC_ISR_ALRAF) {
        if (isr_ctx.cb_a != NULL) {
            isr_ctx.cb_a(isr_ctx.arg_a);
        }
        RTC->ISR &= ~RTC_ISR_ALRAF;
    }
    
    if (RTC->ISR & RTC_ISR_ALRBF) {
        if (isr_ctx.cb_b) {
            isr_ctx.cb_b(isr_ctx.arg_b);
        }
        RTC->ISR &= ~RTC_ISR_ALRBF;
    }
    
    EXTI->PR |= EXTI_PR_BIT;
    cortexm_isr_end();
}

void isr_rtc_wkup(void)
{
    if (RTC->ISR & RTC_ISR_WUTF) {
        RTC->ISR &= ~RTC_ISR_WUTF;
        if (isr_ctx.cb_wkup != NULL) {
            isr_ctx.cb_wkup(isr_ctx.arg_wkup);
        }
    }
    
    EXTI->PR |= EXTI_PR_PR20;
    
    cortexm_isr_end();
}

#endif /* RTC */
