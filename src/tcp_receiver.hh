#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPReceiver
{
  Wrap32 ISN;           // 初始序列号，第一个syn的序列号
  bool SYN_flag;        // 是否收到SYN
  bool FIN_flag;        // 是否收到FIN
  uint64_t checkpoint;  // 检查点
public:
  TCPReceiver();
  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;
};
