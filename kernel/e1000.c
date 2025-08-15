#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs) //负责配置网卡寄存器和DMA描述符环，让硬件准备好收发数据
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs; //e1000寄存器的基地址

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST; //清除并重新设置中断
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring)); //发送描述符数组，每个描述符描述一帧要发的数据位置和长度
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
// First ask the E1000 for the TX ring index at which it's expecting the next packet, by reading the E1000_TDT control register.
// 首先向 E1000 询问 TX 环索引 在它期待下一个数据包时，通过读取 E1000_TDT 控制寄存器。
// Then check if the the ring is overflowing. If E1000_TXD_STAT_DD is not set in the descriptor indexed by E1000_TDT, the E1000 hasn't finished the corresponding previous transmission request, so return an error.
// 然后检查环是否溢出。如果未在索引的 E1000_TDT 描述符中设置 ，则 E1000_TXD_STAT_DD E1000 尚未完成相应的上一个传输请求，因此返回错误。
// Otherwise, use mbuffree() to free the last mbuf that was transmitted from that descriptor (if there was one).
// 否则，用于 mbuffree() 释放从该描述符传输的最后一个 mbuf（如果有）。
// Then fill in the descriptor. m->head points to the packet's content in memory, and m->len is the packet length. Set the necessary cmd flags (look at Section 3.3 in the E1000 manual) and stash away a pointer to the mbuf for later freeing.
// 然后填写描述符。 m->head 指向数据包在内存中的内容， m->len 是数据包长度。设置必要的 cmd 标志（查看 E1000 手册中的第 3.3 节）并隐藏指向 mbuf 的指针以供以后释放。
// Finally, update the ring position by adding one to E1000_TDT modulo TX_RING_SIZE.
// 最后，通过添加一个 E1000_TDT 来更新环位置 模 TX_RING_SIZE .
// If e1000_transmit() added the mbuf successfully to the ring, return 0. On failure (e.g., there is no descriptor available to transmit the mbuf), return -1 so that the caller knows to free the mbuf.
// 如果成功将 mbuf 添加到环中，则 e1000_transmit() 返回 0。失败时（例如，没有可用于传输 mbuf 的描述符），返回 -1，以便调用方知道释放 mbuf。
  acquire(&e1000_lock);

  int TX_index = regs[E1000_TDT];

  if ((tx_ring[TX_index].status & E1000_TXD_STAT_DD) == 0) {
    release(&e1000_lock);
    return -1;
  }

  if (tx_mbufs[TX_index])
    mbuffree(tx_mbufs[TX_index]);

  tx_mbufs[TX_index] = m;
  tx_ring[TX_index].length = m->len;
  tx_ring[TX_index].addr = (uint64) m->head;
  tx_ring[TX_index].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
  regs[E1000_TDT] = (TX_index + 1) % TX_RING_SIZE;

  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  while (1) {
    int idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    if ((rx_ring[idx].status & E1000_RXD_STAT_DD) == 0) {
      // 没有新包了
      return;
    }
    rx_mbufs[idx]->len = rx_ring[idx].length;
    // 向上层network stack传输
    net_rx(rx_mbufs[idx]);
    // 把这个下标清空 放置一个空包
    rx_mbufs[idx] = mbufalloc(0);
    rx_ring[idx].status = 0;
    rx_ring[idx].addr = (uint64)rx_mbufs[idx]->head;
    regs[E1000_RDT] = idx;
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
