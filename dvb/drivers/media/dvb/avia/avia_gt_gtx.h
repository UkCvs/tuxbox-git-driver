#ifndef __GTX_H
#define __GTX_H

#define GTX_PHYSBASE    0x8000000
#define GTX_FB_OFFSET	0x0100000
		        


#define GTX_REG_GMR		0x00
#define GTX_REG_CLTA		0x04
#define GTX_REG_CLTD		0x06
#define GTX_REG_TCR		0x08
#define GTX_REG_CCR		0x0A
#define GTX_REG_GVSA		0x0C
#define GTX_REG_GVP		0x10
#define GTX_REG_GVS		0x14
#define GTX_REG_CSA		0x18
#define GTX_REG_CPOS		0x1C
#define GTX_REG_GFUNC		0x20

#define GTX_REG_VBR		0xF0
#define GTX_REG_VCR		0xF4
#define GTX_REG_VLC		0xF6
#define GTX_REG_VLI1		0xF8
#define GTX_REG_VHT		0xFA
#define GTX_REG_VLT		0xFC
#define GTX_REG_VLI2		0xFE

#define GTX_REG_ISR0		0x80
#define GTX_REG_ISR1		0x82
#define GTX_REG_ISR2		0x84
#define GTX_REG_ISR3		0x92

#define GTX_REG_IMR0		0x86
#define GTX_REG_IMR1		0x88
#define GTX_REG_IMR2		0x8A
#define GTX_REG_IMR3		0x94

#define GTX_REG_IPR0		0x8C
#define GTX_REG_IPR1		0x8E
#define GTX_REG_IPR2		0x90
#define GTX_REG_IPR3		0x94

#define GTX_REG_PCMA		0xe0
#define GTX_REG_PCMN		0xe4
#define GTX_REG_PCMC		0xe8
#define GTX_REG_PCMD		0xec

#define GTX_REG_RR0		0x100
#define GTX_REG_RR1		0x102
#define GTX_REG_CR0		0x104
#define GTX_REG_CR1		0x106

#define GTX_REG_C0CR		0x10C
#define GTX_REG_C1CR		0x10E

#define GTX_REG_DPCR		0x110

#define GTX_REG_PCRPID		0x120
#define GTX_REG_PCR2		0x122
#define GTX_REG_PCR1		0x124
#define GTX_REG_PCR0		0x126
#define GTX_REG_LSTC2		0x128
#define GTX_REG_LSTC1		0x12A
#define GTX_REG_LSTC0		0x12C
#define GTX_REG_STC2		0x12E
#define GTX_REG_STC1		0x130
#define GTX_REG_STC0		0x132

#define GTX_REG_FCR		0x134
#define GTX_REG_SYNCH		0x136
#define GTX_REG_PFIFO		0x138

#define GTX_REG_AVI		0x150

#define GTX_REG_QWPnL		0x180
#define GTX_REG_QWPnH		0x182

#define GTX_REG_QI0		0x1C0

#define GTX_REG_AQRPL		0x1E0
#define GTX_REG_AQRPH		0x1E2
#define GTX_REG_AQWPL		0x1E4
#define GTX_REG_AQWPH		0x1E6
#define GTX_REG_TQRPL		0x1E8
#define GTX_REG_TQRPH		0x1EA
#define GTX_REG_TQWPL		0x1EC
#define GTX_REG_TQWPH		0x1EE
#define GTX_REG_VQRPL		0x1F0
#define GTX_REG_VQRPH		0x1F2
#define GTX_REG_VQWPL		0x1F4
#define GTX_REG_VQWPH		0x1F6

#define GTX_REG_VSCA 		0x260
#define GTX_REG_VSCP		0x264
#define GTX_REG_VCS		0x268

#define GTX_REG_PTS0		0x280
#define GTX_REG_PTS1		0x282
#define GTX_REG_PTSO		0x284
#define GTX_REG_TTCR		0x286
#define GTX_REG_TTSR		0x288

#define GTX_REG_RISCPC		0x170
#define GTX_REG_RISCCON		0x178

#define GTX_RISC_RAM		0x1000


typedef struct {

    unsigned short Reserved1: 10;
    unsigned int Addr: 21;
    unsigned char E: 1;

} sGTX_REG_VCSA;

typedef struct {

    unsigned short HDEC: 4;
    unsigned char Reserved1: 2;
    unsigned short HSIZE: 9;
    unsigned char F: 1;
    unsigned char VDEC: 4;
    unsigned char Reserved2: 2;
    unsigned short VSIZE: 9;
    unsigned char B: 1;

} sGTX_REG_VCS;

typedef struct {

    unsigned char V: 1;
    unsigned char Reserved1: 5;
    unsigned short HPOS: 9;
    unsigned char Reserved2: 3;
    unsigned char OVOFFS: 4;
    unsigned short EVPOS: 9;
    unsigned char Reserved3: 1;

} sGTX_REG_VCSP;

typedef struct {

    unsigned short Reserved1: 10;
    unsigned int Addr: 21;
    unsigned char E: 1;

} sGTX_REG_VPSA;

typedef struct {

    unsigned short OFFSET;
    unsigned char Reserved1: 4;
    unsigned short STRIDE: 11;
    unsigned char B: 1;

} sGTX_REG_VPO;

typedef struct {

    unsigned char Reserved1: 6;
    unsigned short HPOS: 9;
    unsigned char Reserved2: 7;
    unsigned short VPOS: 9;
    unsigned char F: 1;

} sGTX_REG_VPP;

typedef struct {

    unsigned char Reserved1: 6;
    unsigned short WIDTH: 9;
    unsigned char S: 1;
    unsigned char Reserved2: 6;
    unsigned short HEIGHT: 9;
    unsigned char P: 1;

} sGTX_REG_VPS;


#define gtx_reg_16(register) ((__u16)(*((__u16*)(gtxenx_reg_base + GTX_REG_ ## register))))
#define gtx_reg_16n(offset) ((__u16)(*((__u16*)(gtxenx_reg_base + offset))))
#define gtx_reg_32(register) ((__u32)(*((__u32*)(gtxenx_reg_base + GTX_REG_ ## register))))
#define gtx_reg_32n(offset) ((__u32)(*((__u32*)(gtxenx_reg_base + offset))))
#define gtx_reg_s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_32s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_16s(register) ((sGTX_REG_##register *)(&gtx_reg_16(register)))

#endif
