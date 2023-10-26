#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <queue>
#include <deque>

using namespace std;

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t cur_RTO_ms;                      // 当前的超时时间
  queue<TCPSenderMessage> waiting_for_ack;  // 等待被确认的TCP报文段
  deque<TCPSenderMessage> waiting_for_send; // 等待被发送的TCP报文段
  uint64_t receive_window_size;             // 接收方的窗口大小
  uint64_t checkpoint;                      // 检查点
  uint64_t Seqno;                           // 从0开始的64位序列号
  uint64_t nums_in_flight;                  // 还在航线上的字节数，也就是说还没有被确认的字节数
  uint64_t retransmissions_times;           // 重传次数
  enum TCPSender_status
  {
    CLOSED,             // 关闭
    SYN_SENT,           // 等待第二次握手
    ESTABLISHED,        // 建立连接
    FIN_WAIT,           // 等待关闭
    FINISH              // 结束，关闭
  } status;                                 // 发送端的状态
  // 计时器
  uint64_t cur_spent_time;          // 当前经过的时间

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  // 把tcp报文段放进发送队列
  void push_in_que(TCPSenderMessage msg);

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
