#pragma once

#include "serial_device.h"
#include "ring_buffer.h"
#include "ILockable.hpp"
#include "lockguard.hpp"
#include "ISemaphore.h"
#include <stddef.h>

namespace lib
{
class SerialDevice
{
public:
   constexpr static size_t TX_BUFFER_SIZE = 256;
   using SendFunction = void(*)( const uint8_t* data, size_t length );

   SerialDevice( SendFunction sender, uint8_t rxBuffer[], size_t rxBufferSize, lib::ILockable& lockable, lib::ISemaphore& semTxComplete, lib::ISemaphore& semNewRxBytes )
   : m_sender( sender )
   , m_rxBuffer( rxBuffer, rxBufferSize )
   , m_lockable( lockable )
   , m_semTxComplete( semTxComplete )
   , m_semNewRxBytes( semNewRxBytes )
   { }

   ~SerialDevice()
   { }

   ErrorCode   initialize         ( );

   //!< For Tx
   ErrorCode   sendDataAsync      ( const uint8_t* data, size_t length );
   ErrorCode   waitSendComplete   ( uint32_t timeout_ms );
   void        notifySendComplete ( );

   //!< For Rx
   void        flushRxBuffer      ( );

   ErrorCode   pushRxByte         ( uint8_t data );
   ErrorCode   getRxByte          ( uint8_t& data, uint32_t timeout_ms );

private:
   SendFunction             m_sender;
   uint8_t                  m_txBuffer[TX_BUFFER_SIZE];
   lib::RingBuffer<uint8_t> m_rxBuffer;
   lib::ILockable&          m_lockable;
   lib::ISemaphore&         m_semTxComplete;
   lib::ISemaphore&         m_semNewRxBytes;

   bool                     m_isInitialized{ false };
   bool                     m_isSending{ false };
};
} /* namespace lib */
