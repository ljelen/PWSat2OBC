#ifndef SRC_DEVICES_EPS_H_
#define SRC_DEVICES_EPS_H_

#include <stdbool.h>
#include "i2c/i2c.h"

void EpsInit(I2CInterface* bus);
bool EpsOpenSail(void);

#endif /* SRC_DEVICES_EPS_H_ */
