#include "config.h"
extern void HandleMessage(u8* rxBuffer, u8* txBuffer);

u8 _txBuffer[17];
u8 _rxBuffer[18];

u8 _txBufferPos;
u8 _rxBufferPos;

void SPI_Init()
{
  // Initialize SPI registers
  // MSB first
  SPI_CR1_LSBFIRST = 0;
  // SPI Slave
  SPI_CR1_MSTR = 0;
  // Clock polarity low (idle low)
  SPI_CR1_CPOL = 0;
  // Clock phase: second edge
  SPI_CR1_CPHA = 1;
}

void SPI_Reset()
{
  // Reset and enable CRC calculation
  SPI_Disable();
  SPI_SR_CRCERR = 0;
  SPI_CR2_CRCEN = 0;
  SPI_CR2_CRCEN = 1;
  SPI_SR_CRCERR = 0;
  SPI_Enable();

  // Set the first byte
  SPI_DR = GO_FRAME_PREFIX;

  // Enable interrupts
  SPI_ICR_TXIE = 1;
  SPI_ICR_RXIE = 1;

  // Reset the buffer positions
  _txBufferPos = 0;
  _rxBufferPos = 0;

  // Put the first byte of the module ID in the buffer
  // If we have an enumeration request, the first byte will be transmitted
  // before we have the chance to fill the tx buffer
  _txBuffer[0] = ModuleID[0];
  // Clear the rest of the buffer
  for (int i = 1; i < 17; i++)
    _txBuffer[i] = 0x00;
}

INTERRUPT_HANDLER(SPI_IRQ)
{
  //PB_ODR_ODR5 = 1;

  // SPI Overflow
  if (SPI_SR_OVR)
  {
    (void)SPI_DR;
    (void)SPI_SR; // TODO: check this for resetting overflow
    return;
  }

  // Transmit buffer empty
  if (SPI_SR_TXE)
  {
    if (_txBufferPos < 16)
    {
      // Put the next byte in the SPI data register
      SPI_DR = _txBuffer[_txBufferPos++];

      // If we're at the end of the message, transmit the CRC value next
      if (_txBufferPos == 16)
      {
        // Transmit CRC byte after this byte
        SPI_CR2_CRCNEXT = 1;
        // Disable TXE interrupt since we're at the end of the message
        SPI_ICR_TXIE = 0;
      }
    }
  }

  // Receive buffer not empty
  if (SPI_SR_RXNE)
  {
    // Is this an enumeration request?
    if (_rxBufferPos == 0 && SPI_DR == 0xFE) // todo: define something for htis
    {
      // Copy the Module ID to the transmit buffer
      for (int i = 0; i <= 16; i++)
        _txBuffer[i] = ModuleID[i];
    }

    // Read the incoming byte
    _rxBuffer[_rxBufferPos++] = SPI_DR;

    if (_rxBufferPos == 18)
    {
      if (SPI_SR_CRCERR == 0)
      {
        SPI_Reset();
        HandleMessage(_rxBuffer, _txBuffer);
      }
      else
      {
        SPI_Reset();
      }
    }
  }


  PB_ODR_ODR5 = 0;
}
