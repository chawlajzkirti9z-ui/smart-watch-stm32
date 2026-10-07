#ifndef __W25Q64_H
#define __W25Q64_H



void W25Q64_Init(void);
uint32_t W25Q64_ReadID(void);
void W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint16_t len);
void W25Q64_SectorErase(uint32_t addr);
void W25Q64_PageProgram(uint32_t addr, uint8_t *buf, uint16_t len);
void W25Q64_SaveTime(uint32_t counter);
uint32_t W25Q64_ReadTime(void);

#endif