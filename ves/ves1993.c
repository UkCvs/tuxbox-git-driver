/* 
    VES1993  - Single Chip Satellite Channel Receiver driver module
               
    Copyright (C) 2001 Ronny Strutz  <3DES@tuxbox.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/    

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include <dbox/dvb.h>

#include <dbox/ves.h>

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

static void ves_write_reg(int reg, int val);
static void ves_init(void);
static void ves_set_frontend(struct frontend *front);
static void ves_get_frontend(struct frontend *front);
static void ves_reset(void);
static int ves_read_reg(int reg);

struct demod_function_struct ves1993={ves_write_reg, ves_init, ves_set_frontend, ves_get_frontend, ves_get_unc_packet};

static int debug = 9;
#define dprintk	if (debug) printk

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template, *dclient;

static u8 Init1993Tab[] =
{
        0x00, 0x9c, 0x35, 0x80, 0x6a, 0x2b, 0xab, 0xaa,
        0x0e, 0x45, 0x00, 0x00, 0x4c, 0x0a, 0x00, 0x00,
        0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
        0x80, 0x40, 0x21, 0xb0, 0x00, 0x00, 0x00, 0x10,
        0x89, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x55, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x0c, 0x80, 0x00
};

static u8 Init1993WTab[] =
{     //0 1 2 3 4 5 6 7  8 9 a b c d e f
        0,1,1,1,1,1,1,1, 1,1,0,0,1,1,0,0,
        0,1,0,0,0,0,0,0, 1,1,1,1,0,0,0,1,
        1,1,1,0,0,0,0,0, 0,0,1,1,0,0,0,0,
        1,1,1,0,1,1,1,1, 1,1,1,1,1
};

struct ves1993 {
        u32 srate;
        u8 ctr;
        u8 fec;
        u8 inv;
};

static int writereg(struct i2c_client *client, int reg, int data)
{
        int ret;
        unsigned char msg[] = {0x00, 0x1f, 0x00};
        
        msg[1]=reg; msg[2]=data;
        ret=i2c_master_send(client, msg, 3);
        printk("VES_writereg\n");
	if (ret!=3) 
                printk("writereg error\n");
        return ret;
}

static u8 readreg(struct i2c_client *client, u8 reg)
{
        struct i2c_adapter *adap=client->adapter;
        unsigned char mm1[] = {0x00, 0x1e};
        unsigned char mm2[] = {0x00};
        struct i2c_msg msgs[2];
        
        msgs[0].flags=0;
        msgs[1].flags=I2C_M_RD;
        msgs[0].addr=msgs[1].addr=client->addr;
        mm1[1]=reg;
        msgs[0].len=2; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
	i2c_transfer(adap, msgs, 2);
        
        return mm2[0];
}


static int dump(struct i2c_client *client)
{
        int i;
        
        printk("ves1993: DUMP\n");
        
        for (i=0; i<54; i++) 
        {
                printk("%02x ", readreg(client, i));
                if ((i&7)==7)
                        printk("\n");
        }
        printk("\n");
        return 0;
}


static int init(struct i2c_client *client)
{
        struct ves1993 *ves=(struct ves1993 *) client->data;
	int i;
	        
        dprintk("ves1993: init chip\n");

        if (writereg(client, 0, 0)<0)
                printk("ves1993: send error\n");
		
	//Init fuer VES1993
	writereg(client,0x3a, 0x0c);	
/*	writereg(client,0x01, 0x9c);	
	writereg(client,0x02, 0x35);		
	writereg(client,0x03, 0x80);
	writereg(client,0x04, 0x6a);
	writereg(client,0x05, 0x2b);
	writereg(client,0x06, 0xab);
	writereg(client,0x07, 0xaa);
	writereg(client,0x08, 0x0e);
	writereg(client,0x09, 0x45);
	writereg(client,0x0c, 0x4c);
	writereg(client,0x0d, 0x0a);
	writereg(client,0x11, 0x81);
	writereg(client,0x18, 0x80);
	writereg(client,0x19, 0x40);
	writereg(client,0x1a, 0x21);
	writereg(client,0x1b, 0xb0);
	writereg(client,0x1f, 0x10);
	writereg(client,0x20, 0x89);
	writereg(client,0x21, 0x81);
	writereg(client,0x22, 0x00);
	writereg(client,0x2a, 0x00);
	writereg(client,0x2b, 0x00);
	writereg(client,0x30, 0x02);
	writereg(client,0x31, 0x55);
	writereg(client,0x32, 0x03);
	writereg(client,0x34, 0x00);
	writereg(client,0x35, 0x00);
	writereg(client,0x36, 0x00);
	writereg(client,0x37, 0x03);
	writereg(client,0x38, 0x00);
	writereg(client,0x39, 0x00);
	writereg(client,0x3a, 0x0c);
	writereg(client,0x3b, 0x80);
	writereg(client,0x3c, 0x00);
	writereg(client,0x00, 0x01);
	writereg(client,0x00, 0x11);
//	sagem_set_tuner_dword(msg);
	writereg(client,0x00, 0x01);
	
	
	writereg(client,0x3a, 0x0e);
	writereg(client,0x21, 0x81);
	writereg(client,0x00, 0x00);
	writereg(client,0x06, 0x72);
	writereg(client,0x07, 0x8c);
	writereg(client,0x08, 0x09);
	writereg(client,0x09, 0x6b);
	writereg(client,0x20, 0x81);
	writereg(client,0x21, 0x80);
	writereg(client,0x00, 0x01);
	writereg(client,0x0d, 0x0a);

*/

        for (i=0; i<0x3d; i++)
                if (Init1993WTab[i])
        	    writereg(client, i, Init1993Tab[i]);
		    
		    
	writereg(client,0x3a, 0x0e);
	writereg(client,0x21, 0x81);
	writereg(client,0x00, 0x00);
	writereg(client,0x06, 0x72);
	writereg(client,0x07, 0x8c);
	writereg(client,0x08, 0x09);
	writereg(client,0x09, 0x6b);
	writereg(client,0x20, 0x81);
	writereg(client,0x21, 0x80);
	writereg(client,0x00, 0x01);
	writereg(client,0x0d, 0x0a);
	    
		    
        ves->ctr=Init1993Tab[0x1f];
        ves->srate=0;
        ves->fec=9;
        ves->inv=0;

        return 0;
}

static inline void ddelay(int i) 
{
        current->state=TASK_INTERRUPTIBLE;
        schedule_timeout((HZ*i)/100);
}

static void ClrBit1893(struct i2c_client *client)
{
        printk("VES_clrbit1893\n");
        ddelay(2);
        writereg(client, 0, Init1993Tab[0] & 0xfe);
        writereg(client, 0, Init1993Tab[0]);
}

static int SetFEC(struct i2c_client *client, u8 fec)
{
        struct ves1993 *ves=(struct ves1993 *) client->data;
        
        if (fec>=8) 
                fec=8;
        if (ves->fec==fec)
                return 0;
        ves->fec=fec;
        return writereg(client, 0x0d, ves->fec);
}

static int SetSymbolrate(struct i2c_client *client, u32 srate, int doclr)
{
        struct ves1993 *ves=(struct ves1993 *) client->data;
        u32 BDR;
        u32 ratio;
  	u8  ADCONF, FCONF, FNR;
	u32 BDRI;
	u32 tmp;
        
        if (ves->srate==srate) {
                if (doclr)
                        ClrBit1893(client);
                return 0;
        }
        printk("VES_setsymbolrate %d\n", srate);

#define XIN (91000000UL) // dbox Crystal iss 91 MHz !!

	if (srate>XIN/2)
                srate=XIN/2;
        if (srate<500000)
                srate=500000;
        ves->srate=srate;
        
#define MUL (1UL<<25)
#define FIN (XIN>>4)
        tmp=srate<<6;
	ratio=tmp/FIN;

	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
   	
	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;     
	
	FNR = 0xFF;
	
	if (ratio < MUL/3)           FNR = 0;
	if (ratio < (MUL*11)/50)     FNR = 1;
	if (ratio < MUL/6)           FNR = 2;
	if (ratio < MUL/9)           FNR = 3;
	if (ratio < MUL/12)          FNR = 4;
	if (ratio < (MUL*11)/200)    FNR = 5;
	if (ratio < MUL/24)          FNR = 6;
	if (ratio < (MUL*27)/1000)   FNR = 7;
	if (ratio < MUL/48)          FNR = 8;
	if (ratio < (MUL*137)/10000) FNR = 9;

	if (FNR == 0xFF)
	{
		ADCONF = 0x89;		//bypass Filter
		FCONF  = 0x80;		//default
		FNR	= 0;
	} else	{
		ADCONF = 0x81;
		FCONF  = 0x88 | (FNR >> 1) | ((FNR & 0x01) << 5); //default | DFN | AFS
	}


	BDR = ((  (ratio<<(FNR>>1))  >>4)+1)>>1;
	BDRI = (  ((FIN<<8) / ((srate << (FNR>>1))>>2)  ) +1 ) >> 1;

        printk("VES_FNR= %d\n", FNR);
        printk("VES_ratio= %08x\n", ratio);
        printk("VES_BDR= %08x\n", BDR);
        printk("VES_BDRI= %02x\n", BDRI);

	if (BDRI > 0xFF)
	        BDRI = 0xFF;

        writereg(client, 6, 0xff&BDR);
	writereg(client, 7, 0xff&(BDR>>8));
	writereg(client, 8, 0x0f&(BDR>>16));

	writereg(client, 9, BDRI);
	writereg(client, 0x20, ADCONF);
	writereg(client, 0x21, FCONF);

        if (srate<6000000) 
                writereg(client, 5, Init1993Tab[0x05] | 0x80);
        else
                writereg(client, 5, Init1993Tab[0x05] & 0x7f);

	writereg(client, 0, 0);
	writereg(client, 0, 1);

	if (doclr)
	  ClrBit1893(client);
	return 0;
}

static int attach_adapter(struct i2c_adapter *adap)
{
        struct ves1993 *ves;
        struct i2c_client *client;
        
        client_template.adapter=adap;

// siehe ves1820.c ... TODO: check ...
//        if (i2c_master_send(&client_template,NULL,0))
//                return -1;
        
        client_template.adapter=adap;
        
        printk("readreg\n");
        if ((readreg(&client_template, 0x1e)&0xf0)!=0xd0)
        {
          if ((readreg(&client_template, 0x1a)&0xF0)==0x70)
            printk("warning, no ves1993 found but a VES1820\n");
          return -1;
        }
        printk("feID: 1893 %x\n", readreg(&client_template, 0x1e));
        
        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;
        
        client->data=ves=kmalloc(sizeof(struct ves1993),GFP_KERNEL);
        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }
       
        i2c_attach_client(client);
        init(client);
				if (register_demod(&ves1993))
					printk("ves1993.o: can't register demod.\n");
              
        return 0;
}

static int detach_client(struct i2c_client *client)
{
        printk("ves1993: detach_client\n");
        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        unregister_demod(&ves1993);
        return 0;
}

void ves_write_reg(int reg, int val)
{
  writereg(dclient, reg, val);
}

int ves_read_reg(int reg)
{
  return readreg(dclient, reg);
}

void ves_init(void)
{
  init(dclient);
}

void ves_reset(void)
{
  ClrBit1893(dclient);
}

void ves_set_frontend(struct frontend *front)
{
  struct ves1993 *ves=(struct ves1993 *) dclient->data;
  if (ves->inv!=front->inv)
  {
    ves->inv=front->inv;
    writereg(dclient, 0x0c, Init1993Tab[0x0c] ^ (ves->inv ? 0x40 : 0x00));
    ClrBit1893(dclient);
  }
  SetFEC(dclient, front->fec);
  SetSymbolrate(dclient, front->srate, 1);
  printk("sync: %x\n", readreg(dclient, 0x0E));
}

void ves_get_frontend(struct frontend *front)
{
  front->type=FRONT_DVBS;
  front->afc=((int)((char)(readreg(dclient,0x0a)<<1)))/2;
  front->afc=(front->afc*(int)(front->srate/8))/16;
  front->agc=(readreg(dclient,0x0b)<<8);
  front->sync=readreg(dclient,0x0e);
  printk("sync: %x\n", front->sync);
  front->nest=(readreg(dclient,0x1c)<<8);

  front->vber = readreg(dclient,0x15);
  front->vber|=(readreg(dclient,0x16)<<8);
  front->vber|=(readreg(dclient,0x17)<<16);
  printk("vber: %x\n", front->vber);

  if ((front->fec==8) && ((front->sync&0x1f) == 0x1f))
    front->fec=(readreg(dclient, 0x0d)>>4)&0x07;
} 

static void inc_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

static void dec_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_driver dvbt_driver = {
        "VES1993 DVB DECODER",
        I2C_DRIVERID_VES1993,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "VES1993",
        I2C_DRIVERID_VES1993,
        0,
        (0x10 >> 1),
        NULL,
        &dvbt_driver,
        NULL
};


#ifdef MODULE
int init_module(void) {
        int res;
        int sync;
	int i;
	
        if ((res = i2c_add_driver(&dvbt_driver))) 
        {
                printk("ves1993: Driver registration failed, module not inserted.\n");
                return res;
        }
        if (!dclient)
        {
                printk("ves1993: not found.\n");
                i2c_del_driver(&dvbt_driver);
                return -EBUSY;
        }
        
        dprintk("ves1993: init_module\n");
		
	sync=readreg(dclient,0x0e);
	printk("VES_Sync:%x\n",sync);
	sync=dump(&client_template);
	
	ves_write_reg(0x3A,0x0E);
	ves_write_reg(0x21,0x00);
//	ves_write_reg(0x00,0x00);
	ves_write_reg(0x06,0x72);
	ves_write_reg(0x07,0x8C);
	ves_write_reg(0x08,0x09);
	ves_write_reg(0x09,0x6B);
	ves_write_reg(0x20,0x81);
	ves_write_reg(0x21,0x80);
//	ves_write_reg(0x00,0x01);
	ves_write_reg(0x0D,0x0A);
	ves_write_reg(0x05,0xAB); 
	ves_write_reg(0x00,0x00);
	ves_write_reg(0x00,0x01);

	

	for(i=1;i<50;i++){ 
	    sync=readreg(dclient,0x0e);	
	    printk("VES_Sync:%x\n",sync);
	}    
	sync=dump(&client_template);
	
        return 0;
}

void cleanup_module(void)
{
        int res;
        
        if ((res = i2c_del_driver(&dvbt_driver))) 
        {
                printk("dvb-tuner: Driver deregistration failed, "
                       "module not removed.\n");
        }
        dprintk("ves1993: cleanup\n");
}
#endif
