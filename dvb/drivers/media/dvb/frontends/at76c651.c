/*

    $Id: at76c651.c,v 1.24.2.3 2002/01/26 19:18:30 fnbrd Exp $

    AT76C651  - DVB demux driver (dbox-II-project)

    Homepage: http://dbox2.elxsi.de

    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)

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

    $Log: at76c651.c,v $
    Revision 1.24.2.3  2002/01/26 19:18:30  fnbrd
    Kleinigkeit ;) geaendert.

    Revision 1.24.2.1  2002/01/26 13:33:06  fnbrd
    Blind (d.h. ohne Compilationsversuch) an neue API angepasst. Mail mir mal bitte wer die Fehlermeldungen beim compilieren zu.

    Revision 1.24  2002/01/10 23:32:22  fnbrd
    Better debug output.

    Revision 1.23  2002/01/10 20:41:35  derget
    angepasst an neue sagem
    mit at76c651b

    Revision 1.22  2001/08/24 22:35:47  fnbrd
    Fixed bug with sync.

    Revision 1.21  2001/08/22 07:30:35  fnbrd
    Fixed one frequency

    Revision 1.20  2001/08/17 17:18:55  fnbrd
    Removed one warning with gcc 3.0

    Revision 1.19  2001/08/17 17:11:24  fnbrd
    2 new frequencies.

    Revision 1.18  2001/06/24 11:25:53  gillem
    - sync with cvs

    Revision 1.17  2001/05/03 22:16:20  fnbrd
    Sync-Bits (lock) etwas umsortiert.

    Revision 1.16  2001/05/03 11:33:27  fnbrd
    Ein paar printks in dprintks geaendert, damit der Treiber 'leiser' laedt.

    Revision 1.15  2001/04/30 06:21:22  fnbrd
    Aufraeumarbeiten.

    Revision 1.14  2001/04/29 21:09:32  fnbrd
    Doppeltes init abgestellt.

    Revision 1.13  2001/04/28 21:45:00  fnbrd
    Kosmetik.

    Revision 1.12  2001/04/24 20:36:16  fnbrd
    Kleinere Aenderungen.

    Revision 1.11  2001/04/24 01:56:36  fnbrd
    Koennte jetzt funktionieren.

    Revision 1.10  2001/03/29 02:13:25  tmbinc
    Added register_demod, unregister_dmod. be sure to use new load script

    Revision 1.9  2001/03/22 18:00:08  fnbrd
    Kleinigkeiten.

    Revision 1.5  2001/03/16 13:04:16  fnbrd
    Alle Interrupt-Routinen auskommentiert (task), da sie auch beim alten Treiber
    anscheinend nichts gemacht habe.

    Revision 1.4  2001/03/16 12:27:29  fnbrd
    Symbolrate wird berechnet.


*/

#include <linux/delay.h>	/* for mdelay */
#include <linux/kernel.h>
#include <linux/module.h>	/* for module-version */
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/i2c.h>

#include <dbox/dvb_frontend.h>
#include <dbox/fp.h>
//#include <dbox/dvb.h>

#include <asm/8xx_immap.h>

//#include <dbox/ves.h>

//static void ves_init(void);
static void ves_tuner_i2c(int an);
static int tuner_set_freq(int freq);
static int attach_adapter(struct i2c_adapter *adap);
static int detach_client(struct i2c_client *client);
static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg);

/*
struct demod_function_struct at76c651={
	write_reg:		ves_write_reg,
	init:			ves_init,
	set_frontend:	 	ves_set_frontend,
	get_frontend:		ves_get_frontend,
	get_unc_packet:		ves_get_unc_packet,
	set_frequency:		tuner_set_freq,
	set_sec:		0,
	send_diseqc:		0,
	sec_status:		0
};
*/

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

static int debug = 9;
#define dprintk	if (debug) printk

static struct i2c_client *dclient=0;

// ------------------------------------ Tuner ---------------------------------------

#define TUNER_I2C_DRIVERID  0xF0C2

static int tuner_detach_client(struct i2c_client *tuner_client);
static int tuner_attach_adapter(struct i2c_adapter *adapter);
static int set_tuner_dword(u32 tw);
static struct i2c_client *dclient_tuner=0;

struct tuner_data
{
	u32 lastwrite;
};

struct tuner_data *tunerdata=0;

static struct i2c_driver tuner_driver = {
        "AT76c651 tuner driver",
        TUNER_I2C_DRIVERID,
        I2C_DF_NOTIFY,
        &tuner_attach_adapter,
        &tuner_detach_client,
        &dvb_command,
        0,
        0,
};

static struct i2c_client client_template_tuner = {
        "AT76c651 tuner",
        TUNER_I2C_DRIVERID,
        0,
        0x61, // = 0xc2 >> 1
        NULL,
        &tuner_driver,
        NULL
};

static int tuner_detach_client(struct i2c_client *tuner_client)
{
	int err;

	if ((err=i2c_detach_client(tuner_client)))
	{
		dprintk("AT76C651: couldn't detach tuner client driver.\n");
		return err;
	}

	kfree(tuner_client);
	return 0;
}

static int tuner_attach_adapter(struct i2c_adapter *adap)
{
struct i2c_client *client;

  client_template_tuner.adapter=adap;
//  if(i2c_master_send(&client_template_tuner, NULL, 0))
//    return -1;
  if(NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
    return -ENOMEM;
  memcpy(client, &client_template_tuner, sizeof(struct i2c_client));
  dclient_tuner=client;
  client->data=tunerdata=kmalloc(sizeof(struct tuner_data),GFP_KERNEL);
  if (tunerdata==NULL) {
    kfree(client);
    return -ENOMEM;
  }
  memset(tunerdata, 0, sizeof(tunerdata));
  dprintk("AT76C651: attaching tuner at 0x%02x\n", (client->addr)<<1);
  i2c_attach_client(client);
  dprintk("AT76C651: tuner attached to adapter %s\n", adap->name);
  return 0;
}

static int tuner_init(void)
{
	int res;

	ves_tuner_i2c(1); //enable tuner access on at76c651
	dprintk("AT76C651: DBox2 tuner driver\n");
	if ((res=i2c_add_driver(&tuner_driver)))
	{
		printk("AT76C651: tuner driver registration failed!!!\n");
		return res;
	}

	if (!dclient_tuner)
	{
		i2c_del_driver(&tuner_driver);
		printk("AT76C651: Couldn't find tuner.\n");
		return -EBUSY;
	}
	ves_tuner_i2c(0); //disable tuner access on at76c651
//	tuner_set_freq(1198000000);

//	printk("AT76C651: DBox2 tuner driver init fertig\n");
	return 0;
}

static int tuner_close(void)
{
	int res;

	if ((res=i2c_del_driver(&tuner_driver)))
	{
		printk("AT76C651: tuner driver unregistration failed.\n");
		return res;
	}

	return 0;
}

static int writereg(struct i2c_client *client, u8 reg, u8 data)
{
        int ret;
        u8 msg[2];

        msg[0]=reg; msg[1]=data;
        ret=i2c_master_send(client, msg, 2);
        if (ret!=2)
                printk("writereg error\n");
    /*    printk("writereg: [%02x]=0x%02x\n", reg, data); */
        mdelay(10);
        return ret;
}

static u8 readreg(struct i2c_client *client, u8 reg)
{
        struct i2c_adapter *adap=client->adapter;
        unsigned char mm1[1];
        unsigned char mm2[] = {0x00};
        struct i2c_msg msgs[2];

        msgs[0].flags=0;
        msgs[1].flags=I2C_M_RD;
        msgs[0].addr=msgs[1].addr=client->addr;
        mm1[0]=reg;
        msgs[0].len=1; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
        i2c_transfer(adap, msgs, 2);

        return mm2[0];
}

inline void at_restart(void)
{
  // AT neu starten
  writereg(dclient, 0x07, 0x01);
}

static int set_tuner_dword(u32 tw)
{
char msg[4];
int len=4;

  if(tunerdata->lastwrite==tw)
    return 0; // Nichts zu tun
//  dprintk("AT76C651: set_tuner_dword: 0x%08x\n", tw);
  *((u32*)(msg))=tw;
  ves_tuner_i2c(1); //enable tuner access on at76c651
  if (i2c_master_send(dclient_tuner, msg, len)!=len)
    return -1;
  ves_tuner_i2c(0); //disable tuner access on at76c651
  tunerdata->lastwrite=tw;
  at_restart(); // Und den AT neu starten
  return 0;
}

static int tuner_set_freq(int freq)
{
  u32 dw=0;
  dprintk("AT76C651: tuner_set_freq: %d\n", freq);
  // Rechne mal wer
//  unsigned dw=0x17e28e06+(freq-346000000UL)/8000000UL*0x800000;
  switch(freq) {
    case 474000000:
      dw=0x1fe28e85; // Geschaetzt
      break;
    case 466000000:
      dw=0x1f628e85; // Geschaetzt
      break;
    case 458000000:
      dw=0x1ee28e85; // Geschaetzt
      break;
    case 450000000:
      dw=0x1e628e85; // Geschaetzt
      break;
    case 442000000:
      dw=0x1de28e85;
      break;
    case 434000000:
      dw=0x1d628e85;
      break;
    case 426000000:
      dw=0x1ce28e85;
      break;
    case 418000000:
      dw=0x1c628e85; // Geschaetzt
      break;
    case 410000000:
      dw=0x1be28e85;
      break;
    case 402000000:
      dw=0x1b628e85;
      break;
    case 394000000:
      dw=0x1ae28e06;
      break;
    case 386000000:
      dw=0x1a628e06;
      break;
    case 378000000:
      dw=0x19e28e06;
      break;
    case 370000000:
      dw=0x19628e06;
      break;
    case 362000000:
      dw=0x18e28e06;
      break;
    case 354000000:
      dw=0x18628e06;
      break;
    case 346000000:
      dw=0x17e28e06;
      break;
    case 338000000:
      dw=0x17628e06;
      break;
    case 330000000:
      dw=0x16e28e06;
      break;
    default:
      printk("AT76C651: Frequency %d not supported\n", freq);
      break;
  }
  if(dw) {
    set_tuner_dword(dw);
    return 0;
  }
  else
    return -1;
}

//---------------------------------------------------------

typedef struct at76c651 {
//        int inversion;
        u32 srate;
	u32 ber;
        u8 pwm;
	u8 sync;
	u32 uncp; /* uncorrectable packet */
//        u8 reg0;
} at76c651_t;


// Tuner an i2c an/abhaengen
static void ves_tuner_i2c(int an)
{
  if(an) {
    writereg(dclient, 0x0c, 0xc2|1);
//    dprintk("AT76C651: tuner now attached to i2c at 0xc2\n");
  }
  else {
    writereg(dclient, 0x0c, 0xc2);
//    dprintk("AT76C651: tuner now detached from i2c\n");
  }
}

static int init(struct i2c_client *client)
{

//        struct at76c651 *ves=(struct at76c651 *) client->data;

        dprintk("AT76C651: init chip\n");

	// BBFREQ
	writereg(client, 0x04, 0x3f);
	writereg(client, 0x05, 0xee);

	// SYMRATE 6900000
	writereg(client, 0x00, 0xf4);
	writereg(client, 0x01, 0x7c);
	writereg(client, 0x02, 0x06);

	// QAMSEL 64 QAM
	writereg(client, 0x03, 0x06);

        // SETAUTOCFG (setzt alles bis auf SYMRATE, QAMSEL und BBFREQ)
	writereg(client, 0x06, 0x01);

        // Performance optimieren (laut Datenblatt)
	writereg(client, 0x10, 0x06);
	writereg(client, 0x11, 0x10);
	writereg(client, 0x15, 0x28); // BBCFG
//	writereg(client, 0x20, 0x09); // ?
	writereg(client, 0x24, 0x90); // TIMLOOPCFG
	writereg(client, 0x30, 0x90);
//	writereg(client, 0x30, 0x94);

	set_tuner_dword(0x19628e06); // (370000)
        // Und ein Restart
//	at_restart(); // Wird bei set_tuner_dword gemacht
//	writereg(client, 0x07, 0x01);

        /* mask interrupt */
	// Moegliche Trigger: Pins LOCK1/LOCK2, singal loss, frame rate lost und per frame-timer
        // Wird auch im ves_task gemacht, um den IRQ zurueckzusetzen
	writereg(client, 0x0b, 0x0); // keine IRQs

	return 0;
}

// Meine Box hat einen Quartz mit 28.9 -> fref = 2*28.9 = 57.8 (gleich dem Eval-Board zum AT)
/*
// Tabelle zum Berechnen des Exponenten zur gegebenen Symbolrate (fref=57.8 MHz)
u32 expTab[] = {
  5645, 11290, 22579, 45157,
  90313, 180625, 361250, 722500
};
*/

static int SetSymbolrate(struct i2c_client* client, u32 Symbolrate)
{
//        struct at76c651 *ves=(struct at76c651 *) client->data;
#define FREF 57800000UL
//#define FREF 69600000UL
        u32 mantisse;
        u8 exp;
//	int i;
        u32 tmp;
	u32 fref;
	dprintk("AT76C651: SetSymbolrate %u\n", Symbolrate);
        if(Symbolrate > FREF/2)
          Symbolrate=FREF/2;
        if(Symbolrate < 500000)
          Symbolrate=500000;
//        ves->srate=Symbolrate;
        // Ich Kernel-Dummy, Kernel nix float -> haendisch div.
        // Wenn's nur nicht so lange her waere
//  	mantisse= ((float)Symbolrate/FREF)*(1<<(30-exp));
  	tmp=Symbolrate;
        fref=FREF;
  	mantisse=tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
/*
        for(i=6; i>2; i--)
          if(Symbolrate>=expTab[i])
            break;
        exp=i;
*/
	if(Symbolrate<722500) {
          exp=5;
//          mantisse<<=1; // unschoen, evtl. nochmal teilen
          tmp=(tmp%fref)<<8;
          mantisse=(mantisse<<1)+((tmp/fref)>>7);
 	}
 	else
          exp=6;
	dprintk("AT76C651: exp: %02x mantisse: %x <- someone should correct this\n", exp, mantisse);
	dprintk("AT76C651: Not changed, fixed to 6900000 (exp 0x06, mantisse 0x1E8f80).\n");
/*
        // Exponent und Mantisse Bits 0-4 setzen
        writereg(client, 0x02, ((mantisse&0x0000001f)<<3)|exp);
        mantisse>>=5;
        // Mantisse Bits 5-12
        writereg(client, 0x01, mantisse&0x000000ff);
        mantisse>>=8;
        // Mantisse Bits 13-20
        writereg(client, 0x00, mantisse&0x000000ff);
	// Und ein reset um das Dingens richtig einzustellen
  	writereg(client, 0x07, 0x01);
*/
  	return 0;
}

/*
typedef enum QAM_TYPE
{	QAM_16,
	QAM_32,
	QAM_64,
	QAM_128,
	QAM_256
} QAM_TYPE, *PQAM_TYPE;
*/

static const char *qamstr[6]= {
        "QPSK",
	"QAM 16",
	"QAM 32",
	"QAM 64",
	"QAM 128",
	"QAM 256"
};

static int SetQAM(struct i2c_client* client, Modulation QAM_Mode)
{
  u8 qamsel;
  if(QAM_Mode<1 || QAM_Mode>5) {
    dprintk("AT76C651: SetQAM unknow type: %d\n", (int)QAM_Mode);
    return -1;
  }
  dprintk("AT76C651: SetQAM %s\n", qamstr[QAM_Mode]);

	// Read QAMSEL
	qamsel=readreg(client, 0x03);
	qamsel &= 0xf0;
        qamsel |= QAM_Mode+5; // 5 = QAM-16
/*
        qamsel=QAM_Mode+4; // 4 = QAM-16
        // coherent modulation und Maptyp DVB
*/
        writereg(client, 0x03, qamsel);
	// Und ein reset um das Dingens richtig einzustellen
//  	writereg(client, 0x07, 0x01);
  	return 0;
}

static void inc_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

//-------------------------------------------------------------------------

static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  dprintk("AT76C651: dvb_command\n");

	switch (cmd) 
	{
		
	case FE_READ_STATUS:
	{
		FrontendStatus *status=(FrontendStatus *) arg;

                u8 lock;
                lock=readreg(client, 0x80); // Bits: FEC, CAR, EQU, TIM, AGC2, AGC1, ADC, PLL (PLL=0)

		*status=0;

		if (lock&0x08) // AGC2
			*status|=FE_HAS_SIGNAL;
		if (lock&0x40) // CAR
			*status|=FE_HAS_CARRIER;
		if (lock&20) // EQU
			*status|=FE_HAS_SYNC;
		if (lock&0x80) // FEC
			*status|=FE_HAS_LOCK;
		break;
	}

	case FE_WRITEREG:
	{
		u8 *msg = (u8 *) arg;
		writereg(client, msg[0], msg[1]);
		break;
	}
	case FE_INIT:
	{
		init(client);
		break;
	}
	case FE_RESET:
                at_restart();
		break;

	case FE_SET_FRONTEND:
	{

		FrontendParameters *param = (FrontendParameters *) arg;

		SetQAM(client, param->u.qam.QAM);
		SetSymbolrate(client, param->u.qam.SymbolRate);

		break;
	}

	case FE_SETFREQ:
	{
		u32 freq=*(u32*)arg;
                tuner_set_freq(freq);
		break;
	}

	default:
		return -1;
	}
	
	return 0;
} 

//-------------------------------------------------------------------------

static void dec_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
}


static struct i2c_driver dvbt_driver = {
        "AT76C651 DVB DECODER",
        I2C_DRIVERID_EXP2, // experimental use id
//        I2C_DRIVERID_VES1820,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "AT76C651",
        I2C_DRIVERID_EXP2, // experimental use id
//        I2C_DRIVERID_VES1820,
        0,
        0x0d, // =0x1a >> 1
        NULL,
        &dvbt_driver,
        NULL
};

static int attach_adapter(struct i2c_adapter *adap)
{
        dvb_front_t *front;
        struct i2c_client *client;

        client_template.adapter=adap;

        if (readreg(&client_template, 0x0e)!=0x65) {
          printk("no AT76C651(B) found\n");
          return -1;
	}
        
	if (readreg(&client_template, 0x0f)!=0x10)
        {
          if (readreg(&client_template, 0x0f)==0x11) {
            dprintk("AT76C651B found\n");
          }
	  else {
            printk("no AT76C651(B) found\n");
            return -1;
	  }
        }
        else
          dprintk("AT76C651B found\n");
	

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;

        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;

        client->data=kmalloc(sizeof(dvb_front_t), GFP_KERNEL);
        if (client->data==NULL) {
                kfree(client);
                return -ENOMEM;
        }
        front=(dvb_front_t *)client->data;
        dprintk("AT76C651: attaching AT76C651 at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);

        dprintk("AT76C651: attached to adapter %s\n", adap->name);

	front->type=DVB_C;
	front->capabilities=0; // kann auch nix
	front->i2cbus=adap;
	
	front->demod=client;
	front->demod_type=DVB_DEMOD_AT76C651;
	
	register_frontend((dvb_front_t *)client->data);

  return 0;
}

static int detach_client(struct i2c_client *client)
{
        dprintk("AT76C651: detach_client\n");
	unregister_frontend((dvb_front_t *)client->data);
        // IRQs abschalten
	writereg(client, 0x0b, 0x00);

        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void) {
        int res;

        dprintk("AT76C651: $Id: at76c651.c,v 1.24.2.3 2002/01/26 19:18:30 fnbrd Exp $\n");
        if ((res = i2c_add_driver(&dvbt_driver)))
        {
                printk("AT76C651: Driver registration failed, module not inserted.\n");
                return res;
        }
        if (!dclient)
        {
                i2c_del_driver(&dvbt_driver);
                printk("AT76C651: not found.\n");
                return -EBUSY;
        }
	if((res=tuner_init()))
          return res;
//	ves_init(); // wird bei register_demod aufgerufen
//	if (register_demod(&at76c651))
// 		printk("at76c651.o: can't register demod.\n");
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

        dprintk("AT76C651: cleanup\n");
	tuner_close();

}
#endif
