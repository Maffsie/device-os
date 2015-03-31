/**
 ******************************************************************************
 * @file    flash_mal.c
 * @author  Satish Nair, Matthew McGowan
 * @version V1.0.0
 * @date    30-Jan-2015
 * @brief   Media access layer for platform dependent flash interfaces
 ******************************************************************************
  Copyright (c) 2015 Spark Labs, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hw_config.h"
#include "flash_mal.h"
#include "dct.h"
#include "module_info.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static volatile FLASH_Status FLASHStatus = FLASH_COMPLETE;

//Flash variables for OTA use
#ifdef USE_SERIAL_FLASH
static uint32_t External_Flash_Start_Address = 0;
static uint32_t External_Flash_Address = 0;
static uint16_t External_Flash_Update_Index = 0;
#else
static uint32_t Internal_Flash_Start_Address = 0;
static uint32_t Internal_Flash_Address = 0;
static uint16_t Internal_Flash_Update_Index = 0;
#endif

/* Private functions ---------------------------------------------------------*/

uint16_t FLASH_SectorToErase(uint8_t flashDeviceID, uint32_t startAddress)
{
    uint16_t flashSector = 0xFFFF;//Invalid sector

    if (flashDeviceID != FLASH_INTERNAL)
    {
        return flashSector;
    }

    if (startAddress < 0x08004000)
    {
        flashSector = FLASH_Sector_0;
    }
    else if (startAddress < 0x08008000)
    {
        flashSector = FLASH_Sector_1;
    }
    else if (startAddress < 0x0800C000)
    {
        flashSector = FLASH_Sector_2;
    }
    else if (startAddress < 0x08010000)
    {
        flashSector = FLASH_Sector_3;
    }
    else if (startAddress < 0x08020000)
    {
        flashSector = FLASH_Sector_4;
    }
    else if (startAddress < 0x08040000)
    {
        flashSector = FLASH_Sector_5;
    }
    else if (startAddress < 0x08060000)
    {
        flashSector = FLASH_Sector_6;
    }
    else if (startAddress < 0x08080000)
    {
        flashSector = FLASH_Sector_7;
    }
    else if (startAddress < 0x080A0000)
    {
        flashSector = FLASH_Sector_8;
    }
    else if (startAddress < 0x080C0000)
    {
        flashSector = FLASH_Sector_9;
    }
    else if (startAddress < 0x080E0000)
    {
        flashSector = FLASH_Sector_10;
    }
    else if (startAddress < 0x08100000)
    {
        flashSector = FLASH_Sector_11;
    }

    return flashSector;
}

bool FLASH_CheckValidAddressRange(flash_device_t flashDeviceID, uint32_t startAddress, uint32_t length)
{
    uint32_t endAddress = startAddress + length - 1;

    if (flashDeviceID == FLASH_INTERNAL)
    {
        if (startAddress == 0x08000000 && endAddress >= 0x08004000)
        {
            return false;
        }
        else if (startAddress < 0x08020000 || endAddress >= 0x08100000)
        {
            return false;
        }
    }
    else if (flashDeviceID == FLASH_SERIAL)
    {
#ifdef USE_SERIAL_FLASH
        if (startAddress < 0x4000 || endAddress >= 0x100000)
        {
            return false;
        }
#else
        return false;
#endif
    }
    else
    {
        return false;   //Invalid FLASH ID
    }

    return true;
}

bool FLASH_EraseMemory(flash_device_t flashDeviceID, uint32_t startAddress, uint32_t length)
{
    uint32_t eraseCounter = 0;
    uint32_t numPages = 0;

    if (FLASH_CheckValidAddressRange(flashDeviceID, startAddress, length) != true)
    {
        return false;
    }

    if (flashDeviceID == FLASH_INTERNAL)
    {
        /* Check which sector has to be erased */
        uint16_t flashSector = FLASH_SectorToErase(FLASH_INTERNAL, startAddress);

        if (flashSector > FLASH_Sector_11)
        {
            return false;
        }

        /* Unlock the Flash Program Erase Controller */
        FLASH_Unlock();

        /* Define the number of Internal Flash pages to be erased */
        numPages = FLASH_PagesMask(length, INTERNAL_FLASH_PAGE_SIZE);

        /* Clear All pending flags */
        FLASH_ClearFlags();

        /* Erase the Internal Flash pages */
        for (eraseCounter = 0; (eraseCounter < numPages); eraseCounter++)
        {
            FLASHStatus = FLASH_EraseSector(flashSector + (8 * eraseCounter), VoltageRange_3);

            /* If erase operation fails, return Failure */
            if (FLASHStatus != FLASH_COMPLETE)
            {
                return false;
            }
        }

        /* Locks the FLASH Program Erase Controller */
        FLASH_Lock();

        return true;
    }
    else if (flashDeviceID == FLASH_SERIAL)
    {
#ifdef USE_SERIAL_FLASH
        /* Initialize SPI Flash */
        sFLASH_Init();

        /* Define the number of External Flash pages to be erased */
        numPages = FLASH_PagesMask(length, sFLASH_PAGESIZE);

        /* Erase the SPI Flash pages */
        for (eraseCounter = 0; (eraseCounter < numPages); eraseCounter++)
        {
            sFLASH_EraseSector(startAddress + (sFLASH_PAGESIZE * eraseCounter));
        }

        /* Return Success */
        return true;
#endif
    }

    /* Return Failure */
    return false;
}

bool FLASH_CopyMemory(flash_device_t sourceDeviceID, uint32_t sourceAddress,
                      flash_device_t destinationDeviceID, uint32_t destinationAddress,
                      uint32_t length, uint8_t module_function, uint8_t flags)
{
#ifdef USE_SERIAL_FLASH
    uint8_t serialFlashData[4];
#endif
    uint32_t internalFlashData = 0;
    uint32_t endAddress = sourceAddress + length - 1;

    if (FLASH_CheckValidAddressRange(sourceDeviceID, sourceAddress, length) != true)
    {
        return false;
    }

    if (FLASH_CheckValidAddressRange(destinationDeviceID, destinationAddress, length) != true)
    {
        return false;
    }

#ifndef USE_SERIAL_FLASH
    if ((sourceDeviceID == FLASH_INTERNAL) && (flags & MODULE_VERIFY_MASK))
    {
        uint32_t moduleLength = FLASH_ModuleLength(sourceDeviceID, sourceAddress);

        if((flags & (MODULE_VERIFY_LENGTH|MODULE_VERIFY_CRC)) && (length < moduleLength+4))
        {
            return false;
        }
        
        const module_info_t* info = FLASH_ModuleInfo(sourceDeviceID, sourceAddress);
        if ((info->module_function != MODULE_FUNCTION_RESOURCE) && (info->platform_id != PLATFORM_ID))
        {
            return false;
        }
        
        // verify destination address
        if ((flags & MODULE_VERIFY_DESTINATION_IS_START_ADDRESS) && (((uint32_t)info->module_start_address) != destinationAddress)) 
        {
            return false;
        }
        
        if ((flags & MODULE_VERIFY_FUNCTION) && (info->module_function != module_function)) 
        {
            return false;
        }

        if ((flags & MODULE_VERIFY_CRC) && !FLASH_VerifyCRC32(sourceDeviceID, sourceAddress, moduleLength))
        {
            return false;
        }
    }
#endif

    if (FLASH_EraseMemory(destinationDeviceID, destinationAddress, length) != true)
    {
        return false;
    }

    if (sourceDeviceID == FLASH_SERIAL)
    {
#ifdef USE_SERIAL_FLASH
        /* Initialize SPI Flash */
        sFLASH_Init();
#endif
    }

    if (destinationDeviceID == FLASH_INTERNAL)
    {
        /* Unlock the internal flash program erase controller */
        FLASH_Unlock();
    }

    /* Program source to destination */
    while (sourceAddress < endAddress)
    {
        if (sourceDeviceID == FLASH_INTERNAL)
        {
            /* Read data from internal flash source address */
            internalFlashData = (*(__IO uint32_t*) sourceAddress);
        }
#ifdef USE_SERIAL_FLASH
        else if (sourceDeviceID == FLASH_SERIAL)
        {
            /* Read data from serial flash source address */
            sFLASH_ReadBuffer(serialFlashData, sourceAddress, 4);
        }
#endif

        if (destinationDeviceID == FLASH_INTERNAL)
        {
#ifdef USE_SERIAL_FLASH
            if (sourceDeviceID == FLASH_SERIAL)
            {
                internalFlashData = (uint32_t)(serialFlashData[0] | (serialFlashData[1] << 8) | (serialFlashData[2] << 16) | (serialFlashData[3] << 24));
            }
#endif

            /* Program data to internal flash destination address */
            FLASHStatus = FLASH_ProgramWord(destinationAddress, internalFlashData);

            /* If program operation fails, return Failure */
            if (FLASHStatus != FLASH_COMPLETE)
            {
                return false;
            }
        }
#ifdef USE_SERIAL_FLASH
        else if (destinationDeviceID == FLASH_SERIAL)
        {
            if (sourceDeviceID == FLASH_INTERNAL)
            {
                serialFlashData[0] = (uint8_t)(internalFlashData & 0xFF);
                serialFlashData[1] = (uint8_t)((internalFlashData & 0xFF00) >> 8);
                serialFlashData[2] = (uint8_t)((internalFlashData & 0xFF0000) >> 16);
                serialFlashData[3] = (uint8_t)((internalFlashData & 0xFF000000) >> 24);
            }

            /* Program data to serial flash destination address */
            sFLASH_WriteBuffer(serialFlashData, destinationAddress, 4);
        }
#endif

        sourceAddress += 4;
        destinationAddress += 4;
    }

    if (destinationDeviceID == FLASH_INTERNAL)
    {
        /* Locks the internal flash program erase controller */
        FLASH_Lock();
    }

    return true;
}

bool FLASH_CompareMemory(flash_device_t sourceDeviceID, uint32_t sourceAddress,
                         flash_device_t destinationDeviceID, uint32_t destinationAddress,
                         uint32_t length)
{
#ifdef USE_SERIAL_FLASH
    uint8_t serialFlashData[4];
#endif
    uint32_t sourceDeviceData = 0;
    uint32_t destinationDeviceData = 0;
    uint32_t endAddress = sourceAddress + length - 1;

    if (FLASH_CheckValidAddressRange(sourceDeviceID, sourceAddress, length) != true)
    {
        return false;
    }

    if (FLASH_CheckValidAddressRange(destinationDeviceID, destinationAddress, length) != true)
    {
        return false;
    }

    if (sourceDeviceID == FLASH_SERIAL || destinationDeviceID == FLASH_SERIAL)
    {
#ifdef USE_SERIAL_FLASH
        /* Initialize SPI Flash */
        sFLASH_Init();
#endif
    }

    /* Program source to destination */
    while (sourceAddress < endAddress)
    {
        if (sourceDeviceID == FLASH_INTERNAL)
        {
            /* Read data from internal flash source address */
            sourceDeviceData = (*(__IO uint32_t*) sourceAddress);
        }
#ifdef USE_SERIAL_FLASH
        else if (sourceDeviceID == FLASH_SERIAL)
        {
            /* Read data from serial flash source address */
            sFLASH_ReadBuffer(serialFlashData, sourceAddress, 4);
            sourceDeviceData = (uint32_t)(serialFlashData[0] | (serialFlashData[1] << 8) | (serialFlashData[2] << 16) | (serialFlashData[3] << 24));
        }
#endif

        if (destinationDeviceID == FLASH_INTERNAL)
        {
            /* Read data from internal flash destination address */
            destinationDeviceData = (*(__IO uint32_t*) destinationAddress);
        }
#ifdef USE_SERIAL_FLASH
        else if (destinationDeviceID == FLASH_SERIAL)
        {
            /* Read data from serial flash destination address */
            sFLASH_ReadBuffer(serialFlashData, destinationAddress, 4);
            destinationDeviceData = (uint32_t)(serialFlashData[0] | (serialFlashData[1] << 8) | (serialFlashData[2] << 16) | (serialFlashData[3] << 24));
        }
#endif

        if (sourceDeviceData != destinationDeviceData)
        {
            /* Failed comparison check */
            return false;
        }

        sourceAddress += 4;
        destinationAddress += 4;
    }

    /* Passed comparison check */
    return true;
}

bool FLASH_AddToNextAvailableModulesSlot(flash_device_t sourceDeviceID, uint32_t sourceAddress,
                                         flash_device_t destinationDeviceID, uint32_t destinationAddress,
                                         uint32_t length, uint8_t function, uint8_t flags)
{
    //Read the flash modules info from the dct area
    const platform_flash_modules_t* dct_app_data = (const platform_flash_modules_t*)dct_read_app_data(DCT_FLASH_MODULES_OFFSET);
    platform_flash_modules_t flash_modules[MAX_MODULES_SLOT];
    uint8_t flash_module_index = MAX_MODULES_SLOT;

    memcpy(flash_modules, dct_app_data, sizeof(flash_modules));

    //fill up the next available modules slot and return true else false
    //slot 0 is reserved for factory reset module so start from flash_module_index = 1
    for (flash_module_index = GEN_START_SLOT; flash_module_index < MAX_MODULES_SLOT; flash_module_index++)
    {
        if(flash_modules[flash_module_index].magicNumber == 0xABCD)
        {
            continue;
        }
        else
        {
            flash_modules[flash_module_index].sourceDeviceID = sourceDeviceID;
            flash_modules[flash_module_index].sourceAddress = sourceAddress;
            flash_modules[flash_module_index].destinationDeviceID = destinationDeviceID;
            flash_modules[flash_module_index].destinationAddress = destinationAddress;
            flash_modules[flash_module_index].length = length;
            flash_modules[flash_module_index].magicNumber = 0xABCD;
            flash_modules[flash_module_index].module_function = function;
            flash_modules[flash_module_index].flags = flags;

            dct_write_app_data(&flash_modules[flash_module_index],
                               offsetof(application_dct_t, flash_modules[flash_module_index]),
                               sizeof(platform_flash_modules_t));

            return true;
        }
    }

    return false;
}

bool FLASH_AddToFactoryResetModuleSlot(flash_device_t sourceDeviceID, uint32_t sourceAddress,
                                       flash_device_t destinationDeviceID, uint32_t destinationAddress,
                                       uint32_t length, uint8_t function, uint8_t flags)
{
    //Read the flash modules info from the dct area
    const platform_flash_modules_t* dct_app_data = (const platform_flash_modules_t*)dct_read_app_data(DCT_FLASH_MODULES_OFFSET);
    platform_flash_modules_t flash_modules[1];//slot 0 is factory reset module

    memcpy(flash_modules, dct_app_data, sizeof(flash_modules));

    flash_modules[FAC_RESET_SLOT].sourceDeviceID = sourceDeviceID;
    flash_modules[FAC_RESET_SLOT].sourceAddress = sourceAddress;
    flash_modules[FAC_RESET_SLOT].destinationDeviceID = destinationDeviceID;
    flash_modules[FAC_RESET_SLOT].destinationAddress = destinationAddress;
    flash_modules[FAC_RESET_SLOT].length = length;
    flash_modules[FAC_RESET_SLOT].magicNumber = 0x0FAC;
    flash_modules[FAC_RESET_SLOT].module_function = function;
    flash_modules[FAC_RESET_SLOT].flags = flags;

    if(memcmp(flash_modules, dct_app_data, sizeof(flash_modules)) != 0)
    {
        //Only write dct app data if factory reset module slot is different
        dct_write_app_data(&flash_modules[FAC_RESET_SLOT],
                           offsetof(application_dct_t, flash_modules[FAC_RESET_SLOT]),
                           sizeof(platform_flash_modules_t));
    }

    return true;
}

bool FLASH_ClearFactoryResetModuleSlot(void)
{
    //Read the flash modules info from the dct area
    const platform_flash_modules_t* flash_modules = (const platform_flash_modules_t*)dct_read_app_data(DCT_FLASH_MODULES_OFFSET);

    //Set slot 0 factory reset module elements to 0 without sector erase
    FLASH_Unlock();

    uint32_t address = (uint32_t)&flash_modules[FAC_RESET_SLOT];
    uint32_t length = sizeof(platform_flash_modules_t) >> 2;

    while(length--)
    {
        FLASH_ProgramWord(address, 0);
        address += 4;
    }

    FLASH_Lock();

    return true;
}

bool FLASH_RestoreFromFactoryResetModuleSlot(void)
{
    //Read the flash modules info from the dct area
    const platform_flash_modules_t* flash_modules = (const platform_flash_modules_t*)dct_read_app_data(DCT_FLASH_MODULES_OFFSET);
    bool restoreFactoryReset = false;
    
    if(flash_modules[FAC_RESET_SLOT].magicNumber == 0x0FAC)
    {
        //Restore Factory Reset Firmware (slot 0 is factory reset module)
        restoreFactoryReset = FLASH_CopyMemory(flash_modules[FAC_RESET_SLOT].sourceDeviceID,
                                               flash_modules[FAC_RESET_SLOT].sourceAddress,
                                               flash_modules[FAC_RESET_SLOT].destinationDeviceID,
                                               flash_modules[FAC_RESET_SLOT].destinationAddress,
                                               flash_modules[FAC_RESET_SLOT].length,
                                                flash_modules[FAC_RESET_SLOT].module_function,
                                               flash_modules[FAC_RESET_SLOT].flags);
    }
    else
    {        
        // attempt to use the default that the bootloader was built with
        restoreFactoryReset = FLASH_CopyMemory(FLASH_INTERNAL, INTERNAL_FLASH_FAC_ADDRESS, FLASH_INTERNAL, USER_FIRMWARE_IMAGE_LOCATION, FIRMWARE_IMAGE_SIZE, 
            FACTORY_RESET_MODULE_FUNCTION,
            MODULE_VERIFY_CRC | MODULE_VERIFY_DESTINATION_IS_START_ADDRESS | MODULE_VERIFY_FUNCTION);
    }
    return restoreFactoryReset;
}

//This function called in bootloader to perform the memory update process
void FLASH_UpdateModules(void (*flashModulesCallback)(bool isUpdating))
{
    //Read the flash modules info from the dct area
    const platform_flash_modules_t* flash_modules = (const platform_flash_modules_t*)dct_read_app_data(DCT_FLASH_MODULES_OFFSET);
    uint8_t flash_module_index = MAX_MODULES_SLOT;

    //slot 0 is reserved for factory reset module so start from flash_module_index = 1
    for (flash_module_index = GEN_START_SLOT; flash_module_index < MAX_MODULES_SLOT; flash_module_index++)
    {
        if(flash_modules[flash_module_index].magicNumber == 0xABCD)
        {
            //Turn On RGB_COLOR_MAGENTA toggling during flash updating
            if(flashModulesCallback)
            {
                flashModulesCallback(true);
            }

            //Copy memory from source to destination based on flash device id
            FLASH_CopyMemory(flash_modules[flash_module_index].sourceDeviceID,
                             flash_modules[flash_module_index].sourceAddress,
                             flash_modules[flash_module_index].destinationDeviceID,
                             flash_modules[flash_module_index].destinationAddress,
                             flash_modules[flash_module_index].length,
                             flash_modules[flash_module_index].module_function,
                             flash_modules[flash_module_index].flags);

            //Set all flash_modules[flash_module_index] elements to 0 without sector erase
            FLASH_Unlock();

            uint32_t address = (uint32_t)&flash_modules[flash_module_index];
            uint32_t length = sizeof(platform_flash_modules_t) >> 2;

            while(length--)
            {
                FLASH_ProgramWord(address, 0);
                address += 4;
            }

            FLASH_Lock();

            if(flashModulesCallback)
            {
                //Turn Off RGB_COLOR_MAGENTA toggling
                flashModulesCallback(false);
            }
        }
    }
}

const module_info_t* FLASH_ModuleInfo(uint8_t flashDeviceID, uint32_t startAddress)
{
    if(flashDeviceID == FLASH_INTERNAL)
    {
        if (((*(__IO uint32_t*)startAddress) & APP_START_MASK) == 0x20000000)
        {
            startAddress += 0x184;
        }

        const module_info_t* module_info = (const module_info_t*)startAddress;

        return module_info;
    }

    return NULL;
}

uint32_t FLASH_ModuleAddress(uint8_t flashDeviceID, uint32_t startAddress)
{
    const module_info_t* module_info = FLASH_ModuleInfo(flashDeviceID, startAddress);

    if (module_info != NULL)
    {
        return (uint32_t)module_info->module_start_address;
    }

    return 0;
}

uint32_t FLASH_ModuleLength(uint8_t flashDeviceID, uint32_t startAddress)
{
    const module_info_t* module_info = FLASH_ModuleInfo(flashDeviceID, startAddress);

    if (module_info != NULL)
    {
        return ((uint32_t)module_info->module_end_address - (uint32_t)module_info->module_start_address);
    }

    return 0;
}

bool FLASH_isModuleInfoValid(uint8_t flashDeviceID, uint32_t startAddress, uint32_t expectedAddress)
{
    const module_info_t* module_info = FLASH_ModuleInfo(flashDeviceID, startAddress);

    if (module_info != NULL)
    {
        return (((uint32_t)module_info->module_start_address == expectedAddress) && (module_info->platform_id == PLATFORM_ID));
    }

    return false;
}

bool FLASH_VerifyCRC32(uint8_t flashDeviceID, uint32_t startAddress, uint32_t length)
{
    if(flashDeviceID == FLASH_INTERNAL && length > 0)
    {
        uint32_t expectedCRC = __REV((*(__IO uint32_t*) (startAddress + length)));
        uint32_t computedCRC = Compute_CRC32((uint8_t*)startAddress, length);

        if (expectedCRC == computedCRC)
        {
            return true;
        }
    }

    return false;
}

void FLASH_ClearFlags(void)
{
    /* Clear All pending flags */
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

void FLASH_WriteProtection_Enable(uint32_t FLASH_Sectors)
{
    /* Get FLASH_Sectors write protection status */
    uint32_t SectorsWRPStatus = FLASH_OB_GetWRP() & FLASH_Sectors;

    if (SectorsWRPStatus != 0)
    {
        //If FLASH_Sectors are not write protected, enable the write protection

        /* Enable the Flash option control register access */
        FLASH_OB_Unlock();

        /* Clear All pending flags */
        FLASH_ClearFlags();

        /* Enable FLASH_Sectors write protection */
        FLASH_OB_WRPConfig(FLASH_Sectors, ENABLE);

        /* Start the Option Bytes programming process */
        if (FLASH_OB_Launch() != FLASH_COMPLETE)
        {
            //Error during Option Bytes programming process
        }

        /* Disable the Flash option control register access */
        FLASH_OB_Lock();

        /* Get FLASH_Sectors write protection status */
        SectorsWRPStatus = FLASH_OB_GetWRP() & FLASH_Sectors;

        /* Check if FLASH_Sectors are write protected */
        if (SectorsWRPStatus == 0)
        {
            //Write Protection Enable Operation is done correctly
        }
    }
}

void FLASH_WriteProtection_Disable(uint32_t FLASH_Sectors)
{
    /* Get FLASH_Sectors write protection status */
    uint32_t SectorsWRPStatus = FLASH_OB_GetWRP() & FLASH_Sectors;

    if (SectorsWRPStatus == 0)
    {
        //If FLASH_Sectors are write protected, disable the write protection

        /* Enable the Flash option control register access */
        FLASH_OB_Unlock();

        /* Clear All pending flags */
        FLASH_ClearFlags();

        /* Disable FLASH_Sectors write protection */
        FLASH_OB_WRPConfig(FLASH_Sectors, DISABLE);

        /* Start the Option Bytes programming process */
        if (FLASH_OB_Launch() != FLASH_COMPLETE)
        {
            //Error during Option Bytes programming process
        }

        /* Disable the Flash option control register access */
        FLASH_OB_Lock();

        /* Get FLASH_Sectors write protection status */
        SectorsWRPStatus = FLASH_OB_GetWRP() & FLASH_Sectors;

        /* Check if FLASH_Sectors write protection is disabled */
        if (SectorsWRPStatus == FLASH_Sectors)
        {
            //Write Protection Disable Operation is done correctly
        }
    }
}

void FLASH_Erase(void)
{
    FLASH_EraseMemory(FLASH_INTERNAL, CORE_FW_ADDRESS, FIRMWARE_IMAGE_SIZE);
}

void FLASH_Backup(uint32_t FLASH_Address)
{
#ifdef USE_SERIAL_FLASH
    FLASH_CopyMemory(FLASH_INTERNAL, CORE_FW_ADDRESS, FLASH_SERIAL, FLASH_Address, FIRMWARE_IMAGE_SIZE, 0, 0);
#else
    //Don't have enough space in Internal Flash to save a Backup copy of the firmware
#endif
}

void FLASH_Restore(uint32_t FLASH_Address)
{
#ifdef USE_SERIAL_FLASH
    //CRC verification Disabled by default
    FLASH_CopyMemory(FLASH_SERIAL, FLASH_Address, FLASH_INTERNAL, CORE_FW_ADDRESS, FIRMWARE_IMAGE_SIZE, 0, 0);
#else
    //commented below since FIRMWARE_IMAGE_SIZE != Actual factory firmware image size
    //FLASH_CopyMemory(FLASH_INTERNAL, FLASH_Address, FLASH_INTERNAL, USER_FIRMWARE_IMAGE_LOCATION, FIRMWARE_IMAGE_SIZE, true);
    //FLASH_AddToFactoryResetModuleSlot() is now called in HAL_Core_Config() in core_hal.c
#endif
}

uint32_t FLASH_PagesMask(uint32_t imageSize, uint32_t pageSize)
{
    //Calculate the number of flash pages that needs to be erased
    uint32_t numPages = 0x0;

    if ((imageSize % pageSize) != 0)
    {
        numPages = (imageSize / pageSize) + 1;
    }
    else
    {
        numPages = imageSize / pageSize;
    }

    return numPages;
}

void FLASH_Begin(uint32_t FLASH_Address, uint32_t imageSize)
{
    system_flags.OTA_FLASHED_Status_SysFlag = 0x0000;
    Save_SystemFlags();

#ifdef USE_SERIAL_FLASH
    External_Flash_Update_Index = 0;
    External_Flash_Start_Address = FLASH_Address;
    External_Flash_Address = External_Flash_Start_Address;

    FLASH_EraseMemory(FLASH_SERIAL, External_Flash_Start_Address, imageSize);
#else
    Internal_Flash_Update_Index = 0;
    Internal_Flash_Start_Address = FLASH_Address;
    Internal_Flash_Address = Internal_Flash_Start_Address;

    FLASH_EraseMemory(FLASH_INTERNAL, Internal_Flash_Start_Address, imageSize);
#endif
}

uint16_t FLASH_Update(uint8_t *pBuffer, uint32_t bufferSize)
{
#ifdef USE_SERIAL_FLASH
    /* Write Data Buffer to SPI Flash memory */
    sFLASH_WriteBuffer(pBuffer, External_Flash_Address, bufferSize);

    External_Flash_Address += bufferSize;
    External_Flash_Update_Index += 1;

    return External_Flash_Update_Index;
#else
    uint32_t index = 0;

    if (bufferSize & 0x3) /* Not an aligned data */
    {
        for (index = bufferSize; index < ((bufferSize & 0xFFFC) + 4); index++)
        {
            pBuffer[index] = 0xFF;
        }
    }

    /* Unlock the internal flash */
    FLASH_Unlock();

    FLASH_ClearFlags();

    /* Data received are Word multiple */
    for (index = 0; index <  bufferSize; index += 4)
    {
        FLASH_ProgramWord(Internal_Flash_Address, *(uint32_t *)(pBuffer + index));
        Internal_Flash_Address += 4;
    }

    /* Lock the internal flash */
    FLASH_Lock();

    Internal_Flash_Update_Index += 1;

    return Internal_Flash_Update_Index;
#endif
}

void FLASH_End(void)
{
#ifdef USE_SERIAL_FLASH
    system_flags.FLASH_OTA_Update_SysFlag = 0x0005;
    Save_SystemFlags();

    RTC_WriteBackupRegister(RTC_BKP_DR10, 0x0005);
#else
    //FLASH_AddToNextAvailableModulesSlot() should be called in system_update.cpp
#endif
    USB_Cable_Config(DISABLE);

    NVIC_SystemReset();
}
