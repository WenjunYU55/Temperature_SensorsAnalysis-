/* main.c
 *
 *  - MAX31855  		PB5 (SPI1, CS)
 *  - TMP36          	PA0 (ADC1 IN0)
 *  - PT1000 		    PA1 (ADC1 IN1)
 *  - MLX90614        	PB8/PB9 (I2C1)
 *
 */

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --- Global handles ----------------------------------------------------- */
SPI_HandleTypeDef   hspi1;
I2C_HandleTypeDef   hi2c1;
ADC_HandleTypeDef   hadc1;
UART_HandleTypeDef  huart2;
DMA_HandleTypeDef   hdma_i2c1_tx;
DMA_HandleTypeDef   hdma_i2c1_rx;
DMA_HandleTypeDef   hdma_spi1_tx;

/* --- Pin definitions ---------------------------------------------------- */

#define MAX31855_CS_GPIO_Port   GPIOB
#define MAX31855_CS_Pin         GPIO_PIN_5

#define MLX90614_I2C_ADDR       (0x5A << 1)

#define ADC_VREF        3.3f
#define ADC_MAX         4095.0f

#define PT1000_R0       1000.0f        // PT1000 at 0°C
#define PT_REF_R        975.0f        // divider resistor
#define PT_ALPHA        0.00385f      // 1/°C

#define CVD_A                   3.9083e-3f
#define CVD_B                  -5.775e-7f

/* --- Function prototypes ------------------------------------------------ */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
void Error_Handler(void);

/* Sensor prototypes  ----------------------------------------------------- */
float MAX31855_ReadTemp(void);
float TMP36_ReadTemp(void);
float PT1000_ReadTemp(void);
float MLX90614_ReadTemp(void);

/* --- Main ---------------------------------------------------------------- */
int main(void)
{
    /* HAL + clock init */
    HAL_Init();
    SystemClock_Config();

    /* Peripheral inits */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();

    char  uart_buf[128];
    float t1, t2, t3, t4;

    while (1)
    {
        /* Read sensors */
        t1 = MAX31855_ReadTemp();  /* thermocouple */
        t2 = TMP36_ReadTemp();     /* analog */
        t3 = PT1000_ReadTemp();     /* rtd */
        t4 = MLX90614_ReadTemp();  /* IR */

        /* Convert to integer °C * 100 */
        int32_t i1 = (int32_t)roundf(t1 * 100.0f);
        int32_t i2 = (int32_t)roundf(t2 * 100.0f);
        int32_t i3 = (int32_t)roundf(t3 * 100.0f);
        int32_t i4 = (int32_t)roundf(t4 * 100.0f);

        int len = snprintf(uart_buf, sizeof(uart_buf),
                           "%ld,%ld,%ld,%ld\r\n",
                           (long)i1, (long)i2, (long)i3, (long)i4);

        if (len > 0)
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, len, 100);
        }

        HAL_Delay(200); /* 5 Hz output */
    }
}

/* --- System Clock: simple HSI at 16 MHz (no PLL) ------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /* Configure HSI as clock source */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Set HCLK, PCLK1, PCLK2 prescalers */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK   |
                                       RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/* --- GPIO init ----------------------------------------------------------- */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* --- Chip select pin for MAX31855 (output, default high) --- */
    GPIO_InitStruct.Pin   = MAX31855_CS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, MAX31855_CS_Pin, GPIO_PIN_SET);

    /* --- SPI1 pins: PA5 SCK, PA6 MISO, PA7 MOSI (AF5) --- */
    GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* --- I2C1 pins: PB8 SCL, PB9 SDA (AF4, open-drain) --- */
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* --- USART2 pins: PA2 TX, PA3 RX (AF7) --- */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* --- Analog inputs: PA0 (TMP36), PA1 (PT1000) --- */
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* --- SPI1 init ----------------------------------------------------------- */

static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; /* ~500 kHz at 16 MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* --- I2C1 init ----------------------------------------------------------- */

static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000; /* 100 kHz */
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* --- ADC1 init ----------------------------------------------------------- */

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                      = ADC1;
    hadc1.Init.ClockPrescaler          = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution              = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode            = DISABLE;
    hadc1.Init.ContinuousConvMode      = DISABLE;
    hadc1.Init.DiscontinuousConvMode   = DISABLE;
    hadc1.Init.NbrOfDiscConversion     = 0;
    hadc1.Init.ExternalTrigConvEdge    = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv        = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign               = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion         = 1;
    hadc1.Init.DMAContinuousRequests   = DISABLE;
    hadc1.Init.EOCSelection            = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* We will configure the channel before each read (TMP36 vs PT1000),
       so no static channel config here. */
}

/* --- USART2 init (115200 8N1) ------------------------------------------- */

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* --- DMA init (clocks only) --------------------------------------------- */

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
}

/* --- MAX31855: thermocouple read (SPI, 32-bit frame) -------------------- */

float MAX31855_ReadTemp(void)
{
    uint8_t  rx[4] = {0};
    uint32_t raw;
    int16_t  value;

    HAL_GPIO_WritePin(MAX31855_CS_GPIO_Port, MAX31855_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Receive(&hspi1, rx, 4, 100);
    HAL_GPIO_WritePin(MAX31855_CS_GPIO_Port, MAX31855_CS_Pin, GPIO_PIN_SET);

    raw = ((uint32_t)rx[0] << 24) |
          ((uint32_t)rx[1] << 16) |
          ((uint32_t)rx[2] << 8)  |
          (uint32_t)rx[3];

    /* Fault bit (D16) */
    if (raw & 0x00010000)
    {
        return NAN;
    }

    /* Bits 31..18: signed 14-bit thermocouple temperature, 0.25°C/LSB */
    value = (int16_t)(raw >> 18);

    return (float)value * 0.25f;
}

/* --- Helper: averaged ADC read on one channel --------------------------- */

static uint32_t ADC_ReadChannelAveraged(uint32_t channel, uint8_t samples)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t sum = 0;

    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    for (uint8_t i = 0; i < samples; i++)
    {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            sum += HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }

    return sum / samples;
}


/* --- TMP36: analog temp sensor on ADC1/PA0 ------------------------------ */

float TMP36_ReadTemp(void)
{
    uint32_t adc_val = ADC_ReadChannelAveraged(ADC_CHANNEL_0, 16);
    float vout  = ADC_VREF * (float)adc_val / ADC_MAX;
    float tempC = 113.15f * vout - 59.416f;
    return tempC;
}


/* --- PT1000 direct analog read on PA1 / ADC1_IN1 ------------------------ */

#define PT1000_FILTER_ALPHA      0.07f   /* 0..1, smaller = smoother */
#define PT1000_SPIKE_THRESHOLD   5.0f   /* °C difference allowed vs filtered */

float PT1000_ReadTemp(void)
{
    uint32_t adc_val;
    float vmeas, r_pt, t_raw;
    static float t_filtered = NAN;

    /* Take 16 samples and average to reduce noise */
    adc_val = ADC_ReadChannelAveraged(ADC_CHANNEL_1, 16);

    /* 12-bit ADC, Vref = 3.3 V */
    vmeas = ADC_VREF * (float)adc_val / ADC_MAX;

    if (vmeas <= 0.01f || vmeas >= 3.29f)
    {
        return NAN;
    }

    /* Voltage divider: R_PT = R_REF * V / (3.3 - V) */
    r_pt = PT_REF_R * vmeas / (ADC_VREF - vmeas);

    /* Linear PT1000 model: T = (R/R0 - 1) / α */
    t_raw = (r_pt / PT1000_R0 - 1.0f) / PT_ALPHA;

    /* Initialise filtered value at first run */
    if (isnan(t_filtered))
    {
        t_filtered = t_raw;
        return t_filtered;
    }

    /* --- Spike rejection: ignore absurd jumps -------------------------- */
    float diff = t_raw - t_filtered;
    if (diff > PT1000_SPIKE_THRESHOLD || diff < -PT1000_SPIKE_THRESHOLD)
    {
        /* Ignore this sample: just return previous filtered value */
        return t_filtered;
    }

    /* Simple IIR low-pass filter on temperature */
    t_filtered = PT1000_FILTER_ALPHA * t_raw +
                 (1.0f - PT1000_FILTER_ALPHA) * t_filtered;

    return t_filtered;
}

/* --- MLX90614 Temperature Readout -------------------------------------- */

float MLX90614_ReadTemp(void)
{
    uint8_t  buf[3];   /* low, high, PEC */
    uint16_t raw;
    float    tempK;
    float    tempC;

    if (HAL_I2C_Mem_Read(&hi2c1, MLX90614_I2C_ADDR,
                         0x07, I2C_MEMADD_SIZE_8BIT,
                         buf, 3, 100) != HAL_OK)
    {
        return NAN;
    }

    raw = ((uint16_t)buf[1] << 8) | buf[0];

    tempK = (float)raw * 0.02f;      /* 0.02 K/LSB */
    tempC = tempK - 273.15f;

    return tempC;
}
/* --- Error handler ------------------------------------------------------- */

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */
