/****************************************************************

Siano Mobile Silicon, Inc.
MDTV receiver kernel modules.
Copyright (C) 2006-2008, Uri Shkolnik

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/

#include <linux/kernel.h>
#include <asm/irq.h>
#include <mach/hardware.h>

#include <linux/init.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include <linux/spi-tegra.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <mach/dma.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <asm/io.h>
#include "smsdbg_prn.h"
#include <linux/proc_fs.h>
#include <linux/spi/spi.h>
#include "../gpio-names.h"/*wangtao20110702*/
#include "../../../../arch/arm/mach-tegra/include/mach/pinmux.h"/*wangtao20110902*/


/**********************************************************************/
static void chip_powerdown(void);

/*ZTE:add by wangtao for CMMB 20111109 ++ */
#define sms_spi_log(kern, fmt, arg...) \
	printk(kern "[cmmb] %s: " fmt "\n", __func__, ##arg)
#define spi_log(fmt, arg...) sms_spi_log(, fmt, ##arg)
/*ZTE:add by wangtao for CMMB 20111109 -- */


/* physical layer variables */
struct spiphy_dev_s {
    struct spi_device *spi_device;
    struct completion transfer_in_process;
    void (*interruptHandler) (void *);
    void *intr_context;
    struct device *dev;	/*!< device model stuff */
};


static irqreturn_t spibus_interrupt(int irq, void *context)
{
	struct spiphy_dev_s *spiphy_dev = (struct spiphy_dev_s *) context;
	//u_irq_count ++;
	//printk("[cmmbspi]INT counter \n");

	if (spiphy_dev->interruptHandler)
		spiphy_dev->interruptHandler(spiphy_dev->intr_context);
	return IRQ_HANDLED;

}

void spimsg_complete_handler(void *context)
{
	struct spiphy_dev_s *spi = (struct spiphy_dev_s *) context;
      //spi_log("[cmmbspi]spimsg_complete_handler\n") ;
	complete(&spi->transfer_in_process);
}
#define	SPI_BUFSIZ	max(32,SMP_CACHE_BYTES)
void smsspibus_xfer(void *context, unsigned char *txbuf,
		    unsigned long txbuf_phy_addr, unsigned char *rxbuf,
		    unsigned long rxbuf_phy_addr, int len)
{
    int status;     
    int res;
    struct spi_message message;                                        
    struct spi_transfer transfer;   
    struct spiphy_dev_s *spi = (struct spiphy_dev_s *) context;    
   //  printk("[cmmbspi]++++++++++++ smsspibus_xfer++++++++++++ len=%d\n",len) ;
    memset(&transfer, 0, sizeof(transfer));
   // spi_log("smsspibus_xfer entry: transfer=%x,len=%d",transfer,len);
    //judge param              
    spi_message_init(&message);       
    
    init_completion(&spi->transfer_in_process);                  
    message.spi= spi->spi_device;
    message.complete=spimsg_complete_handler;
    //spi_log("smsspibus_xfer: message->complete=%x",message->complete);    
    message.context=context;
    //spi_log("transfer_in_process: init_done=%d",spi->transfer_in_process.done);
    if(txbuf){    
        transfer.tx_buf = txbuf;           
        //spi_log("smsspibus_xfer txbuf=%x,%x,%x,%x,%x,%x,%x,%x\n",
                                        //txbuf[0],txbuf[1],txbuf[2],txbuf[3],txbuf[4],txbuf[5],txbuf[6],txbuf[7]);
    }
    if(rxbuf) 
        transfer.rx_buf = rxbuf;    
    
    transfer.bits_per_word = 8;                                           
    //transfer.speed_hz = 12000000;                                         
    transfer.len = len;  
    INIT_LIST_HEAD(&transfer.transfer_list);
    spi_message_add_tail(&transfer, &message);  
                                       
    status = spi->spi_device->master->transfer(spi->spi_device, &message);  
    if(status)
    {
        printk("smsmdtv DMA transfer failed!!!status=%d",status);
     }
    res=wait_for_completion_timeout(&spi->transfer_in_process, 2 * HZ);
    //spi_log("transfer_in_process: com_done=%d",spi->transfer_in_process.done);
    if(!res)
    {
        printk("smsmdtv DMA timeout!");
        complete(&spi->transfer_in_process);
     }
   /* if(rxbuf) 
        spi_log("smsspibus_xfer rxbuf=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                                        rxbuf[0],rxbuf[1],rxbuf[2],rxbuf[3],rxbuf[4],rxbuf[5],rxbuf[6],rxbuf[7],rxbuf[8],rxbuf[9],rxbuf[10],rxbuf[11]);*/
        
    //printk("[cmmbspi]------------- smsspibus_xfer-----------\n") ;
}

void smschipreset(void *context)
{
}

#if defined (CONFIG_MACH_CARDHU) 
static void chip_poweron(void)
{
    spi_log("[cmmbspi]chip_poweron\n");
    gpio_set_value(TEGRA_GPIO_PS6, 1);
    gpio_set_value(TEGRA_GPIO_PS3, 1);
    gpio_set_value(TEGRA_GPIO_PS7, 1);
    mdelay(100);
    gpio_set_value(TEGRA_GPIO_PS5, 1);
    mdelay(10);
    gpio_set_value(TEGRA_GPIO_PS5, 0);
    mdelay(300);
    gpio_set_value(TEGRA_GPIO_PS5, 1); 
    mdelay(100);
}

void smsspibus_ssp_suspend(void* context )
{
    struct spiphy_dev_s *spiphy_dev ;

    spiphy_dev = (struct spiphy_dev_s *) context;
    printk("[cmmbspi]entering smsspibus_ssp_suspend\n");
    if(!context)
    {
        PERROR("smsspibus_ssp_suspend context NULL \n") ;
        return ;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MOSI,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_SCK,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_CS0_N,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MISO,TEGRA_TRI_TRISTATE);
    chip_powerdown();
    free_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS4),spiphy_dev);
}

static void chip_powerdown(void)
{
    spi_log("[cmmbspi]chip_powerdown\n");
    gpio_set_value(TEGRA_GPIO_PS3, 0);
    gpio_set_value(TEGRA_GPIO_PS7, 0);
    gpio_set_value(TEGRA_GPIO_PS5, 0);
    gpio_set_value(TEGRA_GPIO_PS6, 0);
    mdelay(100);
}

int smsspibus_ssp_resume(void* context) 
{
    int ret;
    struct spiphy_dev_s *spiphy_dev ;
    spi_log("[cmmbspi]entering smsspibus_ssp_resume\n");

    if(!context)
    {
        PERROR("smsspibus_ssp_resume context NULL \n");
        return -1;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MOSI,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_SCK,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_CS0_N,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MISO,TEGRA_TRI_NORMAL);
    chip_poweron();
   // set_irq_type(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS4), IRQ_TYPE_EDGE_RISING/*IRQT_RISING*/);
    ret = request_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS4), spibus_interrupt,
			IRQ_TYPE_EDGE_RISING, "SMSSPI", spiphy_dev);                             
    spi_log("[cmmbspi]interrupt for SMS device. status =%d\n",ret);
    if (ret) {
        printk("[cmmbspi]Could not get interrupt for SMS device. status =%d\n",ret);
        goto error_failed;
    }
    spi_log("[cmmbspi]irq level:%d\n",gpio_get_value(TEGRA_GPIO_PS4));
    return 0 ;
error_failed:
    return -1 ;

}

void smschip_gpio_init(void)
{
    int ret;
    spi_log("[cmmbspi]smschip_gpio_init\n");

    ret = gpio_request(TEGRA_GPIO_PS3, "CMMB_PWR_1V2_EN");
    if (ret)
   {
        printk("[cmmbspi]TEGRA_GPIO_PS3 fail\n"); 
        goto fail_err;
    }

    ret = gpio_request(TEGRA_GPIO_PS7, "CMMB_PWR_1V8_EN");
    if (ret) {
        printk("[cmmbspi]TEGRA_GPIO_PS7 fail\n"); 
        gpio_free(TEGRA_GPIO_PS3);
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PS5, "CMMB_RST");
    if (ret) {
        spi_log("[cmmbspi]TEGRA_GPIO_PS5 fail\n"); 
        gpio_free(TEGRA_GPIO_PS3);
        gpio_free(TEGRA_GPIO_PS7);
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PS6, "CMMB_ACM");
    if (ret) {
        printk("[cmmbspi]TEGRA_GPIO_PS6 fail\n"); 
        gpio_free(TEGRA_GPIO_PS3);
        gpio_free(TEGRA_GPIO_PS7);
        gpio_free(TEGRA_GPIO_PS5);     
        goto fail_err;
    }    
    ret = gpio_request(TEGRA_GPIO_PS4, "cmmb-int-gpio");
    if (ret) 
    {
        printk("[cmmbspi] TEGRA_GPIO_PS4 fail\n");
        gpio_free(TEGRA_GPIO_PS3);
        gpio_free(TEGRA_GPIO_PS7);
        gpio_free(TEGRA_GPIO_PS5);  
        gpio_free(TEGRA_GPIO_PS6);  
        goto fail_err;
    }    

    tegra_gpio_enable(TEGRA_GPIO_PS3);
    tegra_gpio_enable(TEGRA_GPIO_PS7);
    tegra_gpio_enable(TEGRA_GPIO_PS5);
    tegra_gpio_enable(TEGRA_GPIO_PS6);  
    tegra_gpio_enable(TEGRA_GPIO_PS4);

    gpio_direction_output(TEGRA_GPIO_PS3, 0);
    gpio_direction_output(TEGRA_GPIO_PS7, 0);
    gpio_direction_output(TEGRA_GPIO_PS5, 0);
    gpio_direction_output(TEGRA_GPIO_PS6, 0);
    gpio_direction_input(TEGRA_GPIO_PS4);
    mdelay(100);
    return;
    
fail_err:
    spi_log("[cmmbspi] smschip_gpio_init fail\n"); 
    return;
    
}
#endif

#if defined (CONFIG_MACH_TEGRA_ENTERPRISE)
static void chip_poweron(void)
{
    spi_log("[cmmbspi]chip_poweron\n");
    gpio_set_value(TEGRA_GPIO_PE1, 1);
    mdelay(100);
    gpio_set_value(TEGRA_GPIO_PE2, 1);
    mdelay(10);
    gpio_set_value(TEGRA_GPIO_PE2, 0);
    mdelay(300);
    gpio_set_value(TEGRA_GPIO_PE2, 1); 
    mdelay(100);
}

void smsspibus_ssp_suspend(void* context )
{
    struct spiphy_dev_s *spiphy_dev ;

    spiphy_dev = (struct spiphy_dev_s *) context;
    printk("[cmmbspi]entering smsspibus_ssp_suspend\n");
    if(!context)
    {
        PERROR("smsspibus_ssp_suspend context NULL \n") ;
        return ;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MOSI,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_SCK,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_CS0_N,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MISO,TEGRA_TRI_TRISTATE);
    chip_powerdown();
    free_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS0),spiphy_dev);
}

static void chip_powerdown(void)
{
    spi_log("[cmmbspi]chip_powerdown\n");
    gpio_set_value(TEGRA_GPIO_PE2, 0);
    gpio_set_value(TEGRA_GPIO_PE1, 0);
    mdelay(100);
}

int smsspibus_ssp_resume(void* context) 
{
    int ret;
    struct spiphy_dev_s *spiphy_dev ;
    spi_log("[cmmbspi]entering smsspibus_ssp_resume\n");

    if(!context)
    {
        PERROR("smsspibus_ssp_resume context NULL \n");
        return -1;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MOSI,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_SCK,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_CS0_N,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPI1_MISO,TEGRA_TRI_NORMAL);
    chip_poweron();
    ret = request_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS0), spibus_interrupt,
			IRQ_TYPE_EDGE_RISING, "SMSSPI", spiphy_dev);                             
    spi_log("[cmmbspi]interrupt for SMS device. status =%d\n",ret);
    if (ret) {
        printk("[cmmbspi]Could not get interrupt for SMS device. status =%d\n",ret);
        goto error_failed;
    }
    spi_log("[cmmbspi]irq level:%d\n",gpio_get_value(TEGRA_GPIO_PS0));
    return 0 ;
error_failed:
    return -1 ;

}

void smschip_gpio_init(void)
{
    int ret;
    spi_log("[cmmbspi]smschip_gpio_init\n");

    ret = gpio_request(TEGRA_GPIO_PE1, "CMMB_PWR_EN");
    if (ret)
   {
        printk("[cmmbspi]TEGRA_GPIO_PE1 fail\n"); 
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PE2, "CMMB_RST");
    if (ret) {
        spi_log("[cmmbspi]TEGRA_GPIO_PE2 fail\n"); 
        gpio_free(TEGRA_GPIO_PE1);
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PS0, "cmmb-int-gpio");
    if (ret) 
    {
        printk("[cmmbspi] TEGRA_GPIO_PS0 fail\n");
        gpio_free(TEGRA_GPIO_PE1);
        gpio_free(TEGRA_GPIO_PE2);
        goto fail_err;
    }    

    tegra_gpio_enable(TEGRA_GPIO_PE1);
    tegra_gpio_enable(TEGRA_GPIO_PE2);
    tegra_gpio_enable(TEGRA_GPIO_PS0);

    gpio_direction_output(TEGRA_GPIO_PE1, 0);
    gpio_direction_output(TEGRA_GPIO_PE2, 0);
    gpio_direction_input(TEGRA_GPIO_PS0);
    mdelay(100);
    return;
    
fail_err:
    spi_log("[cmmbspi] smschip_gpio_init fail\n"); 
    return;
    
}
#endif


#if defined(CONFIG_MACH_VENTANA)
static void chip_poweron(void)
{
    printk("[cmmbspi]chip_poweron \n");
    gpio_set_value(TEGRA_GPIO_PA3, 1);
    gpio_set_value(TEGRA_GPIO_PX2, 1);
    mdelay(100);
    gpio_set_value(TEGRA_GPIO_PW3, 1);
    mdelay(10);
    gpio_set_value(TEGRA_GPIO_PW3, 0);
    mdelay(300);
    gpio_set_value(TEGRA_GPIO_PW3, 1); 
    mdelay(100);
}

int smsspibus_ssp_resume(void* context) 
{
    int ret;
    struct spiphy_dev_s *spiphy_dev ;
    printk("[cmmbspi]entering smsspibus_ssp_resume\n");

    if(!context)
    {
        PERROR("smsspibus_ssp_resume context NULL \n");
        return -1;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;

    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPID,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPIE,TEGRA_TRI_NORMAL);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPIF,TEGRA_TRI_NORMAL);
    chip_poweron();
    //set_irq_type(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PX1), IRQ_TYPE_EDGE_RISING/*IRQT_RISING*/);
    ret = request_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PX1), spibus_interrupt,
			IRQ_TYPE_EDGE_RISING, "SMSSPI", spiphy_dev);                             
    printk("[cmmbspi]interrupt for SMS device. status =%d\n",ret);
    if (ret) {
        printk("[cmmbspi]Could not get interrupt for SMS device. status =%d\n",ret);
        goto error_failed;
    }
    printk("[cmmbspi]irq level:%d\n",gpio_get_value(TEGRA_GPIO_PX1));
    if(spiphy_dev->spi_device->master->alloc_dma)
        spiphy_dev->spi_device->master->alloc_dma(spiphy_dev->spi_device); 
    return 0 ;

error_failed:
    return -1 ;
}

static void chip_powerdown(void)
{
    printk("[cmmbspi]chip_powerdown \n");
    gpio_set_value(TEGRA_GPIO_PW3, 0);
    gpio_set_value(TEGRA_GPIO_PX2, 0);
    gpio_set_value(TEGRA_GPIO_PA3, 0);
    mdelay(100);
}

void smsspibus_ssp_suspend(void* context )
{
    struct spiphy_dev_s *spiphy_dev ;
    printk("[cmmbspi]entering smsspibus_ssp_suspend\n");
    if(!context)
    {
        PERROR("smsspibus_ssp_suspend context NULL \n") ;
        return ;
    }
    spiphy_dev = (struct spiphy_dev_s *) context;
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPID,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPIE,TEGRA_TRI_TRISTATE);
    tegra_pinmux_set_tristate(TEGRA_PINGROUP_SPIF,TEGRA_TRI_TRISTATE);
    chip_powerdown();
    free_irq(TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PX1),spiphy_dev);
    if(spiphy_dev->spi_device->master->free_dma)
        spiphy_dev->spi_device->master->free_dma(spiphy_dev->spi_device); 
}
void smschip_gpio_init(void)
{
    int ret;
    printk("[cmmbspi] smschip_gpio_init \n"); 
    ret = gpio_request(TEGRA_GPIO_PW3, "CMMB_RST");
    if (ret) {
        printk("[cmmbspi]TEGRA_GPIO_PW3 fail\n"); 
         goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PX2, "CMMB_PWR");
    if (ret)
    {
        printk("[cmmbspi]TEGRA_GPIO_PX2 fail\n"); 
        gpio_free(TEGRA_GPIO_PW3);
        goto fail_err;
    }

    ret = gpio_request(TEGRA_GPIO_PA3, "CMMB_1V2");
    if (ret)
    {
        printk("[cmmbspi]TEGRA_GPIO_PA3 fail\n"); 
        gpio_free(TEGRA_GPIO_PW3);
        gpio_free(TEGRA_GPIO_PX2);
        goto fail_err;
    }

    ret = gpio_request(TEGRA_GPIO_PX1, "cmmb-int-gpio");
    if (ret) 
    {
        printk("[cmmbspi] TEGRA_GPIO_PX1 fail\n");
        gpio_free(TEGRA_GPIO_PW3);
        gpio_free(TEGRA_GPIO_PX2);
        gpio_free(TEGRA_GPIO_PA3);
        goto fail_err;
    }    
        
    tegra_gpio_enable(TEGRA_GPIO_PW3);
    gpio_direction_output(TEGRA_GPIO_PW3, 0);

    tegra_gpio_enable(TEGRA_GPIO_PX2);
    gpio_direction_output(TEGRA_GPIO_PX2, 0);
    
    tegra_gpio_enable(TEGRA_GPIO_PA3);
    gpio_direction_output(TEGRA_GPIO_PA3, 0);

    tegra_gpio_enable(TEGRA_GPIO_PX1);
    gpio_direction_input(TEGRA_GPIO_PX1);
    mdelay(100);
    return;

fail_err:
    printk("[cmmbspi] smschip_gpio_init fail\n"); 
    return;
}
#endif

int smsspibus_write(void *context,char c)
{
	return 0;
}

#if 0
static void scan_boardinfo(struct spi_master *master)
{
	struct boardinfo	*bi;

	mutex_lock(&board_lock);
	list_for_each_entry(bi, &board_list, list) {
		struct spi_board_info	*chip = bi->board_info;
		unsigned		n;

		for (n = bi->n_board_info; n > 0; n--, chip++) {
			if (chip->bus_num != master->bus_num)
				continue;
			/* NOTE: this relies on spi_new_device to
			 * issue diagnostics when given bogus inputs
			 */
			spi_log("[cmmbspi]scan_boardinfo max_speed_hz:%d,bus_num:%d,name:%s\n",
			                chip->max_speed_hz,chip->bus_num,chip->modalias) ;
			(void) spi_new_device(master, chip);
		}
	}
	mutex_unlock(&board_lock);
}
#endif
void *smsspiphy_init(void *context, void (*smsspi_interruptHandler) (void *),
		     void *intr_context)
{
    struct spiphy_dev_s *spiphy_dev;
    struct spi_master	*master;
    struct spi_device *spi_d;
    struct tegra_spi_device_controller_data *controll_d;

    spi_log("[cmmbspi]smsspiphy_init entering\n");    
    spiphy_dev = kmalloc(sizeof(struct spiphy_dev_s), GFP_KERNEL);
    if(!spiphy_dev )
    {
        printk("[cmmbspi]spiphy_dev is null in smsspiphy_init\n") ;
        return NULL;
    } 
    spi_d = kmalloc(sizeof(struct spi_device), GFP_KERNEL);
    if(!spi_d )
    {
        printk("[cmmbspi]master is null in smsspiphy_init\n") ;
        return NULL;
    }
    master = kmalloc(sizeof(struct spi_master), GFP_KERNEL);
    if(!master )
    {
        printk("[cmmbspi]master is null in smsspiphy_init\n") ;
        return NULL;
    }
    controll_d = kmalloc(sizeof(struct tegra_spi_device_controller_data), GFP_KERNEL);
    if(!controll_d )
    {
        printk("[cmmbspi]controll_d is null in smsspiphy_init\n") ;
        return NULL;
    }
    controll_d->is_hw_based_cs=true;
    controll_d->cs_setup_clk_count=2;
    controll_d->cs_hold_clk_count=2;
    
    memset(spi_d, 0, sizeof *spi_d);
    memset(master, 0, sizeof *master);
    master =spi_busnum_to_master(0);  
    // spi_log("[cmmbspi]master =0x%x\n",master) ;
    spi_d->master = master;     
    spiphy_dev->spi_device=spi_d;
    //scan_boardinfo(master ) ;     
    spiphy_dev->spi_device->max_speed_hz=10000 * 1000;
    spiphy_dev->spi_device->chip_select=0;
    spiphy_dev->spi_device->mode=SPI_MODE_0;
    spiphy_dev->spi_device->bits_per_word=8;
    spiphy_dev->spi_device->controller_data=controll_d;
    strcpy(spiphy_dev->spi_device->modalias, "spi_sms2186");
    spi_log("[cmmbspi]smsspiphy_init_2 master->num_chipselect=%d\n",spiphy_dev->spi_device->master->num_chipselect);
    spi_log("[cmmbspi]smsspiphy_init_2 spi_device->max_speed_hz=%d\n", spiphy_dev->spi_device->max_speed_hz);

    smschip_gpio_init();
    spiphy_dev->interruptHandler = smsspi_interruptHandler;
    spiphy_dev->intr_context = intr_context;
    spi_log("[cmmbspi]smsspiphy_init exiting\n");
   // kfree(master);
   // kfree(spi_d);    
    return spiphy_dev;

}

int smsspiphy_deinit(void *context)
{
    struct spiphy_dev_s *spiphy_dev = (struct spiphy_dev_s *) context;
    spi_log("[cmmbspi]smsspiphy_deinit entering\n");
    chip_powerdown();
    spi_log("[cmmbspi]smsspiphy_deinit exiting\n");
    kfree(spiphy_dev->spi_device->controller_data);
    kfree(spiphy_dev->spi_device->master);
    kfree(spiphy_dev->spi_device);   
    return 0;
}

void smsspiphy_set_config(struct spiphy_dev_s *spiphy_dev, int clock_divider)
{
}
void prepareForFWDnl(void *context)
{
}
void fwDnlComplete(void *context, int App)
{
}
