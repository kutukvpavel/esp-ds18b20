#include "ds18b20.h"

#define DS18B20_CMD_CONVERT_TEMP 0x44
#define DS18B20_CMD_WRITE_SCRATCHPAD 0x4E
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

namespace ds18b20
{
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

    
} // namespace ds18b20
