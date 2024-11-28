/**
 * @file ds18b20.h
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

#pragma once

#include "esp_err.h"
#include "onewire_bus.h"

#include <inttypes.h>

namespace ds18b20
{
    typedef enum {
        RESOLUTION_12B = 0x7F, /*!< 750ms convert time */
        RESOLUTION_11B = 0x5F, /*!< 375ms convert time */
        RESOLUTION_10B = 0x3F, /*!< 187.5ms convert time */
        RESOLUTION_9B = 0x1F, /*!< 93.75ms convert time */
    } resolution_t;

    uint8_t search(onewire_bus_handle_t handle, onewire_device_address_t* rom_id_buffer, uint8_t max_instances);

    /**
     * @brief Trigger temperature conversion of DS18B20
     *
     * @param[in] handle 1-wire handle with DS18B20 on
     * @param[in] rom_number ROM number to specify which DS18B20 to send command, NULL to skip ROM
     * @return
     *         - ESP_OK                Trigger tempreture convertsion success.
     *         - ESP_ERR_INVALID_ARG   Invalid argument.
     *         - ESP_ERR_NOT_FOUND     There is no device present on 1-wire bus.
     */
    esp_err_t trigger_temperature_conversion(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number);

    /**
     * @brief Get temperature from DS18B20
     *
     * @param[in] handle 1-wire handle with DS18B20 on
     * @param[in] rom_number ROM number to specify which DS18B20 to read from, NULL to skip ROM
     * @param[out] temperature result from DS18B20
     * @return
     *         - ESP_OK                Get tempreture from DS18B20 success.
     *         - ESP_ERR_INVALID_ARG   Invalid argument.
     *         - ESP_ERR_NOT_FOUND     There is no device present on 1-wire bus.
     *         - ESP_ERR_INVALID_CRC   CRC check failed.
     */
    esp_err_t get_temperature(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number, float *temperature);

    /**
     * @brief Set DS18B20's temperation conversion resolution
     *
     * @param[in] handle 1-wire handle with DS18B20 on
     * @param[in] rom_number ROM number to specify which DS18B20 to read from, NULL to skip ROM
     * @param[in] resolution resolution of DS18B20's temperation conversion
     * @return
     *         - ESP_OK                Set DS18B20 resolution success.
     *         - ESP_ERR_INVALID_ARG   Invalid argument.
     *         - ESP_ERR_NOT_FOUND     There is no device present on 1-wire bus.
     */
    esp_err_t set_resolution(onewire_bus_handle_t handle, const onewire_device_address_t* rom_number, resolution_t resolution);
}