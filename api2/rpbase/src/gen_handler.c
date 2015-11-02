/**
* $Id: $
*
* @brief Red Pitaya library Generate handler interface
*
* @Author Red Pitaya
*
* (c) Red Pitaya  http://www.redpitaya.com
*
* This part of code is written in C programming language.
* Please visit http://en.wikipedia.org/wiki/C_(programming_language)
* for more details on the language used herein.
*/

#include <float.h>
#include <math.h>
#include "common.h"
#include "generate.h"
#include "gen_handler.h"

#include "redpitaya/rp.h"

// global variables
// TODO: should be organized into a system status structure
float         ch_dutyCycle       [2] = {0,0};
float         ch_frequency       [2] = {0,0};
float         ch_phase           [2] = {0,0};
int           ch_burstCount      [2] = {1,1};
int           ch_burstRepetition [2] = {1,1};
uint32_t      ch_burstPeriod     [2] = {0,0};
rp_waveform_t ch_waveform        [2]        ;
uint32_t      ch_size            [2] = {BUFFER_LENGTH, BUFFER_LENGTH};
uint32_t      ch_arb_size        [2] = {BUFFER_LENGTH, BUFFER_LENGTH};

float ch_arbitraryData[2][BUFFER_LENGTH];

int gen_SetDefaultValues() {
    gen_Disable(RP_CH_1);
    gen_Disable(RP_CH_2);
    rp_GenFreq(RP_CH_1, 1000);
    rp_GenFreq(RP_CH_2, 1000);
    gen_setBurstRepetitions(RP_CH_1, 1);
    gen_setBurstRepetitions(RP_CH_2, 1);
    gen_setBurstPeriod(RP_CH_1, (uint32_t) (1 / 1000.0 * MICRO));   // period = 1/frequency in us
    gen_setBurstPeriod(RP_CH_2, (uint32_t) (1 / 1000.0 * MICRO));   // period = 1/frequency in us
    gen_setWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    gen_setWaveform(RP_CH_2, RP_WAVEFORM_SINE);
    rp_GenOffset(RP_CH_1, 0);
    rp_GenOffset(RP_CH_2, 0);
    rp_GenAmp(RP_CH_1, 1);
    rp_GenAmp(RP_CH_2, 1);
    gen_setDutyCycle(RP_CH_1, 0.5);
    gen_setDutyCycle(RP_CH_2, 0.5);
    gen_setGenMode(RP_CH_1, RP_GEN_MODE_CONTINUOUS);
    gen_setGenMode(RP_CH_2, RP_GEN_MODE_CONTINUOUS);
    gen_setBurstCount(RP_CH_1, 1);
    gen_setBurstCount(RP_CH_2, 1);
    gen_setBurstPeriod(RP_CH_1, BURST_PERIOD_MIN);
    gen_setBurstPeriod(RP_CH_2, BURST_PERIOD_MIN);
    gen_setTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_INTERNAL);
    gen_setTriggerSource(RP_CH_2, RP_GEN_TRIG_SRC_INTERNAL);
    gen_setPhase(RP_CH_1, 0.0);
    gen_setPhase(RP_CH_2, 0.0);
    return RP_OK;
}

int gen_Disable(rp_channel_t channel) {
    return generate_setOutputDisable(channel, true);
}

int gen_Enable(rp_channel_t channel) {
    return generate_setOutputDisable(channel, false);
}

int gen_IsEnable(rp_channel_t channel, bool *value) {
    return generate_getOutputEnabled(channel, value);
}

int gen_checkAmplitudeAndOffset(float amplitude, float offset) {
    if (fabs(amplitude) + fabs(offset) > LEVEL_MAX) {
        return RP_EOOR;
    }
    return RP_OK;
}

int gen_setWaveform(rp_channel_t channel, rp_waveform_t type) {
    ch_waveform[channel] = type;
    if (type == RP_WAVEFORM_ARBITRARY) {
        ch_size[channel] = ch_arb_size[channel];
    } else {
        ch_size[channel] = BUFFER_LENGTH;
    }
    return synthesize_signal(channel);
}

int gen_getWaveform(rp_channel_t channel, rp_waveform_t *type) {
    *type = ch_waveform[channel];
    return RP_OK;
}

int gen_setArbWaveform(rp_channel_t channel, float *data, uint32_t length) {
    // Check if data is normalized
    float min = FLT_MAX, max = -FLT_MAX; // initial values
    int i;
    for(i = 0; i < length; i++) {
        if (data[i] < min)
            min = data[i];
        if (data[i] > max)
            max = data[i];
    }
    if (min < ARBITRARY_MIN || max > ARBITRARY_MAX) {
        return RP_ENN;
    }

    // Save data
    float *pointer;
    pointer = ch_arbitraryData[channel];
    for(i = 0; i < length; i++) {
        pointer[i] = data[i];
    }
    for(i = length; i < BUFFER_LENGTH; i++) { // clear the rest of the buffer
        pointer[i] = 0;
    }
    if (channel > RP_CH_2) {
        return RP_EPN;
    }
    ch_arb_size[channel] = length;
    if(ch_waveform[channel]==RP_WAVEFORM_ARBITRARY){
    	return synthesize_signal(channel);
    }

    return RP_OK;
}

int gen_getArbWaveform(rp_channel_t channel, float *data, uint32_t *length) {
    // If this data was not set, then this method will return incorrect data
    float *pointer;
    if (channel > RP_CH_2) {
        return RP_EPN;
    }
    *length = ch_arb_size[channel];
    pointer = ch_arbitraryData[channel];
    for (int i = 0; i < *length; ++i) {
        data[i] = pointer[i];
    }
    return RP_OK;
}

int gen_setDutyCycle(rp_channel_t channel, float ratio) {
    if (ratio < DUTY_CYCLE_MIN || ratio > DUTY_CYCLE_MAX) {
        return RP_EOOR;
    }
    ch_dutyCycle[channel] = ratio;
    return synthesize_signal(channel);
}

int gen_getDutyCycle(rp_channel_t channel, float *ratio) {
    *ratio = ch_dutyCycle[channel];
    return RP_OK;
}

int gen_setGenMode(rp_channel_t channel, rp_gen_mode_t mode) {
    if (mode == RP_GEN_MODE_CONTINUOUS) {
        generate_setGatedBurst(channel, 0);
        generate_setBurstDelay(channel, 0);
        generate_setBurstRepetitions(channel, 0);
        generate_setBurstCount(channel, 0);
        return triggerIfInternal(channel);
    }
    else if (mode == RP_GEN_MODE_BURST) {
        gen_setBurstCount(channel, ch_burstCount[channel]);
        gen_setBurstRepetitions(channel, ch_burstRepetition[channel]);
        gen_setBurstPeriod(channel, ch_burstPeriod[channel]);
        return RP_OK;
    }
    else if (mode == RP_GEN_MODE_STREAM) {
        return RP_EUF;
    }
    else {
        return RP_EIPV;
    }
}

int gen_getGenMode(rp_channel_t channel, rp_gen_mode_t *mode) {
    uint32_t num;
    generate_getBurstCount(channel, &num);
    if (num != 0) {
        *mode = RP_GEN_MODE_BURST;
    }
    else {
        *mode = RP_GEN_MODE_CONTINUOUS;
    }
    return RP_OK;
}

int gen_setBurstCount(rp_channel_t channel, int num) {
    if ((num < BURST_COUNT_MIN || num > BURST_COUNT_MAX) && num == 0) {
        return RP_EOOR;
    }
    ch_burstCount[channel] = num;
    if (num == -1) {    // -1 represents infinity. In FPGA value 0 represents infinity
        num = 0;
    }
    generate_setBurstCount(channel, (uint32_t) num);

    // trigger channel if internal trigger source
    return triggerIfInternal(channel);
}

int gen_getBurstCount(rp_channel_t channel, int *num) {
    return generate_getBurstCount(channel, (uint32_t *) num);
}

int gen_setBurstRepetitions(rp_channel_t channel, int repetitions) {
    if ((repetitions < BURST_REPETITIONS_MIN || repetitions > BURST_REPETITIONS_MAX) && repetitions != -1) {
        return RP_EOOR;
    }
    ch_burstRepetition[channel] = repetitions;
    if (repetitions == -1) {
        repetitions = 0;
    }
    generate_setBurstRepetitions(channel, (uint32_t) (repetitions-1));

    // trigger channel if internal trigger source
    return triggerIfInternal(channel);
}

int gen_getBurstRepetitions(rp_channel_t channel, int *repetitions) {
    uint32_t tmp;
    generate_getBurstRepetitions(channel, &tmp);
    *repetitions = tmp+1;
    return RP_OK;
}

int gen_setBurstPeriod(rp_channel_t channel, uint32_t period) {
    if (period < BURST_PERIOD_MIN || period > BURST_PERIOD_MAX) {
        return RP_EOOR;
    }
    int burstCount;
    burstCount = ch_burstCount[channel];
    // period = signal_time * burst_count + delay_time
    int delay = (int) (period - (1 / ch_frequency[channel] * MICRO) * burstCount);
    if (delay <= 0) {
        // if delay is 0, then FPGA generates continuous signal
        delay = 1;
    }
    generate_setBurstDelay(channel, (uint32_t) delay);

    ch_burstPeriod[channel] = period;

    // trigger channel if internal trigger source
    return triggerIfInternal(channel);
}

int gen_getBurstPeriod(rp_channel_t channel, uint32_t *period) {
    uint32_t delay, burstCount;
    float frequency;
    generate_getBurstDelay(channel, &delay);
    generate_getBurstCount(channel, &burstCount);
    generate_getFrequency(channel, &frequency);

    if (delay == 1) {    // if delay is 0, then FPGA generates continuous signal
        delay = 0;
    }
    *period = (uint32_t) (delay + (1 / frequency * MICRO) * burstCount);
    return RP_OK;
}

int gen_setTriggerSource(rp_channel_t channel, rp_trig_src_t src) {
    if (src == RP_GEN_TRIG_GATED_BURST) {
        generate_setGatedBurst(channel, 1);
        gen_setGenMode(channel, RP_GEN_MODE_BURST);
        return generate_setTriggerSource(channel, 2);
    }
    else {
        generate_setGatedBurst(channel, 0);
    }

    if (src == RP_GEN_TRIG_SRC_INTERNAL) {
        gen_setGenMode(channel, RP_GEN_MODE_CONTINUOUS);
        return generate_setTriggerSource(channel, 1);
    }
    else if (src == RP_GEN_TRIG_SRC_EXT_PE) {
        gen_setGenMode(channel, RP_GEN_MODE_BURST);
        return generate_setTriggerSource(channel, 2);
    }
    else if (src == RP_GEN_TRIG_SRC_EXT_NE) {
        gen_setGenMode(channel, RP_GEN_MODE_BURST);
        return generate_setTriggerSource(channel, 3);
    }
    else {
        return RP_EIPV;
    }
}

int gen_getTriggerSource(rp_channel_t channel, rp_trig_src_t *src) {
    uint32_t gated;
    generate_getGatedBurst(channel, &gated);
    if (gated == 1) {
        *src = RP_GEN_TRIG_GATED_BURST;
    }
    else {
        generate_getTriggerSource(channel, (uint32_t *) &src);
    }
    return RP_OK;
}

int gen_Trigger(uint32_t channel) {
    switch (channel) {
        case 0:
        case 1:
            gen_setGenMode(RP_CH_1, RP_GEN_MODE_BURST);
            return generate_setTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_INTERNAL);
        case 2:
            gen_setGenMode(RP_CH_2, RP_GEN_MODE_BURST);
            return generate_setTriggerSource(RP_CH_2, RP_GEN_TRIG_SRC_INTERNAL);
        case 3:
            gen_setGenMode(RP_CH_1, RP_GEN_MODE_BURST);
            gen_setGenMode(RP_CH_2, RP_GEN_MODE_BURST);
            return generate_simultaneousTrigger();
        default:
            return RP_EOOR;
    }
}

int gen_Synchronise() {
    return generate_Synchronise();
}

int synthesize_signal(rp_channel_t channel) {
    float data[BUFFER_LENGTH];
    rp_waveform_t waveform;
    float dutyCycle, frequency;
    uint32_t size, phase;

    if (channel > RP_CH_2) {
        return RP_EPN;
    }
    waveform = ch_waveform[channel];
    dutyCycle = ch_dutyCycle[channel];
    frequency = ch_frequency[channel];
    size = ch_size[channel];
    phase = (uint32_t) (ch_phase[channel] * BUFFER_LENGTH / 360.0);

    switch (waveform) {
        case RP_WAVEFORM_SINE     : synthesis_sin      (data);                 break;
        case RP_WAVEFORM_TRIANGLE : synthesis_triangle (data);                 break;
        case RP_WAVEFORM_SQUARE   : synthesis_square   (frequency, data);      break;
        case RP_WAVEFORM_RAMP_UP  : synthesis_rampUp   (data);                 break;
        case RP_WAVEFORM_RAMP_DOWN: synthesis_rampDown (data);                 break;
        case RP_WAVEFORM_DC       : synthesis_DC       (data);                 break;
        case RP_WAVEFORM_PWM      : synthesis_PWM      (dutyCycle, data);      break;
        case RP_WAVEFORM_ARBITRARY: synthesis_arbitrary(channel, data, &size); break;
        default:                    return RP_EIPV;
    }
    return generate_writeData(channel, data, phase, size);
}

int synthesis_sin(float *data_out) {
    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        data_out[i] = (float) (sin(2 * M_PI * (float) i / (float) BUFFER_LENGTH));
    }
    return RP_OK;
}

int synthesis_triangle(float *data_out) {
    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        data_out[i] = (float) ((asin(sin(2 * M_PI * (float) i / (float) BUFFER_LENGTH)) / M_PI * 2));
    }
    return RP_OK;
}

int synthesis_rampUp(float *data_out) {
    data_out[BUFFER_LENGTH -1] = 0;
    for(int unsigned i = 0; i < BUFFER_LENGTH-1; i++) {
        data_out[BUFFER_LENGTH - i-2] = (float) (-1.0 * (acos(cos(M_PI * (float) i / (float) BUFFER_LENGTH)) / M_PI - 1));
    }
    return RP_OK;
}

int synthesis_rampDown(float *data_out) {
    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        data_out[i] = (float) (-1.0 * (acos(cos(M_PI * (float) i / (float) BUFFER_LENGTH)) / M_PI - 1));
    }
    return RP_OK;
}

int synthesis_DC(float *data_out) {
    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        data_out[i] = 1.0;
    }
    return RP_OK;
}

int synthesis_PWM(float ratio, float *data_out) {
    // calculate number of samples that need to be high
    int h = (int) (BUFFER_LENGTH/2 * ratio);

    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        if (i < h || i >= BUFFER_LENGTH - h) {
            data_out[i] = 1.0;
        }
        else {
            data_out[i] = (float) -1.0;
        }
    }
    return RP_OK;
}

int synthesis_arbitrary(rp_channel_t channel, float *data_out, uint32_t * size) {
    float *pointer;
    pointer = ch_arbitraryData[channel];
    for (int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        data_out[i] = pointer[i];
    }
    *size = ch_arb_size[channel];
    return RP_OK;
}

int synthesis_square(float frequency, float *data_out) {
    // Various locally used constants - HW specific parameters
    const int trans0 = 30;
    const int trans1 = 300;

    int trans = (int) (frequency / 1e6 * trans1); // 300 samples at 1 MHz

    if (trans <= 10)  trans = trans0;

    for(int unsigned i = 0; i < BUFFER_LENGTH; i++) {
        if      ((0 <= i                      ) && (i <  BUFFER_LENGTH/2 - trans))  data_out[i] =  1.0f;
        else if ((i >= BUFFER_LENGTH/2 - trans) && (i <  BUFFER_LENGTH/2        ))  data_out[i] =  1.0f - (2.0f / trans) * (i - (BUFFER_LENGTH/2 - trans));
        else if ((0 <= BUFFER_LENGTH/2        ) && (i <  BUFFER_LENGTH   - trans))  data_out[i] = -1.0f;
        else if ((i >= BUFFER_LENGTH   - trans) && (i <  BUFFER_LENGTH          ))  data_out[i] = -1.0f + (2.0f / trans) * (i - (BUFFER_LENGTH   - trans));
    }

    return RP_OK;
}

int triggerIfInternal(rp_channel_t channel) {
    uint32_t value;
    generate_getTriggerSource(channel, &value);
    if (value == RP_GEN_TRIG_SRC_INTERNAL) {
        generate_setTriggerSource(channel, 1);
    }
    return RP_OK;
}
