#include "fram_i2c.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"
#include "misc.h"

#include "mbcrc.h"
#include "controller.h"
#include "core_cmFunc.h"

#define FRAM_I2C_GPIO				GPIOB
#define FRAM_I2C_GPIO_RCC			RCC_AHB1Periph_GPIOB
#define FRAM_I2C_GPIO_PIN_SDA		GPIO_Pin_9
#define FRAM_I2C_GPIO_PIN_SCL		GPIO_Pin_8
#define FRAM_I2C_GPIO_PINSOURCE_SDA	GPIO_PinSource9
#define FRAM_I2C_GPIO_PINSOURCE_SCL	GPIO_PinSource8

#define FRAM_I2C					I2C1
#define FRAM_I2C_RCC				RCC_APB1Periph_I2C1
#define FRAM_I2C_AF					GPIO_AF_I2C1
#define FRAM_I2C_ADDRESS			0xA0
#define FRAM_I2C_TIMEOUT			10000

xSemaphoreHandle xI2CBusMutex;

void FRAM_I2C_Init(void)
{
		 GPIO_InitTypeDef GPIO_InitStructure;
	     I2C_InitTypeDef  I2C_InitStructure;
	     RCC_APB1PeriphClockCmd(FRAM_I2C_RCC, ENABLE);

	     RCC_AHB1PeriphClockCmd(FRAM_I2C_GPIO_RCC, ENABLE);

	     GPIO_PinAFConfig(FRAM_I2C_GPIO, FRAM_I2C_GPIO_PINSOURCE_SDA, FRAM_I2C_AF);
	     GPIO_PinAFConfig(FRAM_I2C_GPIO, FRAM_I2C_GPIO_PINSOURCE_SCL, FRAM_I2C_AF);

	     GPIO_InitStructure.GPIO_Pin = FRAM_I2C_GPIO_PIN_SCL | FRAM_I2C_GPIO_PIN_SDA;
	     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	     GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	     GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	     GPIO_Init(FRAM_I2C_GPIO, &GPIO_InitStructure);

	     I2C_DeInit(FRAM_I2C);

	     I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	     I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	     I2C_InitStructure.I2C_OwnAddress1 = FRAM_I2C_ADDRESS;
	     I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	     I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	     I2C_InitStructure.I2C_ClockSpeed = 400000;

	     I2C_Init(FRAM_I2C, &I2C_InitStructure);
	     I2C_Cmd(FRAM_I2C, ENABLE);

	     xI2CBusMutex=xSemaphoreCreateMutex() ;
}

eErrorCode FRAM_I2C_Read_Buffer(uint16_t addr,uint8_t *buf, uint16_t buf_len)
{
	 uint32_t i2c_timeout=FRAM_I2C_TIMEOUT;
	 xSemaphoreTake( xI2CBusMutex, portMAX_DELAY );
	 {
		uint16_t i=0;
		//I2C_AcknowledgeConfig(FRAM_I2C, ENABLE);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_BUSY))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_GenerateSTART(FRAM_I2C, ENABLE);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_Send7bitAddress(FRAM_I2C, FRAM_I2C_ADDRESS, I2C_Direction_Transmitter);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	    {

	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_SendData(FRAM_I2C, (uint8_t)((addr & 0xFF00) >> 8));

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	    {

	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_SendData(FRAM_I2C, (uint8_t)(addr & 0x00FF));

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	    {

	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }
	    I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//----------------------------------------------------------------------------------
	    I2C_AcknowledgeConfig(FRAM_I2C, ENABLE);
	    I2C_NACKPositionConfig(FRAM_I2C, I2C_NACKPosition_Current);

	    I2C_GenerateSTART(FRAM_I2C, ENABLE);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_Send7bitAddress(FRAM_I2C, FRAM_I2C_ADDRESS, I2C_Direction_Receiver);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_ADDR))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    	//	I2C_GenerateSTOP(FRAM_I2C, ENABLE);

	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }
//
//
//
//
//	    for(i=0;i<buf_len;i++)
//	    {
//	    	i2c_timeout=FRAM_I2C_TIMEOUT;
//	    	while(!I2C_CheckEvent(FRAM_I2C,I2C_EVENT_MASTER_BYTE_RECEIVED))
//	    	{
//
//		    	if(i2c_timeout--==0)
//		    	{
//		    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//
//		    		xSemaphoreGive( xI2CBusMutex );
//		    		return ETIMEDOUT;
//		    	}
//	    	}
//
//	    	buf[i]=I2C_ReceiveData(FRAM_I2C);
//
//	    	if(i==(buf_len-1))
//	    	{
//	    		 I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);
//	    	}
//	    	else
//	    	{
//	    		 I2C_AcknowledgeConfig(FRAM_I2C, ENABLE);
//	    	}
//	    }
///-------------------------------

//	    if (buf_len == 1)
//	       {
//				 // Clear Ack bit
//
//				 I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);
//
//				 // EV6_1 -- must be atomic -- Clear ADDR, generate STOP
//
//				 __disable_irq();
//				 (void) FRAM_I2C->SR2;
//				 I2C_GenerateSTOP(FRAM_I2C,ENABLE);
//				 __enable_irq();
//
//				 // Receive data   EV7
//
//			  //   Timed(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_RXNE));
//				i2c_timeout=FRAM_I2C_TIMEOUT;
//				while(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_RXNE))
//				{
//
//					if(i2c_timeout--==0)
//					{
//						I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//						xSemaphoreGive( xI2CBusMutex );
//						return ETIMEDOUT;
//					}
//				}
//
//				*buf++ = I2C_ReceiveData(FRAM_I2C);
//	       }
//	     else if (buf_len == 2)
//	       {
//				 // Set POS flag
//
//				 I2C_NACKPositionConfig(FRAM_I2C, I2C_NACKPosition_Next);
//
//				 // EV6_1 -- must be atomic and in this order
//
//				 __disable_irq();
//				 (void) FRAM_I2C->SR2;                           // Clear ADDR flag
//				 I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);       // Clear Ack bit
//				 __enable_irq();
//
//				 // EV7_3  -- Wait for BTF, program stop, read data twice
//
//				 //Timed(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_BTF));
//					i2c_timeout=FRAM_I2C_TIMEOUT;
//					while(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_BTF))
//					{
//
//						if(i2c_timeout--==0)
//						{
//							I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//							xSemaphoreGive( xI2CBusMutex );
//							return ETIMEDOUT;
//						}
//					}
//
//				 __disable_irq();
//				 I2C_GenerateSTOP(FRAM_I2C,ENABLE);
//				 *buf++ = FRAM_I2C->DR;
//				 __enable_irq();
//
//				 *buf++ = FRAM_I2C->DR;
//	     }
//	     else
//	     {
//				 (void) FRAM_I2C->SR2;                           // Clear ADDR flag
//				 while (buf_len-- != 3)
//				 {
//					  // EV7 -- cannot guarantee 1 transfer completion time, wait for BTF
//							 //        instead of RXNE
//
//
//						i2c_timeout=FRAM_I2C_TIMEOUT;
//					  while(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_BTF))
//					  {
//
//							if(i2c_timeout--==0)
//							{
//								I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//								xSemaphoreGive( xI2CBusMutex );
//								return ETIMEDOUT;
//							}
//					   }
//					  *buf++ = I2C_ReceiveData(FRAM_I2C);
//				}
//
//				i2c_timeout=FRAM_I2C_TIMEOUT;
//				while(!I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_BTF))
//				{
//						if(i2c_timeout--==0)
//						{
//							I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//							xSemaphoreGive( xI2CBusMutex );
//							return ETIMEDOUT;
//						}
//				}
//
//	         // EV7_2 -- Figure 1 has an error, doesn't read N-2 !
//
//				 I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);           // clear ack bit
//
//				 __disable_irq();
//				 *buf++ = I2C_ReceiveData(FRAM_I2C);             // receive byte N-2
//				 I2C_GenerateSTOP(FRAM_I2C,ENABLE);                  // program stop
//				 __enable_irq();
//
//				 *buf++ = I2C_ReceiveData(FRAM_I2C);             // receive byte N-1
//
//				 // wait for byte N
//
//					i2c_timeout=FRAM_I2C_TIMEOUT;
//					while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
//					{
//						if(i2c_timeout--==0)
//						{
//							//I2C_GenerateSTOP(FRAM_I2C, ENABLE);
//							xSemaphoreGive( xI2CBusMutex );
//							return ETIMEDOUT;
//						}
//					}
//				 *buf++ = I2C_ReceiveData(FRAM_I2C);
//
//				  buf_len = 0;
//	       }
//
//
//
//        i2c_timeout=FRAM_I2C_TIMEOUT;
//		while(I2C_GetFlagStatus(FRAM_I2C, I2C_FLAG_STOPF))
//		{
//			if(i2c_timeout--==0)
//			{
//				xSemaphoreGive( xI2CBusMutex );
//				return ETIMEDOUT;
//			}
//		}


//	    I2C_GenerateSTOP(FRAM_I2C, ENABLE);


		if(buf_len>2)
		{								//More than two buf_len to receive
			i2c_timeout=FRAM_I2C_TIMEOUT;
			while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
			{	//Wait for the first byte
				if(i2c_timeout--==0)
				{
					I2C_GenerateSTOP(FRAM_I2C, ENABLE);
					xSemaphoreGive( xI2CBusMutex );
					return ETIMEDOUT;
				}
			}

			while (buf_len-- != 3)
			{
				*buf++=I2C_ReceiveData(FRAM_I2C);

				i2c_timeout=FRAM_I2C_TIMEOUT;
				while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
				{
					if(i2c_timeout--==0)
					{
						I2C_GenerateSTOP(FRAM_I2C, ENABLE);
						xSemaphoreGive( xI2CBusMutex );
						return ETIMEDOUT;
					}
				}
			}

			i2c_timeout=FRAM_I2C_TIMEOUT;
			while(I2C_GetFlagStatus(FRAM_I2C,I2C_FLAG_BTF)!=SET)
			{		//Wait for two buf_len to be received - ref man p712
				if(i2c_timeout--==0)
				{
					I2C_GenerateSTOP(FRAM_I2C, ENABLE);
					xSemaphoreGive( xI2CBusMutex );
					return ETIMEDOUT;
				}
			}
			I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);				//Do not ack the last byte
			*buf++=I2C_ReceiveData(FRAM_I2C);			//Third to last byte
			I2C_GenerateSTOP( FRAM_I2C, ENABLE );				//Enable the STOP here
			*buf++=I2C_ReceiveData(FRAM_I2C);			//Read the Penultimate from buffer
			i2c_timeout=FRAM_I2C_TIMEOUT;
			while(I2C_GetFlagStatus(FRAM_I2C,I2C_FLAG_RXNE)!=SET)
			{		//Last byte received here with a NACK and STOP
				if(i2c_timeout--==0)
				{
					I2C_GenerateSTOP(FRAM_I2C, ENABLE);
					xSemaphoreGive( xI2CBusMutex );
					return ETIMEDOUT;
				}
			}
		}
		else
		{
			I2C_AcknowledgeConfig(FRAM_I2C, DISABLE);				//Do not ack the last byte
			i2c_timeout=FRAM_I2C_TIMEOUT;
			while(I2C_GetFlagStatus(FRAM_I2C,I2C_FLAG_BTF)!=SET)
			{		//Wait for two buf_len to be received - ref man p713
				if(i2c_timeout--==0)
				{
					I2C_GenerateSTOP(FRAM_I2C, ENABLE);
					xSemaphoreGive( xI2CBusMutex );
					return ETIMEDOUT;
				}
			}
			I2C_GenerateSTOP( FRAM_I2C, ENABLE );				//Enable the STOP here
			*buf++=I2C_ReceiveData(FRAM_I2C);			//First byte to lowest location
		}
		*buf=I2C_ReceiveData(FRAM_I2C);					//Clear the buffer (last byte is in it)
		I2C_AcknowledgeConfig(FRAM_I2C, ENABLE);					//Re-enable ACK

	 }
	 xSemaphoreGive( xI2CBusMutex );

	    return ENOERR;
}

eErrorCode FRAM_I2C_Write_Buffer(uint16_t addr,uint8_t *buf, uint16_t buf_len)
{
	uint32_t i2c_timeout=FRAM_I2C_TIMEOUT;
	 xSemaphoreTake( xI2CBusMutex, portMAX_DELAY );
	 {
		uint16_t i=0;
		/*
		 * I2C_AcknowledgeConfig
		 */

	    I2C_GenerateSTART(FRAM_I2C, ENABLE);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_Send7bitAddress(FRAM_I2C, FRAM_I2C_ADDRESS, I2C_Direction_Transmitter);

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_SendData(FRAM_I2C, (uint8_t)((addr & 0xFF00) >> 8));

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    I2C_SendData(FRAM_I2C, (uint8_t)(addr & 0x00FF));

	    i2c_timeout=FRAM_I2C_TIMEOUT;
	    while(! I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	    {
	    	if(i2c_timeout--==0)
	    	{
	    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
	    		xSemaphoreGive( xI2CBusMutex );
	    		return ETIMEDOUT;
	    	}
	    }

	    for(i=0;i<buf_len;i++)
	    {
		    I2C_SendData(FRAM_I2C, buf[i]);

		    i2c_timeout=FRAM_I2C_TIMEOUT;
		    while (!I2C_CheckEvent(FRAM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
		    {
		    	if(i2c_timeout--==0)
		    	{
		    		I2C_GenerateSTOP(FRAM_I2C, ENABLE);
		    		xSemaphoreGive( xI2CBusMutex );
		    		return ETIMEDOUT;
		    	}
		    }
	    }

	    I2C_GenerateSTOP(FRAM_I2C, ENABLE);
	 }
	 xSemaphoreGive( xI2CBusMutex );

	    return ENOERR;
}


eErrorCode FRAM_Read_Settings(stControllerSettings *stSettings)
{
	stControllerSettings stSettings_temp;
	uint16_t Settings_CRC=0;
	if(FRAM_I2C_Read_Buffer(FRAM_SETTINGS_ADDR,&stSettings_temp,sizeof(stSettings_temp))!=ENOERR)
	{
		return EIO;
	}

	if(FRAM_I2C_Read_Buffer(FRAM_SETTINGS_CRC_ADDR,&Settings_CRC,sizeof(Settings_CRC))!=ENOERR)
	{
		return EIO;
	}

	if(Settings_CRC==usMBCRC16(&stSettings_temp,sizeof(stSettings_temp)))
	{
		*stSettings=stSettings_temp;
		return ENOERR;
	}
	else
	{
		*stSettings=stSettingsDefault;
		return ECRCERR;
	}
}

eErrorCode FRAM_Write_Settings(stControllerSettings stSettings)
{
	uint16_t Settings_CRC=0;
	Settings_CRC=usMBCRC16(&stSettings,sizeof(stSettings));

	if(FRAM_I2C_Write_Buffer(FRAM_SETTINGS_ADDR,&stSettings,sizeof(stSettings))!=ENOERR)
	{
		return EIO;
	}

	if(FRAM_I2C_Write_Buffer(FRAM_SETTINGS_CRC_ADDR,&Settings_CRC,sizeof(Settings_CRC))!=ENOERR)
	{
		return EIO;
	}

	return ENOERR;
}



