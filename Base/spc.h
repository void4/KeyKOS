/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*
 * SX9100 SCSI protocol controller chip (SPC) definitions.
 *					87/07/10
 *					constructed by Harada.
 */

/*
 *	MB89351 scsi protocol controller register struct.
 */
typedef volatile struct SPC {
	unsigned char s_bdid;	/* For scsi bus device # register.	*/
	unsigned char dmy0[3];
	unsigned char s_sctl;	/* For spc internal control register.	*/
	unsigned char dmy1[3];
	unsigned char s_scmd;	/* For spc command register.		*/
	unsigned char dmy2[3];
	unsigned char s_dummy0; /* TMOD;
                                   synchronous transfers not implemented */
	unsigned char dmy3[3];
	unsigned char s_ints;	/* For spc interrupt register.		*/
	unsigned char dmy4[3];
	unsigned char s_psns;	/* For scsi bus phase register.		*/
#define s_sdgc s_psns           /* name of the above when writing. */
	unsigned char dmy5[3];
	unsigned char s_ssts;	/* For spc internal status register.	*/
	unsigned char dmy6[3];
	unsigned char s_serr;	/* For spc internal err status register.*/
	unsigned char dmy7[3];
	unsigned char s_pctl;	/* For scsi transfer phase get/set register.*/
	unsigned char dmy8[3];
	unsigned char s_mbc;	/* For spc transfer data count register.*/
	unsigned char dmy9[3];
	unsigned char s_dreg;	/* For spc transfer data read/write register.*/
	unsigned char dmya[3];
	unsigned char s_temp;	/* For scsi data bus control register.	*/
	unsigned char dmyb[3];
	unsigned char s_tch;	/* For spc transfer byte count register.(MSB)*/
	unsigned char dmyc[3];
	unsigned char s_tcm;	/*                                 (2nd Byte)*/
	unsigned char dmyd[3];
	unsigned char s_tcl;	/*				        (LSB)*/
	unsigned char dmye[3];
} vspc;

/*
 *	SCTL register definitions.
 */
#define	DISABLE		0x80
#define	RESET		0x40
#define	DIAGMODE	0x20
#define	ARBITEBL	0x10
#define	PAREBL		0x8
#define	SELEBL		0x4
#define RSELEBL		0x2
#define INTREBL		0x1

/*
 *	SCMD register definitions.
 */
#define	SETACK		0xe0
#define	RSTACK		0xc0
#define	TRASPAUSE	0xa0
#define	TRASCMD		0x80
#define	SETATN		0x60
#define RSTATN		0x40
#define	SELCMD		0x20
#define	RSTOUT		0x10
#define	MANUTRAS	0x8
#define PRGTRAS		0x4
#define PADTRAS		0x1

/*
 *	INTS register definitions.
 */
#define	SELINT		0x80
#define RSELINT		0x40
#define DISCONINT	0x20
#define CMDINT		0x10
#define SRVREQ		0x8
#define TIMEOUT		0x4
#define ERRINT		0x2
#define RSTCOND		0x1

/*
 *	PSNS register definitions.
 */
#define	REQSIG		0x80
#define	ACKSIG		0x40
#define	ATNSIG		0x20
#define	SELSIG		0x10
#define BSYSIG		0x8
#define	MSGSIG		0x4
#define	CTLSIG		0x2
#define	INOUTSIG	0x1

/* phs mode */
#define DOUTPHS		0x0
#define DINPHS		0x1
#define CPHS		0x2
#define SPHS		0x3
#define MOUTPHS		0x6
#define MINPHS		0x7
#define PHSMODE		0x7
#define BUSFREE		0x8		/* HARA 04/07 */
#define CMDWRITE	0x10	/* HARA 04/07 */
#define DATAPHS		0x20	/* HARA 04/07 */
#define STATREAD	0x40	/* HARA 04/07 */
#define MSGREAD		0x80	/* HARA 04/07 */

/*
 *	SDGC register definitions.
 */
#define DIAGREQ		0x80
#define	DIAGACK		0x40
#define XFEREBL		0x20
#define	DIAGBSY		0x8
#define	DIAGMSG		0x4
#define	DIAGCTL		0x2
#define	DIAGIN		0x1

/*
 *	SSTS register definitions.
 */
#define	INIT		0x80
#define	TARG		0x40
#define	SPCBUSY		0x20
#define	XFERACT		0x10
#define	RSTSTAT		0x8
#define	XCNT0		0x4
#define	DREGFUL		0x2
#define	DREGEMP		0x1

/*
 *	SERR register definitions.
 */
#define	IPUTERR		0x120
#define	OPUTERR		0x40
#define	XFEROUT		0x20
#define	PARERR		0x8
#define	SPRIOD		0x1

/*
 *	PCTL register definitions.
 */
#define	DISCONEBL	0x80
#define	MSGINPHS	0x7
#define	MSGOUTPHS	0x6
#define	STATPHS		0x3
#define	CMDPHS		0x2
#define	DATAINPHS	0x1
#define	DATAOUTPHS	0x0

