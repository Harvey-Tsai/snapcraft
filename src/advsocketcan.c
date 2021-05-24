//******************************************************************************
//
// Copyright (c) 2016 Advantech Industrial Automation Group.
//
// Advantech serial CAN card socketCAN driver
// 
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation; either version 2 of the License, or (at your option) 
// any later version.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT 
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
// more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 
// Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 
//
//
//******************************************************************************


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/string.h>
#include "sja1000.h"


MODULE_AUTHOR(" Carl ");
MODULE_DESCRIPTION("SocketCAN driver for Advantech CAN cards");
MODULE_SUPPORTED_DEVICE("Advntech CAN cards");
MODULE_LICENSE("GPL");
static char *serial_version = "1.0.1.0";
static char *serial_revdate = "2018/10/25";

#define Max_CAN_Port 4	//Max can port 
#define CAN_Clock (16000000 / 2) //16000000 : crystal frequency

struct advcan_pci_card {

	int cardnum;
	int portNum;
	int portsernum;
	struct pci_dev *pci_dev;
	struct net_device *net_dev[Max_CAN_Port];

	unsigned int Base[Max_CAN_Port];
	void __iomem * Base_mem[Max_CAN_Port];
	unsigned int addlen[Max_CAN_Port];
};


#define sja1000_CDR             (CDR_CLKOUT_MASK |CDR_CBP )
#define sja1000_OCR             (OCR_TX0_PULLDOWN | OCR_TX0_PULLUP)
#define ADVANTECH_VANDORID	0x13FE

static unsigned int cardnum = 1;
static unsigned int portsernum = 0;
static void advcan_pci_remove_one(struct pci_dev *pdev);
static int advcan_pci_init_one(struct pci_dev *pdev,const struct pci_device_id *ent);

static struct pci_device_id advcan_board_table[] = {
   {ADVANTECH_VANDORID, 0x1680, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0x3680, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0x2052, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0x1681, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc002, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc004, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc101, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc102, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc104, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc201, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc202, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc204, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc301, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc302, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {ADVANTECH_VANDORID, 0xc304, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
   {0,}			
};

static struct pci_driver can_socket_pci_driver = {
	.name = "advcan_pci_socket",
	.id_table = advcan_board_table,
	.probe = advcan_pci_init_one,
	.remove = advcan_pci_remove_one,
};


static u8 advcan_read_io(const struct sja1000_priv *priv, int port)
{
	return inb((unsigned long)priv->reg_base + port);
}

static void advcan_write_io(const struct sja1000_priv *priv,int port, u8 val)
{
	outb(val,(unsigned long) (priv->reg_base + port));
}

static u8 advcan_read_mem(const struct sja1000_priv *priv, int port)
{
	return ioread8(priv->reg_base + (port<<2));
}

static void advcan_write_mem(const struct sja1000_priv *priv,int port, u8 val)
{
	iowrite8(val, priv->reg_base + (port<<2));
}


static inline int check_CAN_chip(const struct sja1000_priv *priv)
{
	unsigned char flg;

	priv->write_reg(priv, REG_MOD, 1);//enter reset mode
	priv->write_reg(priv, REG_CDR, CDR_PELICAN);

	flg = priv->read_reg(priv, REG_CDR);//check enter reset mode

	if (flg == CDR_PELICAN) return 1;
	
	return 0;
}


static int advcan_pci_init_one(struct pci_dev *pdev,const struct pci_device_id *ent)
{	

	unsigned int portNum,address;
	unsigned int bar,barFlag,offset,len;
	int err;
	struct advcan_pci_card *devExt;
	struct sja1000_priv *priv;
	struct net_device *dev;
	
	portNum = 0;
	bar = 0;
	barFlag = 0;
	offset = 0x100;

	if (pci_enable_device(pdev) < 0) 
	{
		dev_err(&pdev->dev, "initialize device failed \n");
		return -ENODEV;
	}

	devExt = kzalloc(sizeof(struct advcan_pci_card), GFP_KERNEL);
	if (devExt == NULL) 
	{
		dev_err(&pdev->dev, "allocate memory failed\n");
		pci_disable_device(pdev);
		return -ENOMEM;
	}

	pci_set_drvdata(pdev, devExt);

  if (   pdev->device == 0xc001
      || pdev->device == 0xc002
      || pdev->device == 0xc004
      || pdev->device == 0xc101
      || pdev->device == 0xc102
      || pdev->device == 0xc104)
   {
      portNum = pdev->device & 0xf;
      len = 0x100;
   }
   else if ( pdev->device == 0xc201
      || pdev->device == 0xc202
      || pdev->device == 0xc204
      || pdev->device == 0xc301
      || pdev->device == 0xc302
      || pdev->device == 0xc304 )
   {
	portNum = pdev->device & 0xf;
   	offset = 0x400;
   	len = 0x400;
   }
   else
   {
      if (pdev->device == 0x1680
         || pdev->device == 0x3680
	 || pdev->device == 0x2052)
      {
         portNum = 2;
         bar = 2;
         barFlag = 1;
         offset = 0x0;
      }
      else if (pdev->device == 0x1681)
      {
         portNum = 1;
         bar = 2;
         barFlag = 1;
         offset = 0x0;
      }
      len = 128;
   }

	devExt->pci_dev		= pdev;
	devExt->cardnum		= cardnum;
	devExt->portNum		= portNum;
	devExt->portsernum  = portsernum;


	for (int i = 0; i < devExt->portNum; i++) 
	{

      		address = pci_resource_start(pdev, bar)+ offset * i ;
 		devExt->Base[i] = address;
        	devExt->addlen[i] = len;
	

		dev = alloc_sja1000dev(sizeof(struct advcan_pci_card));
		if (dev == NULL) 
		{
			goto error_out;
		}
		

		devExt->net_dev[i] = dev;
		priv = netdev_priv(dev);
		priv->priv = devExt;
		priv->irq_flags = IRQF_SHARED;

		dev->irq = pdev->irq;


	      if( pdev->device == 0xc201
	      ||  pdev->device == 0xc202
	      ||  pdev->device == 0xc204
	      ||  pdev->device == 0xc301
	      ||  pdev->device == 0xc302
	      ||  pdev->device == 0xc304 )//Memory
	      {   
			if( request_mem_region(devExt->Base[i] , devExt->addlen[i], "advcan") == NULL ) 
			{
			    printk ("memory map error\n");   
			goto error_out;
			}
			priv->read_reg  = advcan_read_mem;
			priv->write_reg = advcan_write_mem;
			devExt->Base_mem[i] = ioremap(devExt->Base[i], devExt->addlen[i]);

			if (devExt->Base_mem[i] == NULL) 
			{
				printk ("ioremap error\n");
				goto error_out;
			}
			priv->reg_base = devExt->Base_mem[i];
	       }

		else //IO 
		{
			 if ( request_region(address, len, "advcan") == NULL ) 
			 {
			   printk ("IO map error\n"); 
				goto error_out;
			 }
			priv->read_reg  = advcan_read_io;
			priv->write_reg = advcan_write_io;

			priv->reg_base = (void *) (address);
		}


		if (barFlag)
		{
		   bar++ ;
		}

		if (check_CAN_chip(priv)) 
		{

			printk("Check channel OK!\n");

			priv->can.clock.freq = CAN_Clock;
			priv->ocr = sja1000_OCR;
			priv->cdr = sja1000_CDR;
			SET_NETDEV_DEV(dev, &pdev->dev);

			/* Register SJA1000 device */
			err = register_sja1000dev(dev);
			if (err) 
			{
				dev_err(&pdev->dev, "Registering device failed (err=%d)\n", err);
				free_sja1000dev(dev);
				goto error_out;
			}

		} 
		else 
		{
			free_sja1000dev(dev);
		}
		portsernum++;
	}

	cardnum++;
	
	return 0;

error_out:
	printk("init card error\n");
	advcan_pci_remove_one(pdev);
	return -EIO;;
}


static void advcan_pci_remove_one(struct pci_dev *pdev)
{

	struct net_device *dev;
	struct advcan_pci_card *devExt = pci_get_drvdata(pdev);


	for (int i = 0; i < devExt->portNum; i++) 
	{
		dev = devExt->net_dev[i];
		
		if (!dev)
			continue;
		
		dev_info(&pdev->dev, "Removing %s.\n", dev->name);

		unregister_sja1000dev(dev);
		free_sja1000dev(dev);

		if ( pdev->device == 0xc201
	      	|| pdev->device == 0xc202
	      	|| pdev->device == 0xc204
	      	|| pdev->device == 0xc301
	      	|| pdev->device == 0xc302
	      	|| pdev->device == 0xc304 )
		{	 
	    	 	iounmap(devExt->Base_mem[i]);
		        release_mem_region(devExt->Base[i], devExt->addlen[i]);
		}
		else
		{
		release_region(devExt->Base[i], devExt->addlen[i]);
			
		}

	}


	kfree(devExt);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}


static int __init advcan_socket_pci_init(void)
{
	printk("\n");

	printk("=============================================================");
	printk("====\n");
	printk("Advantech SocketCAN Drivers. V%s [%s]\n", serial_version, serial_revdate);
	printk(" ----------------init----------------\n");
	printk("=============================================================");
	printk("====\n");
	
	return pci_register_driver(&can_socket_pci_driver);
}

static void __exit advcan_socket_pci_exit(void)
{
printk(" -----------------exit----------------\n");
	pci_unregister_driver(&can_socket_pci_driver);
}

module_init(advcan_socket_pci_init);
module_exit(advcan_socket_pci_exit);

