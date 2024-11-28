/**
 * @file ds18b20.cpp
 * @author Kutukov Pavel (kutukovps@my.msu.ru)
 * @brief A cutdown version of DS18B20 library adapted for ESP32.
 * @version 1
 * @date 2023-12-02
 * 
 * @copyright This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 */

#include "ds18b20.h"

#include "esp_check.h"
#include "onewire_device.h"
#include "onewire_cmd.h"
#include "onewire_crc.h"

#include <string.h>

#define DS18B20_CMD_CONVERT_TEMP 0x44
#define DS18B20_CMD_WRITE_SCRATCHPAD 0x4E
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

namespace ds18b20
{
    static const char *TAG = "ds18b20";

    typedef struct  {
        uint8_t temp_lsb; /*!< lsb of temperature */
        uint8_t temp_msb; /*!< msb of temperature */
        uint8_t th_user1; /*!< th register or user byte 1 */
        uint8_t tl_user2; /*!< tl register or user byte 2 */
        uint8_t configuration; /*!< configuration register */
        uint8_t _reserved1;
        uint8_t _reserved2;
        uint8_t _reserved3;
        uint8_t crc_value; /*!< crc value of scratchpad data */
    } scratchpad_t;

    /// @brief Search 1-Wire bus for devices.
    /// @param handle OneWire bus handle
    /// @param rom_id_buffer Pointer to the buffer for writing ROM IDs
    /// @param max_instances Maximum number devices to look for
    /// @return Actual number of devices found
    uint8_t search(onewire_bus_handle_t handle, onewire_device_address_t* rom_id_buffer, uint8_t max_instances)
    {
        static_assert(sizeof(onewire_device_address_t) == 8);

        // create 1-wire rom search context
        onewire_device_iter_handle_t context_handler;
        ESP_ERROR_CHECK(onewire_new_device_iter(handle, &context_handler));

        uint8_t device_num = 0;
        onewire_device_t instance;
        // search for devices on the bus
        do {
            esp_err_t search_result = onewire_device_iter_get_next(context_handler, &instance);
            if (search_result != ESP_OK) 
            {
                if (search_result == ESP_ERR_NOT_FOUND) break;
                ESP_LOGE(TAG, "Onewire search err: %s", esp_err_to_name(search_result));
                break;
            } // break on finish or no device
            rom_id_buffer[device_num] = instance.address;
            ESP_LOGI(TAG, "found device with rom id %" PRIu64, rom_id_buffer[device_num]);
            device_num++;
        } while (device_num < max_instances);

        // delete 1-wire rom search context
        ESP_ERROR_CHECK(onewire_del_device_iter(context_handler));
        ESP_LOGI(TAG, "%" PRIu8 " device%s found on 1-wire bus", device_num, device_num > 1 ? "s" : "");

        return device_num;
    }

    /// @brief Trigger DS18B20 temperature conversion
    /// @param handle OneWire bus handle
    /// @param rom_number Device ROM ID (or NULL to broadcast)
    /// @return ESP_OK if succeeded, ESP_ERR_INVALID_ARG if handle is NULL, otherwise see onewire_bus_reset and onewire_bus_write_bytes
    esp_err_t trigger_temperature_conversion(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number)
    {
        ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");

        ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present

        uint8_t tx_buffer[10];
        uint8_t tx_buffer_size;

        if (rom_number) { // specify rom id
            tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
            tx_buffer[9] = DS18B20_CMD_CONVERT_TEMP;
            memcpy(&tx_buffer[1], rom_number, sizeof(onewire_device_address_t));
            tx_buffer_size = 10;
        } else { // skip rom id
            tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
            tx_buffer[1] = DS18B20_CMD_CONVERT_TEMP;
            tx_buffer_size = 2;
        }

        ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                            TAG, "error while triggering temperature convert");

        return ESP_OK;
    }

    /// @brief Read DS18B20 temperature conversion result
    /// @param handle OneWire bus handle
    /// @param rom_number Device ROM ID (or NULL to SKIP ROM, suitable for single device bus)
    /// @param temperature Temperature output buffer
    /// @return ESP_OK if succeeded, ESP_ERR_INVALID_ARG if handle or temperature output buffer is null,
    /// ESP_ERR_INVALID_CRC if CRC doesn't match, otherwise see onewire_bus_reset, onewire_bus_write_bytes, onewire_bus_read_bytes
    esp_err_t get_temperature(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number, float *temperature)
    {
        ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");
        ESP_RETURN_ON_FALSE(temperature, ESP_ERR_INVALID_ARG, TAG, "invalid temperature pointer");

        ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present

        scratchpad_t scratchpad;

        uint8_t tx_buffer[10];
        uint8_t tx_buffer_size;

        if (rom_number) { // specify rom id
            tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
            tx_buffer[9] = DS18B20_CMD_READ_SCRATCHPAD;
            memcpy(&tx_buffer[1], rom_number, sizeof(onewire_device_address_t));
            tx_buffer_size = 10;
        } else {
            tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
            tx_buffer[1] = DS18B20_CMD_READ_SCRATCHPAD;
            tx_buffer_size = 2;
        }

        ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                            TAG, "error while sending read scratchpad command");
        ESP_RETURN_ON_ERROR(onewire_bus_read_bytes(handle, reinterpret_cast<uint8_t*>(&scratchpad), sizeof(scratchpad)),
                            TAG, "error while reading scratchpad command");

        ESP_RETURN_ON_FALSE(onewire_crc8(0, reinterpret_cast<uint8_t*>(&scratchpad), 8) == scratchpad.crc_value, ESP_ERR_INVALID_CRC,
                            TAG, "crc error");

        static const uint8_t lsb_mask[4] = { 0x07, 0x03, 0x01, 0x00 };
        uint8_t lsb_masked = scratchpad.temp_lsb & (~lsb_mask[scratchpad.configuration >> 5]); // mask bits not used in low resolution
        *temperature = ((static_cast<uint16_t>(scratchpad.temp_msb) << 8) | lsb_masked)  / 16.0f;

        return ESP_OK;
    }

    /// @brief Set DS18B20 temperature conversion resolution
    /// @param handle OneWire bus handle
    /// @param rom_number Device ROM ID (or NULL to broadcast)
    /// @param resolution Resolution
    /// @return ESP_OK if succeeded, ESP_ERR_INVALID_ARG if handle is NULL, otherwise see onewire_bus_reset and onewire_bus_write_bytes
    esp_err_t set_resolution(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number, resolution_t resolution)
    {
        ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");

        ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present

        uint8_t tx_buffer[10];
        uint8_t tx_buffer_size;

        if (rom_number) { // specify rom id
            tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
            tx_buffer[9] = DS18B20_CMD_WRITE_SCRATCHPAD;
            memcpy(&tx_buffer[1], rom_number, sizeof(onewire_device_address_t));
            tx_buffer_size = 10;
        } else {
            tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
            tx_buffer[1] = DS18B20_CMD_WRITE_SCRATCHPAD;
            tx_buffer_size = 2;
        }

        ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                            TAG, "error while sending read scratchpad command");

        tx_buffer[0] = 0;
        tx_buffer[1] = 0;
        tx_buffer[2] = resolution;
        ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, 3),
                            TAG, "error while sending write scratchpad command");

        return ESP_OK;
    }
} // namespace ds18b20
