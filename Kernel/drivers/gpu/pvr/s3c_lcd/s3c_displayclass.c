/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 * 		Samsung Electronics System LSI. modify
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful but, except 
 * as otherwise stated in writing, without any warranty; without even the 
 * implied warranty of merchantability or fitness for a particular purpose. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <asm/hardirq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/memory.h>
#include <plat/regs-fb.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"

#include "s3c_lcd.h"

static volatile unsigned int regs;

#define S3C_MAX_BACKBUFFERRS 	1

#define S3C_DISPLAY_FORMAT_NUM 1
#define S3C_DISPLAY_DIM_NUM 1

#define S3CFB_SET_VSYNC_INT 	_IOW ('F', 206, u32)
#define FBIO_WAITFORVSYNC _IO ('F', 32) 

#define VSYCN_IRQ 0x61
//take window config
#ifdef CONFIG_FB_S3C_DEFAULT_WINDOW
#define FB_NUM CONFIG_FB_S3C_DEFAULT_WINDOW
#else
#define FB_NUM 0
#endif

#define S3C_NUM_TOTAL_BUFFER (S3C_MAX_BACKBUFFERRS+1)

extern int s3cfb_direct_ioctl(int id, unsigned int cmd, unsigned long arg);

#define DC_S3C_LCD_COMMAND_COUNT 1

typedef struct S3C_FRAME_BUFFER_TAG
{
	IMG_CPU_VIRTADDR bufferVAddr;
	IMG_SYS_PHYADDR bufferPAddr;
	IMG_UINT32 byteSize;
}S3C_FRAME_BUFFER;

typedef void *		 S3C_HANDLE;


typedef enum tag_s3c_bool
{
	S3C_FALSE = 0,
	S3C_TRUE  = 1,
	
} S3C_BOOL, *S3C_PBOOL;

typedef struct S3C_SWAPCHAIN_TAG
{

	unsigned long   ulBufferCount;

	S3C_FRAME_BUFFER  	*psBuffer;
	
}S3C_SWAPCHAIN;

typedef struct S3C_VSYNC_FLIP_ITEM_TAG
{

	S3C_HANDLE		  hCmdComplete;

	S3C_FRAME_BUFFER	*psFb;

	unsigned long	  ulSwapInterval;

	S3C_BOOL		  bValid;

	S3C_BOOL		  bFlipped;

	S3C_BOOL		  bCmdCompleted;

} S3C_VSYNC_FLIP_ITEM;

typedef struct S3C_LCD_DEVINFO_TAG
{
	IMG_UINT32 						ui32DisplayID;
	DISPLAY_INFO 					sDisplayInfo;

	// sys surface info
	S3C_FRAME_BUFFER				sSysBuffer;

	// number of supported format
	IMG_UINT32 						ui32NumFormats;

	// list of supported display format
	DISPLAY_FORMAT 					asDisplayForamtList[S3C_DISPLAY_FORMAT_NUM];

	IMG_UINT32 						ui32NumDims;
	DISPLAY_DIMS					asDisplayDimList[S3C_DISPLAY_DIM_NUM];

	// jump table into pvr services
	PVRSRV_DC_DISP2SRV_KMJTABLE 	sPVRJTable;

	// jump table into DC
	PVRSRV_DC_SRV2DISP_KMJTABLE 	sDCJTable;

	// backbuffer info
	S3C_FRAME_BUFFER				asBackBuffers[S3C_MAX_BACKBUFFERRS];


	S3C_SWAPCHAIN					*psSwapChain;


	S3C_VSYNC_FLIP_ITEM				asVSyncFlips[S3C_NUM_TOTAL_BUFFER];

	unsigned long					ulInsertIndex;
	

	unsigned long					ulRemoveIndex;
	S3C_BOOL						bFlushCommands;

}S3C_LCD_DEVINFO;

static S3C_LCD_DEVINFO *g_psLCDInfo = NULL;

static volatile unsigned int * LCDControllerBase=NULL;


extern IMG_BOOL IMG_IMPORT PVRGetDisplayClassJTable(PVRSRV_DC_DISP2SRV_KMJTABLE *psJTable);

static void AdvanceFlipIndex(S3C_LCD_DEVINFO *psDevInfo,
							 unsigned long	*pulIndex)
{
	unsigned long	ulMaxFlipIndex;

	ulMaxFlipIndex = psDevInfo->psSwapChain->ulBufferCount - 1;
	if (ulMaxFlipIndex >= S3C_NUM_TOTAL_BUFFER)
	{
		ulMaxFlipIndex = S3C_NUM_TOTAL_BUFFER-1;
	}

	(*pulIndex)++;

	if (*pulIndex > ulMaxFlipIndex )
	{
		*pulIndex = 0;
	}
}
static IMG_VOID ResetVSyncFlipItems(S3C_LCD_DEVINFO* psDevInfo)
{
	unsigned long i;

	psDevInfo->ulInsertIndex = 0;
	psDevInfo->ulRemoveIndex = 0;

	for(i=0; i < S3C_NUM_TOTAL_BUFFER; i++)
	{
		psDevInfo->asVSyncFlips[i].bValid = S3C_FALSE;
		psDevInfo->asVSyncFlips[i].bFlipped = S3C_FALSE;
		psDevInfo->asVSyncFlips[i].bCmdCompleted = S3C_FALSE;
	}
}

static IMG_VOID S3C_Clear_interrupt(void)
{
	u32 cfg = 0;

	cfg = readl(regs + S3C_VIDINTCON1);

	if (cfg & S3C_VIDINTCON1_INTFIFOPEND)
		printk("fifo underrun occur\n");

	cfg |= (S3C_VIDINTCON1_INTVPPEND | S3C_VIDINTCON1_INTI80PEND |
		S3C_VIDINTCON1_INTFRMPEND | S3C_VIDINTCON1_INTFIFOPEND);

	writel(cfg, regs + S3C_VIDINTCON1);
}
#if 1
static IMG_VOID S3C_DisableVSyncInterrupt(void)
{
	
	unsigned int cfg = 0;

	cfg = readl(regs + S3C_VIDINTCON0);
	cfg &= ~(S3C_VIDINTCON0_INTFRMEN_ENABLE | S3C_VIDINTCON0_INT_ENABLE);

	cfg |= (S3C_VIDINTCON0_INTFRMEN_DISABLE |
		S3C_VIDINTCON0_INT_DISABLE);

	writel(cfg, regs + S3C_VIDINTCON0);

}

static IMG_VOID S3C_EnableVSyncInterrupt(void)
{


	unsigned int cfg = 0;
	
	cfg = readl(regs + S3C_VIDINTCON0);
	cfg &= ~(S3C_VIDINTCON0_INTFRMEN_ENABLE | S3C_VIDINTCON0_INT_ENABLE);

	cfg |= (S3C_VIDINTCON0_INTFRMEN_ENABLE |
		S3C_VIDINTCON0_INT_ENABLE);
	
	writel(cfg, regs + S3C_VIDINTCON0);

}
#endif

static IMG_VOID S3C_Flip(S3C_LCD_DEVINFO  *psDevInfo,
					   S3C_FRAME_BUFFER *fb)
{

	LCDControllerBase[0xc/4] =(1<<11);

	switch (FB_NUM)
	{
		case 0:
			LCDControllerBase[0xa0/4] = fb->bufferPAddr.uiAddr;
			LCDControllerBase[0xd0/4] = (fb->bufferPAddr.uiAddr+fb->byteSize)&0xffffff;
			break;

		case 1:
			LCDControllerBase[0xa8/4] = fb->bufferPAddr.uiAddr;
			LCDControllerBase[0xd8/4] = (fb->bufferPAddr.uiAddr+fb->byteSize)&0xffffff;
			break;

		case 2:
			LCDControllerBase[0xB0/4] = fb->bufferPAddr.uiAddr;
			LCDControllerBase[0xE0/4] = (fb->bufferPAddr.uiAddr+fb->byteSize)&0xffffff;
			break;

		case 3:
			LCDControllerBase[0xE8/4] = fb->bufferPAddr.uiAddr;
			LCDControllerBase[0xE8/4] = (fb->bufferPAddr.uiAddr+fb->byteSize)&0xffffff;
			break;

		case 4:
			LCDControllerBase[0xC0/4] = fb->bufferPAddr.uiAddr;
			LCDControllerBase[0xF0/4] = (fb->bufferPAddr.uiAddr+fb->byteSize)&0xffffff;
			break;
	}

	LCDControllerBase[0xc/4] = 0;
}
static void FlushInternalVSyncQueue(S3C_LCD_DEVINFO*psDevInfo)
{
	S3C_VSYNC_FLIP_ITEM*  psFlipItem;


	S3C_DisableVSyncInterrupt();


	psFlipItem = &psDevInfo->asVSyncFlips[psDevInfo->ulRemoveIndex];

	while(psFlipItem->bValid)
	{
		if(psFlipItem->bFlipped ==S3C_FALSE)
		{
		
			S3C_Flip (psDevInfo, psFlipItem->psFb);
		}


		if(psFlipItem->bCmdCompleted == S3C_FALSE)
		{

			psDevInfo->sPVRJTable.pfnPVRSRVCmdComplete((IMG_HANDLE)psFlipItem->hCmdComplete, IMG_FALSE);
		}

		AdvanceFlipIndex(psDevInfo, &psDevInfo->ulRemoveIndex);


		psFlipItem->bFlipped = S3C_FALSE;
		psFlipItem->bCmdCompleted = S3C_FALSE;
		psFlipItem->bValid = S3C_FALSE;

	
		psFlipItem = &psDevInfo->asVSyncFlips[psDevInfo->ulRemoveIndex];
	}

	psDevInfo->ulInsertIndex = 0;
	psDevInfo->ulRemoveIndex = 0;

	S3C_EnableVSyncInterrupt();

}

static irqreturn_t S3C_VSyncISR(int irq, void *dev_id)
{

	S3C_LCD_DEVINFO *psDevInfo = g_psLCDInfo;
	S3C_VSYNC_FLIP_ITEM *psFlipItem;

	S3C_DisableVSyncInterrupt();
	S3C_Clear_interrupt();

	if(psDevInfo == NULL || dev_id != g_psLCDInfo || !psDevInfo->psSwapChain)
	{
		goto Handled;
	}

	psFlipItem = &psDevInfo->asVSyncFlips[psDevInfo->ulRemoveIndex];
	
	while(psFlipItem->bValid)
	{

		if(psFlipItem->bFlipped)
		{
		
			if(!psFlipItem->bCmdCompleted)
			{
				IMG_BOOL bScheduleMISR;
			
#if 1
				bScheduleMISR = IMG_TRUE;
#else
				bScheduleMISR = IMG_FALSE;
#endif

				psDevInfo->sPVRJTable.pfnPVRSRVCmdComplete((IMG_HANDLE)psFlipItem->hCmdComplete, bScheduleMISR);
			
				psFlipItem->bCmdCompleted = S3C_TRUE;
			}

		
			psFlipItem->ulSwapInterval--;

		
			if(psFlipItem->ulSwapInterval == 0)
			{
		
				AdvanceFlipIndex(psDevInfo, &psDevInfo->ulRemoveIndex);

				psFlipItem->bCmdCompleted = S3C_FALSE;
				psFlipItem->bFlipped = S3C_FALSE;

			
				psFlipItem->bValid = S3C_FALSE;
			}
			else
			{
			
				break;
			}
		}
		else
		{
		
			S3C_Flip (psDevInfo, psFlipItem->psFb);


			psFlipItem->bFlipped = S3C_TRUE;


			break;
		}

	
		psFlipItem = &psDevInfo->asVSyncFlips[psDevInfo->ulRemoveIndex];
	}


Handled:
	S3C_EnableVSyncInterrupt();

	return IRQ_HANDLED;
}

static IMG_VOID S3C_InstallVsyncISR(void)
{	
	if(request_irq(VSYCN_IRQ, S3C_VSyncISR, IRQF_SHARED , "s3cfb", g_psLCDInfo))
	{
		printk("S3C_InstallVsyncISR: Couldn't install system LISR on IRQ %d", VSYCN_IRQ);
		return;
	}
	//printk("[s3c_lcd] S3C_InstallVsyn \n");
}
static IMG_VOID S3C_UninstallVsyncISR(void)
{	
	free_irq(VSYCN_IRQ, g_psLCDInfo);
}

static PVRSRV_ERROR OpenDCDevice(IMG_UINT32 ui32DeviceID,
								 IMG_HANDLE *phDevice,
								 PVRSRV_SYNC_DATA* psSystemBufferSyncData)
{
	PVR_UNREFERENCED_PARAMETER(ui32DeviceID);

	*phDevice =  (IMG_HANDLE)g_psLCDInfo;

	return PVRSRV_OK;
}

static PVRSRV_ERROR CloseDCDevice(IMG_HANDLE hDevice)
{
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;

	PVR_UNREFERENCED_PARAMETER(hDevice);


	if(psLCDInfo == g_psLCDInfo)
	{}

	return PVRSRV_OK;
}

static PVRSRV_ERROR EnumDCFormats(IMG_HANDLE		hDevice,
								  IMG_UINT32		*pui32NumFormats,
								  DISPLAY_FORMAT	*psFormat)
{
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;
	int i;

	if(!hDevice || !pui32NumFormats)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*pui32NumFormats = S3C_DISPLAY_FORMAT_NUM;

	if(psFormat)
	{
		for (i = 0 ; i < S3C_DISPLAY_FORMAT_NUM ; i++)
			psFormat[i] = psLCDInfo->asDisplayForamtList[i];
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR EnumDCDims(IMG_HANDLE		hDevice,
							   DISPLAY_FORMAT	*psFormat,
							   IMG_UINT32		*pui32NumDims,
							   DISPLAY_DIMS		*psDim)
{
	int i;

	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;

	if(!hDevice || !psFormat || !pui32NumDims)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*pui32NumDims = S3C_DISPLAY_DIM_NUM;

	if(psDim)
	{
		for (i = 0 ; i < S3C_DISPLAY_DIM_NUM ; i++)
			psDim[i] = psLCDInfo->asDisplayDimList[i];

	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR GetDCSystemBuffer(IMG_HANDLE hDevice, IMG_HANDLE *phBuffer)
{
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;

	if(!hDevice || !phBuffer)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	*phBuffer=(IMG_HANDLE)(&(psLCDInfo->sSysBuffer));
	return PVRSRV_OK;
}

static PVRSRV_ERROR GetDCInfo(IMG_HANDLE hDevice, DISPLAY_INFO *psDCInfo)
{
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;

	if(!hDevice || !psDCInfo)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*psDCInfo = psLCDInfo->sDisplayInfo;

	return PVRSRV_OK;
}

static PVRSRV_ERROR GetDCBufferAddr(IMG_HANDLE		hDevice,
									IMG_HANDLE		hBuffer,
									IMG_SYS_PHYADDR	**ppsSysAddr,
									IMG_UINT32		*pui32ByteSize,
									IMG_VOID		**ppvCpuVAddr,
									IMG_HANDLE		*phOSMapInfo,
									IMG_BOOL		*pbIsContiguous)
{
	S3C_FRAME_BUFFER *buf = (S3C_FRAME_BUFFER *)hBuffer;
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;
	PVR_UNREFERENCED_PARAMETER(psLCDInfo);

	//printk("GetDCBufferAddr+++++ hBuffer=%x\n",(int)hBuffer);

	if(!hDevice || !hBuffer || !ppsSysAddr || !pui32ByteSize)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	*phOSMapInfo = IMG_NULL;
	*pbIsContiguous = IMG_TRUE;

	*ppvCpuVAddr = (IMG_VOID *)buf->bufferVAddr;
	*ppsSysAddr = &(buf->bufferPAddr);
	*pui32ByteSize = buf->byteSize;

//	printk("GetDCBufferAddr:cpuVAddr=%p,sysAddr=%p\n",(*ppvCpuVAddr), (void*)(unsigned int)(**ppsSysAddr));

	return PVRSRV_OK;
}

static PVRSRV_ERROR CreateDCSwapChain(IMG_HANDLE hDevice,
									  IMG_UINT32 ui32Flags,
									  DISPLAY_SURF_ATTRIBUTES *psDstSurfAttrib,
									  DISPLAY_SURF_ATTRIBUTES *psSrcSurfAttrib,
									  IMG_UINT32 ui32BufferCount,
									  PVRSRV_SYNC_DATA **ppsSyncData,
									  IMG_UINT32 ui32OEMFlags,
									  IMG_HANDLE *phSwapChain,
									  IMG_UINT32 *pui32SwapChainID)
{
	IMG_UINT32 i;

	S3C_FRAME_BUFFER *psBuffer;
	S3C_SWAPCHAIN *psSwapChain;
	S3C_LCD_DEVINFO *psDevInfo = (S3C_LCD_DEVINFO*)hDevice;
	
	//printk("CreateDCSwapChain:ui32BufferCount=%d\n",(int)ui32BufferCount);

	PVR_UNREFERENCED_PARAMETER(ui32OEMFlags);
	PVR_UNREFERENCED_PARAMETER(pui32SwapChainID);


	if(!hDevice
	|| !psDstSurfAttrib
	|| !psSrcSurfAttrib
	|| !ppsSyncData
	|| !phSwapChain)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if(ui32BufferCount > S3C_NUM_TOTAL_BUFFER)
	{
		return PVRSRV_ERROR_TOOMANYBUFFERS;
	}
	

	if(psDevInfo->psSwapChain)
	{
		return (PVRSRV_ERROR_FLIP_CHAIN_EXISTS);
	}

	psSwapChain = (S3C_SWAPCHAIN *)kmalloc(sizeof(S3C_SWAPCHAIN),GFP_KERNEL);
	psBuffer = (S3C_FRAME_BUFFER*)kmalloc(sizeof(S3C_FRAME_BUFFER) * ui32BufferCount, GFP_KERNEL);
	
	if(!psBuffer)
	{
		kfree(psSwapChain);
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
	}
	
	psSwapChain->ulBufferCount = (unsigned long)ui32BufferCount;
	psSwapChain->psBuffer = psBuffer;

	psBuffer[0].bufferPAddr = psDevInfo->sSysBuffer.bufferPAddr;
	psBuffer[0].bufferVAddr = psDevInfo->sSysBuffer.bufferVAddr;
	psBuffer[0].byteSize = psDevInfo->sSysBuffer.byteSize;

	for (i=1; i<ui32BufferCount; i++)
	{
		psBuffer[i].bufferPAddr = psDevInfo->asBackBuffers[i-1].bufferPAddr;
		psBuffer[i].bufferVAddr = psDevInfo->asBackBuffers[i-1].bufferVAddr;
		psBuffer[i].byteSize = psDevInfo->asBackBuffers[i-1].byteSize;
	}
	
	//printk("CreateDCSwapChain:swapchain.buffercount=%d,sc=%p\n",(int)psSwapChain->ulBufferCount, psSwapChain);

	*phSwapChain = (IMG_HANDLE)psSwapChain;
	*pui32SwapChainID =(IMG_UINT32)psSwapChain;	
	

	psDevInfo->psSwapChain = psSwapChain;
    
	S3C_DisableVSyncInterrupt();
    ResetVSyncFlipItems(psDevInfo);
	S3C_InstallVsyncISR();
	
	S3C_EnableVSyncInterrupt();

	return PVRSRV_OK;
}


static PVRSRV_ERROR DestroyDCSwapChain(IMG_HANDLE hDevice,
									   IMG_HANDLE hSwapChain)
{
	S3C_SWAPCHAIN *sc = (S3C_SWAPCHAIN *)hSwapChain;
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;

	if(!hDevice
	|| !hSwapChain)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	FlushInternalVSyncQueue(psLCDInfo);


	S3C_Flip(psLCDInfo, &psLCDInfo->sSysBuffer);
		
	kfree(sc->psBuffer);
	kfree(sc);

	if (psLCDInfo->psSwapChain == sc)
		psLCDInfo->psSwapChain = NULL;	
	
	ResetVSyncFlipItems(psLCDInfo);

	S3C_DisableVSyncInterrupt();
	S3C_UninstallVsyncISR();

	return PVRSRV_OK;
}

static PVRSRV_ERROR SetDCDstRect(IMG_HANDLE	hDevice,
								 IMG_HANDLE	hSwapChain,
								 IMG_RECT	*psRect)
{


	PVR_UNREFERENCED_PARAMETER(hDevice);
	PVR_UNREFERENCED_PARAMETER(hSwapChain);
	PVR_UNREFERENCED_PARAMETER(psRect);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}


static PVRSRV_ERROR SetDCSrcRect(IMG_HANDLE	hDevice,
								 IMG_HANDLE	hSwapChain,
								 IMG_RECT	*psRect)
{


	PVR_UNREFERENCED_PARAMETER(hDevice);
	PVR_UNREFERENCED_PARAMETER(hSwapChain);
	PVR_UNREFERENCED_PARAMETER(psRect);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}


static PVRSRV_ERROR SetDCDstColourKey(IMG_HANDLE	hDevice,
									  IMG_HANDLE	hSwapChain,
									  IMG_UINT32	ui32CKColour)
{
	PVR_UNREFERENCED_PARAMETER(hDevice);
	PVR_UNREFERENCED_PARAMETER(hSwapChain);
	PVR_UNREFERENCED_PARAMETER(ui32CKColour);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SetDCSrcColourKey(IMG_HANDLE	hDevice,
									  IMG_HANDLE	hSwapChain,
									  IMG_UINT32	ui32CKColour)
{
	PVR_UNREFERENCED_PARAMETER(hDevice);
	PVR_UNREFERENCED_PARAMETER(hSwapChain);
	PVR_UNREFERENCED_PARAMETER(ui32CKColour);



	return PVRSRV_ERROR_NOT_SUPPORTED;
}

IMG_VOID S3CSetState(IMG_HANDLE hDevice, IMG_UINT32 ui32State)
{
	S3C_LCD_DEVINFO	*psDevInfo;

	psDevInfo = (S3C_LCD_DEVINFO*)hDevice;

	if (ui32State == DC_STATE_FLUSH_COMMANDS)
	{
		if (psDevInfo->psSwapChain != 0)
		{
			FlushInternalVSyncQueue(psDevInfo);
		}

		psDevInfo->bFlushCommands =S3C_TRUE;
	}
	else if (ui32State == DC_STATE_NO_FLUSH_COMMANDS)
	{
		psDevInfo->bFlushCommands = S3C_FALSE;
	}
}

static PVRSRV_ERROR GetDCBuffers(IMG_HANDLE hDevice,
								 IMG_HANDLE hSwapChain,
								 IMG_UINT32 *pui32BufferCount,
								 IMG_HANDLE *phBuffer)
{
	S3C_LCD_DEVINFO *psLCDInfo = (S3C_LCD_DEVINFO*)hDevice;
	int	i;
	

	if(!hDevice
	|| !hSwapChain
	|| !pui32BufferCount
	|| !phBuffer)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	//printk("GetDCBuffers:hSwapChain=%p ui32BufferCount=%d\n",(void*)hSwapChain,(int)*pui32BufferCount);

#if 0
	*pui32BufferCount=S3C_NUM_TOTAL_BUFFER;
	phBuffer[0] = (IMG_HANDLE)(&(psLCDInfo->sSysBuffer));
	phBuffer[1] = (IMG_HANDLE)(&(psLCDInfo->asBackBuffers[0]));
	phBuffer[2] = (IMG_HANDLE)(&(psLCDInfo->asBackBuffers[1]));
#else
	*pui32BufferCount = S3C_MAX_BACKBUFFERRS + 1;
	phBuffer[0] = (IMG_HANDLE)(&(psLCDInfo->sSysBuffer));
	for (i=0; i<S3C_MAX_BACKBUFFERRS; i++)
	{
		phBuffer[i+1] = (IMG_HANDLE)(&(psLCDInfo->asBackBuffers[i]));
	}
#endif
	return PVRSRV_OK;
}

static PVRSRV_ERROR SwapToDCBuffer(IMG_HANDLE	hDevice,
								   IMG_HANDLE	hBuffer,
								   IMG_UINT32	ui32SwapInterval,
								   IMG_HANDLE	hPrivateTag,
								   IMG_UINT32	ui32ClipRectCount,
								   IMG_RECT		*psClipRect)
{

	//printk("SwapToDCBuffer+++\n");

	PVR_UNREFERENCED_PARAMETER(ui32SwapInterval);
	PVR_UNREFERENCED_PARAMETER(hPrivateTag);
	PVR_UNREFERENCED_PARAMETER(psClipRect);

	if(!hDevice
	|| !hBuffer
	|| (ui32ClipRectCount != 0))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	//printk("SwapToDCBuffer:swapinterval=%d,cliprectcount=%d\n", (int)ui32SwapInterval, (int)ui32ClipRectCount);



	return PVRSRV_OK;
}

static PVRSRV_ERROR SwapToDCSystem(IMG_HANDLE hDevice,
								   IMG_HANDLE hSwapChain)
{
	//printk("SwapToDCSystem++++++++++++++\n");

	if(!hDevice
	|| !hSwapChain)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return PVRSRV_OK;
}

#if 0
static int AllocLinearMemory(IMG_UINT32 size,IMG_UINT32 *vaddr,IMG_UINT32 *paddr)
{
	dma_addr_t dma;
	IMG_VOID *pvLinAddr;

	pvLinAddr = dma_alloc_coherent(NULL, size, &dma, GFP_KERNEL);

	if(pvLinAddr == NULL)
	{
		return 1;
	}


	*paddr = (IMG_UINT32)dma;
	*vaddr = (IMG_UINT32)pvLinAddr;

	return 0;
}


static void FreeLinearMemory(  IMG_UINT32 size,
			   IMG_UINT32 *vaddr,
			    IMG_UINT32 *paddr)
{
	dma_free_coherent(NULL, size, vaddr, (dma_addr_t)paddr);
}
#endif

static IMG_BOOL ProcessFlip(IMG_HANDLE	hCmdCookie,
							IMG_UINT32	ui32DataSize,
							IMG_VOID	*pvData)
{
	DISPLAYCLASS_FLIP_COMMAND *psFlipCmd;
	S3C_LCD_DEVINFO *psDevInfo;
	S3C_FRAME_BUFFER *fb;
	S3C_VSYNC_FLIP_ITEM* psFlipItem;


	if(!hCmdCookie || !pvData)
	{
		return IMG_FALSE;
	}

	psFlipCmd = (DISPLAYCLASS_FLIP_COMMAND*)pvData;
	if (psFlipCmd == IMG_NULL || sizeof(DISPLAYCLASS_FLIP_COMMAND) != ui32DataSize)
	{
		return IMG_FALSE;
	}

	psDevInfo = (S3C_LCD_DEVINFO*)psFlipCmd->hExtDevice;
	fb = (S3C_FRAME_BUFFER*)psFlipCmd->hExtBuffer; 
	
	if (psDevInfo->bFlushCommands)
	{
		psDevInfo->sPVRJTable.pfnPVRSRVCmdComplete(hCmdCookie, IMG_FALSE);
		return IMG_TRUE;
	}

	if(psFlipCmd->ui32SwapInterval == 0)
	{

	
		S3C_Flip(psDevInfo, fb);
	

		psDevInfo->sPVRJTable.pfnPVRSRVCmdComplete(hCmdCookie, IMG_FALSE);
	
		return IMG_TRUE;

	}

	S3C_DisableVSyncInterrupt();
	psFlipItem = &psDevInfo->asVSyncFlips[psDevInfo->ulInsertIndex];
	

	if(!psFlipItem->bValid)
	{
		if(psDevInfo->ulInsertIndex == psDevInfo->ulRemoveIndex)
		{
		
			S3C_Flip(psDevInfo, fb);

			psFlipItem->bFlipped = S3C_TRUE;
		}
		else
		{
			psFlipItem->bFlipped = S3C_FALSE;
		}

		psFlipItem->hCmdComplete = hCmdCookie;
		psFlipItem->psFb= fb;
		psFlipItem->ulSwapInterval = (unsigned long)psFlipCmd->ui32SwapInterval;
		//printk("[ProcessFlip] ulSwapInterval=%d\n", (int)psFlipItem->ulSwapInterval);
		psFlipItem->bValid = S3C_TRUE;

		AdvanceFlipIndex(psDevInfo, &psDevInfo->ulInsertIndex);
		S3C_EnableVSyncInterrupt();

		return IMG_TRUE;

	}

	S3C_EnableVSyncInterrupt();
	return IMG_FALSE;
}

int init()
{
	IMG_UINT32 screen_w, screen_h;
	IMG_UINT32 pa_fb, va_fb;
	IMG_UINT32 byteSize;
	int	i;

	int rgb_format, bytes_per_pixel;

	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;


	s3cfb_direct_ioctl(FB_NUM, FBIOGET_FSCREENINFO, (unsigned long)&fix);
	s3cfb_direct_ioctl(FB_NUM, FBIOGET_VSCREENINFO, (unsigned long)&var);

	screen_w = var.xres;
	screen_h = var.yres;
	pa_fb = fix.smem_start;
	printk("PA FB = 0x%X, bits per pixel = %d\n", (unsigned int)fix.smem_start, (unsigned int)var.bits_per_pixel);
	va_fb = (unsigned long)phys_to_virt(pa_fb);

	printk("screen width=%d height=%d va=0x%x pa=0x%x\n", (int)screen_w, (int)screen_h, (unsigned int)va_fb, (unsigned int)pa_fb);
	
#if 1
	regs = (volatile unsigned int)ioremap(0xF8000000, 0x00100000);
#endif
	//spin_lock_init(g_psLCDInfo->psSwapChainLock);

	if (g_psLCDInfo == NULL)
	{
		PFN_CMD_PROC	pfnCmdProcList[DC_S3C_LCD_COMMAND_COUNT];
		IMG_UINT32	aui32SyncCountList[DC_S3C_LCD_COMMAND_COUNT][2];

		g_psLCDInfo = (S3C_LCD_DEVINFO*)kmalloc(sizeof(S3C_LCD_DEVINFO),GFP_KERNEL);


		g_psLCDInfo->ui32NumFormats = S3C_DISPLAY_FORMAT_NUM;
		switch (var.bits_per_pixel)
		{
		case 16:
			rgb_format = PVRSRV_PIXEL_FORMAT_RGB565;
			bytes_per_pixel = 2;
			break;
		case 32:
			rgb_format = PVRSRV_PIXEL_FORMAT_ARGB8888;
			bytes_per_pixel = 4;
			break;
		default:
			rgb_format = PVRSRV_PIXEL_FORMAT_ARGB8888;
			bytes_per_pixel = 4;
			break;
		}

		g_psLCDInfo->asDisplayForamtList[0].pixelformat = rgb_format;
		g_psLCDInfo->ui32NumDims = S3C_DISPLAY_DIM_NUM;
		g_psLCDInfo->asDisplayDimList[0].ui32ByteStride = (bytes_per_pixel) * screen_w;
		g_psLCDInfo->asDisplayDimList[0].ui32Height = screen_h;
		g_psLCDInfo->asDisplayDimList[0].ui32Width = screen_w;

		g_psLCDInfo->sSysBuffer.bufferPAddr.uiAddr = pa_fb;
		g_psLCDInfo->sSysBuffer.bufferVAddr = (IMG_CPU_VIRTADDR)va_fb;
		byteSize = screen_w * screen_h * bytes_per_pixel;
		g_psLCDInfo->sSysBuffer.byteSize = (IMG_UINT32)byteSize;

		for (i=0; i<S3C_MAX_BACKBUFFERRS; i++)
		{
			g_psLCDInfo->asBackBuffers[i].byteSize = g_psLCDInfo->sSysBuffer.byteSize;
#if 1
			// modified by jamie (2010.04.09)
			// to use the frame buffer already allocated by LCD driver.
			g_psLCDInfo->asBackBuffers[i].bufferPAddr.uiAddr = pa_fb + byteSize * (i+1);
			g_psLCDInfo->asBackBuffers[i].bufferVAddr = (IMG_CPU_VIRTADDR)phys_to_virt(g_psLCDInfo->asBackBuffers[i].bufferPAddr.uiAddr);

#else
			if(AllocLinearMemory(
				g_psLCDInfo->asBackBuffers[i].byteSize,
				(IMG_UINT32*)&(g_psLCDInfo->asBackBuffers[i].bufferVAddr),
				&(g_psLCDInfo->asBackBuffers[i].bufferPAddr.uiAddr)))
				return 1;
#endif
	
			printk("Back frameBuffer[%d].VAddr=%p PAddr=%p size=%d\n",
				i, 
				(void*)g_psLCDInfo->asBackBuffers[i].bufferVAddr,
				(void*)g_psLCDInfo->asBackBuffers[i].bufferPAddr.uiAddr,
				(int)g_psLCDInfo->asBackBuffers[i].byteSize);
		}
		
		g_psLCDInfo->bFlushCommands = S3C_FALSE;
		g_psLCDInfo->psSwapChain = NULL;

		PVRGetDisplayClassJTable(&(g_psLCDInfo->sPVRJTable));

		g_psLCDInfo->sDCJTable.ui32TableSize = sizeof(PVRSRV_DC_SRV2DISP_KMJTABLE);
		g_psLCDInfo->sDCJTable.pfnOpenDCDevice = OpenDCDevice;
		g_psLCDInfo->sDCJTable.pfnCloseDCDevice = CloseDCDevice;
		g_psLCDInfo->sDCJTable.pfnEnumDCFormats = EnumDCFormats;
		g_psLCDInfo->sDCJTable.pfnEnumDCDims = EnumDCDims;
		g_psLCDInfo->sDCJTable.pfnGetDCSystemBuffer = GetDCSystemBuffer;
		g_psLCDInfo->sDCJTable.pfnGetDCInfo = GetDCInfo;
		g_psLCDInfo->sDCJTable.pfnGetBufferAddr = GetDCBufferAddr;
		g_psLCDInfo->sDCJTable.pfnCreateDCSwapChain = CreateDCSwapChain;
		g_psLCDInfo->sDCJTable.pfnDestroyDCSwapChain = DestroyDCSwapChain;
		g_psLCDInfo->sDCJTable.pfnSetDCDstRect = SetDCDstRect;
		g_psLCDInfo->sDCJTable.pfnSetDCSrcRect = SetDCSrcRect;
		g_psLCDInfo->sDCJTable.pfnSetDCDstColourKey = SetDCDstColourKey;
		g_psLCDInfo->sDCJTable.pfnSetDCSrcColourKey = SetDCSrcColourKey;
		g_psLCDInfo->sDCJTable.pfnGetDCBuffers = GetDCBuffers;
		g_psLCDInfo->sDCJTable.pfnSwapToDCBuffer = SwapToDCBuffer;
		g_psLCDInfo->sDCJTable.pfnSwapToDCSystem = SwapToDCSystem;
		g_psLCDInfo->sDCJTable.pfnSetDCState = S3CSetState;

		g_psLCDInfo->sDisplayInfo.ui32MinSwapInterval=0;
		g_psLCDInfo->sDisplayInfo.ui32MaxSwapInterval=0;
		g_psLCDInfo->sDisplayInfo.ui32MaxSwapChains=1;
		g_psLCDInfo->sDisplayInfo.ui32MaxSwapChainBuffers=S3C_NUM_TOTAL_BUFFER;
		g_psLCDInfo->sDisplayInfo.ui32PhysicalWidthmm=var.width;	// width of lcd in mm 
		g_psLCDInfo->sDisplayInfo.ui32PhysicalHeightmm=var.height;	// height of lcd in mm 

		strncpy(g_psLCDInfo->sDisplayInfo.szDisplayName, "s3c_lcd", MAX_DISPLAY_NAME_SIZE);

		if(g_psLCDInfo->sPVRJTable.pfnPVRSRVRegisterDCDevice	(&(g_psLCDInfo->sDCJTable),
			(IMG_UINT32 *)(&(g_psLCDInfo->ui32DisplayID))) != PVRSRV_OK)
		{
			return 1;
		}

		//printk("deviceID:%d\n",(int)g_psLCDInfo->ui32DisplayID);

		// register flip command
		pfnCmdProcList[DC_FLIP_COMMAND] = ProcessFlip;
		aui32SyncCountList[DC_FLIP_COMMAND][0] = 0;
		aui32SyncCountList[DC_FLIP_COMMAND][1] = 2;

		if (g_psLCDInfo->sPVRJTable.pfnPVRSRVRegisterCmdProcList(g_psLCDInfo->ui32DisplayID,
			&pfnCmdProcList[0], aui32SyncCountList, DC_S3C_LCD_COMMAND_COUNT)
			!= PVRSRV_OK)
		{
			printk("failing register commmand proc list   deviceID:%d\n",(int)g_psLCDInfo->ui32DisplayID);
			return PVRSRV_ERROR_CANT_REGISTER_CALLBACK;
		}

		LCDControllerBase = (volatile unsigned int *)ioremap(0xf8000000,1024);
	}

	return 0;

}


void deInit()
{
#if 0
	int i;
#endif
	//printk("s3c_displayclass deinit++\n");
	
	g_psLCDInfo->sPVRJTable.pfnPVRSRVRemoveCmdProcList ((IMG_UINT32)g_psLCDInfo->ui32DisplayID,
														DC_S3C_LCD_COMMAND_COUNT);

	g_psLCDInfo->sPVRJTable.pfnPVRSRVRemoveDCDevice(g_psLCDInfo->ui32DisplayID);

#if 0	
	for (i=0; i<S3C_MAX_BACKBUFFERRS; i++)
		FreeLinearMemory(g_psLCDInfo->asBackBuffers[i].byteSize,
			(IMG_UINT32 *)g_psLCDInfo->asBackBuffers[i].bufferVAddr,
			(IMG_UINT32 *)g_psLCDInfo->asBackBuffers[i].bufferPAddr.uiAddr);
#endif
	if (g_psLCDInfo)
		kfree(g_psLCDInfo);

	g_psLCDInfo = NULL;

	if(LCDControllerBase)
		iounmap(LCDControllerBase);

	if(regs)
		iounmap((void*)regs);

}

