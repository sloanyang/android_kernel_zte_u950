
#smsmdtv-objs := smscoreapi.o sms-cards.o smsendian.o
smsspi-objs := smsspilog.o smsspicommon.o smsspiphy_pxa.o smschar.o

#obj-$(CONFIG_SMS_SIANO_MDTV) += smsmdtv.o smsdvb.o
obj-y += smscoreapi.o
obj-y += sms-cards.o 
obj-y += smsendian.o
obj-$(CONFIG_SMS_USB_DRV) += smsusb.o
obj-$(CONFIG_SMS_SDIO_DRV) += smssdio.o			
#obj-$(CONFIG_SMS_SPI_PXA310_DRV) += smsspi.o
obj-y += smsspi.o

EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core

EXTRA_CFLAGS += $(extra-cflags-y) $(extra-cflags-m)

