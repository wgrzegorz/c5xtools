*=============================================================================
*
*  cs4231.inc  -  CS4231 parallel stereo codec register map for the TMS320C5x
*  PIO interface (after TI SPRA097).  Included by codec_loopback.asm.
*
*  The codec occupies four DSP I/O ports (R0..R3).  Its internal registers are
*  reached through the index protocol: write the register index to R0, then the
*  value through R1.  R2 is the status port and R3 the PIO sample-data port.
*
*=============================================================================
CS4231_R0  .set  0              ; Index Address Register  (IAR)
CS4231_R1  .set  1              ; Indexed Data Register   (IDR)
CS4231_R2  .set  2              ; Status Register
CS4231_R3  .set  3              ; PIO sample Data Register
*  status-register bits (R2)
STAT_CAP   .set  020h           ; capture sample ready
STAT_PLY   .set  002h           ; playback sample needed
STAT_INIT  .set  080h           ; codec initialising
*  internal register indices (written through R0, data through R1)
IDX_LADC   .set  000h           ; left ADC input control
IDX_LDAC   .set  006h           ; left DAC output attenuation
IDX_FORMAT .set  008h           ; clock and data format
IDX_CONFIG .set  009h           ; interface configuration
IDX_PINCTL .set  00ah           ; pin control
*  bit fields (combined with | in the code, reference-assembler/c5xasm style)
FMT_16LIN  .set  040h           ; 16-bit linear PCM
FMT_MONO   .set  000h           ; mono
RATE_48KHZ .set  00ch           ; 48 kHz sample rate
LADC_MIC   .set  020h           ; left ADC source = microphone
ADC_GAIN2  .set  002h           ; +1 gain step
CFG_CAP    .set  001h           ; capture enable
CFG_PLAY   .set  002h           ; playback enable
