/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Peripheral setup and initialization (incl. interrupts)
 */

#include "setup.h"
#include "main.h"

SPI_HandleTypeDef hspi;
DMA_HandleTypeDef hdma_spi_rx;
TIM_HandleTypeDef htim2;

void SystemClock_Config();
static void GPIO_Init();
static void DMA_Init();
static void SPI2_Init();
static void TIM2_Init();

void setup()
{
    HAL_Init();

    SystemClock_Config();

    GPIO_Init();
    DMA_Init();
    SPI2_Init();
    TIM2_Init();

#if Serial == USBSerial
    USBSerial.Init();
#elif Serial == Uart
    Uart.Init();
#endif
}

// Initializes the Global MSP.
extern "C" void HAL_MspInit()
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    // NOJTAG: JTAG-DP Disabled and SW-DP Enabled
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void SystemClock_Config()
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    // Initializes the CPU, AHB and APB busses clocks
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // Initializes the CPU, AHB and APB busses clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    HAL_RCC_EnableCSS();
}

static void GPIO_Init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // GPIO Ports Clock Enable
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Configure DIO0 & DIO1 pin
    GPIO_InitStruct.Pin = DIO0_PIN | DIO1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    HAL_GPIO_Init(DIO0_GPIO_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI_DIO0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI_DIO0_IRQn);

    HAL_NVIC_SetPriority(EXTI_DIO1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI_DIO1_IRQn);
}

static void SPI2_Init()
{
    hspi.Instance = SPI2;
    hspi.Init.Mode = SPI_MODE_SLAVE;
    hspi.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
    hspi.Init.DataSize = SPI_DATASIZE_8BIT;
#if SPI_MODE == 0 || SPI_MODE == 1
    hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
#else
    hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
#endif
#if SPI_MODE == 0 || SPI_MODE == 2
    hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
#else
    hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
#endif
    hspi.Init.NSS = SPI_NSS_HARD_INPUT;
    hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi);

    HAL_NVIC_SetPriority(EXTI_NSS_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI_NSS_IRQn);
}

extern "C" void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hspi->Instance == SPI_INSTANCE)
    {
        // Peripheral clock enable
        __HAL_RCC_SPI2_CLK_ENABLE();

        // SPI2 GPIO Configuration
        // PB12     ------> SPI2_NSS
        // PB13     ------> SPI2_SCK
        // PB15     ------> SPI2_MOSI
        GPIO_InitStruct.Pin = SPI_NSS_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(SPI_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = SPI_SCK_PIN | SPI_MOSI_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(SPI_PORT, &GPIO_InitStruct);

        // SPI1 DMA Init
        // SPI1_RX Init
        hdma_spi_rx.Instance = DMA_SPI_Instance;
        hdma_spi_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_spi_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi_rx.Init.Mode = DMA_CIRCULAR;
        hdma_spi_rx.Init.Priority = DMA_PRIORITY_LOW;
        HAL_DMA_Init(&hdma_spi_rx);

        __HAL_LINKDMA(hspi, hdmarx, hdma_spi_rx);
    }
}

extern "C" void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI_INSTANCE)
    {
        // Peripheral clock disable
        __HAL_RCC_SPI2_CLK_DISABLE();

        // SPI2 GPIO Configuration
        // PB12     ------> SPI2_NSS
        // PB13     ------> SPI2_SCK
        // PB15     ------> SPI2_MOSI
        HAL_GPIO_DeInit(SPI_PORT, SPI_NSS_PIN | SPI_SCK_PIN | SPI_MOSI_PIN);

        // SPI1 DMA DeInit
        HAL_DMA_DeInit(hspi->hdmarx);
    }
}

void DMA_Init()
{
    // DMA controller clock enable
    __HAL_RCC_DMA1_CLK_ENABLE();

    // DMA interrupt init
    // DMA channel IRQ interrupt configuration
    HAL_NVIC_SetPriority(DMA_SPI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA_SPI_IRQn);
}

void TIM2_Init()
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

    HAL_TIM_PWM_Init(&htim2);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 500;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2);

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // TIM2 GPIO Configuration
    // PA1     ------> TIM2_CH2
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
}

extern "C" void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
    if (htim_base->Instance == TIM2)
    {
        // Peripheral clock enable
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

extern "C" void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *htim_base)
{
    if (htim_base->Instance == TIM2)
    {
        // Peripheral clock disable
        __HAL_RCC_TIM2_CLK_DISABLE();
    }
}

extern "C" void NMI_Handler()
{
    HAL_RCC_NMI_IRQHandler();
}

extern "C" void HardFault_Handler()
{
    while (true)
    {
    }
}

extern "C" void MemManage_Handler()
{
    while (true)
    {
    }
}

extern "C" void BusFault_Handler()
{
    while (true)
    {
    }
}

extern "C" void UsageFault_Handler()
{
    while (true)
    {
    }
}

extern "C" void SVC_Handler()
{
}

extern "C" void DebugMon_Handler()
{
}

extern "C" void PendSV_Handler()
{
}

extern "C" void EXTI_NSS_IRQHandler()
{
    SpiTrxCompleted();
    HAL_GPIO_EXTI_IRQHandler(SPI_NSS_PIN);
}

extern "C" void DMA_SPI_IRQHandler()
{
    HAL_DMA_IRQHandler(&hdma_spi_rx);
}
