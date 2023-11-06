//
// Created by Kurosu Chan on 2023/10/19.
//
#include "EspHal.h"
uint32_t spiFrequencyToClockDiv(uint32_t freq) {
  uint32_t apb_freq = getApbFrequency();
  if (freq >= apb_freq) {
    return SPI_CLK_EQU_SYSCLK;
  }

  const spiClk_t minFreqReg = {0x7FFFF000};
  uint32_t minFreq          = ClkRegToFreq((spiClk_t *)&minFreqReg);
  if (freq < minFreq) {
    return minFreqReg.value;
  }

  uint8_t calN     = 1;
  spiClk_t bestReg = {0};
  int32_t bestFreq = 0;
  while (calN <= 0x3F) {
    spiClk_t reg = {0};
    int32_t calFreq;
    int32_t calPre;
    int8_t calPreVari = -2;

    reg.clkcnt_n = calN;

    while (calPreVari++ <= 1) {
      calPre = (((apb_freq / (reg.clkcnt_n + 1)) / freq) - 1) + calPreVari;
      if (calPre > 0x1FFF) {
        reg.clkdiv_pre = 0x1FFF;
      } else if (calPre <= 0) {
        reg.clkdiv_pre = 0;
      } else {
        reg.clkdiv_pre = calPre;
      }
      reg.clkcnt_l = ((reg.clkcnt_n + 1) / 2);
      calFreq      = ClkRegToFreq(&reg);
      if (calFreq == (int32_t)freq) {
        memcpy(&bestReg, &reg, sizeof(bestReg));
        break;
      } else if (calFreq < (int32_t)freq) {
        if (RADIOLIB_ABS(freq - calFreq) < RADIOLIB_ABS(freq - bestFreq)) {
          bestFreq = calFreq;
          memcpy(&bestReg, &reg, sizeof(bestReg));
        }
      }
    }
    if (calFreq == (int32_t)freq) {
      break;
    }
    calN++;
  }
  return (bestReg.value);
}
uint32_t getApbFrequency() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);

  if (conf.freq_mhz >= 80) {
    return (80 * MHZ);
  }

  return ((conf.source_freq_mhz * MHZ) / conf.div);
}
