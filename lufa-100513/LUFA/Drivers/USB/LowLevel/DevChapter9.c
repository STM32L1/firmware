/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#define  __INCLUDE_FROM_USB_DRIVER
#include "../HighLevel/USBMode.h"

#if defined(USB_CAN_BE_DEVICE)

#define  __INCLUDE_FROM_DEVCHAPTER9_C
#include "DevChapter9.h"

uint8_t USB_ConfigurationNumber;

#if !defined(NO_DEVICE_SELF_POWER)
bool    USB_CurrentlySelfPowered;
#endif

#if !defined(NO_DEVICE_REMOTE_WAKEUP)
bool    USB_RemoteWakeupEnabled;
#endif

void USB_Device_ProcessControlRequest(void)
{
	bool     RequestHandled = false;
	uint8_t* RequestHeader  = (uint8_t*)&USB_ControlRequest;
	
	for (uint8_t RequestHeaderByte = 0; RequestHeaderByte < sizeof(USB_Request_Header_t); RequestHeaderByte++)
	  *(RequestHeader++) = Endpoint_Read_Byte();
	  
	uint8_t bmRequestType = USB_ControlRequest.bmRequestType;
	
	switch (USB_ControlRequest.bRequest)
	{
		case REQ_GetStatus:
			if ((bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_DEVICE)) ||
			    (bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_ENDPOINT)))
			{
				USB_Device_GetStatus();
				RequestHandled = true;
			}

			break;
		case REQ_ClearFeature:
		case REQ_SetFeature:
			if ((bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_DEVICE)) ||
			    (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_ENDPOINT)))
			{
				USB_Device_ClearSetFeature();
				RequestHandled = true;
			}

			break;
		case REQ_SetAddress:
			if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_DEVICE))
			{
				USB_Device_SetAddress();
				RequestHandled = true;
			}

			break;
		case REQ_GetDescriptor:
			if ((bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_DEVICE)) ||
			    (bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_INTERFACE)))
			{
				USB_Device_GetDescriptor();
				RequestHandled = true;
			}
			
			break;
		case REQ_GetConfiguration:
			if (bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_DEVICE))
			{
				USB_Device_GetConfiguration();
				RequestHandled = true;
			}

			break;
		case REQ_SetConfiguration:
			if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_DEVICE))
			{
				USB_Device_SetConfiguration();
				RequestHandled = true;
			}

			break;
	}

	if (!(RequestHandled))
	  EVENT_USB_Device_UnhandledControlRequest();
	  
	if (Endpoint_IsSETUPReceived())
	{
		Endpoint_StallTransaction();
		Endpoint_ClearSETUP();		
	}
}

static void USB_Device_SetAddress(void)
{
	uint8_t DeviceAddress = (USB_ControlRequest.wValue & 0x7F);

	Endpoint_ClearSETUP();
	
	Endpoint_ClearStatusStage();
	
	while (!(Endpoint_IsINReady()))
	{
		if (USB_DeviceState == DEVICE_STATE_Unattached)
		  return;
	}

	USB_DeviceState = (DeviceAddress) ? DEVICE_STATE_Addressed : DEVICE_STATE_Default;

	UDADDR = ((1 << ADDEN) | DeviceAddress);

	return;
}

static void USB_Device_SetConfiguration(void)
{
#if defined(FIXED_NUM_CONFIGURATIONS)
	if ((uint8_t)USB_ControlRequest.wValue > FIXED_NUM_CONFIGURATIONS)
	  return;
#else
	#if defined(USE_FLASH_DESCRIPTORS)
		#define MemoryAddressSpace  MEMSPACE_FLASH
	#elif defined(USE_EEPROM_DESCRIPTORS)
		#define MemoryAddressSpace  MEMSPACE_EEPROM
	#elif defined(USE_SRAM_DESCRIPTORS)
		#define MemoryAddressSpace  MEMSPACE_SRAM
	#else
		uint8_t MemoryAddressSpace;
	#endif
	
	USB_Descriptor_Device_t* DevDescriptorPtr;

	if (CALLBACK_USB_GetDescriptor((DTYPE_Device << 8), 0, (void*)&DevDescriptorPtr
	#if !defined(USE_FLASH_DESCRIPTORS) && !defined(USE_EEPROM_DESCRIPTORS) && !defined(USE_RAM_DESCRIPTORS)
	                               , &MemoryAddressSpace
	#endif
	                               ) == NO_DESCRIPTOR)
	{
		return;
	}
	
	if (MemoryAddressSpace == MEMSPACE_FLASH)
	{
		if (((uint8_t)USB_ControlRequest.wValue > pgm_read_byte(&DevDescriptorPtr->NumberOfConfigurations)))
		  return;
	}
	else if (MemoryAddressSpace == MEMSPACE_EEPROM)
	{
		if (((uint8_t)USB_ControlRequest.wValue > eeprom_read_byte(&DevDescriptorPtr->NumberOfConfigurations)))
		  return;
	}
	else
	{
		if ((uint8_t)USB_ControlRequest.wValue > DevDescriptorPtr->NumberOfConfigurations)
		  return;
	}
#endif
	
	Endpoint_ClearSETUP();

	USB_ConfigurationNumber = (uint8_t)USB_ControlRequest.wValue;

	Endpoint_ClearStatusStage();

	USB_DeviceState = (USB_ConfigurationNumber) ? DEVICE_STATE_Configured : DEVICE_STATE_Addressed;

	EVENT_USB_Device_ConfigurationChanged();
}

void USB_Device_GetConfiguration(void)
{
	Endpoint_ClearSETUP();

	Endpoint_Write_Byte(USB_ConfigurationNumber);
	Endpoint_ClearIN();

	Endpoint_ClearStatusStage();
}

#if !defined(NO_INTERNAL_SERIAL) && (defined(USB_SERIES_6_AVR) || defined(USB_SERIES_7_AVR))
static char USB_Device_NibbleToASCII(uint8_t Nibble)
{
	Nibble = ((Nibble & 0x0F) + '0');
	return (Nibble > '9') ? (Nibble + ('A' - '9' - 1)) : Nibble;
}

static void USB_Device_GetInternalSerialDescriptor(void)
{
	struct
	{
		USB_Descriptor_Header_t Header;
		int16_t                 UnicodeString[20];
	} SignatureDescriptor;

	SignatureDescriptor.Header.Type = DTYPE_String;
	SignatureDescriptor.Header.Size = sizeof(SignatureDescriptor);
	
	uint8_t SigReadAddress = 0x0E;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		for (uint8_t SerialCharNum = 0; SerialCharNum < 20; SerialCharNum++)
		{
			uint8_t SerialByte = boot_signature_byte_get(SigReadAddress);
			
			if (SerialCharNum & 0x01)
			{
				SerialByte >>= 4;
				SigReadAddress++;
			}
			
			SignatureDescriptor.UnicodeString[SerialCharNum] = USB_Device_NibbleToASCII(SerialByte);
		}
	}
	
	Endpoint_ClearSETUP();

	Endpoint_Write_Control_Stream_LE(&SignatureDescriptor, sizeof(SignatureDescriptor));

	Endpoint_ClearOUT();
}
#endif

static void USB_Device_GetDescriptor(void)
{
	void*    DescriptorPointer;
	uint16_t DescriptorSize;
	
	#if !defined(USE_FLASH_DESCRIPTORS) && !defined(USE_EEPROM_DESCRIPTORS) && !defined(USE_RAM_DESCRIPTORS)
	uint8_t  DescriptorAddressSpace;
	#endif
	
	#if !defined(NO_INTERNAL_SERIAL) && (defined(USB_SERIES_6_AVR) || defined(USB_SERIES_7_AVR))
	if (USB_ControlRequest.wValue == ((DTYPE_String << 8) | USE_INTERNAL_SERIAL))
	{
		USB_Device_GetInternalSerialDescriptor();
		return;
	}
	#endif
	
	if ((DescriptorSize = CALLBACK_USB_GetDescriptor(USB_ControlRequest.wValue, USB_ControlRequest.wIndex,
	                                                 &DescriptorPointer
	#if !defined(USE_FLASH_DESCRIPTORS) && !defined(USE_EEPROM_DESCRIPTORS) && !defined(USE_RAM_DESCRIPTORS)
	                                                 , &DescriptorAddressSpace
	#endif
													 )) == NO_DESCRIPTOR)
	{
		return;
	}
	
	Endpoint_ClearSETUP();

	#if defined(USE_RAM_DESCRIPTORS)
	Endpoint_Write_Control_Stream_LE(DescriptorPointer, DescriptorSize);
	#elif defined(USE_EEPROM_DESCRIPTORS)
	Endpoint_Write_Control_EStream_LE(DescriptorPointer, DescriptorSize);
	#elif defined(USE_FLASH_DESCRIPTORS)
	Endpoint_Write_Control_PStream_LE(DescriptorPointer, DescriptorSize);	
	#else
	if (DescriptorAddressSpace == MEMSPACE_FLASH)
	  Endpoint_Write_Control_PStream_LE(DescriptorPointer, DescriptorSize);	
	else if (DescriptorAddressSpace == MEMSPACE_EEPROM)
	  Endpoint_Write_Control_EStream_LE(DescriptorPointer, DescriptorSize);
	else
	  Endpoint_Write_Control_Stream_LE(DescriptorPointer, DescriptorSize);	
	#endif

	Endpoint_ClearOUT();
}

static void USB_Device_GetStatus(void)
{
	uint8_t CurrentStatus = 0;

	switch (USB_ControlRequest.bmRequestType)
	{
#if !defined(NO_DEVICE_SELF_POWER) || !defined(NO_DEVICE_REMOTE_WAKEUP)	
		case (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_DEVICE):
	#if !defined(NO_DEVICE_SELF_POWER)
			if (USB_CurrentlySelfPowered)
			  CurrentStatus |= FEATURE_SELFPOWERED_ENABLED;
	#endif

	#if !defined(NO_DEVICE_REMOTE_WAKEUP)			
			if (USB_RemoteWakeupEnabled)
			  CurrentStatus |= FEATURE_REMOTE_WAKEUP_ENABLED;
	#endif
			break;
#endif
#if !defined(CONTROL_ONLY_DEVICE)
		case (REQDIR_DEVICETOHOST | REQTYPE_STANDARD | REQREC_ENDPOINT):
			Endpoint_SelectEndpoint((uint8_t)USB_ControlRequest.wIndex & ENDPOINT_EPNUM_MASK);

			CurrentStatus = Endpoint_IsStalled();

			Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);			  

			break;
#endif
		default:
			return;
	}

	Endpoint_ClearSETUP();

	Endpoint_Write_Word_LE(CurrentStatus);
	Endpoint_ClearIN();
	
	Endpoint_ClearStatusStage();
}

static void USB_Device_ClearSetFeature(void)
{
	switch (USB_ControlRequest.bmRequestType & CONTROL_REQTYPE_RECIPIENT)
	{
#if !defined(NO_DEVICE_REMOTE_WAKEUP)			
		case REQREC_DEVICE:
			if ((uint8_t)USB_ControlRequest.wValue == FEATURE_REMOTE_WAKEUP)
			  USB_RemoteWakeupEnabled = (USB_ControlRequest.bRequest == REQ_SetFeature);
			else
			  return;
			
			break;			
#endif
#if !defined(CONTROL_ONLY_DEVICE)
		case REQREC_ENDPOINT:
			if ((uint8_t)USB_ControlRequest.wValue == FEATURE_ENDPOINT_HALT)
			{
				uint8_t EndpointIndex = ((uint8_t)USB_ControlRequest.wIndex & ENDPOINT_EPNUM_MASK);
				
				if (EndpointIndex == ENDPOINT_CONTROLEP)
				  return;

				Endpoint_SelectEndpoint(EndpointIndex);

				if (!(Endpoint_IsEnabled()))
				  return;

				if (USB_ControlRequest.bRequest == REQ_SetFeature)
				{
					Endpoint_StallTransaction();
				}
				else
				{
					Endpoint_ClearStall();
					Endpoint_ResetFIFO(EndpointIndex);
					Endpoint_ResetDataToggle();
				}
			}
			
			break;
#endif
		default:
			return;
	}

	Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);

	Endpoint_ClearSETUP();

	Endpoint_ClearStatusStage();
}

#endif
